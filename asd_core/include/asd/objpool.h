#pragma once
#include "asdbase.h"
#include "lock.h"
#include "util.h"
#include <stack>
#include <atomic>
#include <typeinfo>

namespace asd
{
	template<typename LOCK> struct IsThreadSafeMutex			{ static constexpr bool Value = false; };
	template<> struct IsThreadSafeMutex<asd::Mutex>				{ static constexpr bool Value = true; };
	template<> struct IsThreadSafeMutex<asd::SpinMutex>			{ static constexpr bool Value = true; };
	template<> struct IsThreadSafeMutex<std::mutex>				{ static constexpr bool Value = true; };
	template<> struct IsThreadSafeMutex<std::recursive_mutex>	{ static constexpr bool Value = true; };



	template<
		typename ObjectType,
		bool Recycle = false,
		typename MutexType = asd::NoLock
	> class ObjectPool
	{
	public:
		static constexpr bool IsThreadSafe = IsThreadSafeMutex<MutexType>::Value;

		typedef ObjectType								Object;
		typedef ObjectPool<Object, Recycle, MutexType>	ThisType;

		ObjectPool(IN const ThisType&) = delete;
		ObjectPool& operator=(IN const ThisType&) = delete;

		ObjectPool(MOVE ThisType&& a_mv) = default;
		ObjectPool& operator=(MOVE ThisType&&) = default;

		ObjectPool(IN size_t a_limitCount = std::numeric_limits<size_t>::max(),
				   IN size_t a_initCount = 0)
			: m_limitCount(a_limitCount)
		{
			AddCount(a_initCount);
		}



		virtual ~ObjectPool() asd_noexcept
		{
			Clear();
		}



		template<typename... ARGS>
		Object* Alloc(ARGS&&... a_constructorArgs)
		{
			Object* ret = nullptr;

			auto lock = GetLock(m_lock, true);
			if (m_pool.empty() == false) {
				ret = m_pool.top();
				m_pool.pop();
			}
			lock.unlock();

			if (ret == nullptr) {
				ret = (Object*) ::operator new(sizeof(Object));
				new(ret) Object(std::forward<ARGS>(a_constructorArgs)...);
			}
			else if (Recycle == false) {
				new(ret) Object(std::forward<ARGS>(a_constructorArgs)...);
			}
			return ret;
		}



		// 풀링되면 true, 풀이 꽉 차면 false 리턴
		bool Free(IN Object* a_obj) asd_noexcept
		{
			if (a_obj == nullptr)
				return true;

			if (Recycle == false)
				a_obj->~ObjectType();

			auto lock = GetLock(m_lock, true);
			if (m_pool.size() < m_limitCount) {
				m_pool.push(a_obj);
				return true;
			}
			lock.unlock();

			if (Recycle)
				a_obj->~ObjectType();
			::operator delete(a_obj);
			return false;
		}



		void Clear() asd_noexcept
		{
			const int BufSize = 1024;
			Object* buf[BufSize];

			bool loop;
			do {
				auto lock = GetLock(m_lock, true);
				int i;
				for (i=0; m_pool.empty()==false && i<BufSize; ++i) {
					buf[i] = m_pool.top();
					m_pool.pop();
				}
				loop = m_pool.empty() == false;
				lock.unlock();

				for (int j=0; j<i; ++j) {
					if (Recycle)
						buf[j]->~Object();
					::operator delete(buf[j]);
				}
			} while (loop);
		}



		void AddCount(IN size_t a_count)
		{
			while (a_count > 0) {
				--a_count;

				auto lock = GetLock(m_lock, true);
				if (m_pool.size() >= m_limitCount)
					return;

				Object* p = (Object*) ::operator new(sizeof(Object));
				m_pool.push(p);
			}
		}



		size_t GetCount() const asd_noexcept
		{
			return m_pool.size();
		}



	private:
		const size_t m_limitCount;
		std::stack<Object*> m_pool;
		MutexType m_lock;
	};





	template<
		typename ObjectType,
		bool Recycle = false
	> class ObjectPool2
	{
	public:
		static constexpr bool IsThreadSafe = true;

		typedef ObjectType						Object;
		typedef ObjectPool2<Object, Recycle>	ThisType;


		inline static const size_t Sign()
		{
			static const size_t g_sign = typeid(ThisType).hash_code();
			return g_sign;
		}


	private:
		struct Node final
		{
			const size_t				m_sign = ThisType::Sign();
			const std::atomic<int>*		m_popContention = nullptr;
			bool						m_init = false;
			Node*						m_next = nullptr;
			uint8_t						m_data[sizeof(Object)];

			inline Node() {}
			void SafeWait(IN const std::atomic<int>* a_diff)
			{
				if (m_popContention != nullptr && m_popContention == a_diff) {
					while (*m_popContention > 0);
					m_popContention = nullptr;
				}
			}
			~Node()
			{
				SafeWait(m_popContention);
			}
		};



	public:
		ObjectPool2(IN const ThisType&) = delete;
		ObjectPool2& operator=(IN const ThisType&) = delete;

		ObjectPool2(IN size_t a_limitCount = std::numeric_limits<size_t>::max(),
					IN size_t a_initCount = 0)
			: m_limitCount(a_limitCount)
			, m_pooledCount(0)
			, m_head(nullptr)
			, m_popContention(0)
		{
			AddCount(a_initCount);
		}



		virtual ~ObjectPool2() asd_noexcept
		{
			Clear();
		}



		template<typename... ARGS>
		Object* Alloc(ARGS&&... a_constructorArgs)
		{
			Node* node = PopNode();
			if (node == nullptr)
				node = new Node;

			Object* ret = (Object*)node->m_data;
			if (Recycle == false || node->m_init == false) {
				node->m_init = true;
				new(ret) Object(std::forward<ARGS>(a_constructorArgs)...);
			}
			return ret;
		}



		// 풀링되면 true, 풀이 꽉 차면 false 리턴
		bool Free(IN Object* a_obj)
		{
			if (a_obj == nullptr)
				return true;

			const size_t offset = offsetof(Node, m_data);
			Node* node = (Node*)((uint8_t*)a_obj - offset);

			if (Recycle == false && node->m_init)
				a_obj->~Object();

			return PushNode(node);
		}



		void AddCount(IN size_t a_count)
		{
			if (a_count <= 0)
				return;

			while (a_count > 0) {
				--a_count;
				if (PushNode(new Node) == false)
					break;
			}
		}



		void Clear() asd_noexcept
		{
			++m_popContention;
			Node* snapshot;
			do {
				snapshot = m_head;
			} while (false == m_head.compare_exchange_strong(snapshot, nullptr));

			size_t chkCnt = m_popContention--;
			asd_DAssert(chkCnt > 0);
			if (snapshot != nullptr && chkCnt > 1)
				while (m_popContention > 0);

			while (snapshot != nullptr) {
				Node* del = snapshot;
				snapshot = snapshot->m_next;

				del->m_popContention = nullptr;
				if (Recycle && del->m_init) {
					auto cast = (Object*)del->m_data;
					cast->~Object();
				}
				delete del;

				size_t sz = m_pooledCount--;
				asd_DAssert(sz > 0);
			}
		}



		size_t GetCount() const asd_noexcept
		{
			return m_pooledCount;
		}



	private:
		Node* PopNode() asd_noexcept
		{
			++m_popContention;
			Node* snapshot = m_head;
			while (snapshot != nullptr) {
				Node* next = snapshot->m_next;
				if (false == m_head.compare_exchange_strong(snapshot, next))
					continue;

				break;
			}

			size_t chkCnt = m_popContention--;
			asd_DAssert(chkCnt > 0);
			if (snapshot != nullptr) {
				if (chkCnt > 1)
					snapshot->m_popContention = &m_popContention;
				else
					snapshot->m_popContention = nullptr;
				chkCnt = m_pooledCount--;
				asd_DAssert(chkCnt > 0);
			}
			return snapshot;
		}



		bool PushNode(IN Node* a_node)
		{
			asd_DAssert(a_node != nullptr);
			if (a_node->m_sign != Sign())
				asd_RaiseException("invaild Node pointer");

			a_node->SafeWait(&m_popContention);
			if (++m_pooledCount > m_limitCount) {
				--m_pooledCount;
				delete a_node;
				return false;
			}

			Node* snapshot = m_head;
			do {
				a_node->m_next = snapshot;
			} while (false == m_head.compare_exchange_strong(snapshot, a_node));
			return true;
		}


		const size_t m_limitCount;
		std::atomic<size_t> m_pooledCount;
		std::atomic<Node*> m_head;
		std::atomic<int> m_popContention;
	};



	template <typename ObjectPoolType>
	class ObjectPoolShardSet
	{
		static_assert(ObjectPoolType::IsThreadSafe, "thread unsafe pool");


	public:
		typedef ObjectPoolShardSet<ObjectPoolType>		ThisType;
		typedef ObjectPoolType							ObjectPool;
		typedef typename ObjectPool::Object				Object;



		ObjectPoolShardSet(IN uint32_t a_shardCount = Get_HW_Concurrency(),
						   IN size_t a_totalLimitCount = std::numeric_limits<size_t>::max(),
						   IN size_t a_initCount = 0)
			: m_shardCount(a_shardCount)
		{
			if (m_shardCount == 0)
				asd_RaiseException("invalid shardCount");
			asd_DAssert(m_shardCount <= Prime);
			m_memory.resize(m_shardCount * sizeof(ObjectPool));
			m_shards = (ObjectPool*)m_memory.data();

			const size_t limitCount = a_totalLimitCount / m_shardCount;
			for (size_t i=0; i<m_shardCount; ++i)
				new(&m_shards[i]) ObjectPool(limitCount, a_initCount);
		}



		virtual ~ObjectPoolShardSet() asd_noexcept
		{
			for (size_t i=0; i<m_shardCount; ++i)
				m_shards[i].~ObjectPool();
		}



		inline ObjectPool& GetShard(IN void* a_obj = nullptr) asd_noexcept
		{
			size_t index;
#if 1
			// 컨텐션이 적지만
			// 샤드개수가 쓰레드개수보다 많을 경우 누수의 여지가 있음
			if (a_obj == nullptr)
				index = GetCurrentThreadSequence() % m_shardCount; // alloc
			else
				index = (((size_t)a_obj) % Prime) % m_shardCount; // free
#elif 0
			thread_local size_t t_idx = GetCurrentThreadSequence();
			index = ++t_idx % m_shardCount;
#endif
			return m_shards[index];
		}



		inline Object* AllocMemory()
		{
			auto& pool = GetShard();
			return pool.AllocMemory();
		}



		inline bool FreeMemory(IN Object* a_obj)
		{
			if (a_obj == nullptr)
				return true;
			auto& pool = GetShard(a_obj);
			return pool.FreeMemory(a_obj);
		}



		template<typename... ARGS>
		inline Object* Alloc(ARGS&&... a_constructorArgs)
		{
			auto& pool = GetShard();
			return pool.Alloc(std::forward<ARGS>(a_constructorArgs)...);
		}



		inline bool Free(MOVE Object*& a_obj)
		{
			auto& pool = GetShard(a_obj);
			return pool.Free(a_obj);
		}



	private:
		const static size_t		Prime = 997;
		const size_t			m_shardCount;
		std::vector<uint8_t>	m_memory;
		ObjectPool*				m_shards;
	};
}
