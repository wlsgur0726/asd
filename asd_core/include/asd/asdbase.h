#pragma once
// 이 파일은 모든 asd Header File들이 가장 먼저 Include한다.

#include <cstddef>
#include <cassert>
#include <cstdint>
#include <cstdarg>
#include <exception>
#include <atomic>
#include <memory>
#include <cerrno>
#include <cstdio>
#include <algorithm>
#include <limits>


#define asd_Version 2015010100


// Parameter Type
#ifndef IN
#define IN
#endif

#ifndef OUT
#define OUT
#endif

#ifndef INOUT
#define INOUT
#endif

#ifndef REF
#define REF
#endif

#ifndef MOVE
#define MOVE
#endif



// Platform
#if defined(_WIN32)
#	define asd_Platform_Windows 1
#
#elif defined(__ANDROID__) || defined(ANDROID)
#	define asd_Platform_Android 1
#
#elif defined(__linux__)
#	define asd_Platform_Linux 1
#
#else
#	error This platform is not supported.
#
#endif



// Compiler
#if defined(_MSC_VER)
#	define asd_Compiler_MSVC 1
#
#elif defined(__GNUC__)
#	define asd_Compiler_GCC 1
#
#else
#	error This Compiler is not supported.
#
# endif



// 디버그모드 여부
#if !defined(NDEBUG) && (defined(DEBUG) || defined(_DEBUG))
#	define asd_Debug 1
#
#endif



// 디버그용 정보가 필요한 경우
#ifdef asd_Need_Debug_Info
#
#endif



// _T
#if !defined(_T)
#	if defined(UNICODE)
#		define _T(str) L ## str
#
#	else
#		define _T(str) str
#
#	endif
#
#endif



// asd_noexcept
#if asd_Can_not_use_Exception
#	define asd_noexcept
#
#else
#	define asd_noexcept noexcept
#
#endif



// CompareFunction를 사용해서 비교연산자들을 정의하는 매크로
#define asd_Define_CompareOperator(CompareFunction, Type)					\
	inline bool operator == (IN const Type& a_rval) const asd_noexcept		\
	{																		\
		return CompareFunction(*this, a_rval) == 0;							\
	}																		\
																			\
	inline bool operator != (IN const Type& a_rval) const asd_noexcept		\
	{																		\
		return CompareFunction(*this, a_rval) != 0;							\
	}																		\
																			\
	inline bool operator < (IN const Type& a_rval) const asd_noexcept		\
	{																		\
		return CompareFunction(*this, a_rval) < 0;							\
	}																		\
																			\
	inline bool operator <= (IN const Type& a_rval) const asd_noexcept		\
	{																		\
		return CompareFunction(*this, a_rval) <= 0;							\
	}																		\
																			\
	inline bool operator > (IN const Type& a_rval) const asd_noexcept		\
	{																		\
		return CompareFunction(*this, a_rval) > 0;							\
	}																		\
																			\
	inline bool operator >= (IN const Type& a_rval) const asd_noexcept		\
	{																		\
		return CompareFunction(*this, a_rval) >= 0;							\
	}																		\


#if asd_Platform_Windows
#	define DllExport __declspec(dllexport)
#
#else
#	define DllExport 
#
#endif


namespace asd
{
}
