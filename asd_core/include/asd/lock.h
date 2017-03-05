﻿#pragma once
#include "asdbase.h"
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



	class NoLock
	{
	public:
		asd_DeclareMutexInterface;
	};



	struct MutexData;
	class Mutex final
	{
		std::unique_ptr<MutexData> m_data;
		Mutex(const Mutex&) = delete;

	public:
		Mutex();

		Mutex(MOVE Mutex&& a_rval);

		Mutex& operator = (MOVE Mutex&& a_rval);

		~Mutex() asd_noexcept;

		asd_DeclareMutexInterface;
	};



	class SpinMutex final
	{
		std::atomic<bool> m_lock;

	public:
		asd_DeclareMutexInterface;
	};



	template <typename MutexType>
	class Lock final
	{
		MutexType* m_mutex;
		int m_recursionCount = 0;

	public:
		Lock(REF MutexType& a_mutex,
			 IN bool a_getLock = true)
			: m_mutex(&a_mutex)
		{
			if (a_getLock)
				lock();
		}

		Lock(MOVE Lock<MutexType>&& a_move) asd_noexcept
		{
			m_recursionCount = a_move.m_recursionCount;
			m_mutex = a_move.m_mutex;
			a_move.m_mutex = nullptr;
		}

		~Lock()
		{
			if (m_mutex == nullptr)
				return;

			assert(m_recursionCount >= 0);
			for (; m_recursionCount > 0; --m_recursionCount)
				m_mutex->unlock();
		}

		asd_DeclareMutexInterface;

		Lock(IN const Lock<MutexType>& a_copy) = delete;
		Lock& operator=(IN const Lock<MutexType>& a_copy) = delete;
	};

	template <typename MutexType>
	void Lock<MutexType>::lock()
	{
		m_mutex->lock();
		++m_recursionCount;
		assert(m_recursionCount > 0);
	}

	template <typename MutexType>
	bool Lock<MutexType>::try_lock()
	{
		bool r = m_mutex->try_lock();
		if (r) {
			++m_recursionCount;
			assert(m_recursionCount > 0);
		}
		assert(m_recursionCount >= 0);
		return r;
	}

	template <typename MutexType>
	void Lock<MutexType>::unlock()
	{
		if (m_recursionCount > 0) {
			--m_recursionCount;
			m_mutex->unlock();
		}
		assert(m_recursionCount >= 0);
	}



	template <typename MutexType>
	inline Lock<MutexType> GetLock(REF MutexType& a_mutex,
								   IN bool a_getLock = true)
	{
		return Lock<MutexType>(a_mutex, a_getLock);
	}
}
