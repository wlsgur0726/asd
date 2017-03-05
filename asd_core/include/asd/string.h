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


	char* strcpy(OUT char* a_dst,
				 IN const char* a_src,
				 IN size_t a_dstBufCount = std::numeric_limits<size_t>::max()) asd_noexcept;

	wchar_t* strcpy(OUT wchar_t* a_dst,
					IN const wchar_t* a_sr,
					IN size_t a_dstBufCount = std::numeric_limits<size_t>::max()) asd_noexcept;

	char16_t* strcpy(OUT char16_t* a_dst,
					 IN const char16_t* a_src,
					 IN size_t a_dstBufCount = std::numeric_limits<size_t>::max()) asd_noexcept;

	char32_t* strcpy(OUT char32_t* a_dst,
					 IN const char32_t* a_src,
					 IN size_t a_dstBufCount = std::numeric_limits<size_t>::max()) asd_noexcept;


	// Ascii문자열만 사용 할 것.
	char* strcpy(OUT char* a_dst,
				 IN const wchar_t* a_src,
				 IN size_t a_dstBufCount = std::numeric_limits<size_t>::max()) asd_noexcept;

	// Ascii문자열만 사용 할 것.
	wchar_t* strcpy(OUT wchar_t* a_dst,
					IN const char* a_src,
					IN size_t a_dstBufCount = std::numeric_limits<size_t>::max()) asd_noexcept;


	size_t strlen(IN const char* a_str) asd_noexcept;

	size_t strlen(IN const wchar_t* a_str) asd_noexcept;

	size_t strlen(IN const char16_t* a_str) asd_noexcept;

	size_t strlen(IN const char32_t* a_str) asd_noexcept;

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



	template<typename CharType>
	inline int strcmp(IN const CharType* a_str1,
					  IN const CharType* a_str2,
					  IN bool a_caseSensitive = true)
	{
		if (a_str1 == a_str2)
			return 0;

		const CharType NullChar = '\0';
		if (a_str1 == nullptr)
			a_str1 = &NullChar;

		if (a_str2 == nullptr)
			a_str2 = &NullChar;

		if (a_caseSensitive) {
			for (size_t i=0;; ++i) {
				const CharType c1 = a_str1[i];
				const CharType c2 = a_str2[i];
				if (c1 < c2)
					return -1;
				if (c1 > c2)
					return 1;
				if (c1 == NullChar)
					break;
			}
		}
		else {
			for (size_t i=0;; ++i) {
				const CharType c1 = asd::tolower(a_str1[i]);
				const CharType c2 = asd::tolower(a_str2[i]);
				if (c1 < c2)
					return -1;
				if (c1 > c2)
					return 1;
				if (c1 == NullChar)
					break;
			}
		}
		return 0;
	}



	template<typename CharType, bool CaseSensitive = true>
	struct hash_String
	{
		inline size_t operator() (IN const CharType* a_src) const asd_noexcept
		{
			const size_t cnt = sizeof(size_t) / sizeof(CharType);
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
				for (size_t i=0; i<cnt; ++i) {
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
		// cppformat 라이브러리에서 지원되지 않고,
		// 표준 C 함수도 제공되지 않으므로 미지원
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



		inline BasicString(IN const CharType* a_str) asd_noexcept
		{
			auto p = BaseType::GetArrayPtr(false);
			*p = a_str;
		}



		template<typename... ARGS>
		inline static ThisType Format(IN const CharType* a_format,
									  IN const ARGS&... a_args) asd_noexcept
		{
			ThisType ret;
			if (a_format == nullptr)
				return ret;
			auto p = ret.GetArrayPtr(false);
			*p = fmt::format(a_format, a_args...);
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

		inline ThisType substr(IN size_type a_pos = 0,
							   IN size_type a_count = StdStrType::npos) const asd_noexcept
		{
			return BaseType::GetArrayPtr()->substr(a_pos, a_count);
		}

		template<typename... ARGS>
		inline size_type find(IN ARGS... args) const asd_noexcept
		{
			return BaseType::GetArrayPtr()->find(args...);
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
				return this->operator=(Format(format, a_data));
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