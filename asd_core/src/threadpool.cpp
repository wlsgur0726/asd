#include "stdafx.h"
#include "asd/threadpool.h"
#include "asd/threadutil.h"
#include "asd/semaphore.h"
#include <queue>
#include <deque>
#include <vector>
#include <unordered_set>

namespace asd
{
	typedef MutexController<Mutex> MtxCtl;
	struct ThreadPoolData
	{
		ThreadPoolData(IN const uint32_t a_threadCount)
			: threadCount(a_threadCount)
		{
		}

		// 아래 모든 데이터들을 보호하는 락
		Mutex tpLock;

		// 작업 큐
		std::queue<ThreadPool::Task> taskQueue;

		// 동작 상태
		bool run = false;

		// 이 변수가 true이면 
		// 종료명령이 떨어져도 남은 작업을 모두 처리한 후 종료한다.
		bool overtime = true;

		// 작업을 수행하는 쓰레드 관련
		const uint32_t						threadCount;
		std::vector<std::thread>			threads;
		std::unordered_set<std::thread::id>	workers;
		std::deque<Semaphore*>				waitingList;

		// 사용자가 입력한 작업에서 발생한 예외를 처리하는 핸들러
		std::shared_ptr<ThreadPool::ExceptionHandler> exceptionHandler;
	};



	ThreadPool::ThreadPool(IN uint32_t a_threadCount /*= std::thread::hardware_concurrency()*/)
	{
		Reset(a_threadCount);
	}



	ThreadPool& ThreadPool::Reset(IN uint32_t a_threadCount /*= std::thread::hardware_concurrency()*/)
	{
		m_data.reset(new ThreadPoolData(a_threadCount));
		m_data->threads.resize(m_data->threadCount);
		return *this;
	}



	ThreadPool& ThreadPool::Start()
	{
		MtxCtl mtx(m_data->tpLock);
		if (m_data->run)
			asd_RaiseException("already running");

		m_data->run = true;
		assert(m_data->threads.size() == m_data->threadCount);
		for (auto& t : m_data->threads) {
			t = std::thread([this]()
			{
				MtxCtl lock(m_data->tpLock);
				m_data->workers.insert(GetCurrentThreadID());
				lock.unlock();

				while (m_data->run)
					Poll();
			});
		}
		return *this;
	}



	ThreadPool& ThreadPool::SetExceptionHandler(IN const ExceptionHandler& a_eh)
	{
		m_data->exceptionHandler.reset(new ExceptionHandler(a_eh));
		return *this;
	}



	size_t ThreadPool::Poll(IN uint32_t a_timeoutMs /*= std::numeric_limits<uint32_t>::max()*/,
							IN size_t a_procCountLimit /*= std::numeric_limits<size_t>::max()*/)
	{
		if (a_procCountLimit <= 0)
			return 0;

		thread_local Semaphore t_event;
		size_t procCount = 0;
		MtxCtl mtx(m_data->tpLock);

		// 이벤트 체크
		while (m_data->run || (m_data->overtime && !m_data->taskQueue.empty())) {
			if (m_data->taskQueue.empty()) {
				// 대기
				m_data->waitingList.push_back(&t_event);
				mtx.unlock();
				bool ev = t_event.Wait(a_timeoutMs);
				mtx.lock();
				if (ev)
					continue;
				
				// 타임아웃
				auto it = std::find(m_data->waitingList.begin(), m_data->waitingList.end(), &t_event);
				m_data->waitingList.erase(it);
				break;
			}

			// 작업을 접수
			assert(m_data->taskQueue.empty() == false);
			Task task(std::move(m_data->taskQueue.front()));
			m_data->taskQueue.pop();

			// 작업 수행
			mtx.unlock();
			try {
				assert(task != nullptr);
				task();
			}
			catch (std::exception& e) {
				auto eh = std::atomic_load(&m_data->exceptionHandler);
				if (eh != nullptr)
					(*eh)(e);
				else
					throw e;
			}
			if (++procCount >= a_procCountLimit)
				break;

			mtx.lock();
		}
		return procCount;
	}



	ThreadPool& ThreadPool::PushTask(MOVE Task&& a_task)
	{
		assert(a_task != nullptr);
		MtxCtl mtx(m_data->tpLock);
		if (m_data->run == false)
			return *this;

		m_data->taskQueue.push(std::move(a_task));

		if (m_data->waitingList.size() > 0) {
			m_data->waitingList.front()->Post();
			m_data->waitingList.pop_front();
		}
		return *this;
	}



	ThreadPool& ThreadPool::PushTask(IN const Task& a_task)
	{
		return PushTask(std::move(Task(a_task)));
	}



	void ThreadPool::Stop(IN bool a_overtime /*= true*/)
	{
		if (m_data != nullptr)
		{
			MtxCtl mtx(m_data->tpLock);
			if (m_data->run == false)
				return;

			if (m_data->workers.find(GetCurrentThreadID()) != m_data->workers.end())
				asd_RaiseException("deadlock");

			m_data->run = false;
			m_data->overtime = a_overtime;

			while (m_data->waitingList.size() > 0) {
				m_data->waitingList.front()->Post();
				m_data->waitingList.pop_front();
			}

			while (m_data->threads.size() > 0) {
				auto thread = std::move(*m_data->threads.rbegin());
				m_data->threads.resize(m_data->threads.size() - 1);
				mtx.unlock();
				thread.join();
				mtx.lock();
			}
		}

		m_data.reset();
	}



	ThreadPool::~ThreadPool() asd_noexcept
	{
		asd_Destructor_Start;
		if (m_data != nullptr)
			Stop();
		asd_Destructor_End;
	}

}