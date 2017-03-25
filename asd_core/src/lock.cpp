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



	struct MutexData
	{
#if defined (asd_Platform_Windows)
		CRITICAL_SECTION m_mtx;
#else
		pthread_mutex_t m_mtx;
		int m_recursionCount = 0;
		uint32_t m_ownerThread = 0;
#endif

		MutexData()
		{
#if defined (asd_Platform_Windows)
			auto r = InitializeCriticalSectionAndSpinCount(&m_mtx, 5);
			asd_DAssert(r != 0);
#else
			pthread_mutexattr_t attr;
			if (pthread_mutexattr_init(&attr) != 0) {
				auto e = errno;
				asd_RaiseException("fail pthread_mutexattr_init(), errno:{}", e);
			}
			if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0) {
				auto e = errno;
				asd_RaiseException("fail pthread_mutexattr_settype(), errno:{}", e);
			}
			if (pthread_mutex_init(&m_mtx, &attr) != 0) {
				auto e = errno;
				asd_RaiseException("fail pthread_mutex_init(), errno:{}", e);
			}
#endif
		}


		~MutexData() asd_noexcept
		{
#if defined (asd_Platform_Windows)
			asd_DAssert(m_mtx.RecursionCount == 0);
			asd_DAssert(m_mtx.OwningThread == 0);
			DeleteCriticalSection(&m_mtx);
#else
			asd_DAssert(m_recursionCount == 0);
			asd_DAssert(m_ownerThread == 0);
			if (pthread_mutex_destroy(&m_mtx) != 0) {
				auto e = errno;
				asd_RaiseException("fail pthread_mutex_destroy(), errno:{}", e);
			}
#endif
		}
	};



	Mutex::Mutex()
	{
		m_data.reset(new MutexData);
	}



	Mutex::Mutex(MOVE Mutex&& a_rval)
	{
		(*this) = std::move(a_rval);
	}



	Mutex& Mutex::operator = (MOVE Mutex&& a_rval)
	{
		m_data = std::move(a_rval.m_data);
		return *this;
	}



	Mutex::~Mutex() asd_noexcept
	{
		asd_BeginDestructor();
		m_data.reset();
		asd_EndDestructor();
	}



	void Mutex::lock()
	{
		asd_DAssert(m_data != nullptr);
		
#if defined (asd_Platform_Windows)
		EnterCriticalSection(&m_data->m_mtx);

#else
		if (pthread_mutex_lock(&m_data->m_mtx) != 0) {
			auto e = errno;
			asd_RaiseException("fail pthread_mutex_lock(), errno:{}", e);
		}
		
		if (++m_data->m_recursionCount == 1)
			m_data->m_ownerThread = GetCurrentThreadID();
		asd_DAssert(m_data->m_ownerThread == GetCurrentThreadID());

#endif
	}



	bool Mutex::try_lock()
	{
		asd_DAssert(m_data != nullptr);

#if defined (asd_Platform_Windows)
		return TryEnterCriticalSection(&m_data->m_mtx) != 0;

#else
		if (pthread_mutex_trylock(&m_data->m_mtx) != 0) {
			auto e = errno;
			if ( e != EBUSY ) {
				asd_RaiseException("fail pthread_mutex_trylock(), errno:{}", e);
			}
			return false;
		}
		
		if (++m_data->m_recursionCount == 1)
			m_data->m_ownerThread = GetCurrentThreadID();
		asd_DAssert(m_data->m_ownerThread == GetCurrentThreadID());
		return true;

#endif
	}



	void Mutex::unlock()
	{
		asd_DAssert(m_data != nullptr);

#if defined (asd_Platform_Windows)
		LeaveCriticalSection(&m_data->m_mtx);

#else
		asd_DAssert(m_data->m_ownerThread == GetCurrentThreadID());
		if (--m_data->m_recursionCount == 0)
			m_data->m_ownerThread = 0;

		if (pthread_mutex_unlock(&m_data->m_mtx) != 0) {
			auto e = errno;

			// 롤백
			if (++m_data->m_recursionCount == 1)
				m_data->m_ownerThread = GetCurrentThreadID();

			asd_RaiseException("fail pthread_mutex_unlock(), errno:{}", e);
		}

#endif
	}



	SpinMutex::SpinMutex() asd_noexcept
	{
		m_lock = false;
	}



	void SpinMutex::lock()
	{
		while (true) {
			if (m_lock == false) {
				bool cmp = false;
				if (m_lock.compare_exchange_weak(cmp, true, std::memory_order_acquire))
					break;
			}
		}
	}



	bool SpinMutex::try_lock()
	{
		if (m_lock == false) {
			bool cmp = false;
			if (m_lock.compare_exchange_weak(cmp, true, std::memory_order_acquire))
				return true;
		}
		return false;
	}



	void SpinMutex::unlock()
	{
		m_lock.store(false, std::memory_order_release);
	}
}
