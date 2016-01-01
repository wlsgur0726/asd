#include "stdafx.h"
#include <vector>
#include <thread>
#include "asd/threadpool.h"

namespace asd
{
	struct Job {
		ThreadPool::Routine m_routine = nullptr;
		void* m_parameter = nullptr;
	};
	struct WorkerThread {
		std::thread m_thread;
	};
	struct ThreadPoolData {
		std::vector<WorkerThread*> m_threads;
	};

	ThreadPool::ThreadPool() {
	}

	ThreadPool::~ThreadPool() {
	}
}