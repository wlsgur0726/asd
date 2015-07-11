#pragma once
#include "../../asd/include/asdbase.h"
#include "../../asd/include/lock.h"
#include <stack>

namespace asd
{
	template<typename ObjectType, typename MutexType = asd::NoLock>
	class ObjectPool
	{
	public:
		typedef ObjectType					Object;
		typedef ObjectPool<ObjectType>		ThisType;
		typedef MutexController<MutexType>	MtxCtl;


		// a_limitCount가 0이면 무제한
		ObjectPool(IN std::size_t a_limitCount = 0,
				   IN std::size_t a_initCount = 0)
			asd_Throws(std::bad_alloc)
		{
			SetLimitCount(a_limitCount);
			AddCount(a_initCount);
		}



		virtual ~ObjectPool() 
			asd_NoThrow
		{
			Clear();
		}
		


		template<typename... ARGS>
		Object* Get(ARGS&&... a_constructorArgs)
			asd_Throws(asd::Exception, std::bad_alloc)
		{
			Object* ret = nullptr;

			MtxCtl lock(m_lock, true);
			if (m_pool.empty() == false) {
				ret = m_pool.top();
				m_pool.pop();
			}
			lock.unlock();

			if (ret == nullptr)
				ret = (Object*) ::operator new(sizeof(Object));

			new(ret) Object(a_constructorArgs...);

			return ret;
		}



		// 풀링되면 true, 풀이 꽉 차면 false 리턴
		bool Release(MOVE Object*& a_obj)
			asd_NoThrow
		{
			if (a_obj == nullptr)
				return true;

			Object* p = a_obj;
			a_obj = nullptr;

			p->~Object();
			
			return Release_Internal(p);
		}



		void Clear()
			asd_NoThrow
		{
			const int BufSize = 1024;
			Object* buf[BufSize];

			bool loop;
			do {
				MtxCtl lock(m_lock, true);
				int i;
				for (i=0; m_pool.empty()==false && i<BufSize; ++i) {
					buf[i] = m_pool.top();
					m_pool.pop();
				}
				loop = m_pool.empty() == false;
				lock.unlock();

				for (int j=0; j<i; ++j)
					::operator delete(buf[j]);
			} while (loop);
		}



		void SetLimitCount(IN std::size_t a_limitCount)
			asd_NoThrow
		{
			MtxCtl lock(m_lock, true);
			m_limitCount = std::max(a_limitCount, (std::size_t)0);
		}



		void AddCount(IN std::size_t a_count)
			asd_Throws(std::bad_alloc)
		{
			if (a_count <= 0)
				return;

			while (a_count > 0) {
				--a_count;
				Object* p = (Object*) ::operator new(sizeof(Object));;
				if (Release_Internal(p) == false)
					break;
			}
		}



		std::size_t GetCount() const 
			asd_NoThrow
		{
			return m_pool.size();
		}



	private:
		ObjectPool(const ObjectPool&) = delete;
		ObjectPool& operator = (const ObjectPool&) = delete;



		// 풀링되면 true, 풀이 꽉 차면 false 리턴
		inline bool Release_Internal(IN Object* a_obj)
			asd_NoThrow
		{
			assert(a_obj != nullptr);

			MtxCtl lock(m_lock, true);
			if (m_limitCount > 0 && m_pool.size()>=m_limitCount) {
				lock.unlock();
				::operator delete(a_obj);
				return false;
			}
			else {
				m_pool.push(a_obj);
				return true;
			}
		}



		// 0이면 무제한
		std::size_t m_limitCount;

		std::stack<Object*> m_pool;

		MutexType m_lock;
	};
}