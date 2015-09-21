#pragma once
#include "asd/asdbase.h"
#include "asd/sharedarray.h"
#include <string>

namespace asd
{
	int vsprintf(OUT char* a_targetbuf,
				 IN int a_bufsize,
				 IN const char* a_format,
				 IN va_list& a_args) noexcept;

	int vsprintf(OUT wchar_t* a_targetbuf,
				 IN int a_bufsize,
				 IN const wchar_t* a_format,
				 IN va_list& a_args) noexcept;


	int vfprintf(IN FILE* a_fp,
				 IN const char* a_format,
				 IN va_list& a_args) noexcept;

	int vfprintf(IN FILE* a_fp,
				 IN const wchar_t* a_format,
				 IN va_list& a_args) noexcept;


	int vprintf(IN const char* a_format,
				IN va_list& a_args) noexcept;

	int vprintf(IN const wchar_t* a_format,
				IN va_list& a_args) noexcept;


	int sprintf(OUT char* a_targetbuf,
				IN int a_bufsize,
				IN const char* a_format,
				IN ...) noexcept;

	int sprintf(OUT wchar_t* a_targetbuf,
				IN int a_bufsize,
				IN const wchar_t* a_format,
				IN ...) noexcept;


	int printf(IN const char* a_format,
			   IN ...) noexcept;

	int printf(IN const wchar_t* a_format,
			   IN ...) noexcept;


	int vscprintf(IN const char* a_format,
				  IN va_list& a_args) noexcept;

	int vscprintf(IN const wchar_t* a_format,
				  IN va_list& a_args) noexcept;


	int scprintf(IN const char* a_format,
				 IN ...) noexcept;

	int scprintf(IN const wchar_t* a_format,
				 IN ...) noexcept;


	int fputs(IN const char* a_str,
			  IN FILE* a_fp) noexcept;

	int fputs(IN const wchar_t* a_str,
			  IN FILE* a_fp) noexcept;

	
	int puts(IN const char* a_str) noexcept;

	int puts(IN const wchar_t* a_str) noexcept;


	int strcmp(IN const char* a_str1, 
			   IN const char* a_str2,
			   IN bool a_caseSensitive = true) noexcept;

	int strcmp(IN const wchar_t* a_str1, 
			   IN const wchar_t* a_str2,
			   IN bool a_caseSensitive = true) noexcept;


	char* strcpy(OUT char* a_dst,
				 IN const char* a_src) noexcept;

	wchar_t* strcpy(OUT wchar_t* a_dst,
					IN const wchar_t* a_src) noexcept;


	// Ascii문자열만 사용 할 것.
	char* strcpy(OUT char* a_dst,
				 IN const wchar_t* a_src) noexcept;

	// Ascii문자열만 사용 할 것.
	wchar_t* strcpy(OUT wchar_t* a_dst,
					IN const char* a_src) noexcept;


	size_t strlen(IN const char* a_str) noexcept;

	size_t strlen(IN const wchar_t* a_str) noexcept;

	// SizeOfChar값 단위로 문자열 길이를 구한다.
	template<int SizeOfChar>
	inline size_t strlen(IN const void* a_str) noexcept
	{
		static_assert(SizeOfChar==1 || SizeOfChar==2 || SizeOfChar==4,
					  "invalid SizeOfChar");

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
				  IN int a_sizeOfChar) noexcept;


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
		inline size_t operator() (IN const CharType* a_src) const noexcept
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
			size_t ret = 0;
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
							   IN const CharType* a_right) const noexcept
		{
			return asd::strcmp(a_left, a_right, CaseSensitive) == 0;
		}
	};



	template<typename CHARTYPE>
	class BasicString : protected asd::SharedArray<CHARTYPE>
	{
		// char16_t와 char32_t는 
		// 표준 C 함수가 제공되지 않으므로 미지원
		static_assert(std::is_same<CHARTYPE, char>::value ||
					  std::is_same<CHARTYPE, wchar_t>::value,
					  "CHARTYPE is not supported.");
	public:
		typedef CHARTYPE							CharType;
		typedef BasicString<CharType>				ThisType;
		typedef asd::SharedArray<CharType>			BaseType;
		typedef std::basic_string<CharType>			SupportType_StdString;
		typedef hash_String<CharType, true>			Hash_CaseSensitive;
		typedef hash_String<CharType, false>		Hash_IgnoreCase;
		typedef equal_to_String<CharType, true>		EqualTo_CaseSensitive;
		typedef equal_to_String<CharType, false>	EqualTo_CaseInsensitive;

		static const bool CaseSensitive_Default = true;


		BasicString() noexcept
		{
		}



		template<typename... ARGS>
		BasicString(IN const CharType* a_format,
					IN ARGS&&... a_args) noexcept
		{
			Format(a_format, a_args...);
		}



		// '\0'을 제외한 캐릭터 수를 리턴
		inline size_t GetLength() const noexcept
		{
			CharType* p = BaseType::get();
			if (p == nullptr)
				return 0;

			auto cnt = BaseType::GetCount() - 1;
			assert(cnt >= 0);
			assert(p[cnt] == '\0');

			return cnt;
		}



		// 문자열의 시작 포인터를 리턴
		inline CharType* GetData() const noexcept
		{
			CharType* ret = BaseType::get();
			if (ret == nullptr) {
				const static CharType NullChar = '\0';
				ret = (CharType*)&NullChar;
			}

			assert(ret[GetLength()] == '\0');
			assert(ret != nullptr);
			return ret;
		}



		inline operator const CharType*() const noexcept
		{
			return GetData();
		}

		inline operator CharType*() noexcept
		{
			return GetData();
		}



		inline operator const void*() const noexcept
		{
			return GetData();
		}

		inline operator void*() noexcept
		{
			return GetData();
		}



		inline operator SupportType_StdString() const noexcept
		{
			return SupportType_StdString(GetData(), GetLength());
		}



		inline size_t GetHash() const noexcept
		{
			hash_String<CharType, CaseSensitive_Default> func;
			return func(GetData());
		}



		// 값을 수정하는 모든 메소드들은 반드시 이것을 호출한다.
		inline void Resize(IN size_t a_newLen,
						   IN bool a_preserveOldData = false) noexcept
		{
			if (a_newLen == 0) {
				BaseType::Resize(a_newLen, a_preserveOldData);
				return;
			}
			assert(a_newLen > 0);

			BaseType::Resize(a_newLen + 1, a_preserveOldData);
			BaseType::get()[a_newLen] = '\0';

			assert(GetLength() > 0);
		}



		// 현재 문자열 뒤에 인자로 받은 문자열을 적용한다.
		//  a_str  :  추가할 문자열
		//  a_len  :  a_str에서 '\0'을 제외한 원소 개수 (a_len = strlen(a_str))
		inline void Append(IN const CharType* a_str,
						   IN size_t a_len) noexcept
		{
			const auto orgLen = GetLength();
			const auto addLen = a_len;
			const auto newLen = orgLen + addLen;
			if (newLen==orgLen || a_str==nullptr)
				return;

			assert(a_str[0] != '\0');
			assert(addLen > 0);
			assert(newLen > 0);
			assert(newLen > orgLen);

			Resize(newLen, true);
			std::memcpy(GetData() + orgLen,
						a_str, 
						sizeof(CharType)*addLen);
			assert(GetLength() > 0);
		}



		ThisType& Format(IN const CharType* a_format,
						 IN ...) noexcept
		{
			va_list args;
			va_start(args, a_format);
			FormatV(a_format, args);
			va_end(args);

			return *this;
		}



		inline ThisType& FormatV(IN const CharType* a_format,
								 IN va_list& a_args) noexcept
		{
			auto length = asd::vscprintf(a_format, a_args);
			assert(length >= 0);

			Resize(length, false);
			auto r = asd::vsprintf(GetData(),
								   length + 1,
								   a_format,
								   a_args);
			assert(r >= 0);
			assert(r == length);
			assert(GetLength() >= 0);

			return *this;
		}



#define asd_String_Define_CompareOperator(CompareFunction, TemplateType, CaseSensitive_DefaultVar)		\
																										\
		inline int Compare(IN TemplateType a_rval,														\
						   IN bool a_caseSensitive = CaseSensitive_DefaultVar) const noexcept			\
		{																								\
			return CompareFunction(GetData(), a_rval, a_caseSensitive);									\
		}																								\
																										\
		inline bool operator == (IN TemplateType a_rval) const noexcept									\
		{																								\
			return Compare(a_rval) == 0;																\
		}																								\
																										\
		inline bool operator != (IN TemplateType a_rval) const noexcept									\
		{																								\
			return Compare(a_rval) != 0;																\
		}																								\
																										\
		inline bool operator < (IN TemplateType a_rval) const noexcept									\
		{																								\
			return Compare(a_rval) < 0;																	\
		}																								\
																										\
		inline bool operator <= (IN TemplateType a_rval) const noexcept									\
		{																								\
			return Compare(a_rval) <= 0;																\
		}																								\
																										\
		inline bool operator > (IN TemplateType a_rval) const noexcept									\
		{																								\
			return Compare(a_rval) > 0;																	\
		}																								\
																										\
		inline bool operator >= (IN TemplateType a_rval) const noexcept									\
		{																								\
			return Compare(a_rval) >= 0;																\
		}																								\



		// static int Compare() 함수를 정의하면서 다른 비교연산자들까지 오버로딩하는 매크로.
#define asd_String_Define_CompareFunction(CharTypePointer_LeftVar,										\
										  TemplateType,													\
										  Template_RightVar,											\
										  IgnoreCaseVar,												\
										  CaseSensitive_DefaultVar)										\
																										\
		asd_String_Define_CompareOperator(Compare, TemplateType, CaseSensitive_DefaultVar)				\
																										\
		inline static int Compare(IN TemplateType a_templateVar,										\
								  IN const CharType* a_stringVar,										\
								  IN bool a_caseSensitive = CaseSensitive_DefaultVar) noexcept			\
		{																								\
			return Compare(a_stringVar, a_templateVar, a_caseSensitive);								\
		}																								\
																										\
		inline static int Compare(IN const CharType* CharTypePointer_LeftVar,							\
								  IN TemplateType Template_RightVar,									\
								  IN bool IgnoreCaseVar = CaseSensitive_DefaultVar) noexcept			\



		// 대입연산자(=)를 정의하면서 생성자까지 한번에 정의하는 매크로.
#define asd_String_Define_AssignmentOperator_Substitute(ArgType, ArgVar)								\
		BasicString(IN ArgType ArgVar) noexcept															\
		{																								\
			*this = ArgVar;																				\
		}																								\
																										\
		inline ThisType& operator = (IN ArgType ArgVar) noexcept										\



		// += 연산자를 정의하면서 << 와 + 연산자를 한번에 정의하는 매크로.
#define asd_String_Define_AssignmentOperator_Append(ArgType, ArgVar)									\
		inline ThisType operator + (IN ArgType ArgVar) const noexcept									\
		{																								\
			ThisType ret;																				\
			ret << *this << ArgVar;																		\
			return ret;																					\
		}																								\
																										\
		inline ThisType& operator << (IN ArgType ArgVar) noexcept										\
		{																								\
			return *this += ArgVar;																		\
		}																								\
																										\
		inline ThisType& operator += (IN ArgType ArgVar) noexcept										\



		// += 연산자를 기반으로 생성자와 기타 Assignment 연산자들을 한번에 정의하는 매크로.
#define asd_String_Define_AssignmentOperator(ArgType, ArgVar)											\
		asd_String_Define_AssignmentOperator_Substitute(ArgType, ArgVar)								\
		{																								\
			BaseType::reset();																			\
			return *this += ArgVar;																		\
		}																								\
																										\
		asd_String_Define_AssignmentOperator_Append(ArgType, ArgVar)									\



		// asd_Define_AssignmentOperator를 정의하면서 
		// asd_Define_CompareFunction까지 정의하는 매크로.
#define asd_String_Define_AssignmentOperator_GenCompare(ArgType, ArgVar, CaseSensitive_DefaultVar)				\
		asd_String_Define_CompareFunction(a_left, ArgType, a_right, a_caseSensitive, CaseSensitive_DefaultVar)	\
		{																										\
			return Compare(a_left, ThisType(a_right), a_caseSensitive);											\
		}																										\
																												\
		asd_String_Define_AssignmentOperator(ArgType, ArgVar)													\



		// CharType 관련.
		//   문자열 비교 함수.
		//   매크로에 의해 오버로딩된 대부분의 Compare함수들은 최종적으로 이것을 호출한다.
		//     return -  :  left <  right
		//     return +  :  left >  right
		//     return 0  :  left == right
		inline static int Compare(IN const CharType* a_left,
								  IN const CharType* a_right,
								  IN bool a_caseSensitive = CaseSensitive_Default) noexcept
		{
			return asd::strcmp(a_left,
							   a_right,
							   a_caseSensitive);
		}

		asd_String_Define_CompareOperator(Compare, const CharType*, CaseSensitive_Default)

		asd_String_Define_AssignmentOperator(const CharType*, a_rval)
		{
			Append(a_rval, asd::strlen(a_rval));
			return *this;
		}



		// ThisType 관련.
		//   ambiguous error를 피하기 위한 오버로딩.
		inline static int Compare(IN const CharType* a_left,
								  IN const ThisType& a_right,
								  IN bool a_caseSensitive = CaseSensitive_Default) noexcept
		{
			return Compare(a_left,
						   a_right.GetData(),
						   a_caseSensitive);
		}

		asd_String_Define_CompareOperator(Compare, const ThisType&, CaseSensitive_Default)

		asd_String_Define_AssignmentOperator_Substitute(REF const ThisType&, a_share)
		{
			// 공유
			BaseType::operator=(a_share);
			return *this;
		}

		asd_String_Define_AssignmentOperator_Append(IN const ThisType&, a_rval)
		{
			if (GetLength() == 0) {
				// 기존데이터가 없다면 대입 (공유)
				return (*this = a_rval);
			}
			
			// 복사
			Append(a_rval.GetData(), a_rval.GetLength());
			return *this;
		}



		// std::string 관련.
		asd_String_Define_CompareFunction(a_left,
										  const SupportType_StdString&,
										  a_right,
										  a_caseSensitive,
										  CaseSensitive_Default)
		{
			return Compare(a_left,
						   a_right.data(),
						   a_caseSensitive);
		}

		asd_String_Define_AssignmentOperator(const SupportType_StdString&, a_rval)
		{
			Append(a_rval.data(), a_rval.length());
			return *this;
		}



		// 정수
		asd_String_Define_AssignmentOperator_GenCompare(const int32_t, a_rval, true)
		{
			const CharType format[] = {'%', 'd', 0};
			ThisType temp(format, a_rval);
			return *this += temp;
		}

		asd_String_Define_AssignmentOperator_GenCompare(const uint32_t, a_rval, true)
		{
			const CharType format[] = {'%', 'u', 0};
			ThisType temp(format, a_rval);
			return *this += temp;
		}

		asd_String_Define_AssignmentOperator_GenCompare(const int64_t, a_rval, true)
		{
			const CharType format[] = {'%', 'l', 'l', 'd', 0};
			ThisType temp(format, a_rval);
			return *this += temp;
		}

		asd_String_Define_AssignmentOperator_GenCompare(const uint64_t, a_rval, true)
		{
			const CharType format[] = {'%', 'l', 'l', 'u', 0};
			ThisType temp(format, a_rval);
			return *this += temp;
		}



		// 실수
		asd_String_Define_AssignmentOperator_GenCompare(const double, a_rval, false)
		{
			const CharType format[] = {'%', 'l', 'f', 0};
			ThisType temp(format, a_rval);
			return *this += temp;
		}



		// byte
		asd_String_Define_AssignmentOperator_GenCompare(const uint8_t, a_rval, false)
		{
			const CharType format[] = {'%', 'x', 0};
			ThisType temp(format, (uint32_t)a_rval);
			return *this += temp;
		}



		// pointer
		asd_String_Define_AssignmentOperator_GenCompare(const void*, a_rval, false)
		{
			const CharType format[] = {'%', 'p', 0};
			ThisType temp(format, a_rval);
			return *this += temp;
		}



		// bool
		asd_String_Define_AssignmentOperator_GenCompare(const bool, a_rval, false)
		{
			if (a_rval) {
				const CharType str_true[] = {'t', 'r', 'u', 'e', 0};
				Append(str_true,
					   sizeof(str_true) / sizeof(CharType) - 1);
			}
			else {
				const CharType str_false[] = {'f', 'a', 'l', 's', 'e', 0};
				Append(str_false,
					   sizeof(str_false) / sizeof(CharType) - 1);
			}
			return *this;
		}



		// std style
		typedef CharType value_type;
		typedef size_t size_type;
		
		inline const value_type* data() const noexcept
		{
			return GetData();
		}

		inline value_type* data() noexcept
		{
			return GetData();
		}

		inline const value_type* c_str() const noexcept
		{
			return GetData();
		}

		inline value_type* c_str() noexcept
		{
			return GetData();
		}

		inline size_type size() const noexcept
		{
			return GetLength();
		}

		inline size_type length() const noexcept
		{
			return GetLength();
		}

		inline void resize(IN size_type a_count) noexcept
		{
			Resize(a_count, true);
			assert(data()[size()] == '\0');
			assert(size() == a_count);
		}

		inline void resize(IN size_type a_count,
						   IN value_type a_fill) noexcept
		{
			const auto OldLen = size();
			resize(a_count);
			if (a_count > OldLen) {
				const auto NewLen = size();
				value_type* p = data();
				for (auto i=OldLen; i<NewLen; ++i)
					p[i] = a_fill;
			}
		}

		inline ThisType& append(IN const CharType* a_str,
								IN size_type a_len) noexcept
		{
			Append(a_str, a_len);
			return *this;
		}

		inline ThisType& append(IN const ThisType& a_str) noexcept
		{
			return append(a_str, a_str.size());
		}

		inline ThisType& append(IN const CharType* a_str) noexcept
		{
			return append(a_str, asd::strlen(a_str));
		}
	};



	// MultiByte String
	typedef BasicString<char>		MString;

	// Wide String
	typedef BasicString<wchar_t>	WString;

}



namespace std
{
	template<typename CharType>
	struct hash<asd::BasicString<CharType> >
		: public asd::hash_String<CharType, asd::BasicString<CharType>::CaseSensitive_Default>
	{
	};
}



#include <iostream>

template <typename CharType>
std::basic_ostream<CharType>& operator << (IN std::basic_ostream<CharType>& a_left,
										   IN const asd::BasicString<CharType>& a_right)
{
	return a_left << (const CharType*)a_right;
}

template <typename CharType>
std::basic_istream<CharType>& operator >> (IN std::basic_istream<CharType>& a_left,
										   IN asd::BasicString<CharType>& a_right)
{
	std::basic_string<CharType> temp;
	a_left >> temp;
	a_right = temp;
	return a_left;
}
