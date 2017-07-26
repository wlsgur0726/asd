#include "asd_pch.h"
#include "asd/lock.h"
#include "asd/threadutil.h"
#include <thread>
#include<unordered_map>

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
			asd_RAssert(FALSE != ::InitializeCriticalSectionEx(&m_mtx, 1, CRITICAL_SECTION_NO_DEBUG_INFO),
						"fail InitializeCriticalSectionEx, GetLastError:{}",
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
			if (::pthread_mutex_init(&m_mtx, nullptr) != 0) {
				auto e = errno;
				asd_RaiseException("fail pthread_mutex_init(), errno:{}", e);
			}
		}

		void lock()
		{
			auto curTID = GetCurrentThreadID();
			if (m_ownerThread == curTID) {
				++m_recursionCount;
				return;
			}

			if (::pthread_mutex_lock(&m_mtx) != 0) {
				auto e = errno;
				asd_RaiseException("fail pthread_mutex_lock(), errno:{}", e);
			}

			m_recursionCount = 1;
			m_ownerThread = curTID;
		}

		bool try_lock()
		{
			auto curTID = GetCurrentThreadID();
			if (m_ownerThread == curTID) {
				++m_recursionCount;
				return true;
			}

			if (::pthread_mutex_trylock(&m_mtx) != 0) {
				auto e = errno;
				if (e != EBUSY)
					asd_RaiseException("fail pthread_mutex_trylock(), errno:{}", e);
				return false;
			}

			m_recursionCount = 1;
			m_ownerThread = curTID;
			return true;
		}

		void unlock()
		{
			asd_DAssert(m_ownerThread == GetCurrentThreadID());
			if (--m_recursionCount > 0)
				return;

			m_ownerThread = 0;
			if (::pthread_mutex_unlock(&m_mtx) != 0) {
				auto e = errno;

				// 롤백
				m_recursionCount = 1;
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


#define asd_SharedMutex_DebugReader 0
#if defined (asd_Platform_Windows)
	struct SharedMutexData final
	{
		SRWLOCK m_lock;
		std::atomic<int> m_readerCount;
		uint32_t m_writer = 0;
		int m_writerRecursionCount = 0;

#if asd_SharedMutex_DebugReader
		SpinMutex m_readerLock;
		std::unordered_map<uint32_t, int> m_readers;
#endif

		SharedMutexData()
		{
			::InitializeSRWLock(&m_lock);
			m_readerCount = 0;
		}

		void lock()
		{
			uint32_t tid = GetCurrentThreadID();
			if (m_writer == tid) {
				++m_writerRecursionCount;
				return;
			}

			::AcquireSRWLockExclusive(&m_lock);
			asd_RAssert(m_writer == 0, "unknown error");
			m_writer = tid;
			++m_writerRecursionCount;
		}

		bool try_lock()
		{
			uint32_t tid = GetCurrentThreadID();
			if (m_writer == tid) {
				++m_writerRecursionCount;
				return true;
			}

			if (::TryAcquireSRWLockExclusive(&m_lock)) {
				asd_RAssert(m_writer == 0, "unknown error");
				m_writer = tid;
				++m_writerRecursionCount;
				return true;
			}
			return false;
		}

		void unlock()
		{
			if (--m_writerRecursionCount == 0) {
				m_writer = 0;
				::ReleaseSRWLockExclusive(&m_lock);
			}
		}

		void lock_shared()
		{
			::AcquireSRWLockShared(&m_lock);
			++m_readerCount;

#if asd_SharedMutex_DebugReader
			auto lock = GetLock(m_readerLock);
			m_readers[GetCurrentThreadID()]++;
#endif
		}

		bool try_lock_shared()
		{
			if (::TryAcquireSRWLockShared(&m_lock)) {
				++m_readerCount;

#if asd_SharedMutex_DebugReader
				auto lock = GetLock(m_readerLock);
				m_readers[GetCurrentThreadID()]++;
#endif
				return true;
			}
			return false;
		}

		void unlock_shared()
		{
#if asd_SharedMutex_DebugReader
			{
				auto lock = GetLock(m_readerLock);
				if (--m_readers[GetCurrentThreadID()] == 0)
					m_readers.erase(GetCurrentThreadID());
			}
#endif
			--m_readerCount;
			::ReleaseSRWLockShared(&m_lock);
		}
	};

#else
	struct SharedMutexData final
	{
		pthread_rwlock_t m_lock;
		std::atomic<int> m_readerCount;
		uint32_t m_writer = 0;
		int m_writerRecursionCount = 0;

#if asd_SharedMutex_DebugReader
		SpinMutex m_readerLock;
		std::unordered_map<uint32_t, int> m_readers;
#endif

		static void IsDeadLock(IN int a_line)
		{
			asd_RAssert(false, "deadlock detected({})", a_line);
			for (;;)
				std::this_thread::sleep_for(std::chrono::seconds(1));
		}

		SharedMutexData()
		{
			int e = ::pthread_rwlock_init(&m_lock, nullptr);
			if (e != 0)
				asd_RaiseException("fail pthread_rwlock_init(), errno:{}", e);
		}

		~SharedMutexData()
		{
			int e = ::pthread_rwlock_destroy(&m_lock);
			if (e != 0)
				asd_RaiseException("fail pthread_rwlock_destroy(), errno:{}", e);
		}

		void lock()
		{
			uint32_t tid = GetCurrentThreadID();
			if (m_writer == tid) {
				++m_writerRecursionCount;
				return;
			}

			int e = ::pthread_rwlock_wrlock(&m_lock);
			if (e == EDEADLK)
				IsDeadLock(__LINE__);
			else if (e != 0)
				asd_RaiseException("fail pthread_rwlock_wrlock(), errno:{}", e);

			asd_RAssert(m_writer == 0, "unknown error");
			m_writer = tid;
			++m_writerRecursionCount;
		}

		bool try_lock()
		{
			uint32_t tid = GetCurrentThreadID();
			if (m_writer == tid) {
				++m_writerRecursionCount;
				return true;
			}

			if (0 == ::pthread_rwlock_trywrlock(&m_lock)) {
				asd_RAssert(m_writer == 0, "unknown error");
				m_writer = tid;
				++m_writerRecursionCount;
				return true;
			}
			return false;
		}

		void unlock()
		{
			if (--m_writerRecursionCount == 0) {
				m_writer = 0;
				int e = ::pthread_rwlock_unlock(&m_lock);
				if (e != 0)
					asd_RaiseException("fail pthread_rwlock_unlock(), errno:{}", e);
			}
		}

		void lock_shared()
		{
			for (int tryCnt=1;; ++tryCnt) {
				int e = ::pthread_rwlock_rdlock(&m_lock);
				switch (e) {
					case 0:
						break;
					case EAGAIN:
						std::this_thread::yield();
						continue;
					case EDEADLK:
						IsDeadLock(__LINE__);
						continue;
					default:
						asd_RaiseException("fail pthread_rwlock_rdlock(), errno:{}", e);
				}
				break;
			}
			++m_readerCount;

#if asd_SharedMutex_DebugReader
			auto lock = GetLock(m_readerLock);
			m_readers[GetCurrentThreadID()]++;
#endif
		}

		bool try_lock_shared()
		{
			if (0 == ::pthread_rwlock_tryrdlock(&m_lock)) {
				++m_readerCount;

#if asd_SharedMutex_DebugReader
				auto lock = GetLock(m_readerLock);
				m_readers[GetCurrentThreadID()]++;
#endif
				return true;
			}
			return false;
		}

		void unlock_shared()
		{
#if asd_SharedMutex_DebugReader
			{
				auto lock = GetLock(m_readerLock);
				if (--m_readers[GetCurrentThreadID()] == 0)
					m_readers.erase(GetCurrentThreadID());
			}
#endif
			--m_readerCount;
			int e = ::pthread_rwlock_unlock(&m_lock);
			if (e != 0)
				asd_RaiseException("fail pthread_rwlock_unlock(), errno:{}", e);
		}
	};

#endif

	SharedMutex::SharedMutex()
	{
		m_data.reset(new SharedMutexData);
	}

	SharedMutex::SharedMutex(MOVE SharedMutex&& a_rval)
	{
		operator=(std::move(a_rval));
	}

	SharedMutex& SharedMutex::operator=(MOVE SharedMutex&& a_rval)
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

	SharedMutex::~SharedMutex() asd_noexcept
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

	void SharedMutex::lock()
	{
		m_data->lock();
	}

	bool SharedMutex::try_lock()
	{
		return m_data->try_lock();
	}

	void SharedMutex::unlock()
	{
		m_data->unlock();
	}

	void SharedMutex::lock_shared()
	{
		m_data->lock_shared();
	}

	bool SharedMutex::try_lock_shared()
	{
		return m_data->try_lock_shared();
	}

	void SharedMutex::unlock_shared()
	{
		m_data->unlock_shared();
	}
}
