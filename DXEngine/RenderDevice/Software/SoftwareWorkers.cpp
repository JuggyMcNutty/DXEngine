
#include "Precomp.h"
#include "SoftwareWorkers.h"
#include <algorithm>
#include <cstdlib>
#include <system_error>

#if defined(__linux__)
#include <sched.h>
#include <fstream>
#include <string>
#endif

#if defined(USE_SSE2)
#include <emmintrin.h>
#endif

// A short wait in a spin
static inline void Pause()
{
#if defined(USE_SSE2)
	_mm_pause();
#elif defined(__aarch64__)
	asm volatile("yield");
#endif
}

#if defined(__linux__)
// The CPUs a cgroup's quota gives, rounded up, 0 for no quota: cgroup v2's
// cpu.max ("max" or "<quota> <period>") of the process's group and of each
// group above it, the smallest taken; with none, cgroup v1's cpu.cfs_quota_us
// over cpu.cfs_period_us at the cpu controller's mount (a container's own)
static int CgroupCpus()
{
	auto quotaCpus = [](long long quota, long long period) -> int {
		if (quota <= 0 || period <= 0)
			return 0;
		return (int)std::min<long long>((quota + period - 1) / period, 1 << 20);
	};

	int cpus = 0;
	std::string group;
	{
		std::ifstream file("/proc/self/cgroup");
		std::string line;
		while (std::getline(file, line))
		{
			if (line.compare(0, 3, "0::") == 0)
			{
				group = line.substr(3);
				break;
			}
		}
	}
	while (!group.empty())
	{
		std::ifstream file("/sys/fs/cgroup" + (group == "/" ? std::string() : group) + "/cpu.max");
		std::string quota;
		long long period = 0;
		if (file >> quota >> period && quota != "max")
		{
			int count = quotaCpus(std::atoll(quota.c_str()), period);
			if (count > 0 && (cpus == 0 || count < cpus))
				cpus = count;
		}
		size_t slash = group.find_last_of('/');
		if (group.size() <= 1 || slash == std::string::npos)
			break;
		group = slash == 0 ? "/" : group.substr(0, slash);
	}
	if (cpus > 0)
		return cpus;

	std::ifstream quotaFile("/sys/fs/cgroup/cpu/cpu.cfs_quota_us");
	std::ifstream periodFile("/sys/fs/cgroup/cpu/cpu.cfs_period_us");
	long long quota = 0, period = 0;
	if (quotaFile >> quota && periodFile >> period)
		cpus = quotaCpus(quota, period);
	return cpus;
}
#endif

int SoftwareWorkers::DefaultThreads()
{
	if (const char* threads = std::getenv("DXE_SW_THREADS"))
	{
		int count = std::atoi(threads);
		if (count >= 1)
			return std::min(count, 256);
	}

	// The CPUs the process may run on, which an affinity mask (taskset, a
	// container's cpuset) or a quota holds below the machine's: each phase
	// waits for every worker, so a thread more than these waits its turn on
	// one of them (bugs.md REN.82)
	int threads = std::max((int)std::thread::hardware_concurrency(), 1);
#if defined(__linux__)
	cpu_set_t allowed;
	CPU_ZERO(&allowed);
	if (sched_getaffinity(0, sizeof(allowed), &allowed) == 0 && CPU_COUNT(&allowed) > 0)
		threads = std::min(threads, (int)CPU_COUNT(&allowed));
	if (int quota = CgroupCpus(); quota > 0)
		threads = std::min(threads, quota);
#elif defined(WIN32)
	DWORD_PTR processMask = 0, systemMask = 0;
	if (GetProcessAffinityMask(GetCurrentProcess(), &processMask, &systemMask) && processMask != 0)
	{
		int allowed = 0;
		for (DWORD_PTR mask = processMask; mask; mask &= mask - 1)
			allowed++;
		threads = std::min(threads, allowed);
	}
#endif
	return threads;
}

SoftwareWorkers::SoftwareWorkers(int threads)
{
	// A thread the system refuses (a low process limit) ends the pool at the
	// threads it got, which share the work among them: the device runs on,
	// where a throw out of here would leave the threads made unjoined and
	// std::terminate the game at the pool's destruction
	for (int i = 1; i < threads; i++)
	{
		try
		{
			Pool.emplace_back([this, i]() { Worker(i); });
		}
		catch (const std::system_error&)
		{
			break;
		}
	}
}

SoftwareWorkers::~SoftwareWorkers()
{
	if (BackgroundThread.joinable())
	{
		try
		{
			Wait();
		}
		catch (...)
		{
			// The device's own Wait has taken any throw before it lets the
			// workers go
		}
		{
			std::lock_guard<std::mutex> lock(BackgroundMutex);
			StopBackground = true;
		}
		BackgroundWake.notify_all();
		BackgroundThread.join();
	}

	{
		std::lock_guard<std::mutex> lock(Mutex);
		Exiting = true;
		Generation.fetch_add(1, std::memory_order_release);
	}
	Wake.notify_all();
	for (std::thread& thread : Pool)
		thread.join();
}

void SoftwareWorkers::Run(int count, const std::function<void(int item, int thread)>& work, bool callerShares)
{
	if (count <= 0)
		return;
	if (Pool.empty() || count == 1)
	{
		for (int i = 0; i < count; i++)
			work(i, 0);
		return;
	}

	// The phase published, then shared: no worker is left in the last one,
	// each having finished it before Run returned
	Work = &work;
	Count = count;
	Next.store(0, std::memory_order_relaxed);
	Finished.store(0, std::memory_order_relaxed);
	{
		std::lock_guard<std::mutex> lock(Mutex);
		Generation.fetch_add(1, std::memory_order_release);
	}
	Wake.notify_all();

	if (callerShares)
	{
		Share(0);
		while (Finished.load(std::memory_order_acquire) < (int)Pool.size())
			Pause();
	}
	else
	{
		std::unique_lock<std::mutex> lock(Mutex);
		PhaseDone.wait(lock, [&]() { return Finished.load(std::memory_order_acquire) == (int)Pool.size(); });
	}

	// An item's throw, on the caller now that no thread reads the phase:
	// on a worker it would std::terminate the game with nothing logged
	if (Error)
	{
		std::exception_ptr error = std::move(Error);
		Error = nullptr;
		std::rethrow_exception(error);
	}
}

void SoftwareWorkers::Share(int thread)
{
	for (;;)
	{
		int item = Next.fetch_add(1, std::memory_order_relaxed);
		if (item >= Count)
			break;
		try
		{
			(*Work)(item, thread);
		}
		catch (...)
		{
			// The first kept for Run; no item is begun after it
			std::lock_guard<std::mutex> lock(ErrorMutex);
			if (!Error)
				Error = std::current_exception();
			Next.store(Count, std::memory_order_relaxed);
		}
	}
}

void SoftwareWorkers::Worker(int thread)
{
	uint64_t seen = 0;
	for (;;)
	{
		// The next phase: spun for a while, as a frame's phases follow each
		// other closely, then waited for asleep
		uint64_t generation = Generation.load(std::memory_order_acquire);
		for (int spin = 0; generation == seen && spin < 4000; spin++)
		{
			Pause();
			generation = Generation.load(std::memory_order_acquire);
		}
		if (generation == seen)
		{
			std::unique_lock<std::mutex> lock(Mutex);
			Wake.wait(lock, [&]() { return Generation.load(std::memory_order_acquire) != seen; });
			generation = Generation.load(std::memory_order_acquire);
		}
		seen = generation;
		if (Exiting)
			return;

		Share(thread);
		if (Finished.fetch_add(1, std::memory_order_acq_rel) + 1 == (int)Pool.size())
		{
			// The last: a caller waiting asleep woken
			std::lock_guard<std::mutex> lock(Mutex);
			PhaseDone.notify_all();
		}
	}
}

void SoftwareWorkers::Start(std::function<void()> job)
{
	// A thread the system refuses (a low process limit) leaves the job to
	// this one, as the pool's are shared out: its throw left Lock or a draw
	if (!BackgroundThread.joinable() && !NoBackground)
	{
		try
		{
			BackgroundThread = std::thread([this]() { Background(); });
		}
		catch (const std::system_error&)
		{
			NoBackground = true;
		}
	}
	if (NoBackground)
	{
		job();
		return;
	}
	{
		std::lock_guard<std::mutex> lock(BackgroundMutex);
		Job = std::move(job);
		Pending.store(true, std::memory_order_release);
	}
	BackgroundWake.notify_one();
}

void SoftwareWorkers::Wait()
{
	if (Pending.load(std::memory_order_acquire))
	{
		std::unique_lock<std::mutex> lock(BackgroundMutex);
		BackgroundDone.wait(lock, [&]() { return !Pending.load(std::memory_order_acquire); });
	}
	// Set before Pending was cleared, so seen once it is
	if (BackgroundError)
	{
		std::exception_ptr error = std::move(BackgroundError);
		BackgroundError = nullptr;
		std::rethrow_exception(error);
	}
}

void SoftwareWorkers::Background()
{
	std::unique_lock<std::mutex> lock(BackgroundMutex);
	for (;;)
	{
		BackgroundWake.wait(lock, [&]() { return Job != nullptr || StopBackground; });
		if (StopBackground)
			return;
		std::function<void()> job = std::move(Job);
		Job = nullptr;
		lock.unlock();
		std::exception_ptr error;
		try
		{
			job();
		}
		catch (...)
		{
			error = std::current_exception();
		}
		lock.lock();
		BackgroundError = error;
		Pending.store(false, std::memory_order_release);
		BackgroundDone.notify_all();
	}
}
