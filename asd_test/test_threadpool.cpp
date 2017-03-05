#include "stdafx.h"
#include "asd/string.h"
#include "asd/threadpool.h"
#include "asd/threadutil.h"
#include <atomic>


namespace asdtest_threadpool
{
	const int SleepMs = 1000 * 1;
	const uint64_t ThreadWorkCost = 1
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
		tp->PushTask([tp]()
		{
			t_counter.Increase();
			Push_Counter(tp);
		});
	}


	void Push_Counter(asd::SequentialThreadPool<uint32_t>* tp)
	{
		tp->PushTask(rand() % 1000, [tp](uint32_t&)
		{
			thread_local bool srandInit = false;
			if (srandInit == false) {
				srandInit = true;
				asd::srand();
			}

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
		auto report = tp.GetReport();
		printf("total          :  %llu\n", report.totalProcCount);
		printf("conflict       :  %llu\n", report.conflictCount);
		printf("conflict rate  :  %lf\n", report.GetConflictRate());
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
				tp.PushTask(ptr, [ptr](void*& key)
				{
					EXPECT_EQ(ptr, key);
					ptr->Increase();
				});
			}
		}

		tp.Stop();
		for (auto i=ThreadCount; i>0; --i) {
			Counter& cnt = counter[i-1];
			EXPECT_EQ(cnt.m_count, TestCount * ThreadWorkCost);
		}

		auto report = tp.GetReport();
		printf("total          :  %llu\n", report.totalProcCount);
		printf("conflict       :  %llu\n", report.conflictCount);
		printf("conflict rate  :  %lf\n", report.GetConflictRate());
	}
}
