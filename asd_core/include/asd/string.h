#pragma once
#include "asdbase.h"
#include "caster.h"
#include "../../built-in/cppformat/fmt/ostream.h"
#include "../../built-in/cppformat/fmt/format.h"
#include <string>
#include <iostream>

namespace asd
{
	int vsprintf(OUT char* a_targetbuf,
				 IN int a_bufsize,
				 IN const char* a_format,
				 IN va_list& a_args) asd_noexcept;

	int vsprintf(OUT wchar_t* a_targetbuf,
				 IN int a_bufsize,
				 IN const wchar_t* a_format,
				 IN va_list& a_args) asd_noexcept;


	int vfprintf(IN FILE* a_fp,
				 IN const char* a_format,
				 IN va_list& a_args) asd_noexcept;

	int vfprintf(IN FILE* a_fp,
				 IN const wchar_t* a_format,
				 IN va_list& a_args) asd_noexcept;


	int vprintf(IN const char* a_format,
				IN va_list& a_args) asd_noexcept;

	int vprintf(IN const wchar_t* a_format,
				IN va_list& a_args) asd_noexcept;


	int sprintf(OUT char* a_targetbuf,
				IN int a_bufsize,
				IN const char* a_format,
				IN ...) asd_noexcept;

	int sprintf(OUT wchar_t* a_targetbuf,
				IN int a_bufsize,
				IN const wchar_t* a_format,
				IN ...) asd_noexcept;


	int printf(IN const char* a_format,
			   IN ...) asd_noexcept;

	int printf(IN const wchar_t* a_format,
			   IN ...) asd_noexcept;


	int vscprintf(IN const char* a_format,
				  IN va_list& a_args) asd_noexcept;

	int vscprintf(IN const wchar_t* a_format,
				  IN va_list& a_args) asd_noexcept;


	int scprintf(IN const char* a_format,
				 IN ...) asd_noexcept;

	int scprintf(IN const wchar_t* a_format,
				 IN ...) asd_noexcept;


	int fputs(IN const char* a_str,
			  IN FILE* a_fp) asd_noexcept;

	int fputs(IN const wchar_t* a_str,
			  IN FILE* a_fp) asd_noexcept;

	
	int puts(IN const char* a_str) asd_noexcept;

	int puts(IN const wchar_t* a_str) asd_noexcept;


	int strcmp(IN const char* a_str1, 
			   IN const char* a_str2,
			   IN bool a_caseSensitive = true) asd_noexcept;

	int strcmp(IN const wchar_t* a_str1, 
			   IN const wchar_t* a_str2,
			   IN bool a_caseSensitive = true) asd_noexcept;


	char* strcpy(OUT char* a_dst,
				 IN const char* a_src) asd_noexcept;

	wchar_t* strcpy(OUT wchar_t* a_dst,
					IN const wchar_t* a_src) asd_noexcept;


	// Ascii문자열만 사용 할 것.
	char* strcpy(OUT char* a_dst,
				 IN const wchar_t* a_src) asd_noexcept;

	// Ascii문자열만 사용 할 것.
	wchar_t* strcpy(OUT wchar_t* a_dst,
					IN const char* a_src) asd_noexcept;


	size_t strlen(IN const char* a_str) asd_noexcept;

	size_t strlen(IN const wchar_t* a_str) asd_noexcept;

	// SizeOfChar값 단위로 문자열 길이를 구한다.
	template<int SizeOfChar>
	inline size_t strlen(IN const void* a_str) asd_noexcept
	{
		static_assert(SizeOfChar==1 || SizeOfChar==2 || SizeOfChar==4,
					  "invalid SizeOfChar");

		if (a_str == nullptr)
			return 0;

		const int32_t Null = 0;
		const char* p = (const char*)a_str;
		size_t ret = 0;
		while (0 != std::memcmp(p, &Null, SizeOfChar)) {
			++ret;
			p += SizeOfChar;
		}
		return ret;
	}

	// 위 템플릿함수의 런타임 버전
	size_t strlen(IN const void* a_str,
				  IN int a_sizeOfChar) asd_noexcept;


	template<typename CHARTYPE>
	inline CHARTYPE toupper(IN CHARTYPE a_char)
	{
		if (a_char < 'a' || a_char > 'z')
			return a_char;
		return a_char - 0x20;
	}

	template<typename CHARTYPE>
	inline CHARTYPE tolower(IN CHARTYPE a_char)
	{
		if (a_char < 'A' || a_char > 'Z')
			return a_char;
		return a_char + 0x20;
	}



	template<typename CharType, bool CaseSensitive = true>
	struct hash_String
	{
		inline size_t operator() (IN const CharType* a_src) const asd_noexcept
		{
			const int cnt = sizeof(size_t) / sizeof(CharType);
			static_assert(sizeof(size_t) >= sizeof(CharType),
						  "invalid tempalte parameter");
			static_assert(sizeof(size_t) % sizeof(CharType) == 0,
						  "invalid tempalte parameter");
			static_assert(cnt >= 1,
						  "invalid tempalte parameter");

			if (a_src == nullptr)
				return 0;

			const CharType* p = a_src;
			size_t ret = 0x5555555555555555;
			while (*p != '\0') {
				size_t block = 0;
				for (int i=0; i<cnt; ++i) {
					CharType c;
					if (CaseSensitive)
						c = asd::toupper(*p);
					else
						c = *p;

					block |= c;
					++p;
					if (*p == '\0')
						break;
					else
						block <<= sizeof(CharType);
				}
				ret ^= block;
			}

			return ret;
		}
	};



	template<typename CharType, bool CaseSensitive = true>
	struct equal_to_String
	{
		inline bool operator()(IN const CharType* a_left,
							   IN const CharType* a_right) const asd_noexcept
		{
			return asd::strcmp(a_left, a_right, CaseSensitive) == 0;
		}
	};



	template<typename CHARTYPE>
	class BasicString : public asd::SharedArray<std::basic_string<CHARTYPE>>
	{
		// char16_t와 char32_t는 
		// 표준 C 함수가 제공되지 않으므로 미지원
		static_assert(   std::is_same<CHARTYPE, char>::value
					  || std::is_same<CHARTYPE, wchar_t>::value,
					  "CHARTYPE is not supported.");
	public:
		typedef CHARTYPE							CharType;
		typedef BasicString<CharType>				ThisType;
		typedef std::basic_string<CharType>			StdStrType;
		typedef asd::SharedArray<StdStrType>		BaseType;
		typedef hash_String<CharType, true>			Hash_CaseSensitive;
		typedef hash_String<CharType, false>		Hash_CaseInsensitive;
		typedef equal_to_String<CharType, true>		EqualTo_CaseSensitive;
		typedef equal_to_String<CharType, false>	EqualTo_CaseInsensitive;

		static const bool CaseSensitive_Default = true;



		inline BasicString() asd_noexcept
		{
		}



		inline BasicString(IN const ThisType& a_data) asd_noexcept
		{
			BaseType::operator=(a_data);
		}



		inline BasicString(MOVE ThisType&& a_data) asd_noexcept
		{
			BaseType::operator=(std::forward<BaseType>(a_data));
		}



		template<typename T>
		inline BasicString(IN const T& a_data) asd_noexcept
		{
			this->operator=(a_data);
		}



		template<typename... ARGS>
		inline BasicString(IN const CharType* a_format,
						   IN const ARGS&... a_args) asd_noexcept
		{
			Format(a_format, a_args...);
		}



		template<typename... ARGS>
		inline ThisType& Format(IN const CharType* a_format,
								IN const ARGS&... a_args) asd_noexcept
		{
			if (a_format == nullptr)
				return *this;
			auto p = BaseType::GetArrayPtr(false);
			*p = fmt::format(a_format, a_args...);
			return *this;
		}



		inline static ThisType StdFormat(IN const CharType* a_format,
										 IN ...) asd_noexcept
		{
			va_list args;
			va_start(args, a_format);
			ThisType ret = StdFormatV(a_format, args);
			va_end(args);
			return ret;
		}



		inline static ThisType StdFormatV(IN const CharType* a_format,
										  IN va_list& a_args) asd_noexcept
		{
			ThisType ret;
			auto len = asd::vscprintf(a_format, a_args);
			assert(len >= 0);
			ret.resize(len);

			if (len == 0)
				return ret;

			auto r = asd::vsprintf(ret.data(),
								   len + 1,
								   a_format,
								   a_args);
			assert(r >= 0);
			assert(r == len);
			return ret;
		}



		inline operator const CharType*() const asd_noexcept
		{
			return BaseType::data();
		}

		inline operator CharType*() asd_noexcept
		{
			return BaseType::data();
		}



		inline operator const void*() const asd_noexcept
		{
			return BaseType::data();
		}

		inline operator void*() asd_noexcept
		{
			return BaseType::data();
		}



		inline operator StdStrType() const asd_noexcept
		{
			return StdStrType(*BaseType::GetArrayPtr());
		}



		inline size_t GetHash() const asd_noexcept
		{
			hash_String<CharType, CaseSensitive_Default> func;
			return func(BaseType::data());
		}



		// std style
		asd_SharedArray_Define_StdStyleType(BaseType, value_type);
		asd_SharedArray_Define_StdStyleType(BaseType, size_type);
		asd_SharedArray_Define_StdStyleType(BaseType, reference);
		asd_SharedArray_Define_StdStyleType(BaseType, const_reference);
		asd_SharedArray_Define_StdStyleType(BaseType, iterator);
		asd_SharedArray_Define_StdStyleType(BaseType, const_iterator);
		asd_SharedArray_Define_StdStyleType(BaseType, reverse_iterator);
		asd_SharedArray_Define_StdStyleType(BaseType, const_reverse_iterator);

		inline size_type length() const asd_noexcept
		{
			return BaseType::GetArrayPtr()->length();
		}

		inline value_type* c_str() asd_noexcept
		{
			return (value_type*)BaseType::GetArrayPtr()->c_str();
		}

		inline const value_type* c_str() const asd_noexcept
		{
			return BaseType::GetArrayPtr()->c_str();
		}

		inline ThisType& append(IN const CharType* a_str,
								IN size_type a_len) asd_noexcept
		{
			BaseType::GetArrayPtr()->append(a_str, a_len);
			return *this;
		}

		inline ThisType& append(IN const ThisType& a_str) asd_noexcept
		{
			return append(a_str, a_str.size());
		}

		inline ThisType& append(IN const CharType* a_str) asd_noexcept
		{
			return append(a_str, asd::strlen(a_str));
		}



		// 비교
		template<typename T>
		inline static int Compare(IN const CharType* a_left,
								  IN const T& a_right,
								  IN bool a_caseSensitive = CaseSensitive_Default) asd_noexcept
		{
			return asd::strcmp(a_left,
							   ThisType(a_right).data(),
							   a_caseSensitive);
		}

		template<typename T>
		inline int Compare(IN const T& a_data,
						   IN bool a_caseSensitive = CaseSensitive_Default) const asd_noexcept
		{
			return Compare(BaseType::data(),
						   a_data,
						   a_caseSensitive);
		}

		template<typename T>
		inline bool operator==(IN const T& a_data) const asd_noexcept
		{
			return Compare(a_data) == 0;
		}

		template<typename T>
		inline bool operator!=(IN const T& a_data) const asd_noexcept
		{
			return Compare(a_data) != 0;
		}

		template<typename T>
		inline bool operator<(IN const T& a_data) const asd_noexcept
		{
			return Compare(a_data) < 0;
		}

		template<typename T>
		inline bool operator<=(IN const T& a_data) const asd_noexcept
		{
			return Compare(a_data) <= 0;
		}

		template<typename T>
		inline bool operator>(IN const T& a_data) const asd_noexcept
		{
			return Compare(a_data) > 0;
		}

		template<typename T>
		inline bool operator>=(IN const T& a_data) const asd_noexcept
		{
			return Compare(a_data) >= 0;
		}



		// 대입
		template<typename T>
		inline ThisType& operator+=(IN const T& a_data) asd_noexcept
		{
			const CharType format[] = {'{', '}', 0};
			if (length() == 0)
				return Format(format, a_data);
			auto conv = fmt::format(format, a_data);
			return append(conv.data(), conv.length());
		}

		template<typename T>
		inline ThisType& operator<<(IN const T& a_data) asd_noexcept
		{
			return this->operator+=(a_data);
		}

		template<typename T>
		inline ThisType operator+(IN const T& a_data) const asd_noexcept
		{
			ThisType ret;
			return ret << *this << a_data;
		}

		template<typename T>
		inline ThisType& operator=(IN const T& a_data) asd_noexcept
		{
			BaseType::resize(0);
			return this->operator+=(a_data);
		}



		// 특정 타입들 예외처리
		inline ThisType& operator=(IN const ThisType& a_data) asd_noexcept
		{
			BaseType::operator=(a_data);
			return *this;
		}

		inline ThisType& operator=(MOVE ThisType&& a_data) asd_noexcept
		{
			BaseType::operator=(std::forward<BaseType>(a_data));
			return *this;
		}

		inline ThisType& operator+=(IN const ThisType& a_data) asd_noexcept
		{
			if (length() == 0) {
				BaseType::operator=(a_data);
				return *this;
			}
			return append(a_data.data(), a_data.length());
		}

		inline ThisType& operator+=(IN const CharType* a_data) asd_noexcept
		{
			return append(a_data);
		}

		inline ThisType& operator+=(IN CharType* a_data) asd_noexcept
		{
			return append(a_data);
		}

		inline ThisType& operator+=(IN const std::nullptr_t&) asd_noexcept
		{
			return *this;
		}

#if asd_Compiler_MSVC
		inline ThisType& operator+=(IN const Caster& a_data) asd_noexcept
		{
			return this->operator+=(a_data.operator ThisType());
		}
#endif
	};

}



namespace std
{
	template<typename CharType>
	struct hash<asd::BasicString<CharType> >
		: public asd::hash_String<CharType, asd::BasicString<CharType>::CaseSensitive_Default>
	{
	};
}



template <typename CharType>
inline std::basic_ostream<CharType>& operator<<(IN std::basic_ostream<CharType>& a_left,
												IN const asd::BasicString<CharType>& a_right)
{
	return a_left << a_right.data();
}

template <typename CharType>
inline std::basic_istream<CharType>& operator>>(IN std::basic_istream<CharType>& a_left,
												OUT asd::BasicString<CharType>& a_right)
{
	std::basic_string<CharType> temp;
	a_left >> temp;
	a_right = temp;
	return a_left;
}