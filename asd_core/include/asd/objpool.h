#pragma once
#include "asd/asdbase.h"
#include "asd/lock.h"
#include "asd/util.h"
#include <stack>
#include <atomic>

namespace asd
{
	template<typename ObjectType, typename MutexType = asd::NoLock>
	class ObjectPool
	{
	public:
		typedef ObjectPool<ObjectType>		ThisType;
		typedef ObjectType					Object;
		typedef MutexController<MutexType>	MtxCtl;


		ObjectPool(const ThisType&) = delete;
		ObjectPool& operator = (const ThisType&) = delete;
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
		Object* Get(MOVE ARGS&&... a_constructorArgs)
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
		bool Release(MOVE Object*& a_obj) asd_noexcept
		{
			if (a_obj == nullptr)
				return true;

			Object* p = a_obj;
			a_obj = nullptr;

			p->~Object();
			
			return Release_Internal(p);
		}



		void Clear() asd_noexcept
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



		void AddCount(IN size_t a_count)
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



		size_t GetCount() const asd_noexcept
		{
			return m_pool.size();
		}



	private:
		// 풀링되면 true, 풀이 꽉 차면 false 리턴
		inline bool Release_Internal(IN Object* a_obj) asd_noexcept
		{
			assert(a_obj != nullptr);

			MtxCtl lock(m_lock, true);
			if (m_pool.size() >= m_limitCount) {
				lock.unlock();
				::operator delete(a_obj);
				return false;
			}
			else {
				m_pool.push(a_obj);
				return true;
			}
		}

		const size_t m_limitCount;
		std::stack<Object*> m_pool;
		MutexType m_lock;
	};





	template<typename ObjectType>
	class ObjectPool2
	{
	public:
		typedef ObjectPool2<ObjectType>		ThisType;
		typedef ObjectType					Object;
		static const size_t Sign = sizeof(Object);



	private:
		struct Node final
		{
			const void*					m_sign = &Sign;
			const std::atomic<int>*		m_popContention = nullptr;
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
		ObjectPool2(const ThisType&) = delete;
		ObjectPool2& operator = (const ThisType&) = delete;
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
		Object* Get(MOVE ARGS&&... a_constructorArgs)
		{
			Node* pop = PopNode();
			if (pop == nullptr)
				pop = new Node;

			Object* ret = (Object*)pop->m_data;
			new(ret) Object(a_constructorArgs...);
			return ret;
		}



		// 풀링되면 true, 풀이 꽉 차면 false 리턴
		bool Release(MOVE Object*& a_obj)
		{
			if (a_obj == nullptr)
				return true;

			Object* obj = a_obj;
			a_obj = nullptr;
			obj->~Object();

			const size_t offset = offsetof(Node, m_data);
			Node* node = (Node*)((uint8_t*)obj - offset);
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
			assert(chkCnt > 0);
			if (snapshot != nullptr && chkCnt > 1)
				while (m_popContention > 0);

			while (snapshot != nullptr) {
				Node* del = snapshot;
				snapshot = snapshot->m_next;

				del->m_popContention = nullptr;
				delete del;

				size_t sz = m_pooledCount--;
				assert(sz > 0);
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
			Node* snapshot;
			while (true) {
				snapshot = m_head;
				if (snapshot == nullptr)
					break;

				Node* next = snapshot->m_next;
				if (false == m_head.compare_exchange_strong(snapshot, next))
					continue;

				break;
			}

			size_t chkCnt = m_popContention--;
			assert(chkCnt > 0);
			if (snapshot != nullptr) {
				if (chkCnt > 1)
					snapshot->m_popContention = &m_popContention;
				else
					snapshot->m_popContention = nullptr;
				chkCnt = m_pooledCount--;
				assert(chkCnt > 0);
			}
			return snapshot;
		}



		bool PushNode(IN Node* a_node)
		{
			if (a_node->m_sign != &Sign)
				asd_RaiseException("invaild Node pointer");

			a_node->SafeWait(&m_popContention);
			if (++m_pooledCount > m_limitCount) {
				--m_pooledCount;
				delete a_node;
				return false;
			}

			Node* snapshot;
			do {
				snapshot = m_head;
				a_node->m_next = snapshot;
			} while (false == m_head.compare_exchange_strong(snapshot, a_node));
			return true;
		}


		const size_t m_limitCount;
		std::atomic<size_t> m_pooledCount;
		std::atomic<Node*> m_head;
		std::atomic<int> m_popContention;
	};




	template <typename PoolType>
	struct IsThreadSafePool
	{
		static constexpr bool Value() { return false; }
	};

	template<typename ObjectType>
	struct IsThreadSafePool<ObjectPool2<ObjectType>>
	{
		static constexpr bool Value() { return true; }
	};

	template<typename ObjectType>
	struct IsThreadSafePool<ObjectPool<ObjectType, asd::Mutex>>
	{
		static constexpr bool Value() { return true; }
	};

	template<typename ObjectType>
	struct IsThreadSafePool<ObjectPool<ObjectType, asd::SpinMutex>>
	{
		static constexpr bool Value() { return true; }
	};

	template<typename ObjectType>
	struct IsThreadSafePool<ObjectPool<ObjectType, std::mutex>>
	{
		static constexpr bool Value() { return true; }
	};

	template<typename ObjectType>
	struct IsThreadSafePool<ObjectPool<ObjectType, std::recursive_mutex>>
	{
		static constexpr bool Value() { return true; }
	};




	template <typename ObjectPoolType>
	class ObjectPoolShardSet
	{
		static_assert(IsThreadSafePool<ObjectPoolType>::Value(), "thread unsafe pool");


	public:
		typedef ObjectPoolShardSet<ObjectPoolType>		ThisType;
		typedef ObjectPoolType							ObjectPool;
		typedef typename ObjectPool::Object				Object;



		ObjectPoolShardSet(IN uint32_t a_shardCount = std::thread::hardware_concurrency(),
						   IN size_t a_limitCount = std::numeric_limits<size_t>::max(),
						   IN size_t a_initCount = 0)
			: m_shardCount(a_shardCount)
		{
			assert(m_shardCount > 0 && m_shardCount <= Prime);
			m_memory.resize(m_shardCount * sizeof(ObjectPool));
			m_shards = (ObjectPool*)m_memory.data();
			for (uint32_t i=0; i<m_shardCount; ++i)
				new(&m_shards[i]) ObjectPool(a_limitCount, a_initCount);
		}



		virtual ~ObjectPoolShardSet() asd_noexcept
		{
			for (uint32_t i=0; i<m_shardCount; ++i)
				m_shards[i].~ObjectPool();
		}



		inline ObjectPool& GetShard(IN void* a_key = nullptr) asd_noexcept
		{
			uint32_t index;
			if (a_key == nullptr)
				index = GetCurrentThreadSequence() % m_shardCount;
			else {
				index = (((uint64_t)a_key) % Prime) % m_shardCount;
			}
			return m_shards[index];
		}



		template<typename... ARGS>
		inline Object* Get(MOVE ARGS&&... a_constructorArgs)
		{
			auto& pool = GetShard();
			return pool.Get(std::move(a_constructorArgs)...);
		}



		inline bool Release(MOVE Object*& a_obj)
		{
			if (a_obj == nullptr)
				return true;
			auto& pool = GetShard(a_obj);
			return pool.Release(a_obj);
		}



	private:
		const static uint32_t	Prime = 997;
		const uint32_t			m_shardCount;
		std::vector<uint8_t>	m_memory;
		ObjectPool*				m_shards;
	};
}