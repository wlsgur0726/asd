#pragma once
#include "asdbase.h"
#include "lock.h"
#include "util.h"
#include "objpool.h"
#include "container.h"


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
		using Pool = Global<ObjectPoolShardSet< ObjectPool<Object, Mutex> >>;

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
			return Manager::Instance().Find(m_id);
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
			Manager::Instance().Erase(id);
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
				added = Manager::Instance().Insert(m_id, obj);
			} while (!added);
		}

		class Manager final
			: public Global<Manager>
			, public ShardedHashMap<ID, Object>
		{
		public:
			inline ID NewID()
			{
				ID id;
				do {
					id = ++m_lastID;
				} while (id == Null);
				return id;
			}
			std::atomic<ID> m_lastID;
		};
	};


}
