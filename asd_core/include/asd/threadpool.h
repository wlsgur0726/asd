#pragma once
#include "asdbase.h"
#include "timer.h"


namespace asd
{
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

			atomic_t(IN const atomic_t& a_copy)
			{
				Base::operator=(a_copy.load());
			}

			atomic_t& operator=(IN const atomic_t& a_copy)
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



	struct ThreadPoolData;
	class ThreadPool
	{
	public:
		ThreadPool(IN const ThreadPoolOption& a_option);
		virtual ~ThreadPool();


		ThreadPoolStats Reset(IN const ThreadPoolOption& a_option);
		ThreadPoolStats Stop();
		void Start();


		ThreadPoolStats GetStats() const;


		Task_ptr Push(MOVE Task_ptr&& a_task);

		inline Task_ptr Push(IN const Task_ptr& a_task)
		{
			return Push(Task_ptr(a_task));
		}

		template <typename FUNC, typename... PARAMS>
		inline Task_ptr Push(FUNC&& a_func,
							 PARAMS&&... a_params)
		{
			return Push(CreateTask(std::forward<FUNC>(a_func),
								   std::forward<PARAMS>(a_params)...));
		}


		Task_ptr PushAt(IN Timer::TimePoint a_timepoint,
						MOVE Task_ptr&& a_task);

		inline Task_ptr PushAt(IN Timer::TimePoint a_timepoint,
							   IN const Task_ptr& a_task)
		{
			return PushAt(a_timepoint, Task_ptr(a_task));
		}

		template <typename FUNC, typename... PARAMS>
		inline Task_ptr PushAt(IN Timer::TimePoint a_timepoint,
							   FUNC&& a_func,
							   PARAMS&&... a_params)
		{
			return PushAt(a_timepoint,
						  CreateTask(std::forward<FUNC>(a_func),
									 std::forward<PARAMS>(a_params)...));
		}

		template <typename DURATION>
		inline Task_ptr PushAfter(IN DURATION a_after,
								  MOVE Task_ptr&& a_task)
		{
			return PushAt(Timer::Now() + a_after,
						  std::move(a_task));
		}

		template <typename DURATION>
		inline Task_ptr PushAfter(IN DURATION a_after,
								  IN const Task_ptr& a_task)
		{
			return PushAt(Timer::Now() + a_after,
						  a_task);
		}

		template <typename DURATION, typename FUNC, typename... PARAMS>
		inline Task_ptr PushAfter(IN DURATION a_after,
								  FUNC&& a_func,
								  PARAMS&&... a_params)
		{
			return PushAfter(a_after,
							 CreateTask(std::forward<FUNC>(a_func),
										std::forward<PARAMS>(a_params)...));
		}


		Task_ptr PushSeq(IN size_t a_hash,
						 MOVE Task_ptr&& a_task);

		inline Task_ptr PushSeq(IN size_t a_hash,
								IN const Task_ptr& a_task)
		{
			return PushSeq(a_hash,
						   Task_ptr(a_task));
		}

		template <typename FUNC, typename... PARAMS>
		inline Task_ptr PushSeq(IN size_t a_hash,
								FUNC&& a_func,
								PARAMS&&... a_params)
		{
			return PushSeq(a_hash,
						   CreateTask(std::forward<FUNC>(a_func),
									  std::forward<PARAMS>(a_params)...));
		}


		Task_ptr PushSeqAt(IN Timer::TimePoint a_timepoint,
						   IN size_t a_hash,
						   MOVE Task_ptr&& a_task);

		inline Task_ptr PushSeqAt(IN Timer::TimePoint a_timepoint,
								  IN size_t a_hash,
								  const Task_ptr& a_task)
		{
			return PushSeqAt(a_timepoint,
							 a_hash,
							 Task_ptr(a_task));
		}

		template <typename FUNC, typename... PARAMS>
		inline Task_ptr PushSeqAt(IN Timer::TimePoint a_timepoint,
								  IN size_t a_hash,
								  FUNC&& a_func,
								  PARAMS&&... a_params)
		{
			return PushSeqAt(a_timepoint,
							 a_hash,
							 CreateTask(std::forward<FUNC>(a_func),
										std::forward<PARAMS>(a_params)...));
		}

		template <typename DURATION>
		inline Task_ptr PushSeqAfter(IN DURATION a_after,
									 IN size_t a_hash,
									 MOVE Task_ptr&& a_task)
		{
			return PushSeqAt(Timer::Now() + a_after,
							 a_hash,
							 std::move(a_task));
		}

		template <typename DURATION>
		inline Task_ptr PushSeqAfter(IN DURATION a_after,
									 IN size_t a_hash,
									 IN const Task_ptr& a_task)
		{
			return PushSeqAt(Timer::Now() + a_after,
							 a_hash,
							 a_task);
		}

		template <typename DURATION, typename FUNC, typename... PARAMS>
		inline Task_ptr PushSeqAfter(IN DURATION a_after,
									 IN size_t a_hash,
									 FUNC&& a_func,
									 PARAMS&&... a_params)
		{
			return PushSeqAfter(a_after,
								a_hash,
								CreateTask(std::forward<FUNC>(a_func),
										   std::forward<PARAMS>(a_params)...));
		}


	private:
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
		ScalableThreadPool(IN const ScalableThreadPoolOption& a_option);
		~ScalableThreadPool();

		bool AddWorker();
		ThreadPoolStats Stop();
		ThreadPoolStats GetStats();

		template <typename FUNC, typename... PARAMS>
		inline Task_ptr Push(FUNC&& a_func,
							 PARAMS&&... a_params)
		{
			return Push(CreateTask(std::forward<FUNC>(a_func),
								   std::forward<PARAMS>(a_params)...));
		}

		Task_ptr Push(MOVE Task_ptr&& a_task);


	private:
		std::shared_ptr<ScalableThreadPoolData> m_data;
	};
}
