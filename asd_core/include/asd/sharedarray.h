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
		static const ArrayType* NullPtr()
		{
			static const ArrayType s_null;
			return &s_null;
		}


		inline ArrayType* GetArrayPtr(bool a_preservation = true)
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


		inline const ArrayType* GetArrayPtr() const
		{
			ArrayType* ptr = BaseType::get();
			if (ptr == nullptr)
				return NullPtr();
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


		inline size_type size() const
		{
			return GetArrayPtr()->size();
		}

		inline bool empty() const
		{
			return GetArrayPtr()->empty();
		}

		inline const value_type* data() const
		{
			return GetArrayPtr()->data();
		}

		inline value_type* data()
		{
			return (value_type*)GetArrayPtr()->data();
		}

		inline reference at(size_type a_index)
		{
			return GetArrayPtr()->at(a_index);
		}

		inline const_reference at(size_type a_index) const
		{
			return GetArrayPtr()->at(a_index);
		}

		inline void resize(size_type a_count)
		{
			return GetArrayPtr()->resize(a_count);
		}

		inline void resize(size_type a_count,
						   value_type a_fill)
		{
			return GetArrayPtr()->resize(a_count, a_fill);
		}

		inline iterator begin()
		{
			return GetArrayPtr()->begin();
		}

		inline const_iterator begin() const
		{
			return GetArrayPtr()->begin();
		}

		inline iterator end()
		{
			return GetArrayPtr()->end();
		}

		inline const_iterator end() const
		{
			return GetArrayPtr()->end();
		}

		inline reverse_iterator rbegin()
		{
			return GetArrayPtr()->rbegin();
		}

		inline const_reverse_iterator rbegin() const
		{
			return GetArrayPtr()->rbegin();
		}

		inline reverse_iterator rend()
		{
			return GetArrayPtr()->rend();
		}

		inline const_reverse_iterator rend() const
		{
			return GetArrayPtr()->rend();
		}

		inline reference operator[](size_type a_index)
		{
			return GetArrayPtr()->at(a_index);
		}

		inline const_reference operator[](size_type a_index) const
		{
			return GetArrayPtr()->at(a_index);
		}

	};



	template <typename T>
	class SharedVector : public SharedArray<std::vector<T>>
	{
	};
}
