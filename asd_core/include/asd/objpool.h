#pragma once
#include "asdbase.h"
#include "lock.h"
#include "util.h"
#include <stack>
#include <atomic>
#include <typeinfo>

namespace asd
{
	template<
		typename ObjectType,
		bool Recycle = false,
		typename MutexType = asd::NoLock
	> class ObjectPool
	{
	public:
		typedef ObjectType									Object;
		typedef ObjectPool<ObjectType, Recycle, MutexType>	ThisType;
		typedef MtxCtl<MutexType>							Lock;


		ObjectPool(IN const ThisType&) = delete;
		ObjectPool& operator=(IN const ThisType&) = delete;

		ObjectPool(MOVE ThisType&& a_mv) = default;
		ObjectPool& operator=(MOVE ThisType&&) = delete;

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
		Object* Alloc(MOVE ARGS&&... a_constructorArgs)
		{
			Object* ret = nullptr;

			Lock lock(m_lock, true);
			if (m_pool.empty() == false) {
				ret = m_pool.top();
				m_pool.pop();
			}
			lock.unlock();

			if (ret == nullptr)
				ret = new Object(std::move(a_constructorArgs)...);
			else if (Recycle == false)
				new(ret) Object(std::move(a_constructorArgs)...);

			assert(ret != nullptr);
			return ret;
		}



		// 풀링되면 true, 풀이 꽉 차면 false 리턴
		bool Free(IN Object* a_obj) asd_noexcept
		{
			if (a_obj == nullptr)
				return true;

			if (Recycle == false)
				a_obj->~Object();
			return FreeMemory(a_obj);
		}



		void Clear() asd_noexcept
		{
			const int BufSize = 1024;
			Object* buf[BufSize];

			bool loop;
			do {
				Lock lock(m_lock, true);
				int i;
				for (i=0; m_pool.empty()==false && i<BufSize; ++i) {
					buf[i] = m_pool.top();
					m_pool.pop();
				}
				loop = m_pool.empty() == false;
				lock.unlock();

				for (int j=0; j<i; ++j) {
					if (Recycle)
						delete buf[j];
					else
						::operator delete(buf[j]);
				}
			} while (loop);
		}



		void AddCount(IN size_t a_count)
		{
			if (a_count <= 0)
				return;

			while (a_count > 0) {
				--a_count;
				Object* p;
				if (Recycle)
					p = new Object;
				else
					p = (Object*) ::operator new(sizeof(Object));;
				if (FreeMemory(p) == false)
					break;
			}
		}



		size_t GetCount() const asd_noexcept
		{
			return m_pool.size();
		}



	private:
		// 풀링되면 true, 풀이 꽉 차면 false 리턴
		bool FreeMemory(IN Object* a_obj) asd_noexcept
		{
			if (a_obj == nullptr)
				return true;

			Lock lock(m_lock, true);
			if (m_pool.size() < m_limitCount) {
				m_pool.push(a_obj);
				return true;
			}
			lock.unlock();
			if (Recycle)
				delete a_obj;
			else
				::operator delete(a_obj);
			return false;
		}



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
		typedef ObjectPool2<ObjectType, Recycle>	ThisType;
		typedef ObjectType							Object;

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
		Object* Alloc(MOVE ARGS&&... a_constructorArgs)
		{
			Node* node = PopNode();
			if (node == nullptr)
				node = new Node;

			Object* ret = (Object*)node->m_data;
			if (Recycle == false || node->m_init == false) {
				node->m_init = true;
				new(ret) Object(a_constructorArgs...);
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
			assert(chkCnt > 0);
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
			assert(a_node != nullptr);
			if (a_node->m_sign != Sign())
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
		static constexpr bool Value = false;
	};

	template<typename ObjectType>
	struct IsThreadSafePool<ObjectPool2<ObjectType, true>>
	{
		static constexpr bool Value = true;
	};

	template<typename ObjectType>
	struct IsThreadSafePool<ObjectPool2<ObjectType, false>>
	{
		static constexpr bool Value = true;
	};

	template<typename ObjectType>
	struct IsThreadSafePool<ObjectPool<ObjectType, true, asd::Mutex>>
	{
		static constexpr bool Value = true;
	};

	template<typename ObjectType>
	struct IsThreadSafePool<ObjectPool<ObjectType, false, asd::Mutex>>
	{
		static constexpr bool Value = true;
	};

	template<typename ObjectType>
	struct IsThreadSafePool<ObjectPool<ObjectType, true, asd::SpinMutex>>
	{
		static constexpr bool Value = true;
	};

	template<typename ObjectType>
	struct IsThreadSafePool<ObjectPool<ObjectType, false, asd::SpinMutex>>
	{
		static constexpr bool Value = true;
	};

	template<typename ObjectType>
	struct IsThreadSafePool<ObjectPool<ObjectType, true, std::mutex>>
	{
		static constexpr bool Value = true;
	};

	template<typename ObjectType>
	struct IsThreadSafePool<ObjectPool<ObjectType, false, std::mutex>>
	{
		static constexpr bool Value = true;
	};

	template<typename ObjectType>
	struct IsThreadSafePool<ObjectPool<ObjectType, true, std::recursive_mutex>>
	{
		static constexpr bool Value = true;
	};

	template<typename ObjectType>
	struct IsThreadSafePool<ObjectPool<ObjectType, false, std::recursive_mutex>>
	{
		static constexpr bool Value = true;
	};



	template <typename ObjectPoolType>
	class ObjectPoolShardSet
	{
		static_assert(IsThreadSafePool<ObjectPoolType>::Value, "thread unsafe pool");


	public:
		typedef ObjectPoolShardSet<ObjectPoolType>		ThisType;
		typedef ObjectPoolType							ObjectPool;
		typedef typename ObjectPool::Object				Object;



		ObjectPoolShardSet(IN uint32_t a_shardCount = std::thread::hardware_concurrency(),
						   IN size_t a_totalLimitCount = std::numeric_limits<size_t>::max(),
						   IN size_t a_initCount = 0)
			: m_shardCount(a_shardCount)
		{
			assert(m_shardCount > 0 && m_shardCount <= Prime);
			m_memory.resize(m_shardCount * sizeof(ObjectPool));
			m_shards = (ObjectPool*)m_memory.data();

			const size_t limitCount = a_totalLimitCount / m_shardCount;
			for (uint32_t i=0; i<m_shardCount; ++i)
				new(&m_shards[i]) ObjectPool(limitCount, a_initCount);
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
			else
				index = (((uint64_t)a_key) % Prime) % m_shardCount;
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
		inline Object* Alloc(MOVE ARGS&&... a_constructorArgs)
		{
			auto& pool = GetShard();
			return pool.Alloc(std::move(a_constructorArgs)...);
		}



		inline bool Free(MOVE Object*& a_obj)
		{
			auto& pool = GetShard(a_obj);
			return pool.Free(a_obj);
		}



	private:
		const static uint32_t	Prime = 997;
		const uint32_t			m_shardCount;
		std::vector<uint8_t>	m_memory;
		ObjectPool*				m_shards;
	};
}