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


#define asd_Version 2015010100


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



// Exception 사용 가능 여부
#ifdef asd_Can_not_use_Exception
#	define asd_NoThrow
#	define asd_Throws(...)
#
#else
#	define asd_NoThrow throw()
#	define asd_Throws(...) throw(__VA_ARGS__)
#
#endif



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
#		define _T(x) L ## x
#
#	else
#		define _T(x) x
#
#	endif
#
#endif



namespace asd
{
}
