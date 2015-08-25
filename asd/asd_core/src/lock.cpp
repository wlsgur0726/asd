#include "stdafx.h"
#include "asd/lock.h"
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
	thread_local std::thread::id t_tid;

	const std::thread::id& GetCurrentThreadID() asd_NoThrow
	{
		if (t_tid == std::thread::id())
			t_tid = std::this_thread::get_id();

		assert(t_tid != std::thread::id());
		return t_tid;
	}


	thread_local uint32_t t_HW_Concurrency = 0xFFFFFFFF;
	const uint32_t& Get_HW_Concurrency() asd_NoThrow
	{
		if (t_HW_Concurrency == 0xFFFFFFFF) {
			t_HW_Concurrency = std::thread::hardware_concurrency();
		}
		return t_HW_Concurrency;
	}


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
		std::thread::id m_ownerThread;
#endif

		MutexData()
		{
#if defined (asd_Platform_Windows)
			auto r = InitializeCriticalSectionAndSpinCount(&m_mtx, 5);
			assert(r != 0);
#else
			pthread_mutexattr_t attr;
			if (pthread_mutexattr_init(&attr) != 0) {
				auto e = errno;
				asd_RaiseException("fail pthread_mutexattr_init(), errno:0x%x", e);
			}
			if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0) {
				auto e = errno;
				asd_RaiseException("fail pthread_mutexattr_settype(), errno:0x%x", e);
			}
			if (pthread_mutex_init(&m_mtx, &attr) != 0) {
				auto e = errno;
				asd_RaiseException("fail pthread_mutex_init(), errno:0x%x", e);
			}
#endif
		}


		~MutexData()
		{
#if defined (asd_Platform_Windows)
			assert(m_mtx.RecursionCount == 0);
			assert(m_mtx.OwningThread == 0);
			DeleteCriticalSection(&m_mtx);
#else
			assert(m_recursionCount == 0);
			assert(m_ownerThread == std::thread::id());
			if (pthread_mutex_destroy(&m_mtx) != 0) {
				auto e = errno;
				asd_RaiseException("fail pthread_mutex_destroy(), errno:0x%x", e);
			}
#endif
		}
	};



	Mutex::Mutex() 
		asd_Throws(asd::Exception) 
	{
		m_data = new MutexData;
	}



	Mutex::Mutex(MOVE Mutex&& a_rval)
		asd_Throws(asd::Exception)
	{
		(*this) = std::move(a_rval);
	}



	Mutex& Mutex::operator = (MOVE Mutex&& a_rval) 
		asd_Throws(asd::Exception)
	{
		if (m_data != nullptr)
			delete m_data;

		m_data = a_rval.m_data;
		a_rval.m_data = nullptr;

		return *this;
	}



	Mutex::~Mutex() 
		asd_NoThrow
	{
		asd_Destructor_Start
			if (m_data == nullptr)
				return; // move된 경우
			delete m_data;
		asd_Destructor_End
	}



	void Mutex::lock()
	{
		assert(m_data != nullptr);
		
#if defined (asd_Platform_Windows)
		EnterCriticalSection(&m_data->m_mtx);
#else
		if (pthread_mutex_lock(&m_data->m_mtx) != 0) {
			auto e = errno;
			asd_RaiseException("fail pthread_mutex_lock(), errno:0x%x", e);
		}
		
		if (++m_data->m_recursionCount == 1)
			m_data->m_ownerThread = GetCurrentThreadID();
		assert(m_data->m_ownerThread == GetCurrentThreadID());
#endif
	}



	bool Mutex::try_lock()
	{
		assert(m_data != nullptr);

#if defined (asd_Platform_Windows)
		return TryEnterCriticalSection(&m_data->m_mtx) != 0;
#else
		if (pthread_mutex_trylock(&m_data->m_mtx) != 0) {
			auto e = errno;
			if ( e != EBUSY ) {
				asd_RaiseException("fail pthread_mutex_trylock(), errno:0x%x", e);
			}
			return false;
		}
		
		if (++m_data->m_recursionCount == 1)
			m_data->m_ownerThread = GetCurrentThreadID();
		assert(m_data->m_ownerThread == GetCurrentThreadID());
		return true;
#endif
	}



	void Mutex::unlock()
	{
		assert(m_data != nullptr);

#if defined (asd_Platform_Windows)
		LeaveCriticalSection(&m_data->m_mtx);
#else
		if (--m_data->m_recursionCount == 0)
			m_data->m_ownerThread = std::thread::id();

		if (pthread_mutex_unlock(&m_data->m_mtx) != 0) {
			auto e = errno;

			// 롤백
			if (++m_data->m_recursionCount == 1)
				m_data->m_ownerThread = GetCurrentThreadID();

			asd_RaiseException("fail pthread_mutex_unlock(), errno:0x%x", e);
		}
		
#endif
	}



	SpinMutex::SpinMutex()
		asd_Throws(asd::Exception)
	{
		if (Get_HW_Concurrency() <= 1)
			m_mtx = new Mutex;
		else
			m_lock = false;
	}



	SpinMutex::~SpinMutex()
		asd_NoThrow
	{
		if (m_mtx != nullptr)
			delete m_mtx;
	}



	void SpinMutex::lock()
	{
		if (m_mtx != nullptr)
			m_mtx->lock();
		else {
			while (true) {
				if (m_lock == false) {
					bool cmp = false;
					if (m_lock.compare_exchange_weak(cmp, true, std::memory_order_acquire))
						break;
				}
			}
		}
	}



	bool SpinMutex::try_lock()
	{
		if (m_mtx != nullptr)
			return m_mtx->try_lock();
		
		if (m_lock == false) {
			bool cmp = false;
			if (m_lock.compare_exchange_weak(cmp, true, std::memory_order_acquire))
				return true;
		}
		return false;
	}



	void SpinMutex::unlock()
	{
		if (m_mtx != nullptr)
			m_mtx->unlock();
		else
			m_lock.store(false, std::memory_order_release);
	}
}
