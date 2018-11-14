#include "stdafx.h"
#include "asd/string.h"
#include "asd/threadpool.h"
#include "asd/threadutil.h"
#include "asd/random.h"
#include "asd/sysres.h"
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


	struct PerfTestReport
	{
		uint64_t lastCount;
		clock::time_point beginTime;
		clock::time_point lastPrintTime;

		PerfTestReport()
		{
			lastCount = 0;
			beginTime = lastPrintTime = clock::now();
		}

		void printStats(const asd::ThreadPoolStats& stats)
		{
			auto now = clock::now();
			ms totalElapsed = std::max(ms(1), duration_cast<ms>(now - beginTime));
			ms elapsed = std::max(ms(1), duration_cast<ms>(now - lastPrintTime));
			double cpu = asd::CpuUsage();

			auto push = stats.totalPushCount.load();
			auto proc = stats.totalProcCount.load();
			print("  totalElapsed   :  {} sec\n", totalElapsed.count() / 1000.0);
			print("  cpu            :  {} %%\n", cpu * 100);
			print("  push           :  {}\n", push);
			print("  proc           :  {}\n", proc);
			print("  push - proc    :  {}\n", push - proc);
			print("  sleep thread   :  {}\n", stats.sleepingThreadCount.load());
			print("  total thread   :  {}\n", stats.threadCount);
			print("  waiting time   :  {} ms\n", stats.RecentWaitingTimeMs());
			print("  speed(total)   :  {} cnt per ms\n", proc / (double)totalElapsed.count());
			print("  speed(recent)  :  {} cnt per ms\n", (proc-lastCount) / (double)elapsed.count());
			print("  conflict       :  {}\n", stats.totalConflictCount.load());
			print("  conflict rate  :  {} %%\n", stats.TotalConflictRate() * 100);
			print("---------------------------------------------\n");
			lastCount = proc;
			lastPrintTime = now;
		};
	};

	using TaskGeneratorFunc = std::function<asd::Task_ptr()>;

	TaskGeneratorFunc TaskGenerator_IO(ns procTime, uint64_t loopCount)
	{
		return [=]() -> asd::Task_ptr {
			return asd::CreateTask([](ns procTime, uint64_t loopCount) {
				for (uint64_t i=0; i<loopCount; ++i)
					std::this_thread::sleep_for(procTime);
			}, procTime, loopCount);
		};
	}


	TaskGeneratorFunc TaskGenerator_CPU(ns procTime, uint64_t loopCount)
	{
		return [=]() -> asd::Task_ptr {
			return asd::CreateTask([](ns procTime, uint64_t loopCount) {
				for (uint64_t i=0; i<loopCount; ++i) {
					auto now = clock::now();
					auto target = now + procTime;
					for (; now < target; now = clock::now());
				}
			}, procTime, loopCount);
		};
	}


	using PushFunc = std::function<void(int64_t seqKey, asd::Task_ptr&& task)>;

	template <typename ThreadPool>
	PushFunc CreatePushFunc(ThreadPool& tp)
	{
		return [&](int64_t seqKey, asd::Task_ptr&& task)
		{
			tp.Push(std::move(task));
		};
	}

	template <typename ThreadPool>
	PushFunc CreatePushSeqFunc(ThreadPool& tp)
	{
		return [&](int64_t seqKey, asd::Task_ptr&& task)
		{
			tp.PushSeq(seqKey, std::move(task));
		};
	}


	template <typename ThreadPool>
	void SelfPushTask(std::atomic_bool& run,
					  ThreadPool& tp,
					  PushFunc pushFunc,
					  int64_t SeqKeyRangeBegin,
					  int64_t SeqKeyRangeEnd)
	{
		auto seqKey = asd::Random::Uniform<int64_t>(SeqKeyRangeBegin, SeqKeyRangeEnd);
		pushFunc(seqKey, asd::CreateTask([=, &run, &tp]() mutable
		{
			if (run)
				SelfPushTask(run, tp, pushFunc, SeqKeyRangeBegin, SeqKeyRangeEnd);
		}));
	}


	struct LoadGeneratorOption
	{
		double PushSpeedPerMs = 100;
		int64_t SeqKeyRangeBegin = 1;
		int64_t SeqKeyRangeEnd = 1000;
		TaskGeneratorFunc TaskGenerator;
		PushFunc PushFunc;
	};

	class LoadGenerator
	{
	public:
		LoadGenerator(const LoadGeneratorOption& option) : m_option(option) {}
		void Start() { m_thread = std::thread(std::mem_fn(&LoadGenerator::PushThread), this); }
		void Stop() { m_run = false; }
		void Join()
		{
			Stop();
			if (m_thread.joinable())
				m_thread.join();
		}
		~LoadGenerator() { Join(); }

	private:
		void PushThread()
		{
			auto termNs = (typename ns::rep)(duration_cast<ns>(ms(1)).count() / m_option.PushSpeedPerMs);
			auto term = ns(termNs);
			for (auto tp=clock::now(); m_run; tp+=term) {
				std::this_thread::sleep_until(tp);
				
				auto seqKey = asd::Random::Uniform<int64_t>(m_option.SeqKeyRangeBegin, m_option.SeqKeyRangeEnd);
				auto task = m_option.TaskGenerator();
				m_option.PushFunc(seqKey, std::move(task));
			}
		}
		const LoadGeneratorOption& m_option;
		std::thread m_thread;
		volatile bool m_run = true;
	};

	class LoadGeneratorSet
	{
	public:
		void Start(LoadGeneratorOption& opt, size_t threadCount, double totalPushSpeedPerMs)
		{
			opt.PushSpeedPerMs = totalPushSpeedPerMs / threadCount;
			for (size_t i=0; i<threadCount; ++i)
				m_list.emplace_back(std::unique_ptr<LoadGenerator>(new LoadGenerator(opt)));
			for (auto& lg : m_list)
				lg->Start();
		}

		void Stop()
		{
			for (auto& lg : m_list)
				lg->Stop();
			m_list.clear();
		}

		~LoadGeneratorSet() { Stop(); }

	private:
		std::vector<std::unique_ptr<LoadGenerator>> m_list;
	};


	template <typename ThreadPool>
	void PushPopOverheadTest(ThreadPool& tp, PushFunc pushFunc)
	{
		PerfTestReport report;

		std::atomic_bool run;
		run = true;
		for (size_t i=0; i<asd::Get_HW_Concurrency(); ++i)
			SelfPushTask(run, tp, pushFunc, 1, 1000);

		std::this_thread::sleep_for(ms(1000 * 10));
		run = false;
		report.printStats(tp.Stop());
	}

	TEST(ThreadPool, PushPopOverheadTest_ThreadPool_NonSeq)
	{
		asd::ThreadPoolOption tpopt;
		tpopt.CollectStats = true;
		//tpopt.UseNotifier = false;
		//tpopt.PickAlgorithm = asd::ThreadPoolOption::Pick::RoundRobin;
		asd::ThreadPool tp(tpopt);
		tp.Start();

		PushPopOverheadTest(tp, CreatePushFunc(tp));
	}

	TEST(ThreadPool, PushPopOverheadTest_ThreadPool_Seq)
	{
		asd::ThreadPoolOption tpopt;
		tpopt.CollectStats = true;
		//tpopt.UseNotifier = false;
		//tpopt.PickAlgorithm = asd::ThreadPoolOption::Pick::RoundRobin;
		asd::ThreadPool tp(tpopt);
		tp.Start();

		PushPopOverheadTest(tp, CreatePushSeqFunc(tp));
	}

	TEST(ThreadPool, PushPopOverheadTest_ScalableThreadPool)
	{
		asd::ScalableThreadPoolOption tpopt;
		tpopt.MinWorkerCount = tpopt.MaxWorkerCount = asd::Get_HW_Concurrency();
		asd::ScalableThreadPool tp(tpopt);

		PushPopOverheadTest(tp, CreatePushFunc(tp));
	}


	template <typename ThreadPool>
	void StressTest(ThreadPool& tp, bool isCpuBoundTask, PushFunc pushFunc)
	{
		const uint64_t CpuTaskLoad = 100; // microsec

		LoadGeneratorOption lgopt;
		lgopt.PushFunc = std::move(pushFunc);
		lgopt.TaskGenerator = isCpuBoundTask
			? TaskGenerator_CPU(ns(1000), CpuTaskLoad)
			: TaskGenerator_IO(duration_cast<ns>(ms(2)), 2);

		LoadGeneratorSet lgs;
		double pushSpeed = isCpuBoundTask
			? (asd::Get_HW_Concurrency() * (1000/CpuTaskLoad)) * 0.8
			: 80;

		PerfTestReport report;
		lgs.Start(lgopt, 2, pushSpeed);

		for (int sec=1; sec<=60*3; ++sec) {
			std::this_thread::sleep_for(ms(1000));
			report.printStats(tp.GetStats());
		}

		lgs.Stop();
		report.printStats(tp.Stop());

	}

	TEST(ThreadPool, StressTest_CPU_ThreadPool_NonSeq)
	{
		asd::ThreadPoolOption tpopt;
		tpopt.CollectStats = true;
		//tpopt.UseNotifier = false;
		tpopt.UseEmbeddedTimer = true;

		asd::ThreadPool tp(tpopt);
		tp.Start();

		StressTest(tp, true, CreatePushFunc(tp));
	}

	TEST(ThreadPool, StressTest_CPU_ThreadPool_Seq)
	{
		asd::ThreadPoolOption tpopt;
		tpopt.CollectStats = true;
		//tpopt.UseNotifier = false;
		tpopt.UseEmbeddedTimer = true;

		asd::ThreadPool tp(tpopt);
		tp.Start();

		StressTest(tp, true, CreatePushSeqFunc(tp));
	}

	TEST(ThreadPool, StressTest_CPU_ScalableThreadPool)
	{
		asd::ScalableThreadPoolOption tpopt;
		tpopt.MinWorkerCount = tpopt.MaxWorkerCount = asd::Get_HW_Concurrency();
		asd::ScalableThreadPool tp(tpopt);

		StressTest(tp, true, CreatePushFunc(tp));
	}

	TEST(ThreadPool, StressTest_IO_ScalableThreadPool)
	{
		asd::ScalableThreadPoolOption tpopt;
		asd::ScalableThreadPool tp(tpopt);

		StressTest(tp, false, CreatePushFunc(tp));
	}


	TEST(ThreadPool, SequentialTest)
	{
		asd::ThreadPoolOption tpopt;
		tpopt.CollectStats = true;

		asd::ThreadPool tp(tpopt);
		tp.Start();

		const auto pushThreadCount = asd::Get_HW_Concurrency();
		const auto keyCount = pushThreadCount * 10;
		auto counts = new std::atomic<uint64_t>[keyCount];
		for (uint32_t i=0; i<keyCount; ++i)
			counts[i] = 0;

		PerfTestReport report;

		std::vector<std::thread> pushThreads;
		for (uint32_t i=0; i<pushThreadCount; ++i) {
			pushThreads.emplace_back(std::thread([&](uint32_t index) mutable
			{
				auto myKeyCount = keyCount / pushThreadCount;
				auto beginKey = myKeyCount * index;
				const uint64_t pushUnit = 10;
				for (uint64_t n=0; n<pushUnit*1000; n+=pushUnit) {
					for (int64_t k=0; k<myKeyCount; ++k) {
						int64_t key = k + beginKey;
						for (uint64_t num=n; num<n+pushUnit; ++num) {
							tp.PushSeq(key, [&counts](int64_t key, uint64_t num) mutable {
								auto& count = counts[key];
								auto exp = num;
								count.compare_exchange_strong(exp, num+1);
								EXPECT_EQ(exp, num);
							}, key, num);
						}
					}
				}
			}, i));
		}

		for (auto& thread : pushThreads)
			thread.join();
		report.printStats(tp.Stop());
		delete[] counts;
	}

	template <typename ThreadPool>
	int TimerTestMore(ThreadPool& tp,
					  asd::Timer::TimePoint pushTime, 
					  ms timeout,
					  std::atomic<int>& count)
	{
		tp; pushTime; timeout; count;
		return 0;
	}

	template <typename ThreadPool>
	void TimerTest(ThreadPool& tp)
	{
		std::atomic<int> count;
		count = 0;

		ms timeout(1000);
		auto pushTime = asd::Timer::Now();

		tp.Push(pushTime + timeout, [&]()
		{
			auto elapsed = asd::Timer::Now() - pushTime;
			EXPECT_GE(elapsed, timeout);
			++count;
		});

		tp.Push(timeout, [&]()
		{
			auto elapsed = asd::Timer::Now() - pushTime;
			EXPECT_GE(elapsed, timeout);
			++count;
		});

		int moreTestCount = TimerTestMore(tp, pushTime, timeout, count);

		std::this_thread::sleep_for(timeout + ms(5));
		tp.Stop();
		EXPECT_EQ(2 + moreTestCount, count.load());
	}

	template <>
	int TimerTestMore<asd::ThreadPool>(asd::ThreadPool& tp,
									   asd::Timer::TimePoint pushTime,
									   ms timeout,
									   std::atomic<int>& count)
	{
		tp.PushSeq(pushTime + timeout, 0, [&]()
		{
			auto elapsed = asd::Timer::Now() - pushTime;
			EXPECT_GE(elapsed, timeout);
			++count;
		});

		tp.PushSeq(pushTime + timeout, 0, [&]()
		{
			auto elapsed = asd::Timer::Now() - pushTime;
			EXPECT_GE(elapsed, timeout);
			++count;
		});

		return 2;
	}

	TEST(ThreadPool, TimerTest_ThreadPool)
	{
		asd::ThreadPoolOption tpopt;
		asd::ThreadPool tp(tpopt);
		tp.Start();
		TimerTest(tp);
	}

	TEST(ThreadPool, TimerTest_ScalableThreadPool)
	{
		asd::ScalableThreadPoolOption tpopt;
		asd::ScalableThreadPool tp(tpopt);
		TimerTest(tp);
	}
}
