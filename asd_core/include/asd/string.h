#pragma once
#include "asdbase.h"
#include "caster.h"
#include "../../built-in/cppformat/fmt/ostream.h"
#include "../../built-in/cppformat/fmt/format.h"
#include <string>
#include <iostream>

namespace asd
{
	int vsprintf(char* a_targetbuf /*Out*/,
				 int a_bufsize,
				 const char* a_format,
				 va_list& a_args);

	int vsprintf(wchar_t* a_targetbuf /*Out*/,
				 int a_bufsize,
				 const wchar_t* a_format,
				 va_list& a_args);


	int vfprintf(FILE* a_fp,
				 const char* a_format,
				 va_list& a_args);

	int vfprintf(FILE* a_fp,
				 const wchar_t* a_format,
				 va_list& a_args);


	int vprintf(const char* a_format,
				va_list& a_args);

	int vprintf(const wchar_t* a_format,
				va_list& a_args);


	int sprintf(char* a_targetbuf /*Out*/,
				int a_bufsize,
				const char* a_format,
				...);

	int sprintf(wchar_t* a_targetbuf /*Out*/,
				int a_bufsize,
				const wchar_t* a_format,
				...);


	int printf(const char* a_format,
			   ...);

	int printf(const wchar_t* a_format,
			   ...);


	int vscprintf(const char* a_format,
				  va_list& a_args);

	int vscprintf(const wchar_t* a_format,
				  va_list& a_args);


	int scprintf(const char* a_format,
				 ...);

	int scprintf(const wchar_t* a_format,
				 ...);


	int fputs(const char* a_str,
			  FILE* a_fp);

	int fputs(const wchar_t* a_str,
			  FILE* a_fp);

	
	int puts(const char* a_str);

	int puts(const wchar_t* a_str);


	char* strcpy(char* a_dst /*Out*/,
				 const char* a_src,
				 size_t a_dstBufCount = std::numeric_limits<size_t>::max());

	wchar_t* strcpy(wchar_t* a_dst /*Out*/,
					const wchar_t* a_sr,
					size_t a_dstBufCount = std::numeric_limits<size_t>::max());

	char16_t* strcpy(char16_t* a_dst /*Out*/,
					 const char16_t* a_src,
					 size_t a_dstBufCount = std::numeric_limits<size_t>::max());

	char32_t* strcpy(char32_t* a_dst /*Out*/,
					 const char32_t* a_src,
					 size_t a_dstBufCount = std::numeric_limits<size_t>::max());


	// Ascii문자열만 사용 할 것.
	char* strcpy(char* a_dst /*Out*/,
				 const wchar_t* a_src,
				 size_t a_dstBufCount = std::numeric_limits<size_t>::max());

	// Ascii문자열만 사용 할 것.
	wchar_t* strcpy(wchar_t* a_dst /*Out*/,
					const char* a_src,
					size_t a_dstBufCount = std::numeric_limits<size_t>::max());


	size_t strlen(const char* a_str);

	size_t strlen(const wchar_t* a_str);

	size_t strlen(const char16_t* a_str);

	size_t strlen(const char32_t* a_str);

	// SizeOfChar값 단위로 문자열 길이를 구한다.
	template<int SizeOfChar>
	inline size_t strlen(const void* a_str)
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
	size_t strlen(const void* a_str,
				  int a_sizeOfChar);


	template<typename CHARTYPE>
	inline CHARTYPE toupper(CHARTYPE a_char)
	{
		if (a_char < 'a' || 'z' < a_char)
			return a_char;
		return a_char - 0x20;
	}

	template<typename CHARTYPE>
	inline CHARTYPE tolower(CHARTYPE a_char)
	{
		if (a_char < 'A' || 'Z' < a_char)
			return a_char;
		return a_char + 0x20;
	}



	template<typename CharType>
	inline int strcmp(const CharType* a_str1,
					  const CharType* a_str2,
					  bool a_caseSensitive = true)
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
		inline size_t operator() (const CharType* a_src) const
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
		inline bool operator()(const CharType* a_left,
							   const CharType* a_right) const
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



		inline BasicString()
		{
		}



		inline BasicString(const ThisType& a_data)
		{
			BaseType::operator=(a_data);
		}



		inline BasicString(ThisType&& a_data)
		{
			BaseType::operator=(std::forward<BaseType>(a_data));
		}



		template<typename T>
		inline BasicString(const T& a_data)
		{
			this->operator=(a_data);
		}



		inline BasicString(const CharType* a_str)
		{
			auto p = BaseType::GetArrayPtr(false);
			*p = a_str;
		}



		template<typename... ARGS>
		inline static ThisType Format(const CharType* a_format,
									  const ARGS&... a_args)
		{
			ThisType ret;
			if (a_format == nullptr)
				return ret;
			auto p = ret.GetArrayPtr(false);
			*p = fmt::format(a_format, a_args...);
			return ret;
		}



		inline operator const CharType*() const
		{
			return BaseType::data();
		}

		inline operator CharType*()
		{
			return BaseType::data();
		}



		inline operator const void*() const
		{
			return BaseType::data();
		}

		inline operator void*()
		{
			return BaseType::data();
		}



		inline operator StdStrType() const
		{
			return StdStrType(*BaseType::GetArrayPtr());
		}



		inline size_t GetHash() const
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

		inline size_type length() const
		{
			return BaseType::GetArrayPtr()->length();
		}

		inline value_type* c_str()
		{
			return (value_type*)BaseType::GetArrayPtr()->c_str();
		}

		inline const value_type* c_str() const
		{
			return BaseType::GetArrayPtr()->c_str();
		}

		inline ThisType& append(const CharType* a_str,
								size_type a_len)
		{
			BaseType::GetArrayPtr()->append(a_str, a_len);
			return *this;
		}

		inline ThisType& append(const ThisType& a_str)
		{
			return append(a_str, a_str.size());
		}

		inline ThisType& append(const CharType* a_str)
		{
			return append(a_str, asd::strlen(a_str));
		}

		inline ThisType substr(size_type a_pos = 0,
							   size_type a_count = StdStrType::npos) const
		{
			return BaseType::GetArrayPtr()->substr(a_pos, a_count);
		}

		template<typename... ARGS>
		inline size_type find(ARGS... args) const
		{
			return BaseType::GetArrayPtr()->find(args...);
		}



		// 비교
		template<typename T>
		inline static int Compare(const CharType* a_left,
								  const T& a_right,
								  bool a_caseSensitive = CaseSensitive_Default)
		{
			return asd::strcmp(a_left,
							   ThisType(a_right).data(),
							   a_caseSensitive);
		}

		template<typename T>
		inline int Compare(const T& a_data,
						   bool a_caseSensitive = CaseSensitive_Default) const
		{
			return Compare(BaseType::data(),
						   a_data,
						   a_caseSensitive);
		}

		template<typename T>
		inline bool operator==(const T& a_data) const
		{
			return Compare(a_data) == 0;
		}

		template<typename T>
		inline bool operator!=(const T& a_data) const
		{
			return Compare(a_data) != 0;
		}

		template<typename T>
		inline bool operator<(const T& a_data) const
		{
			return Compare(a_data) < 0;
		}

		template<typename T>
		inline bool operator<=(const T& a_data) const
		{
			return Compare(a_data) <= 0;
		}

		template<typename T>
		inline bool operator>(const T& a_data) const
		{
			return Compare(a_data) > 0;
		}

		template<typename T>
		inline bool operator>=(const T& a_data) const
		{
			return Compare(a_data) >= 0;
		}



		// 대입
		template<typename T>
		inline ThisType& operator+=(const T& a_data)
		{
			const CharType format[] = {'{', '}', 0};
			if (length() == 0)
				return this->operator=(Format(format, a_data));
			auto conv = fmt::format(format, a_data);
			return append(conv.data(), conv.length());
		}

		template<typename T>
		inline ThisType& operator<<(const T& a_data)
		{
			return this->operator+=(a_data);
		}

		template<typename T>
		inline ThisType operator+(const T& a_data) const
		{
			ThisType ret;
			return ret << *this << a_data;
		}

		template<typename T>
		inline ThisType& operator=(const T& a_data)
		{
			BaseType::resize(0);
			return this->operator+=(a_data);
		}



		// 특정 타입들 예외처리
		inline ThisType& operator=(const ThisType& a_data)
		{
			BaseType::operator=(a_data);
			return *this;
		}

		inline ThisType& operator=(ThisType&& a_data)
		{
			BaseType::operator=(std::forward<BaseType>(a_data));
			return *this;
		}

		inline ThisType& operator+=(const ThisType& a_data)
		{
			if (length() == 0) {
				BaseType::operator=(a_data);
				return *this;
			}
			return append(a_data.data(), a_data.length());
		}

		inline ThisType& operator+=(const CharType* a_data)
		{
			return append(a_data);
		}

		inline ThisType& operator+=(CharType* a_data)
		{
			return append(a_data);
		}

		inline ThisType& operator+=(const std::nullptr_t&)
		{
			return *this;
		}

#if asd_Compiler_MSVC
		inline ThisType& operator+=(const Caster& a_data)
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

	template <typename CharType, typename Traits>
	inline basic_ostream<CharType, Traits>& operator<<(basic_ostream<CharType, Traits>& a_left,
													   const asd::BasicString<CharType>& a_right)
	{
		return a_left << a_right.data();
	}

	template <typename CharType, typename Traits>
	inline basic_istream<CharType, Traits>& operator>>(basic_istream<CharType, Traits>& a_left,
													   asd::BasicString<CharType>& a_right /*Out*/)
	{
		basic_string<CharType> temp;
		a_left >> temp;
		a_right = temp;
		return a_left;
	}
}
