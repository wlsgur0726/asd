#include "stdafx.h"
#include "asd/string.h"
#include "asd/threadpool.h"
#include "asd/threadutil.h"
#include "asd/random.h"
#include <atomic>


namespace asdtest_threadpool
{
	template <typename... ARGS>
	void print(ARGS... args)
	{
		::printf(asd::MString::Format(args...));
	}

	using clock = std::chrono::high_resolution_clock;
	using ms = std::chrono::milliseconds;
	using ns = std::chrono::nanoseconds;

	template <typename To, typename From>
	To duration_cast(From from)
	{
		return std::chrono::duration_cast<To>(from);
	}


	const uint64_t ThreadWorkCost = 1			// 1
								//* 512 * 2		// 1K
								//* 512 * 2		// 1M
								//* 512 * 2		// 1G
								;

	std::atomic<uint64_t> g_count;
	struct Counter
	{
		volatile uint64_t count = 0;
		uint64_t Increase()
		{
			for (auto i=ThreadWorkCost; i>0; --i) {
				if (++count == std::numeric_limits<uint64_t>::max())
					throw std::exception();
			}
			g_count += ThreadWorkCost;
			return count;
		}
	};


	template <typename Duration>
	void RunTask(asd::ThreadPool<int>* tp,
				 int range,
				 Duration sleepTime)
	{
		auto task = [=]()
		{
			thread_local Counter t_counter;
			if (sleepTime.count() > 0)
				std::this_thread::sleep_for(sleepTime);
			t_counter.Increase();
			RunTask(tp, range, sleepTime);
		};

		if (range == 0) {
			tp->Push(std::move(task));
		}
		else {
			auto rnd = asd::Random::Uniform<int>(1, range);
			tp->PushSeq(rnd, std::move(task));
		}
	}

	template <typename Key, typename Duration>
	std::vector<std::thread> CreatePushThread(asd::ThreadPool<Key>* tp,
											  Key range,
											  int64_t taskCount,
											  uint32_t threadCount,
											  ms testTime,
											  Duration sleepTime)
	{
		std::vector<std::thread> threads(threadCount);
		for (auto& t : threads) {
			t = std::thread([=]() mutable
			{
				const auto start = clock::now();
				const auto stop = start + testTime;
				const auto interval = duration_cast<ns>(testTime) / (taskCount / threadCount);

				auto task = [=]()
				{
					thread_local Counter t_counter;
					if (sleepTime.count() > 0)
						std::this_thread::sleep_for(sleepTime);
					t_counter.Increase();
				};

				for (auto next=start; next<stop; next+=interval) {
					std::this_thread::sleep_until(next);
					if (range == 0) {
						tp->Push(std::move(task));
					}
					else {
						auto key = asd::Random::Uniform<Key>(1, range);
						tp->PushSeq(key, std::move(task));
					}
				}
			});
		}
		return threads;
	}


	TEST(ThreadPool, PerformanceTest_PushPop)
	{
		g_count = 0;
		const auto SleepMs = ms(1000 * 10);

		asd::ThreadPoolOption opt;
		opt.CollectStats = true;
		opt.UseNotifier = true;

		asd::ThreadPool<int> tp(opt);
		tp.Start();

		print("start\n");
		auto start = clock::now();
		int range = 5000;

#if 0
		auto threads = CreatePushThread(&tp,
										range,
										1000 * SleepMs.count(),
										2,
										SleepMs,
										ms(0));
		for (auto& t : threads)
			t.join();
#else
		for (auto t=opt.StartThreadCount*(1+opt.SpinWaitCount); t>0; --t) {
			RunTask(&tp, range, ms(0));
		}
		std::this_thread::sleep_for(SleepMs);
#endif
		auto stats = tp.GetStats();
		auto stop = clock::now();
		auto elapsed = duration_cast<ms>(stop - start);
		print("stop {}\n", elapsed.count());

		auto total = tp.Stop().totalProcCount.load();
		print("  result         :  {} count per millisec\n", stats.totalProcCount/(double)elapsed.count());
		print("  proc           :  {}\n", stats.totalProcCount.load());
		print("  total          :  {}\n", total);
		print("  conflict       :  {}\n", stats.totalConflictCount.load());
		print("  conflict rate  :  {} %%\n", stats.TotalConflictRate() * 100);
		print("  waiting time   :  {} ms\n", stats.RecentWaitingTimeMs());
		print("  point          :  {}\n", g_count.load() / (double)opt.StartThreadCount / (double)elapsed.count());
	}



	TEST(ThreadPool, SequentialTest)
	{
		const auto useAutoScalingOptions = {true, false};
		for (bool useAutoScaling : useAutoScalingOptions) {
			asd::ThreadPoolOption opt;
			opt.AutoScaling.Use = useAutoScaling;
			opt.CollectStats = true;

			asd::ThreadPool<void*> tp(opt);
			tp.Start();

			const uint64_t TestCount = 100 * 1000;
			const auto ThreadCount = std::thread::hardware_concurrency();

			struct TestObj
			{
				Counter expect;
				Counter check;
			};
			std::vector<TestObj> testObj;
			testObj.resize(ThreadCount);

			for (auto i=TestCount; i>0; --i) {
				for (auto j=ThreadCount; j>0; --j) {
					TestObj* ptr = &testObj[j-1];
					uint64_t expect = ptr->expect.Increase();
					tp.PushSeq((void*)ptr, [ptr, expect]()
					{
						EXPECT_EQ(expect, ptr->check.Increase());
					});
				}
			}

			auto stats = tp.Stop();
			for (auto i=ThreadCount; i>0; --i) {
				Counter& cnt = testObj[i-1].check;
				EXPECT_EQ(cnt.count, TestCount * ThreadWorkCost);
			}

			print("useAutoScaling {}\n", useAutoScaling);
			print("  total          :  {}\n", stats.totalProcCount.load());
			print("  conflict       :  {}\n", stats.totalConflictCount.load());
			print("  conflict rate  :  {} %%\n", stats.TotalConflictRate() * 100);
			print("  waiting time   :  {} ms\n", stats.RecentWaitingTimeMs());
		}
	}



	TEST(ThreadPool, AutoScaling)
	{
		auto startTime = clock::now();
		int range = 5000;

		asd::ThreadPoolOption opt;
		opt.CollectStats = true;
		opt.UseNotifier = true;
		opt.AutoScaling.Use = true;
		opt.UseEmbeddedTimer = true;

		asd::ThreadPool<int> tp(opt);
		uint64_t last = 0;
		auto lastPrintTime = clock::now();
		auto printStats = [&]()
		{
			auto now = clock::now();
			auto stats = tp.GetStats();
			ms totalElapsed = std::max(ms(1), duration_cast<ms>(now - startTime));
			ms elapsed = std::max(ms(1), duration_cast<ms>(now - lastPrintTime));
			double cpu = asd::CpuUsage();

			uint64_t cur = g_count.load();
			print("  total          :  {}\n", stats.totalProcCount.load());
			print("  conflict       :  {}\n", stats.totalConflictCount.load());
			print("  conflict rate  :  {} %%\n", stats.TotalConflictRate() * 100);
			print("  waiting time   :  {} ms\n", stats.RecentWaitingTimeMs());
			print("  threadCount    :  {}\n", stats.threadCount);
			print("  sleepThread    :  {}\n", stats.sleepingThreadCount.load());
			print("  point(total)   :  {}\n", cur / (double)totalElapsed.count());
			print("  point(recent)  :  {}\n", (cur-last) / (double)elapsed.count());
			print("  cpu            :  {} %%\n", cpu * 100);
			print("--------------------------------\n");
			last = cur;
			lastPrintTime = now;
		};

#if 0
		print("cpu bound task\n");
		{
			g_count = 0;
			startTime = clock::now();

			tp.Reset(opt);
			tp.Start();

			ms testTime = ms(1000 * 60);

			for (auto t=asd::Get_HW_Concurrency()*2; t>0; --t)
				RunTask(&tp, range, false);

			for (auto printTime=startTime; printTime<startTime+testTime;) {
				printStats();
				printTime += ms(1000);
				std::this_thread::sleep_until(printTime);
			}

			tp.Stop();
		}
#endif

		print("io bound task\n");
		{
			auto taskCount = range ? range : asd::Get_HW_Concurrency()*1000;

			g_count = 0;
			startTime = clock::now();
			opt.AutoScaling.CpuUsageHigh = 0.95;
			opt.AutoScaling.CpuUsageLow = 0.8;
			opt.StartThreadCount = 100;

			tp.Reset(opt);
			tp.Start();

			ns sleepTime = ns(1000 * 100);
			ms testTime = ms(1000 * 60 * 10);

			auto threads = CreatePushThread(&tp,
											range,
											(sleepTime.count()/500) * testTime.count(),
											2,
											testTime,
											sleepTime);

			for (auto printTime=startTime; printTime<startTime+testTime;) {
				printStats();
				printTime += ms(1000);
				std::this_thread::sleep_until(printTime);
			}
			for (auto& t : threads)
				t.join();
			tp.Stop();
		}
	}
}
