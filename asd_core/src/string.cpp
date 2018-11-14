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
	const char32_t NullChar = '\0';


	inline bool SamePtr(const void* a_p1,
						const void* a_p2)
	{
		return a_p1 == a_p2;
	}



	int vsprintf(char* a_targetbuf /*Out*/,
				 int a_bufsize,
				 const char* a_format,
				 va_list& a_args)
	{
		return asd_vsprintf_a(a_targetbuf,
							  a_bufsize,
							  a_format,
							  a_args);
	}


	int vsprintf(wchar_t* a_targetbuf /*Out*/,
				 int a_bufsize,
				 const wchar_t* a_format,
				 va_list& a_args)
	{
		return asd_vsprintf_w(a_targetbuf,
							  a_bufsize,
							  a_format,
							  a_args);
	}



	int vfprintf(FILE* a_fp,
				 const char* a_format,
				 va_list& a_args)
	{
		return asd_vfprintf_a(a_fp, a_format, a_args);
	}

	int vfprintf(FILE* a_fp,
				 const wchar_t* a_format,
				 va_list& a_args)
	{
		return asd_vfprintf_w(a_fp, a_format, a_args);
	}



	int vprintf(const char* a_format,
				va_list& a_args)
	{
		return asd_vprintf_a(a_format, a_args);
	}


	int vprintf(const wchar_t* a_format,
				va_list& a_args)
	{
		return asd_vprintf_w(a_format, a_args);
	}



	int sprintf(char* a_targetbuf /*Out*/,
				int a_bufsize,
				const char* a_format,
				...)
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


	int sprintf(wchar_t* a_targetbuf /*Out*/,
				int a_bufsize,
				const wchar_t* a_format,
				...)
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



	int printf(const char* a_format,
			   ...)
	{
		va_list args;
		va_start(args, a_format);
		int r = asd::vprintf(a_format, args);
		va_end(args);
		return r;
	}


	int printf(const wchar_t* a_format,
			   ...)
	{
		va_list args;
		va_start(args, a_format);
		int r = asd::vprintf(a_format, args);
		va_end(args);
		return r;
	}



	int fputs(const char* a_str,
			  FILE* a_fp)
	{
		return ::fputs(a_str, a_fp);
	}


	int fputs(const wchar_t* a_str,
			  FILE* a_fp)
	{
		return ::fputws(a_str, a_fp);
	}



	int puts(const char* a_str)
	{
		return ::puts(a_str);
	}


	int puts(const wchar_t* a_str)
	{
		return ::fputws(a_str, stdout);
	}



#if !defined(asd_Platform_Windows)
#define asd_vsctprintf_Common_Logic			\
	va_list args;							\
	va_copy(args, a_args);					\
	int r = asd::vsprintf((CharType*)buf,	\
						  (int)size,		\
						  a_format,			\
						  args);			\
	if (r >= 0)								\
		return r;							\
	size *= 2;								\

	template<typename CharType>
	inline int vsctprintf(const CharType* a_format,
						  va_list a_args)
	{
		const int LimitBytes = 1024 * 1024 * 16;
		const int StartBytes = 1024 * 512;
		int size = StartBytes;

#if defined(asd_Compiler_GCC)
		do {
			uint8_t buf[size];
			asd_vsctprintf_Common_Logic;
		} while (size <= LimitBytes);

#else 
		uint8_t* buf;
		uint8_t stackBuf[StartBytes];
		std::unique_ptr<uint8_t> heapBuf;
		buf = stackBuf;
		do {
			asd_vsctprintf_Common_Logic;
			buf = new uint8_t[size];
			heapBuf.reset(buf, 
						  std::default_delete<uint8_t[]>());
		} while (size <= LimitBytes);

#endif // defined(asd_Compiler_GCC)

		// fail
		return -1;
	}

#endif // !defined(asd_Platform_Windows)



	int vscprintf(const char* a_format,
				  va_list& a_args)
	{
#if defined(asd_Platform_Windows)
		return ::_vscprintf(a_format, a_args);
#else
		return vsctprintf<char>(a_format, a_args);
#endif
	}


	int vscprintf(const wchar_t* a_format,
				  va_list& a_args)
	{
#if defined(asd_Platform_Windows)
		return ::_vscwprintf(a_format, a_args);
#else
		return vsctprintf<wchar_t>(a_format, a_args);
#endif
	}



	int scprintf(const char* a_format,
				 ...)
	{
		va_list args;
		va_start(args, a_format);
		int r = asd::vscprintf(a_format, args);
		va_end(args);
		return r;
	}


	int scprintf(const wchar_t* a_format,
				 ...)
	{
		va_list args;
		va_start(args, a_format);
		int r = asd::vscprintf(a_format, args);
		va_end(args);
		return r;
	}



	size_t strlen(const char* a_str)
	{
		if (a_str == nullptr)
			return 0;
		return ::strlen(a_str);
	}


	size_t strlen(const wchar_t* a_str)
	{
		if (a_str == nullptr)
			return 0;
		return ::wcslen(a_str);
	}


	size_t strlen(const char16_t* a_str)
	{
		return asd::strlen<sizeof(char16_t)>(a_str);
	}


	size_t strlen(const char32_t* a_str)
	{
		return asd::strlen<sizeof(char32_t)>(a_str);
	}


	size_t strlen(const void* a_str,
				  int a_sizeOfChar)
	{
		asd_DAssert(a_sizeOfChar==1 || a_sizeOfChar==2 || a_sizeOfChar==4);
		switch (a_sizeOfChar) {
			case 1:
				return asd::strlen((const char*)a_str);
			case 2:
				return asd::strlen<2>(a_str);
			case 4:
				return asd::strlen<4>(a_str);
		}
		return 0;
	}



	template<typename ReturnType, typename ProxyType, bool AsciiOnly=false>
	inline void strcpy_internal(ReturnType* a_dst /*Out*/,
								const ProxyType* a_src,
								size_t a_dstBufCount)
	{
		if (SamePtr(a_dst, a_src))
			return;

		if (a_src == nullptr)
			a_src = (const ProxyType*)&NullChar;

		size_t offset = 0;
		for (; *a_src!='\0' && offset<a_dstBufCount; ++a_src, ++a_dst) {
			if (AsciiOnly) {
				bool isAsciiChar = (0x7F - *a_src) >= 0;
				asd_DAssert(isAsciiChar);
			}
			*a_dst = (ReturnType)*a_src;
		}

		if (offset >= a_dstBufCount) {
			if (offset > 0)
				a_dst[offset-1] = '\0';
			return;
		}

		asd_DAssert(*a_src == '\0');
		*a_dst = (ReturnType)*a_src;
	}


	char* strcpy(char* a_dst /*Out*/,
				 const char* a_src,
				 size_t a_dstBufCount /*= std::numeric_limits<size_t>::max()*/)
	{
		strcpy_internal(a_dst, a_src, a_dstBufCount);
		return a_dst;
	}


	wchar_t* strcpy(wchar_t* a_dst /*Out*/,
					const wchar_t* a_src,
					size_t a_dstBufCount /*= std::numeric_limits<size_t>::max()*/)
	{
		strcpy_internal(a_dst, a_src, a_dstBufCount);
		return a_dst;
	}


	char16_t* strcpy(char16_t* a_dst /*Out*/,
					 const char16_t* a_src,
					 size_t a_dstBufCount /*= std::numeric_limits<size_t>::max()*/)
	{
		strcpy_internal(a_dst, a_src, a_dstBufCount);
		return a_dst;
	}


	char32_t* strcpy(char32_t* a_dst /*Out*/,
					 const char32_t* a_src,
					 size_t a_dstBufCount /*= std::numeric_limits<size_t>::max()*/)
	{
		strcpy_internal(a_dst, a_src, a_dstBufCount);
		return a_dst;
	}


	// Ascii문자열만 사용 할 것.
	char* strcpy(char* a_dst /*Out*/,
				 const wchar_t* a_src,
				 size_t a_dstBufCount /*= std::numeric_limits<size_t>::max()*/)
	{
		strcpy_internal<char, wchar_t, true>(a_dst, a_src, a_dstBufCount);
		return a_dst;
	}


	// Ascii문자열만 사용 할 것.
	wchar_t* strcpy(wchar_t* a_dst /*Out*/,
					const char* a_src,
					size_t a_dstBufCount /*= std::numeric_limits<size_t>::max()*/)
	{
		strcpy_internal<wchar_t, char, true>(a_dst, a_src, a_dstBufCount);
		return a_dst;
	}

}
