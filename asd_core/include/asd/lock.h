#pragma once
#include "asdbase.h"
#include "exception.h"
#include <thread>
#include <mutex>


namespace asd
{
	// 템플릿에서 사용하기 위해
	// std::mutex와 동일한 함수명 사용
#define asd_DeclareMutexInterface	\
	void lock();					\
									\
	bool try_lock();				\
									\
	void unlock();					\

#define asd_DeclareSharedMutexInterface	\
	void lock_shared();					\
										\
	bool try_lock_shared();				\
										\
	void unlock_shared();				\


	class NoLock
	{
	public:
		asd_DeclareMutexInterface;
	};



	struct MutexData;
	class Mutex final
	{
	public:
		Mutex();

		Mutex(Mutex&& a_rval);

		Mutex& operator=(Mutex&& a_rval);

		~Mutex();

		asd_DeclareMutexInterface;

	private:
		std::unique_ptr<MutexData> m_data;
		Mutex(const Mutex&) = delete;
	};



	class SpinMutex final
	{
	public:
		SpinMutex();

		SpinMutex(SpinMutex&& a_rval);

		SpinMutex& operator=(SpinMutex&& a_rval);

		asd_DeclareMutexInterface;

	private:
		std::atomic<uint32_t> m_lock;
		int m_recursionCount = 0;
		SpinMutex(const SpinMutex&) = delete;
	};



	struct SharedMutexData;
	class SharedMutex
	{
	public:
		SharedMutex();

		SharedMutex(SharedMutex&& a_rval);

		SharedMutex& operator=(SharedMutex&& a_rval);

		~SharedMutex();

		asd_DeclareMutexInterface;

		asd_DeclareSharedMutexInterface;

	private:
		std::unique_ptr<SharedMutexData> m_data;
		SharedMutex(const SharedMutex&) = delete;
	};



	struct AsyncMutexData;
	class AsyncMutex
	{
	public:
		using Lock = std::shared_ptr<AsyncMutexData>;
		using Callback = std::function<void(Lock)>;

		AsyncMutex();

		void GetLock(Callback&& a_callback);

		size_t WaitingCount() const;

	private:
		std::shared_ptr<AsyncMutexData> m_data;
		AsyncMutex(const AsyncMutex&) = delete;
	};



	template <typename MUTEX_TYPE>
	class Lock final
	{
		MUTEX_TYPE* m_mutex;
		int m_recursionCount = 0;

	public:
		Lock(MUTEX_TYPE& a_mutex,
			 bool a_getLock = true)
			: m_mutex(&a_mutex)
		{
			if (a_getLock)
				lock();
		}

		Lock(Lock<MUTEX_TYPE>&& a_move)
		{
			m_recursionCount = a_move.m_recursionCount;
			m_mutex = a_move.m_mutex;
			a_move.m_mutex = nullptr;
		}

		~Lock()
		{
			if (m_mutex == nullptr)
				return;

			asd_DAssert(m_recursionCount >= 0);
			for (; m_recursionCount > 0; --m_recursionCount)
				m_mutex->unlock();
		}

		asd_DeclareMutexInterface;

		Lock(const Lock<MUTEX_TYPE>&) = delete;
		Lock& operator=(const Lock<MUTEX_TYPE>&) = delete;
	};

	template <typename MUTEX_TYPE>
	void Lock<MUTEX_TYPE>::lock()
	{
		m_mutex->lock();
		++m_recursionCount;
		asd_DAssert(m_recursionCount > 0);
	}

	template <typename MUTEX_TYPE>
	bool Lock<MUTEX_TYPE>::try_lock()
	{
		bool r = m_mutex->try_lock();
		if (r) {
			++m_recursionCount;
			asd_DAssert(m_recursionCount > 0);
		}
		asd_DAssert(m_recursionCount >= 0);
		return r;
	}

	template <typename MUTEX_TYPE>
	void Lock<MUTEX_TYPE>::unlock()
	{
		if (m_recursionCount > 0) {
			--m_recursionCount;
			m_mutex->unlock();
		}
		asd_DAssert(m_recursionCount >= 0);
	}


	template <typename MUTEX_TYPE>
	inline Lock<MUTEX_TYPE> GetLock(MUTEX_TYPE& a_mutex,
									bool a_getLock = true)
	{
		return Lock<MUTEX_TYPE>(a_mutex, a_getLock);
	}



	template <typename MUTEX_TYPE>
	class SharedLock final
	{
		MUTEX_TYPE* m_mutex;
		int m_recursionCount = 0;

	public:
		SharedLock(MUTEX_TYPE& a_mutex,
				   bool a_getLock = true)
			: m_mutex(&a_mutex)
		{
			if (a_getLock)
				lock_shared();
		}

		SharedLock(SharedLock<MUTEX_TYPE>&& a_move)
		{
			m_recursionCount = a_move.m_recursionCount;
			m_mutex = a_move.m_mutex;
			a_move.m_mutex = nullptr;
		}

		~SharedLock()
		{
			if (m_mutex == nullptr)
				return;

			asd_DAssert(m_recursionCount >= 0);
			for (; m_recursionCount > 0; --m_recursionCount)
				m_mutex->unlock_shared();
		}

		asd_DeclareSharedMutexInterface;
	};

	template <typename MUTEX_TYPE>
	void SharedLock<MUTEX_TYPE>::lock_shared()
	{
		m_mutex->lock_shared();
		++m_recursionCount;
	}

	template <typename MUTEX_TYPE>
	bool SharedLock<MUTEX_TYPE>::try_lock_shared()
	{
		bool r = m_mutex->try_lock_shared();
		if (r) {
			++m_recursionCount;
			asd_DAssert(m_recursionCount > 0);
		}
		asd_DAssert(m_recursionCount >= 0);
		return r;
	}

	template <typename MUTEX_TYPE>
	void SharedLock<MUTEX_TYPE>::unlock_shared()
	{
		if (m_recursionCount > 0) {
			--m_recursionCount;
			m_mutex->unlock_shared();
		}
		asd_DAssert(m_recursionCount >= 0);
	}


	template <typename MUTEX_TYPE>
	inline SharedLock<MUTEX_TYPE> GetSharedLock(MUTEX_TYPE& a_mutex,
												bool a_getLock = true)
	{
		return SharedLock<MUTEX_TYPE>(a_mutex, a_getLock);
	}
}
