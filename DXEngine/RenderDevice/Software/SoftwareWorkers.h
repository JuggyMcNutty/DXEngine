#pragma once

#include <atomic>
#include <cstdint>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

// The software device's threads: a pool of workers that, with the game's
// thread, share out each phase of a frame's drawing -- the chunks set up,
// the tiles drawn, the present's rows -- an item at a time, the game's
// thread returning once all are done. Between phases the workers sleep.
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
	// game's; returns when every item is done
	void Run(int count, const std::function<void(int item, int thread)>& work);

	// The threads a machine gives the device: each of its hardware threads,
	// or DXE_SW_THREADS's number
	static int DefaultThreads();

private:
	void Worker(int thread);
	void Share(int thread);

	std::vector<std::thread> Pool;
	std::mutex Mutex;
	std::condition_variable Wake;
	std::atomic<uint64_t> Generation{ 0 }; // a phase each, read by the workers
	std::atomic<bool> Exiting{ false };

	// The phase's: written before its generation is, read after
	const std::function<void(int, int)>* Work = nullptr;
	int Count = 0;
	std::atomic<int> Next{ 0 };
	std::atomic<int> Finished{ 0 }; // the workers done with the phase
};
