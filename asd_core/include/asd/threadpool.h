#pragma once
#include "asdbase.h"
#include "timer.h"


namespace asd
{
	struct ThreadPoolStats
	{
		struct atomic_t : public std::atomic<uint64_t>
		{
			using Base = std::atomic<uint64_t>;
			using Base::Base;
			using Base::operator=;

			atomic_t()
			{
				Base::operator=(0);
			}

			atomic_t(const atomic_t& a_copy)
			{
				Base::operator=(a_copy.load());
			}

			atomic_t& operator=(const atomic_t& a_copy)
			{
				Base::operator=(a_copy.load());
				return *this;
			}
		};

		uint64_t WaitingCount() const
		{
			return totalPushCount - totalProcCount;
		}

		double RecentWaitingTimeMs() const
		{
			if (recentPushCount == 0)
				return 0;

			double period = (double)recentPeriod.count();
			if (period == 0)
				return std::numeric_limits<double>::max();

			return (double)recentWaitingCount / (recentPushCount / period);
		}

		double TotalConflictRate() const
		{
			if (totalPushCount == 0)
				return 0;
			return totalConflictCount / (double)totalPushCount;
		}

		void Refresh()
		{
			auto now = Timer::Now();
			recentPeriod = Timer::Diff(recentRefreshTime, now);
			recentRefreshTime = now;

			recentPushCount = totalPushCount - recentTotalPushCount;
			recentTotalPushCount = totalPushCount;

			recentWaitingCount = WaitingCount();
		}

		uint64_t Push()
		{
			return ++totalPushCount;
		}

		uint64_t Pop()
		{
			return ++totalProcCount;
		}

		atomic_t totalPushCount;
		atomic_t totalProcCount;
		atomic_t totalConflictCount;

		uint64_t recentPushCount = 0;
		uint64_t recentTotalPushCount = 0;
		uint64_t recentWaitingCount = 0;

		Timer::Millisec recentPeriod = Timer::Millisec(0);
		Timer::TimePoint recentRefreshTime = Timer::Now();

		uint64_t threadCount = 0;
		atomic_t sleepingThreadCount;
	};



	struct ThreadPoolOption
	{
		uint32_t ThreadCount = Get_HW_Concurrency();

		bool CollectStats = false;
		Timer::Millisec CollectStats_Interval = Timer::Millisec(1000);

		bool UseNotifier = true;
		int SpinWaitCount = 5;

		bool UseEmbeddedTimer = false;
		 
		enum struct Pick {
			ShortestQueue,
			RoundRobin,
		};
		Pick PickAlgorithm = Pick::ShortestQueue;
	};

	struct ThreadPoolData;
	class ThreadPool
	{
	public:
		ThreadPool(const ThreadPoolOption& a_option);

		virtual ~ThreadPool();

		void Start();

		ThreadPoolStats Stop();

		ThreadPoolStats Reset(const ThreadPoolOption& a_option);

		ThreadPoolStats GetStats() const;


		template <typename FUNC, typename... PARAMS>
		inline Task_ptr Push(FUNC&& a_func,
							 PARAMS&&... a_params)
		{
			return PushTask(CreateTask(std::forward<FUNC>(a_func),
									   std::forward<PARAMS>(a_params)...));
		}

		template <typename FUNC, typename... PARAMS>
		inline Task_ptr Push(Timer::TimePoint a_timepoint,
							 FUNC&& a_func,
							 PARAMS&&... a_params)
		{
			return PushTask(a_timepoint,
							CreateTask(std::forward<FUNC>(a_func),
									   std::forward<PARAMS>(a_params)...));
		}

		template <typename DURATION, typename FUNC, typename... PARAMS>
		inline Task_ptr Push(DURATION a_after,
							 FUNC&& a_func,
							 PARAMS&&... a_params)
		{
			return PushTask(Timer::Now() + a_after,
							CreateTask(std::forward<FUNC>(a_func),
									   std::forward<PARAMS>(a_params)...));
		}


		template <typename FUNC, typename... PARAMS>
		inline Task_ptr PushSeq(size_t a_hash,
								FUNC&& a_func,
								PARAMS&&... a_params)
		{
			return PushSeqTask(a_hash,
							   CreateTask(std::forward<FUNC>(a_func),
										  std::forward<PARAMS>(a_params)...));
		}

		template <typename FUNC, typename... PARAMS>
		inline Task_ptr PushSeq(Timer::TimePoint a_timepoint,
								IN size_t a_hash,
								FUNC&& a_func,
								PARAMS&&... a_params)
		{
			return PushSeqTask(a_timepoint,
							   a_hash,
							   CreateTask(std::forward<FUNC>(a_func),
										  std::forward<PARAMS>(a_params)...));
		}

		template <typename DURATION, typename FUNC, typename... PARAMS>
		inline Task_ptr PushSeq(DURATION a_after,
								IN size_t a_hash,
								FUNC&& a_func,
								PARAMS&&... a_params)
		{
			return PushSeqTask(Timer::Now() + a_after,
							   a_hash,
							   CreateTask(std::forward<FUNC>(a_func),
										  std::forward<PARAMS>(a_params)...));
		}


	private:
		Task_ptr PushTask(Task_ptr&& a_task);

		Task_ptr PushTask(Timer::TimePoint a_timepoint,
						  Task_ptr&& a_task);

		Task_ptr PushSeqTask(size_t a_hash,
							 Task_ptr&& a_task);

		Task_ptr PushSeqTask(Timer::TimePoint a_timepoint,
							 size_t a_hash,
							 Task_ptr&& a_task);

		std::shared_ptr<ThreadPoolData> m_data;
	};



	struct ScalableThreadPoolOption
	{
		uint32_t MinWorkerCount = 1;
		uint32_t MaxWorkerCount = 100 * Get_HW_Concurrency();
		uint32_t WorkerExpireTimeMs = 1000 * 60;
		double ScaleUpCpuUsage = 0.8;
		double ScaleDownCpuUsage = 0.95;
		uint32_t ScaleUpWorkerCountPerSec = 1 * Get_HW_Concurrency();
	};

	struct ScalableThreadPoolData;
	class ScalableThreadPool
	{
	public:
		ScalableThreadPool(const ScalableThreadPoolOption& a_option);
		virtual ~ScalableThreadPool();

		bool AddWorker();
		ThreadPoolStats Stop();
		ThreadPoolStats GetStats();

		template <typename FUNC, typename... PARAMS>
		inline Task_ptr Push(FUNC&& a_func,
							 PARAMS&&... a_params)
		{
			return PushTask(CreateTask(std::forward<FUNC>(a_func),
									   std::forward<PARAMS>(a_params)...));
		}

		template <typename FUNC, typename... PARAMS>
		inline Task_ptr Push(Timer::TimePoint a_timepoint,
							 FUNC&& a_func,
							 PARAMS&&... a_params)
		{
			return PushTask(a_timepoint,
							CreateTask(std::forward<FUNC>(a_func),
									   std::forward<PARAMS>(a_params)...));
		}

		template <typename DURATION, typename FUNC, typename... PARAMS>
		inline Task_ptr Push(DURATION a_after,
							 FUNC&& a_func,
							 PARAMS&&... a_params)
		{
			return PushTask(Timer::Now() + a_after,
							CreateTask(std::forward<FUNC>(a_func),
									   std::forward<PARAMS>(a_params)...));
		}


	private:
		Task_ptr PushTask(Task_ptr&& a_task);

		Task_ptr PushTask(Timer::TimePoint a_timepoint,
						  Task_ptr&& a_task);

		std::shared_ptr<ScalableThreadPoolData> m_data;
	};
}
