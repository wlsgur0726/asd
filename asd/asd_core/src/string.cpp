#include "stdafx.h"
#include "asd/string.h"
#include "asd/tempbuffer.h"


#if defined(asd_Platform_Windows)
#	include <Windows.h>
#
#	define asd_vsprintf_a	::vsprintf_s
#	define asd_vsprintf_w	::vswprintf_s
#
#	define asd_vfprintf_a	::vfprintf_s
#	define asd_vfprintf_w	::vfwprintf_s
#
#	define asd_vprintf_a	::vprintf_s
#	define asd_vprintf_w	::vwprintf_s
#
#else
#	include <cwchar>
#
#	define asd_vsprintf_a	::vsnprintf
#	define asd_vsprintf_w	::vswprintf
#
#	define asd_vfprintf_a	::vfprintf
#	define asd_vfprintf_w	::vfwprintf
#
#	define asd_vprintf_a	::vprintf
#	define asd_vprintf_w	::vwprintf
#
#endif


namespace asd
{
	const wchar_t NullChar = '\0';


	inline bool SamePtr(IN const void* a_p1,
						IN const void* a_p2)
	{
		return a_p1 == a_p2;
	}



	int vsprintf(OUT char* a_targetbuf,
				 IN int a_bufsize,
				 IN const char* a_format,
				 IN va_list& a_args) asd_NoThrow
	{
		return asd_vsprintf_a(a_targetbuf,
							  a_bufsize,
							  a_format,
							  a_args);
	}


	int vsprintf(OUT wchar_t* a_targetbuf,
				 IN int a_bufsize,
				 IN const wchar_t* a_format,
				 IN va_list& a_args) asd_NoThrow
	{
		return asd_vsprintf_w(a_targetbuf,
							  a_bufsize,
							  a_format,
							  a_args);
	}



	int vfprintf(IN FILE* a_fp,
				 IN const char* a_format,
				 IN va_list& a_args) asd_NoThrow
	{
		return asd_vfprintf_a(a_fp, a_format, a_args);
	}

	int vfprintf(IN FILE* a_fp,
				 IN const wchar_t* a_format,
				 IN va_list& a_args) asd_NoThrow
	{
		return asd_vfprintf_w(a_fp, a_format, a_args);
	}



	int vprintf(IN const char* a_format,
				IN va_list& a_args) asd_NoThrow
	{
		return asd_vprintf_a(a_format, a_args);
	}


	int vprintf(IN const wchar_t* a_format,
				IN va_list& a_args) asd_NoThrow
	{
		return asd_vprintf_w(a_format, a_args);
	}



	int sprintf(OUT char* a_targetbuf,
				IN int a_bufsize,
				IN const char* a_format,
				IN ...) asd_NoThrow
	{
		va_list args;
		va_start(args, a_format);
		int r = asd::vsprintf(a_targetbuf,
							  a_bufsize,
							  a_format,
							  args);
		va_end(args);
		return r;
	}


	int sprintf(OUT wchar_t* a_targetbuf,
				IN int a_bufsize,
				IN const wchar_t* a_format,
				IN ...) asd_NoThrow
	{
		va_list args;
		va_start(args, a_format);
		int r = asd::vsprintf(a_targetbuf,
							  a_bufsize,
							  a_format,
							  args);
		va_end(args);
		return r;
	}



	int printf(IN const char* a_format,
			   IN ...) asd_NoThrow
	{
		va_list args;
		va_start(args, a_format);
		int r = asd::vprintf(a_format, args);
		va_end(args);
		return r;
	}


	int printf(IN const wchar_t* a_format,
			   IN ...) asd_NoThrow
	{
		va_list args;
		va_start(args, a_format);
		int r = asd::vprintf(a_format, args);
		va_end(args);
		return r;
	}



	int fputs(IN const char* a_str,
			  IN FILE* a_fp) asd_NoThrow
	{
		return ::fputs(a_str, a_fp);
	}


	int fputs(IN const wchar_t* a_str,
			  IN FILE* a_fp) asd_NoThrow
	{
		return ::fputws(a_str, a_fp);
	}



	int puts(IN const char* a_str) asd_NoThrow
	{
		return ::puts(a_str);
	}


	int puts(IN const wchar_t* a_str) asd_NoThrow
	{
		return ::fputws(a_str, stdout);
	}



#if !defined(asd_Platform_Windows)
	template<typename CharType>
	inline int vsctprintf(IN const CharType* a_format,
						  IN va_list a_args) asd_NoThrow
	{
		const int LimitBytes = 1024 * 1024 * 8;
		int size = 1024 * 4;
		do {
			TempBuffer<char> tempbuf(size);
			va_list args;
			va_copy(args, a_args);
			int r = asd::vsprintf((CharType*)((char*)tempbuf),
								  (int)size,
								  a_format,
								  args);
			if (r >= 0)
				return r;
			size *= 2;
		} while (size <= LimitBytes);

		// fail
		return -1;
	}
#endif



	int vscprintf(IN const char* a_format,
				  IN va_list& a_args) asd_NoThrow
	{
#if defined(asd_Platform_Windows)
		return ::_vscprintf(a_format, a_args);
#else
		return vsctprintf<char>(a_format, a_args);
#endif
	}


	int vscprintf(IN const wchar_t* a_format,
				  IN va_list& a_args) asd_NoThrow
	{
#if defined(asd_Platform_Windows)
		return ::_vscwprintf(a_format, a_args);
#else
		return vsctprintf<wchar_t>(a_format, a_args);
#endif
	}



	size_t strlen(IN const char* a_str) asd_NoThrow
	{
		return ::strlen(a_str);
	}


	size_t strlen(IN const wchar_t* a_str) asd_NoThrow
	{
		return ::wcslen(a_str);
	}


	size_t strlen(IN const void* a_str,
				  IN int a_sizeOfChar) asd_NoThrow
	{
		assert(a_sizeOfChar==1 || a_sizeOfChar==2 || a_sizeOfChar==4);
		switch (a_sizeOfChar) {
			case 1:
				return ::strlen((const char*)a_str);
			case 2:
				return asd::strlen<2>(a_str);
			case 4:
				return asd::strlen<4>(a_str);
		}
		return -1;
	}



	int strcmp(IN const char* a_str1, 
			   IN const char* a_str2,
			   IN bool a_ignoreCase /*= false*/) asd_NoThrow
	{
		if (SamePtr(a_str1, a_str2))
			return 0;

		if (a_str1 == nullptr)
			a_str1 = (const char*)&NullChar;

		if (a_str2 == nullptr)
			a_str2 = (const char*)&NullChar;

		if (a_ignoreCase == false)
			return ::strcmp(a_str1, a_str2);

#if defined(asd_Platform_Windows)
		return _stricmp(a_str1, a_str2);
#else
		return strcasecmp(a_str1, a_str2);
#endif
	}


	int strcmp(IN const wchar_t* a_str1, 
			   IN const wchar_t* a_str2,
			   IN bool a_ignoreCase /*= false*/) asd_NoThrow
	{
		if (SamePtr(a_str1, a_str2))
			return 0;

		if (a_str1 == nullptr)
			a_str1 = &NullChar;

		if (a_str2 == nullptr)
			a_str2 = &NullChar;

		if (a_ignoreCase == false)
			return ::wcscmp(a_str1, a_str2);

#if defined(asd_Platform_Windows)
		return _wcsicmp(a_str1, a_str2);
#else
		return wcscasecmp(a_str1, a_str2);
#endif
	}



	template<typename DstType, typename SrcType, bool AsciiOnly=false>
	inline void strcpy_internal(OUT DstType* a_dst,
								IN const SrcType* a_src) asd_NoThrow
	{
		// 잘못된 메모리를 접근하는 경우는 없다. (caller를 믿는다)
		for (; *a_src!='\0'; ++a_src,++a_dst) {
			if (AsciiOnly) {
				bool isAsciiChar = (0x7F - *a_src) >= 0;
				assert(isAsciiChar);
			}
			*a_dst = (DstType)*a_src;
		}

		assert(*a_src == '\0');
		*a_dst = (DstType)*a_src;
	}


	char* strcpy(OUT char* a_dst,
				 IN const char* a_src) asd_NoThrow
	{
		if (SamePtr(a_dst, a_src))
			return a_dst;

		if (a_src == nullptr)
			a_src = (const char*)&NullChar;

#if defined(asd_Platform_Windows)
		strcpy_internal(a_dst, a_src);
		return a_dst;
#else
		return ::strcpy(a_dst, a_src);
#endif
	}


	wchar_t* strcpy(OUT wchar_t* a_dst,
					IN const wchar_t* a_src) asd_NoThrow
	{
		if (SamePtr(a_dst, a_src))
			return a_dst;

		if (a_src == nullptr)
			a_src = &NullChar;

#if defined(asd_Platform_Windows)
		strcpy_internal(a_dst, a_src);
		return a_dst;
#else
		return ::wcscpy(a_dst, a_src);
#endif
	}


	// Ascii문자열만 사용 할 것.
	char* strcpy(OUT char* a_dst,
				 IN const wchar_t* a_src) asd_NoThrow
	{
		if (SamePtr(a_dst, a_src))
			return a_dst;

		if (a_src == nullptr)
			a_src = &NullChar;

		strcpy_internal<char, wchar_t, true>(a_dst, a_src);
		return a_dst;
	}


	// Ascii문자열만 사용 할 것.
	wchar_t* strcpy(OUT wchar_t* a_dst,
					IN const char* a_src) asd_NoThrow
	{
		if (SamePtr(a_dst, a_src))
			return a_dst;

		if (a_src == nullptr)
			a_src = (const char*)&NullChar;

		strcpy_internal<wchar_t, char, true>(a_dst, a_src);
		return a_dst;
	}

}
