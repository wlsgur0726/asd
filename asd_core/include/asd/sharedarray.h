#pragma once
#include "asd/asdbase.h"
#include <cstring>
#include <vector>
#include <list>

namespace asd
{
	template <typename T>
	class SharedArray : public std::shared_ptr<T>
	{
	public:
		typedef T ElemType;
		typedef std::shared_ptr<ElemType> BaseType;
		typedef SharedArray<ElemType> ThisType;

	private:
		inline static void* GetStartPtr(IN ElemType* a_ptr) noexcept
		{
			uint8_t* p = (uint8_t*)a_ptr;
			return p - sizeof(size_t);
		}

		struct DeleteFunctor
		{
			inline void operator()(IN ElemType* a_ptr) noexcept
			{
				uint8_t* p = (uint8_t*)GetStartPtr(a_ptr);
				delete[] p;
			}
		};


	public:
		inline size_t GetCount() const noexcept
		{
			auto ptr = BaseType::get();
			if (ptr == nullptr)
				return 0;

			size_t* sz = (size_t*)GetStartPtr(ptr);
			assert(*sz > 0);
			return *sz;
		}


		// 버퍼를 a_newCount 개수로 초기화한다.
		inline void Resize(IN size_t a_newCount,
						   IN bool a_preserveOldData = false) noexcept
		{
			if (a_newCount == 0) {
				BaseType::reset();
				return;
			}
			assert(a_newCount > 0);

			size_t* sz;
			const size_t oldCount = GetCount();
			if (oldCount>0 && a_newCount<=GetCount() && BaseType::use_count()==1) {
				// 공유를 하고있지 않는 경우 사이즈값만 수정한다.
				sz = (size_t*)GetStartPtr(BaseType::get());
			}
			else {
				// 공유를 풀어야 하기 때문에 무조건 재할당을 한다.
				uint8_t* p = new uint8_t[sizeof(size_t) + (a_newCount * sizeof(ElemType))];
				ElemType* data = (ElemType*)(p + sizeof(size_t));

				// 기존 데이터 복사
				if (a_preserveOldData) {
					if (oldCount > 0) {
						std::memcpy(data,
									BaseType::get(),
									sizeof(ElemType) * std::min(oldCount, a_newCount));
					}
				}

				// 기존 버퍼를 폐기하고 새로운 버퍼로 교체
				BaseType::reset(data, DeleteFunctor());
				sz = (size_t*)p;
			}

			// 사이즈 재설정
			*sz = a_newCount;
			assert(GetCount() == a_newCount);
		}


		inline ElemType& operator [] (IN size_t a_index) const noexcept
		{
			assert(BaseType::get() != nullptr);
			assert(a_index < GetCount());
			return *(BaseType::get() + a_index);
		}


		template <typename IterableLinearContainer>
		static int Compare(IN const ThisType& a_left,
						   IN const IterableLinearContainer& a_right) noexcept
		{
			auto sz1 = a_left.size();
			auto sz2 = a_right.size();
			if (sz1 < sz2)
				return -1;
			else if (sz1 > sz2)
				return 1;

			size_t i = 0;
			for (auto& re : a_right) {
				const ElemType& le = a_left[i];

				if (le < re)
					return -1;
				else if (le > re)
					return 1;
				++i;
			}
			return 0;
		}

		asd_Define_CompareOperator(Compare, ThisType);
		asd_Define_CompareOperator(Compare, std::vector<ElemType>);
		asd_Define_CompareOperator(Compare, std::list<ElemType>);


		// std style
		typedef ElemType value_type;
		typedef size_t size_type;
		typedef ElemType* iterator;

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

		inline iterator begin()
		{
			return (iterator)data();
		}

		inline const iterator begin() const
		{
			return (const iterator)data();
		}

		inline iterator end()
		{
			return BaseType::get() + size();
		}

		inline const iterator end() const
		{
			return BaseType::get() + size();
		}
	};
}
