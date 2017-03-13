#pragma once
#include "asdbase.h"
#include "lock.h"
#include "util.h"
#include "objpool.h"
#include <vector>
#include <unordered_map>


namespace asd
{
	template <typename OBJECT_TYPE, typename ID_TYPE = uint64_t>
	class Handle
	{
	public:
		using ID = ID_TYPE;
		using Object = OBJECT_TYPE;
		using Object_ptr = std::shared_ptr<Object>;
		using HandleType = Handle<Object, ID>;
		using Pool = Global<ObjectPoolShardSet< ObjectPool<Object, false, Mutex> >>;

		static const ID Null = 0;

	protected:
		ID m_id;

	public:
		inline Handle(IN ID a_id = Null)
			: m_id(a_id)
		{
		}

		inline Handle(IN const HandleType&) asd_noexcept = default;
		inline Handle(MOVE HandleType&& a_mv) asd_noexcept
		{
			operator=(std::move(a_mv));
		}

		inline HandleType& operator=(IN const HandleType&) asd_noexcept = default;
		inline HandleType& operator=(MOVE HandleType&& a_mv) asd_noexcept
		{
			operator=(a_mv);
			a_mv.m_id = Null;
			return *this;
		}

		inline ID GetID() const asd_noexcept
		{
			return m_id;
		}

		inline operator ID() const asd_noexcept
		{
			return GetID();
		}

		inline Object_ptr GetObj() const asd_noexcept
		{
			if (m_id == Null)
				return Object_ptr();
			auto map = Manager::Instance().Shard(m_id);
			return map->Find(m_id);
		}

		inline operator Object_ptr() const asd_noexcept
		{
			return GetObj();
		}

		inline bool IsValid() const asd_noexcept
		{
			return GetObj() != nullptr;
		}

		void Free() asd_noexcept
		{
			if (m_id == Null)
				return;
			ID id = m_id;
			m_id = Null;
			auto map = Manager::Instance().Shard(id);
			map->Delete(id);
		}

		template <typename... ARGS>
		void Alloc(ARGS&&... a_constructorArgs)
		{
			Alloc_Internal(Pool::Instance().Alloc(std::forward<ARGS>(a_constructorArgs)...));
		}

	private:
		void Alloc_Internal(IN Object* a_obj) asd_noexcept
		{
			Object_ptr obj(a_obj,
						   [](IN Object* a_ptr) { Pool::Instance().Free(a_ptr); });
			bool added;
			do {
				m_id = Manager::Instance().NewID();
				auto map = Manager::Instance().Shard(m_id);
				added = map->Add(m_id, obj);
			} while (!added);
		}

		class Manager final : public Global<Manager>
		{
		public:
			class Map final : protected std::unordered_map<ID, Object_ptr>
			{
			public:
				inline bool Add(IN ID a_id,
								IN Object_ptr& a_obj) asd_noexcept
				{
					auto lock = GetLock(m_lock);
					return emplace(a_id, a_obj).second;
				}

				inline Object_ptr Find(IN ID a_id) asd_noexcept
				{
					auto lock = GetLock(m_lock);
					auto it = find(a_id);
					if (it == end())
						return nullptr;
					return it->second;
				}

				inline void Delete(IN ID a_id) asd_noexcept
				{
					auto lock = GetLock(m_lock);
					auto it = find(a_id);
					if (it == end())
						return;
					Object_ptr obj = std::move(it->second);
					erase(it);
					lock.unlock();
					obj.reset();
				}

				~Map() asd_noexcept
				{
					auto lock = GetLock(m_lock);
					clear();
				}

			private:
				asd::Mutex m_lock;

			};

			Manager() asd_noexcept
			{
				m_shards.resize(2 * Get_HW_Concurrency());
				for (auto& shard : m_shards)
					shard.reset(new Map);
			}

			inline ID NewID()
			{
				ID id;
				do {
					id = ++m_lastID;
				} while (id == Null);
				return id;
			}

			inline Map* Shard(IN ID a_id)
			{
				return m_shards[a_id % m_shards.size()].get();
			}

			std::vector<std::unique_ptr<Map>> m_shards;
			std::atomic<ID> m_lastID;
		};
	};


}
