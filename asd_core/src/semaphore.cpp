#include "stdafx.h"
#include "asd/semaphore.h"

#if !defined(asd_Platform_Windows)
#	include <semaphore.h>
#endif


namespace asd
{
	struct SemaphoreData {
#if defined(asd_Platform_Windows)
		HANDLE m_handle;
		std::atomic<uint32_t> m_count;
#else
		sem_t m_handle;
#endif

#ifdef asd_Debug
		bool m_init = false;
#endif


		SemaphoreData(IN uint32_t a_initCount)
		{
#if defined(asd_Platform_Windows)
			m_handle = CreateSemaphore(NULL,
									   a_initCount,
									   INT32_MAX,
									   NULL);
			if (m_handle == NULL) {
				auto e = GetLastError();
				asd_RaiseException("fail CreateSemaphore(), GetLastError:%#x", e);
			}
			m_count = a_initCount;
#else
			if (sem_init(&m_handle, 0, a_initCount) == -1) {
				auto e = errno;
				asd_RaiseException("fail sem_init(), errno:%#x", e);
			}
#endif

#ifdef asd_Debug
			m_init = true;
#endif
		}


		~SemaphoreData()
			noexcept(false)
		{
#ifdef asd_Debug
			assert(m_init);
#endif

#if defined(asd_Platform_Windows)
			if (CloseHandle(m_handle) == 0) {
				auto e = GetLastError();
				asd_RaiseException("GetLastError:%#x", e);
			}
#else
			if (sem_destroy(&m_handle) != 0) {
				auto e = errno;
				asd_RaiseException("errno:%#x", e);
			}
#endif
		}
	};


	Semaphore::Semaphore(IN uint32_t a_initCount /*= 0*/)
	{
		m_data.reset(new SemaphoreData(a_initCount));
	}



	Semaphore::Semaphore(MOVE Semaphore&& a_rval)
	{
		(*this) = std::move(a_rval);
	}



	Semaphore& Semaphore::operator = (MOVE Semaphore&& a_rval)
	{
		m_data.swap(a_rval.m_data);
		return *this;
	}
	
	

	Semaphore::~Semaphore() asd_noexcept
	{
		asd_Destructor_Start
			assert(GetCount() >= 0);
			m_data.release();
		asd_Destructor_End
	}



	uint32_t Semaphore::GetCount() const
	{
		assert(m_data != nullptr);

#if defined(asd_Platform_Windows)
		assert(m_data->m_count >= 0);
		return m_data->m_count;
#else
		int sval;
		if (sem_getvalue(&m_data->m_handle, &sval) != 0) {
			auto e = errno;
			asd_RaiseException("errno:%#x", e);
		}
		assert(sval >= 0);
		return sval;
#endif
	}



	bool Semaphore::Wait(IN uint32_t a_timeoutMs /*= Infinite*/)
	{
		assert(m_data != nullptr);

#if defined(asd_Platform_Windows)
		assert(INFINITE == Infinite);
		auto r = WaitForSingleObject(m_data->m_handle, a_timeoutMs);
		switch (r) {
			case WAIT_OBJECT_0:
				--m_data->m_count;
				assert(m_data->m_count >= 0);
				return true;
			case WAIT_TIMEOUT:
				break;
			default:
				assert(r == WAIT_FAILED);
				auto e = GetLastError();
				asd_RaiseException("GetLastError:%#x", e);
				break;
		}
		assert(GetCount() >= 0);
		return false;
#else
		int r;
		switch (a_timeoutMs)
		{
			case 0:
				r = sem_trywait(&m_data->m_handle);
				break;
			case Infinite:
				r = sem_wait(&m_data->m_handle);
				break;
			default: {
				assert(a_timeoutMs > 0);
				timespec t;
				if (clock_gettime(CLOCK_REALTIME, &t) != 0) {
					auto e = errno;
					asd_RaiseException("errno:%#x", e);
				}
				t.tv_nsec += a_timeoutMs * (1000*1000);
				t.tv_sec += t.tv_nsec / (1000*1000*1000);
				t.tv_nsec %= (1000*1000*1000);
				r = sem_timedwait(&m_data->m_handle, &t);
				break;
			}
		}

		if (r != 0) {
			auto e = errno;
			if (e != ETIMEDOUT &&
				e != EAGAIN &&
				e != EINTR) 
			{
				asd_RaiseException("errno:%#x", e);
			}
			else 
			{
				assert(a_timeoutMs != Infinite);
			}
			assert(GetCount() >= 0);
			return false;
		}
		assert(GetCount() >= 0);
		return true;
#endif
	}



	void Semaphore::Post(IN uint32_t a_count /*= 1*/)
	{
		assert(m_data != nullptr);
		if (a_count == 0)
			return;

#if defined(asd_Platform_Windows)
		m_data->m_count += a_count;
		if (ReleaseSemaphore(m_data->m_handle, a_count, NULL) == 0) {
			auto e = GetLastError();
			m_data->m_count -= a_count;
			asd_RaiseException("GetLastError:%#x", e);
		}
#else
		for (uint32_t i=0; i<a_count; ++i) {
			if (sem_post(&m_data->m_handle) != 0) {
				auto e = errno;
				asd_RaiseException("errno:%#x; successCount:%u", e, i);
			}
		}
#endif
		assert(GetCount() >= 0);
	}
}
