#include "stdafx.h"
#include "asd/threadpool.h"

namespace asd
{
	ScalableThreadPool::ScalableThreadPool(IN const Option& a_option)
		: m_data(std::make_shared<Data>(a_option))
	{
		CpuUsage(); // 인스턴스 초기화를 위해 호출

		auto lock = GetLock(m_data->lock);
		for (size_t i=0; i<m_data->option.MinWorkerCount; ++i)
			AddWorker();
	}


	bool ScalableThreadPool::AddWorker()
	{
		return AddWorker(m_data);
	}


	bool ScalableThreadPool::AddWorker(REF std::shared_ptr<Data>& a_data)
	{
		auto lock = GetLock(a_data->lock);

		if (a_data->stop) {
			asd_OnErr("thread-pool was stopped");
			return false;
		}

		if (a_data->workers.size() >= a_data->option.MaxWorkerCount)
			return false;

		auto worker = std::make_shared<Worker>();
		a_data->workers[worker.get()] = worker;

		lock.unlock();

		std::thread(&ScalableThreadPool::Working, a_data, worker).detach();
		return true;
	}


	void ScalableThreadPool::DeleteWorker(REF std::shared_ptr<Data>& a_data,
										  IN std::shared_ptr<Worker> a_worker)
	{
		auto lock = GetLock(a_data->lock);

		for (auto it=a_data->waiters.begin(); it!=a_data->waiters.end(); ) {
			if (*it == a_worker.get())
				it = a_data->waiters.erase(it);
			else
				++it;
		}

		a_data->workers.erase(a_worker.get());
	}


	ThreadPoolStats ScalableThreadPool::Stop()
	{
		auto lock = GetLock(m_data->lock);

		m_data->stop = true;

		for (; m_data->workers.size()>0; lock.lock()) {
			for (auto& it : m_data->workers) {
				auto& worker = it.second;
				if (GetCurrentThreadID() == worker->tid)
					asd_RaiseException("self-deadlock");
			}

			Worker* worker;
			while (worker = PopWaiter(m_data))
				worker->notify.Post();
			lock.unlock();
			std::this_thread::sleep_for(Timer::Millisec(1));
		}

		return GetStats();
	}

	
	ScalableThreadPool::~ScalableThreadPool()
	{
		asd_BeginDestructor();
		Stop();
		asd_EndDestructor();
	}


	ThreadPoolStats ScalableThreadPool::GetStats()
	{
		auto lock = GetLock(m_data->lock);
		m_data->stats.sleepingThreadCount = m_data->waiters.size();
		m_data->stats.threadCount = m_data->workers.size();

		m_data->stats.Refresh();
		return m_data->stats;
	}


	Task_ptr ScalableThreadPool::PushTask(REF std::shared_ptr<Data>& a_data,
										  REF Task_ptr& a_task)
	{
		auto data = a_data.get();
		double cpuUsage = CpuUsage();

		auto lock = GetLock(data->lock);

		if (data->stop) {
			asd_OnErr("thread-pool was stopped");
			return nullptr;
		}

		data->stats.Push();
		data->taskQueue.emplace_back(Task_ptr(a_task));

		auto worker = PopWaiter(a_data);
		if (worker) {
			worker->signaled = true;
			lock.unlock();
			worker->notify.Post();
			return a_task;
		}

		bool needScaleUp = [&]() {
			if (cpuUsage >= data->option.ScaleUpCpuUsage)
				return false;

			auto now = Timer::Now();
			if (data->scaleUpCount <= 0 || now - data->beginScaleUpTime > Timer::Millisec(1000)) {
				data->scaleUpCount = 1;
				data->beginScaleUpTime = now;
			}
			else if (data->scaleUpCount >= data->option.ScaleUpWorkerCountPerSec)
				return false;
			else
				data->scaleUpCount++;
			return true;
		}();
		if (needScaleUp)
			AddWorker(a_data);

		return a_task;
	}


	void ScalableThreadPool::Working(IN std::shared_ptr<Data> a_data,
									 IN std::shared_ptr<Worker> a_worker)
	{
		auto data = a_data.get();
		auto worker = a_worker.get();
		worker->tid = asd::GetCurrentThreadID();

		for (Task_ptr task;;) {
			if (task) {
				asd_BeginTry();
				task->Execute();
				asd_EndTryUnknown_Default();
				data->stats.Pop();
				task.reset();
			}

			bool needScaleDown = CpuUsage() >= data->option.ScaleDownCpuUsage;
			auto lock = GetLock(data->lock);

			bool signaled = worker->signaled;
			worker->signaled = false;

			if (data->taskQueue.size() > 0 && (signaled || !needScaleDown)) {
				task = std::move(data->taskQueue.front());
				data->taskQueue.pop_front();
				continue;
			}

			if (data->stop) {
				DeleteWorker(a_data, a_worker);
				return;
			}

			if (needScaleDown)
				data->waiters.emplace_front(worker);
			else
				data->waiters.emplace_back(worker);
			lock.unlock();

			uint32_t waitTimeMs = needScaleDown ? 0 : data->option.WorkerExpireTimeMs;
			while (!worker->notify.Wait(waitTimeMs)) {
				if (CheckExpired(a_data, a_worker))
					return;
			}
		}
	}


	ScalableThreadPool::Worker* ScalableThreadPool::PopWaiter(REF std::shared_ptr<Data>& a_data)
	{
		if (a_data->waiters.empty())
			return nullptr;
		auto ret = std::move(a_data->waiters.back());
		a_data->waiters.pop_back();
		return ret;
	}


	bool ScalableThreadPool::CheckExpired(REF std::shared_ptr<Data>& a_data, 
										  REF std::shared_ptr<Worker>& a_worker)
	{
		auto lock = GetLock(a_data->lock);

		if (a_worker->signaled)
			return false;

		if (a_data->workers.size() <= a_data->option.MinWorkerCount)
			return false;

		DeleteWorker(a_data, a_worker);
		return true;
	}

}
