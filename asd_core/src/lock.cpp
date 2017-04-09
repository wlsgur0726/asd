#include "asd_pch.h"
#include "asd/lock.h"
#include "asd/threadutil.h"
#include <thread>

#if defined(asd_Platform_Windows)	
#	include <Windows.h>
#
#else
#	include <pthread.h>
#
#endif


namespace asd
{
	void NoLock::lock() {
	}

	bool NoLock::try_lock() {
		return true;
	}

	void NoLock::unlock() {
	}



#if defined (asd_Platform_Windows)
	struct MutexData final
	{
		CRITICAL_SECTION m_mtx;

		MutexData()
		{
#if 1
			::InitializeCriticalSection(&m_mtx);
#else
			asd_RAssert(FALSE != ::InitializeCriticalSectionAndSpinCount(&m_mtx, 5),
						"fail InitializeCriticalSectionAndSpinCount, GetLastError:{}",
						::GetLastError());
#endif
		}

		void lock()
		{
			::EnterCriticalSection(&m_mtx);
		}

		bool try_lock()
		{
			return ::TryEnterCriticalSection(&m_mtx) != 0;
		}

		void unlock()
		{
			::LeaveCriticalSection(&m_mtx);
		}

		~MutexData() asd_noexcept
		{
			asd_DAssert(m_mtx.RecursionCount == 0);
			asd_DAssert(m_mtx.OwningThread == 0);
			::DeleteCriticalSection(&m_mtx);
		}
	};


#else
	struct MutexData
	{
		pthread_mutex_t m_mtx;
		int m_recursionCount = 0;
		uint32_t m_ownerThread = 0;

		MutexData()
		{
			pthread_mutexattr_t attr;
			if (::pthread_mutexattr_init(&attr) != 0) {
				auto e = errno;
				asd_RaiseException("fail pthread_mutexattr_init(), errno:{}", e);
			}
			if (::pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0) {
				auto e = errno;
				asd_RaiseException("fail pthread_mutexattr_settype(), errno:{}", e);
			}
			if (::pthread_mutex_init(&m_mtx, &attr) != 0) {
				auto e = errno;
				asd_RaiseException("fail pthread_mutex_init(), errno:{}", e);
			}
		}

		void lock()
		{
			if (::pthread_mutex_lock(&m_mtx) != 0) {
				auto e = errno;
				asd_RaiseException("fail pthread_mutex_lock(), errno:{}", e);
			}

			if (++m_recursionCount == 1)
				m_ownerThread = GetCurrentThreadID();
			asd_DAssert(m_ownerThread == GetCurrentThreadID());
		}

		bool try_lock()
		{
			if (::pthread_mutex_trylock(&m_mtx) != 0) {
				auto e = errno;
				if (e != EBUSY)
					asd_RaiseException("fail pthread_mutex_trylock(), errno:{}", e);
				return false;
			}

			if (++m_recursionCount == 1)
				m_ownerThread = GetCurrentThreadID();
			asd_DAssert(m_ownerThread == GetCurrentThreadID());
			return true;
		}

		void unlock()
		{
			asd_DAssert(m_ownerThread == GetCurrentThreadID());
			if (--m_recursionCount == 0)
				m_ownerThread = 0;

			if (::pthread_mutex_unlock(&m_mtx) != 0) {
				auto e = errno;

				// 롤백
				if (++m_recursionCount == 1)
					m_ownerThread = GetCurrentThreadID();

				asd_RaiseException("fail pthread_mutex_unlock(), errno:{}", e);
			}
		}

		~MutexData() asd_noexcept
		{
			asd_DAssert(m_recursionCount == 0);
			asd_DAssert(m_ownerThread == 0);
			if (::pthread_mutex_destroy(&m_mtx) != 0) {
				auto e = errno;
				asd_RaiseException("fail pthread_mutex_destroy(), errno:{}", e);
			}
		}
	};

#endif



	Mutex::Mutex()
	{
		m_data.reset(new MutexData);
	}

	Mutex::Mutex(MOVE Mutex&& a_rval)
	{
		operator=(std::move(a_rval));
	}

	Mutex& Mutex::operator=(MOVE Mutex&& a_rval)
	{
		auto a = m_data.get();
		auto b = a_rval.m_data.get();
		if (a > b)
			std::swap(a, b);

		if (a != nullptr)
			a->lock();
		if (b != nullptr)
			b->lock();

		auto del = std::move(m_data);
		m_data = std::move(a_rval.m_data);

		if (a != nullptr)
			a->unlock();
		if (b != nullptr)
			b->unlock();

		return *this;
	}

	void Mutex::lock()
	{
		asd_DAssert(m_data != nullptr);
		m_data->lock();
	}

	bool Mutex::try_lock()
	{
		asd_DAssert(m_data != nullptr);
		return m_data->try_lock();
	}

	void Mutex::unlock()
	{
		asd_DAssert(m_data != nullptr);
		m_data->unlock();
	}

	Mutex::~Mutex() asd_noexcept
	{
		if (m_data == nullptr)
			return;

		asd_BeginDestructor();
		m_data->lock();
		auto data = std::move(m_data);
		data->unlock();
		data.reset();
		asd_EndDestructor();
	}



	SpinMutex::SpinMutex() asd_noexcept
	{
		m_lock = 0;
	}

	SpinMutex::SpinMutex(MOVE SpinMutex&& a_rval) asd_noexcept
	{
		m_lock = 0;
		operator=(std::move(a_rval));
	}

	SpinMutex& SpinMutex::operator=(MOVE SpinMutex&& a_rval) asd_noexcept
	{
		auto a = this;
		auto b = &a_rval;
		if (a > b)
			std::swap(a, b);

		a->lock();
		b->lock();
		std::swap(a->m_recursionCount, b->m_recursionCount);
		a->unlock();
		b->unlock();
		return *this;
	}

	inline bool TrySpinLock(REF std::atomic<uint32_t>& a_lock,
							IN uint32_t a_tid,
							REF int& a_recursionCount) asd_noexcept
	{
		uint32_t lock = a_lock;
		if (lock == 0) {
			uint32_t cmp = 0;
			if (a_lock.compare_exchange_weak(cmp, a_tid, std::memory_order_acquire))
				goto OK;
		}
		else if (lock == a_tid)
			goto OK;

		return false;

	OK:
		++a_recursionCount;
		return true;
	}

	void SpinMutex::lock()
	{
		uint32_t curTID = GetCurrentThreadID();
		while (!TrySpinLock(m_lock, curTID, m_recursionCount));
	}

	bool SpinMutex::try_lock()
	{
		return TrySpinLock(m_lock, GetCurrentThreadID(), m_recursionCount);
	}

	void SpinMutex::unlock()
	{
		if (--m_recursionCount == 0)
			m_lock.store(0, std::memory_order_release);
	}
}
