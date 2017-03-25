#pragma once
#include "asdbase.h"
#include "lock.h"
#include "util.h"
#include "objpool.h"
#include "container.h"
#include <typeinfo>


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


	private:
		struct ObjHeader : public HasMagicCode<HandleType>
		{
			ID ID;
		};

		using PoolType = ObjectPoolShardSet<
			ObjectPool<Object, Mutex, false, sizeof(ObjHeader)>
		>;
		using Pool = Global<PoolType>;


	public:
		static const ID Null = 0;

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
			m_id = a_mv.m_id;
			a_mv.m_id = Null;
			return *this;
		}

		inline static HandleType GetHandle(IN Object* a_obj) asd_noexcept
		{
			return HandleType(GetID(a_obj));
		}

		inline static ID GetID(IN Object* a_obj) asd_noexcept
		{
			if (a_obj == nullptr)
				return Null;

			ObjHeader* header = PoolType::template GetHeader<ObjHeader>(a_obj);
			if (header->IsValidMagicCode() == false)
				return Null;

			return header->ID;
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

		template <typename... ARGS>
		Object_ptr Alloc(ARGS&&... a_constructorArgs)
		{
			Object_ptr obj(Pool::Instance().Alloc(std::forward<ARGS>(a_constructorArgs)...),
						   [](IN Object* a_ptr) { Pool::Instance().Free(a_ptr); });

			bool added;
			do {
				m_id = Manager::Instance().NewID();
				added = Manager::Instance().Insert(m_id, obj);
			} while (!added);

			ObjHeader* header = PoolType::template GetHeader<ObjHeader>(obj.get());
			new(header) ObjHeader(); // init MagicCode
			header->ID = m_id;
			return obj;
		}

		Object_ptr Free() asd_noexcept
		{
			if (m_id == Null)
				return nullptr;
			ID id = m_id;
			m_id = Null;
			return Manager::Instance().Erase(id);
		}

		static void AllClear()
		{
			Manager::Instance().clear();
		}

		inline static int Compare(IN ID a_left,
								  IN ID a_right) asd_noexcept
		{
			if (a_left < a_right)
				return -1;
			else if (a_left > a_right)
				return 1;
			return 0;
		}

		asd_Define_CompareOperator(Compare, ID);


	private:
		ID m_id;

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


namespace std
{
	template <typename OBJECT_TYPE, typename ID_TYPE>
	struct hash< asd::Handle<OBJECT_TYPE, ID_TYPE> >
	{
		inline size_t operator()(IN const asd::Handle<OBJECT_TYPE, ID_TYPE>& a_handle) const asd_noexcept
		{
			return std::hash<ID_TYPE>()(a_handle.GetID());
		}
	};
}
