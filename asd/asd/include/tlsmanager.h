#pragma once
#include "../../asd/include/asdbase.h"
#include <memory>
#include <unordered_map>
#include <thread>
#include <mutex>

namespace asd
{
	// Thread Local Storage Manager
#if defined(_MSC_VER) && _MSC_VER <= 1800
#define thread_local __declspec(thread) // VS2013 이하
#endif

	// MSVC의 __declspec(thread)가 생성자, 소멸자를 사용 할 수 없으므로 날 포인터를 사용해야 함.
	// 그 포인터를 자동으로 delete해주기 위한 클래스.
	template<typename T>
	class TLSManager final
	{
		typedef std::shared_ptr<T> ThreadLocalData;
		typedef std::unordered_map<std::thread::id, 
								   ThreadLocalData>  ThreadLocalDataList;

		std::mutex m_lock;
		ThreadLocalDataList m_objects;

	public:
		void Register(IN T*& a_tsd)
		{
			auto tid = std::this_thread::get_id();
			
			m_lock.lock();
			assert(m_objects.find(tid) == m_objects.end());
			m_objects.emplace(tid, ThreadLocalData(a_tsd));
			m_lock.unlock();
		}
		

		void Delete(INOUT T*& a_tsd)
		{
			auto tid = std::this_thread::get_id();
			
			m_lock.lock();
			assert(m_objects[tid].get() == a_tsd);
			m_objects.erase(tid);
			m_lock.unlock();

			a_tsd = nullptr;
		}
	};
}
