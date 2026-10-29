#pragma once

#include <atomic>
#include <cstdint>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

// The software device's threads: a pool of workers that, with the thread
// that runs the phase, share out each phase of a frame's drawing -- the
// chunks set up, the tiles drawn, the present's rows -- an item at a time,
// the phase returning once all are done. Between phases the workers sleep.
// A job may run on a thread of its own (Start), its phases on the pool,
// while the game's thread records what comes next.
class SoftwareWorkers
{
public:
	// threads: all of them, the game's own among them
	explicit SoftwareWorkers(int threads);
	~SoftwareWorkers();

	SoftwareWorkers(const SoftwareWorkers&) = delete;
	SoftwareWorkers& operator=(const SoftwareWorkers&) = delete;

	int Threads() const { return (int)Pool.size() + 1; }

	// work(item, thread) for every item from 0 to count, thread 0 the
	// caller's; returns when every item is done. callerShares: the caller
	// takes items too, else it waits asleep for the workers.
	void Run(int count, const std::function<void(int item, int thread)>& work, bool callerShares = true);

	// A job on the background thread, which may Run phases; the caller goes
	// on, and Waits for it before it Runs or Starts anything itself
	void Start(std::function<void()> job);
	void Wait();
	bool Busy() const { return Pending.load(std::memory_order_acquire); }

	// The threads a machine gives the device: each of its hardware threads,
	// or DXE_SW_THREADS's number
	static int DefaultThreads();

private:
	void Worker(int thread);
	void Share(int thread);
	void Background();

	std::vector<std::thread> Pool;
	std::mutex Mutex;
	std::condition_variable Wake;
	std::condition_variable PhaseDone; // the last worker's, for a caller that waits asleep
	std::atomic<uint64_t> Generation{ 0 }; // a phase each, read by the workers
	std::atomic<bool> Exiting{ false };

	// The phase's: written before its generation is, read after
	const std::function<void(int, int)>* Work = nullptr;
	int Count = 0;
	std::atomic<int> Next{ 0 };
	std::atomic<int> Finished{ 0 }; // the workers done with the phase

	// The background thread, made with the first job
	std::thread BackgroundThread;
	std::mutex BackgroundMutex;
	std::condition_variable BackgroundWake;
	std::condition_variable BackgroundDone;
	std::function<void()> Job;
	std::atomic<bool> Pending{ false };
	bool StopBackground = false;
	bool NoBackground = false; // the system refused it: jobs run in Start
};
