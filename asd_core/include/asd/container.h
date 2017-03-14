#pragma once
#include "asdbase.h"
#include "lock.h"
#include "util.h"
#include <map>
#include <unordered_map>

namespace asd
{
	template <typename SHARD_KEY, typename CONTAINER>
	class ShardSet
	{
	public:
		using ShardKey = SHARD_KEY;

		class Container : public CONTAINER
		{
		public:
			using BaseContainer = CONTAINER;
			using BaseContainer::BaseContainer;

			inline Lock<Mutex> GetLock(IN bool a_getLock = true) const
			{
				return asd::GetLock(m_lock, a_getLock);
			}

			inline void clear()
			{
				auto lock = GetLock();
				BaseContainer::clear();
			}

			virtual ~Container()
			{
				clear();
			}

		private:
			mutable Mutex m_lock;
		};


		inline ShardSet(IN size_t a_shardCount = 2*Get_HW_Concurrency()) asd_noexcept
		{
			m_shardCount = std::max(size_t(1), a_shardCount);
			m_memory.resize(sizeof(Container) * m_shardCount);
			m_shards = (Container*)m_memory.data();
			for (size_t i=0; i<m_shardCount; ++i)
				new(&m_shards[i]) Container();
		}


		inline Container* GetShard(IN const ShardKey& a_shardKey) asd_noexcept
		{
			return &m_shards[a_shardKey % m_shardCount];
		}


		inline const Container* GetShard(IN const ShardKey& a_shardKey) const asd_noexcept
		{
			return &m_shards[a_shardKey % m_shardCount];
		}


		inline void clear()
		{
			for (size_t i=0; i<m_shardCount; ++i)
				m_shards[i].clear();
		}


		virtual ~ShardSet() asd_noexcept
		{
			for (size_t i=0; i<m_shardCount; ++i)
				m_shards[i].~Container();
		}


	private:
		std::vector<uint8_t> m_memory;
		Container* m_shards;
		size_t m_shardCount;

	};



	template <typename KEY, typename VALUE, typename MAP>
	class ShardedMapTemplate
		: public ShardSet<KEY, MAP>
	{
	public:
		using key_type		= KEY;
		using mapped_type	= std::shared_ptr<VALUE>;
		using value_type	= std::pair<const key_type, mapped_type>;
		using Map			= MAP;
		using BaseType		= ShardSet<KEY, Map>;

		static_assert(IsEqualType<value_type, typename Map::value_type>::Value, "type missmatch");


		using BaseType::BaseType;


		bool Insert(IN const key_type& a_key,
					IN const mapped_type& a_val) asd_noexcept
		{
			auto shard = GetShard(a_key);
			auto lock = shard->GetLock();
			return shard->emplace(a_key, a_val).second;
		}


		mapped_type Find(IN const key_type& a_key) asd_noexcept
		{
			auto shard = GetShard(a_key);
			auto lock = shard->GetLock();
			auto it = shard->find(a_key);
			if (it == shard->end())
				return nullptr;
			return it->second;
		}


		const mapped_type Find(IN const key_type& a_key) const asd_noexcept
		{
			auto shard = GetShard(a_key);
			auto lock = shard->GetLock();
			auto it = shard->find(a_key);
			if (it == shard->end())
				return nullptr;
			return it->second;
		}


		bool Erase(IN const key_type& a_key) asd_noexcept
		{
			auto shard = GetShard(a_key);
			auto lock = shard->GetLock();
			auto it = shard->find(a_key);
			if (it == shard->end())
				return false;
			auto del = std::move(it->second);
			shard->erase(it);
			lock.unlock();
			del.reset();
			return true;
		}
	};


	template <typename KEY, typename VALUE, typename... ARGS>
	using ShardedMap = ShardedMapTemplate<KEY, VALUE, std::map<KEY, std::shared_ptr<VALUE>, ARGS...>>;


	template <typename KEY, typename VALUE, typename... ARGS>
	using ShardedHashMap = ShardedMapTemplate<KEY, VALUE, std::unordered_map<KEY, std::shared_ptr<VALUE>, ARGS...>>;
}
