
#include "Precomp.h"
#include "SoftwareWorkers.h"
#include <algorithm>
#include <cstdlib>

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

int SoftwareWorkers::DefaultThreads()
{
	if (const char* threads = std::getenv("DXE_SW_THREADS"))
	{
		int count = std::atoi(threads);
		if (count >= 1)
			return std::min(count, 256);
	}
	return std::max((int)std::thread::hardware_concurrency(), 1);
}

SoftwareWorkers::SoftwareWorkers(int threads)
{
	for (int i = 1; i < threads; i++)
		Pool.emplace_back([this, i]() { Worker(i); });
}

SoftwareWorkers::~SoftwareWorkers()
{
	{
		std::lock_guard<std::mutex> lock(Mutex);
		Exiting = true;
		Generation.fetch_add(1, std::memory_order_release);
	}
	Wake.notify_all();
	for (std::thread& thread : Pool)
		thread.join();
}

void SoftwareWorkers::Run(int count, const std::function<void(int item, int thread)>& work)
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

	Share(0);
	while (Finished.load(std::memory_order_acquire) < (int)Pool.size())
		Pause();
}

void SoftwareWorkers::Share(int thread)
{
	for (;;)
	{
		int item = Next.fetch_add(1, std::memory_order_relaxed);
		if (item >= Count)
			break;
		(*Work)(item, thread);
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
		Finished.fetch_add(1, std::memory_order_acq_rel);
	}
}
