#include "asd_pch.h"
#include "asd/semaphore.h"

#if !defined(asd_Platform_Windows)
#	include <semaphore.h>
#endif


namespace asd
{
#if defined(asd_Platform_Windows)
	struct SemaphoreData
	{
		HANDLE m_handle;
		std::atomic<uint32_t> m_count;

		SemaphoreData(IN uint32_t a_initCount)
		{
			m_handle = ::CreateSemaphore(NULL,
										 a_initCount,
										 INT32_MAX,
										 NULL);
			if (m_handle == NULL) {
				auto e = ::GetLastError();
				asd_RaiseException("fail CreateSemaphore(), GetLastError:{}", e);
			}
			m_count = a_initCount;
		}

		~SemaphoreData() asd_noexcept
		{
			if (::CloseHandle(m_handle) == 0) {
				auto e = ::GetLastError();
				asd_RAssert(false, "GetLastError:{}", e);
			}
		}
	};

#else
	struct SemaphoreData
	{
		sem_t m_handle;

		SemaphoreData(IN uint32_t a_initCount)
		{
			if (0 != ::sem_init(&m_handle, 0, a_initCount)) {
				auto e = errno;
				asd_RaiseException("fail sem_init(), errno:{}", e);
			}
		}

		~SemaphoreData() asd_noexcept
		{
			if (0 != ::sem_destroy(&m_handle)) {
				auto e = errno;
				asd_RAssert(false, "fail sem_destroy(), errno:{}", e);
			}
		}
	};

#endif



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
		asd_DAssert(GetCount() >= 0);
		m_data.reset();
	}



	uint32_t Semaphore::GetCount() const
	{
		asd_DAssert(m_data != nullptr);

#if defined(asd_Platform_Windows)
		asd_DAssert(m_data->m_count >= 0);
		return m_data->m_count;
#else
		int sval;
		if (::sem_getvalue(&m_data->m_handle, &sval) != 0) {
			auto e = errno;
			asd_RaiseException("errno:{}", e);
		}
		asd_DAssert(sval >= 0);
		return sval;
#endif
	}



	bool Semaphore::Wait(IN uint32_t a_timeoutMs /*= Infinite*/)
	{
		asd_DAssert(m_data != nullptr);

#if defined(asd_Platform_Windows)
		static_assert(INFINITE == Infinite, "unexpected INFINITE value");
		auto r = ::WaitForSingleObject(m_data->m_handle, a_timeoutMs);
		switch (r) {
			case WAIT_OBJECT_0:
				--m_data->m_count;
				asd_DAssert(m_data->m_count >= 0);
				return true;
			case WAIT_TIMEOUT:
				break;
			default:
				asd_DAssert(r == WAIT_FAILED);
				auto e = ::GetLastError();
				asd_RaiseException("GetLastError:{}", e);
				break;
		}
		asd_DAssert(GetCount() >= 0);
		return false;
#else
		int r;
		switch (a_timeoutMs) {
			case 0:
				r = ::sem_trywait(&m_data->m_handle);
				break;
			case Infinite:
				r = ::sem_wait(&m_data->m_handle);
				break;
			default: {
				asd_DAssert(a_timeoutMs > 0);
				timespec t;
				if (::clock_gettime(CLOCK_REALTIME, &t) != 0) {
					auto e = errno;
					asd_RaiseException("errno:{}", e);
				}
				t.tv_nsec += a_timeoutMs * (1000*1000);
				t.tv_sec += t.tv_nsec / (1000*1000*1000);
				t.tv_nsec %= (1000*1000*1000);
				r = ::sem_timedwait(&m_data->m_handle, &t);
				break;
			}
		}

		if (r != 0) {
			auto e = errno;
			switch (e) {
				case ETIMEDOUT:
				case EAGAIN:
				case EINTR:
					asd_DAssert(a_timeoutMs != Infinite);
					break;
				default:
					asd_RaiseException("errno:{}", e);
			}
			asd_DAssert(GetCount() >= 0);
			return false;
		}
		asd_DAssert(GetCount() >= 0);
		return true;
#endif
	}



	void Semaphore::Post(IN uint32_t a_count /*= 1*/)
	{
		asd_DAssert(m_data != nullptr);
		if (a_count == 0)
			return;

#if defined(asd_Platform_Windows)
		m_data->m_count += a_count;
		if (::ReleaseSemaphore(m_data->m_handle, a_count, NULL) == 0) {
			auto e = ::GetLastError();
			m_data->m_count -= a_count;
			asd_RaiseException("GetLastError:{}", e);
		}
#else
		for (uint32_t i=0; i<a_count; ++i) {
			if (sem_post(&m_data->m_handle) != 0) {
				auto e = errno;
				asd_RaiseException("errno:{}; successCount:{}", e, i);
			}
		}
#endif
		asd_DAssert(GetCount() >= 0);
	}
}
