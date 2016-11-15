#include "asd_pch.h"
#include "asd/timer.h"
#include "asd/lock.h"
#include "asd/classutil.h"
#include <unordered_map>
#include <map>
#include <deque>


namespace asd
{
	namespace Timer
	{
		struct Task
		{
			Callback m_callback;
			bool m_cancel;
		};
		typedef std::unique_ptr<Task> Task_ptr;


		class Scheduler : public Global<Scheduler>
		{
		public:
			struct SchedulerData
			{
				Mutex										m_taskListLock;
				std::map<TimePoint, std::deque<uint64_t>>	m_taskList;

				Mutex										m_handleLock;
				std::unordered_map<uint64_t, Task_ptr>		m_handles; // K:핸들, V:true이면 실행전에 취소된 이벤트
				uint64_t									m_lastHandle = 0;
				std::deque<Task_ptr>						m_pool;

				std::atomic<TimePoint>						m_offset = Now();
			};
			typedef std::shared_ptr<SchedulerData> SchedulerData_ptr;


			SchedulerData_ptr m_data = SchedulerData_ptr(new SchedulerData);


			Scheduler()
			{
				auto data = m_data;
				std::thread thread([data]()
				{
					Scheduler::SchedulerThread(data);
				});
				thread.detach();
			}


			static void SchedulerThread(REF SchedulerData_ptr data) asd_noexcept
			{
				std::map<TimePoint, std::deque<uint64_t>> taskList;
				for (;; taskList.clear()) {
					TimePoint offset = data->m_offset;
					auto taskListLock = GetLock(data->m_taskListLock);
					for (auto it=data->m_taskList.begin(); it!=data->m_taskList.end(); ) {
						if (it->first > offset)
							break;
						taskList[it->first] = std::move(it->second);
						it = data->m_taskList.erase(it);
					}
					taskListLock.unlock();

					for (auto& it : taskList) {
						for (uint64_t handle : it.second) {
							// 핸들을 반납하면서 task 객체를 가져온다.
							auto handleLock = GetLock(data->m_handleLock);
							auto handle_iter = data->m_handles.find(handle);
							asd_Assert(handle_iter != data->m_handles.end(), "unknown logic error");
							auto task = std::move(handle_iter->second);
							data->m_handles.erase(handle_iter);

							// 풀에 task 객체 반납
							bool cancel = task->m_cancel;
							auto callback = std::move(task->m_callback);
							if (data->m_pool.size() < 10000)
								data->m_pool.emplace_back(std::move(task));

							// callback 실행
							handleLock.unlock();
							if (cancel == false)
								callback();
						}
					}

					offset += Milliseconds(1);
					data->m_offset = offset;
					std::this_thread::sleep_until(offset);
				}
			}
		};

		auto& g_scheduler = Scheduler::GlobalInstance();



		TimePoint Now() asd_noexcept
		{
			return std::chrono::high_resolution_clock::now();
		}



		TimePoint CurrentOffset() asd_noexcept
		{
			TimePoint o = g_scheduler.m_data->m_offset;
			return o - Milliseconds(1);
		}



		uint64_t PushAt(IN TimePoint a_timepoint,
						MOVE Callback&& a_task) asd_noexcept
		{
			// 새로운 Task 객체 얻기
			auto handleLock = GetLock(g_scheduler.m_data->m_handleLock);
			Task_ptr task;
			if (g_scheduler.m_data->m_pool.empty())
				task.reset(new Task);
			else {
				task = std::move(*g_scheduler.m_data->m_pool.rbegin());
				g_scheduler.m_data->m_pool.pop_back();
			}

			// 새로운 핸들 발급
			task->m_cancel = false;
			task->m_callback = std::move(a_task);
			uint64_t newHandle;
			for (;;) {
				newHandle = ++g_scheduler.m_data->m_lastHandle;
				if (newHandle == 0)
					continue;
				
				auto emplace = g_scheduler.m_data->m_handles.emplace(newHandle, Task_ptr());
				if (emplace.second == false)
					continue;

				emplace.first->second = std::move(task);
				break;
			}
			handleLock.unlock();

			// 스케쥴러에 등록
			auto taskListLock = GetLock(g_scheduler.m_data->m_taskListLock);
			g_scheduler.m_data->m_taskList[a_timepoint].emplace_back(newHandle);
			return newHandle;
		}



		bool Cancel(IN uint64_t a_handle) asd_noexcept
		{
			auto handleLock = GetLock(g_scheduler.m_data->m_handleLock);

			auto it = g_scheduler.m_data->m_handles.find(a_handle);
			if (it == g_scheduler.m_data->m_handles.end())
				return false;

			it->second->m_cancel = true;
			return true;
		}

	}
}
