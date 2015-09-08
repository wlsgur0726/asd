#pragma once
#include "asd/asdbase.h"
#include <cstring>

namespace asd
{
	template <typename T>
	class SharedArray : public std::shared_ptr<T>
	{
	public:
		typedef T ElemType;
		typedef std::shared_ptr<ElemType> BaseType;


	private:
		inline static void* GetStartPtr(IN ElemType* a_ptr) asd_NoThrow
		{
			uint8_t* p = (uint8_t*)a_ptr;
			return p - sizeof(size_t);
		}

		struct DeleteFunctor
		{
			inline void operator()(IN ElemType* a_ptr) asd_NoThrow
			{
				uint8_t* p = (uint8_t*)GetStartPtr(a_ptr);
				delete[] p;
			}
		};


	public:
		inline size_t GetCount() const asd_NoThrow
		{
			auto ptr = BaseType::get();
			if (ptr == nullptr)
				return 0;

			size_t* sz = (size_t*)GetStartPtr(ptr);
			assert(*sz > 0);
			return *sz;
		}


		// 버퍼를 a_newCount 개수로 초기화한다.
		// 값이 수정되면서 공유를 풀어야 하기 때문에 무조건 재할당을 한다.
		inline void Resize(IN size_t a_newCount,
						   IN bool a_preserveOldData = false) asd_NoThrow
		{
			if (a_newCount == 0) {
				BaseType::reset();
				return;
			}
			assert(a_newCount > 0);

			// 새로운 버퍼 할당
			uint8_t* p = new uint8_t[sizeof(size_t) + (a_newCount * sizeof(ElemType))];
			ElemType* data = (ElemType*)(p + sizeof(size_t));

			// 기존 데이터 복사
			if (a_preserveOldData) {
				size_t oldCount = GetCount();
				if (oldCount > 0) {
					std::memcpy(data,
								BaseType::get(),
								sizeof(ElemType) * std::min(oldCount, a_newCount));
				}
			}

			// 기존 버퍼를 폐기하고 새로운 버퍼로 교체
			BaseType::reset(data, DeleteFunctor());
			size_t* sz = (size_t*)p;
			*sz = a_newCount;
			assert(GetCount() == a_newCount);
		}


		inline ElemType& operator [] (IN size_t a_index) const asd_NoThrow
		{
			assert(BaseType::get() != nullptr);
			assert(a_index < GetCount());
			return *(BaseType::get() + a_index);
		}



		// std style
		typedef ElemType value_type;
		typedef size_t size_type;

		inline const value_type* data() const
		{
			return BaseType::get();
		}

		inline value_type* data()
		{
			return BaseType::get();
		}

		inline size_type size() const
		{
			return GetCount();
		}

		inline void resize(IN size_type a_count)
		{
			Resize(a_count, true);
		}

		inline void resize(IN size_type a_count,
						   IN value_type a_fill)
		{
			const auto OldLen = size();
			resize(a_count);
			if (a_count > OldLen) {
				const auto NewLen = size();
				value_type* p = data();
				for (auto i=OldLen; i<NewLen; ++i)
					p[i] = a_fill;
			}
		}
	};
}
