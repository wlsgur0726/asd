﻿#pragma once
#include "asdbase.h"
#include "exception.h"

namespace asd
{
	// std::get_temporary_buffer와 std::return_temporary_buffer를
	// 좀 더 편리하게 사용하고자 래핑한 클래스.
	template<typename T>
	struct TempBuffer final
	{
	public:
		typedef T Object;
		typedef TempBuffer<T> ThisType;


		TempBuffer(IN size_t a_count)
		{
			assert(a_count > 0);
			auto buf = std::get_temporary_buffer<Object>((ptrdiff_t)a_count);
			m_arr = buf.first;
			m_count = buf.second;
		}


		TempBuffer(MOVE ThisType&& a_rval)
		{
			*this = std::move(a_rval);
		}


		inline TempBuffer& operator = (MOVE ThisType&& a_rval)
		{
			this->~TempBuffer();
			m_arr = a_rval.m_arr;
			m_count = a_rval.m_count;
			a_rval.m_arr = nullptr;
			return *this;
		}


		~TempBuffer()
		{
			if (m_arr != nullptr) {
				std::return_temporary_buffer<Object>(m_arr);
			}
			else {
				// Move된 경우
			}
		}


		inline operator void* () const
		{
			assert(m_arr != nullptr);
			return m_arr;
		}


		inline operator Object* () const
		{
			assert(m_arr != nullptr);
			return m_arr;
		}


		inline size_t GetCount() const
		{
			assert(m_count > 0);
			return m_count;
		}


		inline Object& At(IN size_t a_index) const 
		{
			if (a_index < 0 || a_index >= GetCount()) {
				asd_RaiseException("invalid argument (a_index : {})", a_index);
			}
			return m_arr[a_index];
		}


		inline Object& operator [] (IN size_t a_index) const
		{
			return At(a_index);
		}


		template<typename... ARGS>
		inline void Constructor(IN size_t a_index,
								IN ARGS&&... a_constructorArgs)
		{
			Object& obj = At(a_index);
			new(&obj) Object(a_constructorArgs...);
		}


		inline void Destrucotr(IN size_t a_index)
		{
			Object& obj = At(a_index);
			obj.~Objcet();
		}


	private:
		TempBuffer(const ThisType&) = delete;
		TempBuffer& operator = (const ThisType&) = delete;

		Object* m_arr = nullptr;
		size_t m_count = 0;
	};
}
