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
		typename OBJECT_TYPE,
		typename MUTEX_TYPE = asd::NoLock,
		bool RECYCLE = false,
		size_t HEADER_SIZE = 0
	> class ObjectPool
		: public HasMagicCode< ObjectPool<OBJECT_TYPE, MUTEX_TYPE, RECYCLE, HEADER_SIZE> >
	{
	public:
		using Object	= OBJECT_TYPE;
		using Mutex		= MUTEX_TYPE;
		using ThisType	= ObjectPool<Object, Mutex, RECYCLE, HEADER_SIZE>;

		static constexpr bool IsThreadSafe = IsThreadSafeMutex<Mutex>::Value;
		static constexpr bool Recycle = RECYCLE;
		static constexpr size_t HeaderSize = HEADER_SIZE;

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
				ret = AllocMemory();
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

			if (IsValidMagicCode() == false) {
				a_obj->~OBJECT_TYPE();
				FreeMemory(a_obj);
				return false;
			}

			if (Recycle == false)
				a_obj->~OBJECT_TYPE();

			auto lock = GetLock(m_lock, true);
			if (m_pool.size() < m_limitCount) {
				m_pool.push(a_obj);
				return true;
			}
			lock.unlock();

			if (Recycle)
				a_obj->~OBJECT_TYPE();
			FreeMemory(a_obj);
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
					FreeMemory(buf[j]);
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

				Object* p = AllocMemory();
				m_pool.push(p);
			}
		}



		size_t GetCount() const asd_noexcept
		{
			return m_pool.size();
		}



		template <typename CAST>
		inline static CAST* GetHeader(IN Object* a_obj) asd_noexcept
		{
			static_assert(sizeof(CAST) <= HeaderSize, "invalid cast");
			auto p = (uint8_t*)a_obj;
			return (CAST*)(p - HeaderSize);
		}



	private:
		inline static Object* AllocMemory() asd_noexcept
		{
			auto block = (uint8_t*)::operator new(sizeof(Object) + HeaderSize);
			Object* ret = (Object*)&block[HeaderSize];
			return ret;
		}


		inline static void FreeMemory(IN Object* a_ptr) asd_noexcept
		{
			auto p = (uint8_t*)a_ptr;
			auto block = p - HeaderSize;
			::operator delete(block);
		}

		const size_t m_limitCount;
		std::stack<Object*> m_pool;
		Mutex m_lock;

	};





	template<
		typename OBJECT_TYPE,
		bool RECYCLE = false,
		size_t HEADER_SIZE = 0
	> class ObjectPool2
		: public HasMagicCode< ObjectPool2<OBJECT_TYPE, RECYCLE, HEADER_SIZE> >
	{
	public:
		using Object = OBJECT_TYPE;
		using ThisType = ObjectPool2<Object, RECYCLE, HEADER_SIZE>;

		static constexpr bool IsThreadSafe = true;
		static constexpr bool Recycle = RECYCLE;
		static constexpr size_t HeaderSize = HEADER_SIZE;

	private:
		struct Node final
			: public HasMagicCode<Node>
		{
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

			if (IsValidMagicCode() == false) {
				asd_ChkErrAndRetVal(!node->IsValidMagicCode(), false, "invaild Node pointer");
				a_obj->~OBJECT_TYPE();
				delete node;
				return false;
			}

			if (Recycle == false && node->m_init)
				a_obj->~OBJECT_TYPE();

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
					cast->~OBJECT_TYPE();
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



		template <typename CAST>
		inline static CAST* GetHeader(IN Object* a_obj) asd_noexcept
		{
			static_assert(sizeof(CAST) <= HeaderSize, "invalid cast");
			auto p = (uint8_t*)a_obj;
			return (CAST*)(p - HeaderSize);
		}



	private:
		inline static Object* AllocMemory() asd_noexcept
		{
			auto block = (uint8_t*)::operator new(sizeof(Object) + HeaderSize);
			Object* ret = (Object*)&block[HeaderSize];
			return ret;
		}


		inline static void FreeMemory(IN Object* a_ptr) asd_noexcept
		{
			auto p = (uint8_t*)a_ptr;
			auto block = p - HeaderSize;
			::operator delete(block);
		}


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
			asd_ChkErrAndRetVal(!a_node->IsValidMagicCode(), false, "invaild Node pointer");

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


		struct ShardSetHeader
		{
			size_t ShardIndex;
		};


		template <typename POOL>
		struct PoolTemplate {};

		template <typename OBJECT_TYPE, typename MUTEX_TYPE, bool RECYCLE, size_t HEADER_SIZE>
		struct PoolTemplate < asd::ObjectPool<OBJECT_TYPE, MUTEX_TYPE, RECYCLE, HEADER_SIZE> >
		{
			using Type = asd::ObjectPool<
				OBJECT_TYPE,
				MUTEX_TYPE,
				RECYCLE,
				HEADER_SIZE + sizeof(ShardSetHeader)
			>;
		};

		template <typename OBJECT_TYPE, bool RECYCLE, size_t HEADER_SIZE>
		struct PoolTemplate < asd::ObjectPool2<OBJECT_TYPE, RECYCLE, HEADER_SIZE> >
		{
			using Type = asd::ObjectPool2<
				OBJECT_TYPE,
				RECYCLE,
				HEADER_SIZE + sizeof(ShardSetHeader)
			>;
		};


		static constexpr size_t OrgHeaderSize = ObjectPoolType::HeaderSize;



	public:
		typedef ObjectPoolShardSet<ObjectPoolType>				ThisType;
		typedef typename PoolTemplate<ObjectPoolType>::Type		Pool;
		typedef typename Pool::Object							Object;



		ObjectPoolShardSet(IN size_t a_shardCount = 4*Get_HW_Concurrency(),
						   IN size_t a_totalLimitCount = std::numeric_limits<size_t>::max(),
						   IN size_t a_initCount = 0)
			: m_shardCount(std::max(a_shardCount, size_t(1)))
		{
			m_memory.resize(m_shardCount * sizeof(Pool));
			m_shards = (Pool*)m_memory.data();

			const size_t limitCount = a_totalLimitCount / m_shardCount;
			for (size_t i=0; i<m_shardCount; ++i)
				new(&m_shards[i]) Pool(limitCount, a_initCount);
		}



		virtual ~ObjectPoolShardSet() asd_noexcept
		{
			for (size_t i=0; i<m_shardCount; ++i)
				m_shards[i].~Pool();
		}



		template<typename... ARGS>
		inline Object* Alloc(ARGS&&... a_constructorArgs)
		{
			size_t index = GetShardIndex();
			Object* ret = m_shards[index].Alloc(std::forward<ARGS>(a_constructorArgs)...);
			ShardSetHeader* header = GetShardSetHeader(ret);
			header->ShardIndex = index;
			return ret;
		}



		inline bool Free(MOVE Object*& a_obj)
		{
			size_t index = GetShardIndex(a_obj);
			return m_shards[index].Free(a_obj);
		}



		template <typename CAST>
		inline static CAST* GetHeader(IN Object* a_obj) asd_noexcept
		{
			return Pool::template GetHeader<CAST>(a_obj);
		}



	private:
		inline size_t GetShardIndex(IN Object* a_obj = nullptr) asd_noexcept
		{
			size_t index;
			if (a_obj == nullptr) {
				// alloc
				index = GetCurrentThreadSequence() % m_shardCount;
			}
			else {
				// free
				ShardSetHeader* header = GetShardSetHeader(a_obj);
				index = header->ShardIndex;
			}
			return index;
		}


		inline static ShardSetHeader* GetShardSetHeader(IN Object* a_obj) asd_noexcept
		{
			auto block = Pool::template GetHeader<uint8_t>(a_obj);
			return (ShardSetHeader*)(block + OrgHeaderSize);
		}


		const size_t			m_shardCount;
		std::vector<uint8_t>	m_memory;
		Pool*					m_shards;

	};
}
