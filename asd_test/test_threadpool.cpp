#include "stdafx.h"
#include "asd/string.h"
#include "asd/threadpool.h"
#include "asd/threadutil.h"
#include "asd/random.h"
#include <atomic>


namespace asdtest_threadpool
{
	const int SleepMs = 1000 * 1;
	const uint64_t ThreadWorkCost = 1			// 1 B
								* 512 * 2		// 1 KB
								//* 512 * 2		// 1 MB
								//* 512 * 2		// 1 GB
								;

	std::atomic<uint64_t> g_count;
	struct Counter
	{
		uint64_t m_count = 0;
		void Increase()
		{
			for (auto i=ThreadWorkCost; i>0; --i) {
				if (++m_count == std::numeric_limits<uint64_t>::max())
					throw std::exception();
			}
		}
		~Counter()
		{
			g_count += m_count;
		}
	};
	thread_local Counter t_counter;


	void Push_Counter(asd::ThreadPool* tp)
	{
		tp->Push([tp]()
		{
			t_counter.Increase();
			Push_Counter(tp);
		});
	}


	void Push_Counter(asd::SequentialThreadPool<uint32_t>* tp)
	{
		auto rnd = asd::Random::Uniform<uint32_t>(0, 999);
		tp->PushSeq(rnd, [tp]()
		{
			t_counter.Increase();
			Push_Counter(tp);
		});
	}


	template <typename ThreadPoolType>
	void PerformanceTest_PushPop(ThreadPoolType& tp)
	{
		g_count = 0;
		tp.Start();

		auto start = std::chrono::high_resolution_clock::now();
		auto threadcount = std::thread::hardware_concurrency();
		for (; threadcount!=0; --threadcount)
			Push_Counter(&tp);

		std::this_thread::sleep_for(std::chrono::milliseconds(SleepMs));
		tp.Stop(false);
		auto end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::milli> elapsed = end - start;

		printf("g_count :  %llu\n", (uint64_t)g_count);
		printf("result  :  %lf count per millisec\n", (g_count/ThreadWorkCost)/elapsed.count());
	}



	TEST(ThreadPool, PerformanceTest_PushPop_ThreadPool)
	{
		asd::ThreadPool tp;
		PerformanceTest_PushPop<asd::ThreadPool>(tp);
	}



	TEST(ThreadPool, PerformanceTest_PushPop_SequentialThreadPool)
	{
		asd::SequentialThreadPool<uint32_t> tp;
		PerformanceTest_PushPop<asd::SequentialThreadPool<uint32_t>>(tp);
		auto stats = tp.GetStats();
		printf("total          :  %llu\n", stats.totalProcCount);
		printf("conflict       :  %llu\n", stats.totalConflictCount);
		printf("conflict rate  :  %lf\n", stats.TotalConflictRate());
		printf("waiting time   :  %lf\n", stats.RecentWaitingTimeMs());
	}



	TEST(ThreadPool, SequentialTest)
	{
		asd::SequentialThreadPool<void*> tp;
		tp.Start();

		const uint64_t TestCount = 12345;
		const auto ThreadCount = std::thread::hardware_concurrency();
		std::vector<Counter> counter;
		counter.resize(ThreadCount);

		for (auto i=TestCount; i>0; --i) {
			for (auto j=ThreadCount; j>0; --j) {
				Counter* ptr = &counter[j-1];
				tp.PushSeq((void*)ptr, [ptr]()
				{
					ptr->Increase();
				});
			}
		}

		tp.Stop();
		for (auto i=ThreadCount; i>0; --i) {
			Counter& cnt = counter[i-1];
			EXPECT_EQ(cnt.m_count, TestCount * ThreadWorkCost);
		}

		auto stats = tp.GetStats();
		printf("total          :  %llu\n", stats.totalProcCount);
		printf("conflict       :  %llu\n", stats.totalConflictCount);
		printf("conflict rate  :  %lf\n", stats.TotalConflictRate());
		printf("waiting time   :  %lf\n", stats.RecentWaitingTimeMs());
	}
}
