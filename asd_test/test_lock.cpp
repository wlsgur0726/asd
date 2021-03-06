﻿#include "stdafx.h"
#include "asd/string.h"
#include "asd/lock.h"
#include "asd/semaphore.h"
#include "asd/handle.h"
#include <thread>
#include <typeinfo>


namespace asdtest_lock
{
	template <typename MutexType, typename Task>
	void Counting(MutexType&&, size_t threadCount, Task& task)
	{
		asd::puts(typeid(MutexType).name());

		MutexType mutex;

		asd::Semaphore ready;
		volatile bool wait = true;
		volatile bool run = true;
		std::vector<std::thread> threads;
		std::vector<uint64_t> counts;
		uint64_t totalCount = 0;
		threads.resize(threadCount);
		counts.resize(threadCount);

		for (size_t i=0; i<threadCount; ++i) {
			counts[i] = 0;
			threads[i] = std::thread([&](size_t index)
			{
				ready.Post();
				while (wait);
				while (run) {
					auto lock = asd::GetLock(mutex);
					++totalCount;
					++counts[index];
					task();
				}
			}, i);
		}
		for (auto& t : threads)
			ready.Wait();

		auto start = std::chrono::high_resolution_clock::now();
		wait = false;

		std::this_thread::sleep_for(std::chrono::milliseconds(1000));

		run = false;
		for (auto& t : threads)
			t.join();

		auto end = std::chrono::high_resolution_clock::now();
		auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

		double max = 0;
		double total = 0;
		for (auto c : counts) {
			if (max < (double)c)
				max = (double)c;
			total += c;
		}
		ASSERT_EQ(total, totalCount);

		asd::puts(asd::MString::Format("    total  :  {}", totalCount));
		asd::puts(asd::MString::Format("    speed  :  {} count per millisec", total / elapsedMs.count()));
		if (max != 0 && threadCount > 1) {
			asd::puts("    histogram");
			for (size_t i=0; i<threadCount; ++i) {
				auto bar = (counts[i] / max) * 20;;
				asd::MString print;
				for (int j=0; j<bar; ++j)
					print += "=";
				print += asd::MString::Format("    ({}%)", counts[i]/total*100);
				asd::puts(asd::MString::Format("      thread{:02d} {}", i, print));
			}
		}
	}

	template <typename Task>
	void Counting(size_t threadCount, Task task)
	{
		Counting(asd::Mutex(), threadCount, task);
		Counting(asd::SpinMutex(), threadCount, task);
		//Counting(std::mutex(), threadCount, task);
		Counting(std::recursive_mutex(), threadCount, task);
	}


	auto g_smallTask = [](){};
	TEST(Lock, HighContention_SmallTask)
	{
		size_t threadCount = asd::Get_HW_Concurrency();
		if (threadCount <= 1)
			return;
		Counting(threadCount, g_smallTask);
	}
	TEST(Lock, ZeroContention_SmallTask)
	{
		Counting(1, g_smallTask);
	}


	auto g_quingTask = []()
	{
		static std::deque<std::function<void()>> s_queue;
		thread_local uint64_t t_call = 0;

		if (++t_call % 2 == 1)
			s_queue.emplace_back([](){});
		else {
			auto f = std::move(s_queue.front());
			s_queue.pop_front();
			f();
		}
	};
	TEST(Lock, HighContention_QuingTask)
	{
		size_t threadCount = asd::Get_HW_Concurrency();
		if (threadCount <= 1)
			return;
		Counting(threadCount, g_quingTask);
	}
	TEST(Lock, ZeroContention_QuingTask)
	{
		Counting(1, g_quingTask);
	}


	auto g_handleTask = []()
	{
		struct TestData { std::vector<int> data; };
		using TestHandle = asd::Handle<TestData>;

		static TestHandle s_handle;
		static uint64_t s_call = 0;

		const uint64_t Cycle = 100;
		auto step = s_call++ % Cycle;
		if (step == 0)
			s_handle.Alloc();
		else if (step == Cycle-1)
			s_handle.Free();
		else {
			auto p = s_handle.GetObj();
			if (p != nullptr)
				p->data.clear();
		}
	};
	TEST(Lock, HighContention_HandleTask)
	{
		size_t threadCount = asd::Get_HW_Concurrency();
		if (threadCount <= 1)
			return;
		Counting(threadCount, g_handleTask);
	}
	TEST(Lock, ZeroContention_HandleTask)
	{
		Counting(1, g_handleTask);
	}


	TEST(Lock, AsyncMutex)
	{
		asd::AsyncMutex amtx;
		auto Test = [&](int seq)
		{
			auto sleep = [](){std::this_thread::sleep_for(std::chrono::milliseconds(10));};
			asd::puts(asd::MString::Format("seq:{}, ready", seq));

			amtx.GetLock([=](auto lock)
			{
				sleep();
				asd::puts(asd::MString::Format("seq:{}, run1", seq));

				std::thread([=]() mutable
				{
					sleep();
					asd::puts(asd::MString::Format("seq:{}, run2", seq));

					std::thread([=]() mutable
					{
						sleep();
						asd::puts(asd::MString::Format("seq:{}, run3", seq));
						lock.reset();
					}).detach();
				}).detach();
			});
		};

		for (int i=1; i<=5; ++i)
			Test(i);

		asd::Semaphore wait;
		amtx.GetLock([&](asd::AsyncMutex::Lock) { wait.Post(); });
		wait.Wait();
	}
}
