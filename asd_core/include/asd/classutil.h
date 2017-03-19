#pragma once
#include "asdbase.h"
#include "exception.h"
#include <memory>
#include <atomic>
#include <cassert>
#include <mutex>


namespace asd
{
	template <typename T1, typename T2>
	struct IsEqualType
	{
		static constexpr bool Value
			=  std::is_same<T1, T2>::value
			|| (sizeof(T1) == sizeof(T2)
				&& std::numeric_limits<T1>::min() == std::numeric_limits<T2>::min()
				&& std::numeric_limits<T1>::max() == std::numeric_limits<T2>::max());
	};



	template <typename T>
	class Global
	{
	public:
		static T& GlobalInstance()
		{
			static std::unique_ptr<T> s_globalObject = nullptr;
			static std::once_flag s_init;
			std::call_once(s_init, []()
			{
				s_globalObject.reset(new T);
			});
			assert(s_globalObject != nullptr);
			return *s_globalObject;
		}

		inline static T& Instance()
		{
			return GlobalInstance();
		}
	};



	template <typename T>
	class ThreadLocal
	{
	public:
		static T& ThreadLocalInstance()
		{
			thread_local std::unique_ptr<T> t_threadLocalObject = nullptr;
			if (t_threadLocalObject == nullptr)
				t_threadLocalObject.reset(new T);
			return *t_threadLocalObject;
		}

		inline static T& Instance()
		{
			return ThreadLocalInstance();
		}
	};



	template <typename T>
	class HasMagicCode
	{
	public:
		virtual ~HasMagicCode() asd_noexcept
		{
			m_magicCode = nullptr;
		}

		static const char* GetMagicCode() asd_noexcept
		{
			static const char* s_magicCode = typeid(T).name();
			return s_magicCode;
		}

		inline bool IsValidMagicCode() const asd_noexcept
		{
			return m_magicCode == GetMagicCode();
		}

	protected:
		const char* m_magicCode = GetMagicCode();
	};



	template <typename T>
	inline T& Default()
	{
		static T s_default;
		return s_default;
	}



	template <typename T, typename... Args>
	inline void Reset(REF T& a_target,
					  IN const Args&... a_constructorArgs)
	{
		a_target.~T();
		new(&a_target) T(a_constructorArgs...);
	}



	template<typename T>
	struct DeleteFunction_ptr
	{
		typedef void(*Function)(IN T*);
		Function m_deleteFunction;

		inline DeleteFunction_ptr(IN Function a_deleteFunction = nullptr) asd_noexcept
			: m_deleteFunction(a_deleteFunction)
		{
		}

		inline void operator()(IN T* a_ptr) const asd_noexcept
		{
			if (m_deleteFunction != nullptr)
				m_deleteFunction(a_ptr);
			else
				delete a_ptr;
		}

		inline operator Function() const asd_noexcept
		{
			return m_deleteFunction;
		}
	};

	template<typename T>
	struct UniquePtr
		: public std::unique_ptr<T, DeleteFunction_ptr<T>>
	{
		typedef std::unique_ptr<T, DeleteFunction_ptr<T>>	Base;
		typedef typename Base::deleter_type::Function		DeleteFunction;

		inline UniquePtr(REF T* a_ptr = nullptr,
						 IN DeleteFunction a_deleter = nullptr) asd_noexcept
			: Base(a_ptr, typename Base::deleter_type(a_deleter))
		{
		}
	};
}
