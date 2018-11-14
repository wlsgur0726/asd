#pragma once
#include "asdbase.h"
#include "lock.h"
#include "semaphore.h"
#include "timer.h"
#include <deque>

namespace asd
{
	template <typename CHILD, typename CONNECTION>
	class ConnectionPoolBase : public std::enable_shared_from_this<CHILD>
	{
	public:
		using Child = CHILD;
		using Connection = CONNECTION;
		using Connection_ptr = std::shared_ptr<Connection>;

		struct Config
		{
			size_t LimitCount = 10000;
			Timer::Millisec Expire = Timer::Millisec(1000*60*10);
			size_t RetryCount = 5;
			Timer::Millisec RetryTerm = Timer::Millisec(1000);
		};

		ConnectionPoolBase(const Config& a_config)
			: m_config(a_config)
		{
		}

		virtual ~ConnectionPoolBase()
		{
			Clear();
		}

		void Clear()
		{
			auto lock = GetLock(m_lock);
			auto pool = std::move(m_pool);
			if (m_lastExpireTask) {
				m_lastExpireTask->Cancel();
				m_lastExpireTask = nullptr;
			}
			lock.unlock();

			for (auto& res : pool)
				Delete(res.connection);
		}

		Connection_ptr Get()
		{
			return Connection_ptr(ConnectionPoolBase::Alloc(),
								  [this, pool=shared_from_this()](Connection* a_conn) { pool->Free(a_conn); });
		}


		virtual Connection* New() const { return new Connection; };
		virtual bool Connect(Connection* a_conn) = 0;


	private:
		struct Resource
		{
			Connection* connection = nullptr;
			Timer::TimePoint expireTime;
		};

		Connection* Alloc()
		{
			for (size_t i=0; i<m_config.RetryCount; std::this_thread::sleep_for(m_config.RetryTerm), ++i) {
				auto lock = GetLock(m_lock);
				Connection* conn = nullptr;
				if (m_pool.size() > 0) {
					conn = m_pool.back().connection;
					m_pool.pop_back();
				}
				lock.unlock();

				if (conn == nullptr) {
					conn = New();
					if (conn == nullptr) {
						asd_OnErr("fail New()");
						continue;
					}
				}

				if (!conn->IsConnected()){
					if (!Connect(conn)) {
						asd_OnErr("fail Connect()");
						delete conn;
						continue;
					}
				}

				return conn;
			}
			return nullptr;
		}

		void Free(Connection* a_conn)
		{
			if (a_conn == nullptr) {
				asd_OnErr("invalid a_conn");
				return;
			}

			Resource res;
			res.connection = a_conn;
			res.expireTime = Timer::Now() + m_config.Expire;

			auto lock = GetLock(m_lock);
			if (m_pool.size() < m_config.LimitCount) {
				m_pool.emplace_back(res);
				Expire();
			}
			else {
				lock.unlock();
				Delete(a_conn);
			}
		}

		void Expire()
		{
			if (m_pool.size()<=1 || m_lastExpireTask)
				return;

			m_lastExpireTask = Timer::Instance().PushAt(m_pool.front().expireTime, [this, pool=shared_from_this()]() mutable
			{
				auto now = Timer::Now();

				for (auto lock=GetLock(m_lock); m_pool.size()>1; lock.lock()) {
					Resource res = m_pool.front();
					if (now < res.expireTime)
						break;
					m_pool.pop_front();
					lock.unlock();
					Delete(res.connection);
				}

				auto lock = GetLock(m_lock);
				m_lastExpireTask = nullptr;
				Expire();
				lock.unlock();
				pool.reset();
			});
		}

		static void Delete(Connection* a_conn)
		{
			a_conn->Disconnect();
			delete a_conn;
		}

		Mutex m_lock;
		std::deque<Resource> m_pool;
		Task_ptr m_lastExpireTask;
		const Config m_config;

	};

}
