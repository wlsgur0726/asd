#pragma once
#include "asdbase.h"
#include <cstring>
#include <vector>

namespace asd
{
	template <typename ARRAY>
	class SharedArray : public std::shared_ptr<ARRAY>
	{
	public:
		typedef ARRAY							ArrayType;
		typedef typename ArrayType::value_type	ElemType;
		typedef std::shared_ptr<ArrayType>		BaseType;
		typedef SharedArray<ArrayType>			ThisType;


	protected:
		inline ArrayType* GetArrayPtr(IN bool a_preservation = true) asd_noexcept
		{
			ArrayType* orgptr = BaseType::get();
			if (orgptr == nullptr) {
				BaseType::reset(new ArrayType);
				return BaseType::get();
			}

			if (BaseType::use_count() == 1)
				return orgptr;

			ArrayType* newptr;
			if (a_preservation)
				newptr = new ArrayType(orgptr->begin(), orgptr->end());
			else
				newptr = new ArrayType;
			BaseType::reset(newptr);
			return newptr;
		}


		inline const ArrayType* GetArrayPtr() const asd_noexcept
		{
			ArrayType* ptr = BaseType::get();
			if (ptr == nullptr)
			{
				static const ArrayType g_null;
				return &g_null;
			}
			return ptr;
		}


	public:
		using BaseType::BaseType;
		using BaseType::operator=;

		// std style
#define asd_SharedArray_Define_StdStyleType(Base, TypeName) typedef typename Base::TypeName TypeName
		asd_SharedArray_Define_StdStyleType(ArrayType, value_type);
		asd_SharedArray_Define_StdStyleType(ArrayType, size_type);
		asd_SharedArray_Define_StdStyleType(ArrayType, reference);
		asd_SharedArray_Define_StdStyleType(ArrayType, const_reference);
		asd_SharedArray_Define_StdStyleType(ArrayType, iterator);
		asd_SharedArray_Define_StdStyleType(ArrayType, const_iterator);
		asd_SharedArray_Define_StdStyleType(ArrayType, reverse_iterator);
		asd_SharedArray_Define_StdStyleType(ArrayType, const_reverse_iterator);


		inline size_type size() const asd_noexcept
		{
			return GetArrayPtr()->size();
		}

		inline const value_type* data() const asd_noexcept
		{
			return GetArrayPtr()->data();
		}

		inline value_type* data() asd_noexcept
		{
			return (value_type*)GetArrayPtr()->data();
		}

		inline reference at(IN size_type a_index) asd_noexcept
		{
			return GetArrayPtr()->at(a_index);
		}

		inline const_reference at(IN size_type a_index) const asd_noexcept
		{
			return GetArrayPtr()->at(a_index);
		}

		inline void resize(IN size_type a_count) asd_noexcept
		{
			return GetArrayPtr()->resize(a_count);
		}

		inline void resize(IN size_type a_count,
						   IN value_type a_fill) asd_noexcept
		{
			return GetArrayPtr()->resize(a_count, a_fill);
		}

		inline iterator begin() asd_noexcept
		{
			return GetArrayPtr()->begin();
		}

		inline const_iterator begin() const asd_noexcept
		{
			return GetArrayPtr()->begin();
		}

		inline iterator end() asd_noexcept
		{
			return GetArrayPtr()->end();
		}

		inline const_iterator end() const asd_noexcept
		{
			return GetArrayPtr()->end();
		}

		inline reverse_iterator rbegin() asd_noexcept
		{
			return GetArrayPtr()->rbegin();
		}

		inline const_reverse_iterator rbegin() const asd_noexcept
		{
			return GetArrayPtr()->rbegin();
		}

		inline reverse_iterator rend() asd_noexcept
		{
			return GetArrayPtr()->rend();
		}

		inline const_reverse_iterator rend() const asd_noexcept
		{
			return GetArrayPtr()->rend();
		}

		inline reference operator[](IN size_type a_index) asd_noexcept
		{
			return GetArrayPtr()->at(a_index);
		}

		inline const_reference operator[](IN size_type a_index) const asd_noexcept
		{
			return GetArrayPtr()->at(a_index);
		}

	};



	template <typename T>
	class SharedVector : public SharedArray<std::vector<T>>
	{
	};
}
