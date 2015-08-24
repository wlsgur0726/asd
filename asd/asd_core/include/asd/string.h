#pragma once
#include "asd/asdbase.h"
#include <cstring>
#include <string>

namespace asd
{
	int vsprintf(OUT char* a_targetbuf,
				 IN int a_bufsize,
				 IN const char* a_format,
				 IN va_list& a_args) asd_NoThrow;

	int vsprintf(OUT wchar_t* a_targetbuf,
				 IN int a_bufsize,
				 IN const wchar_t* a_format,
				 IN va_list& a_args) asd_NoThrow;


	int vfprintf(IN FILE* a_fp,
				 IN const char* a_format,
				 IN va_list& a_args) asd_NoThrow;

	int vfprintf(IN FILE* a_fp,
				 IN const wchar_t* a_format,
				 IN va_list& a_args) asd_NoThrow;


	int vprintf(IN const char* a_format,
				IN va_list& a_args) asd_NoThrow;

	int vprintf(IN const wchar_t* a_format,
				IN va_list& a_args) asd_NoThrow;


	int sprintf(OUT char* a_targetbuf,
				IN int a_bufsize,
				IN const char* a_format,
				IN ...) asd_NoThrow;

	int sprintf(OUT wchar_t* a_targetbuf,
				IN int a_bufsize,
				IN const wchar_t* a_format,
				IN ...) asd_NoThrow;


	int printf(IN const char* a_format,
			   IN ...) asd_NoThrow;

	int printf(IN const wchar_t* a_format,
			   IN ...) asd_NoThrow;


	int vscprintf(IN const char* a_format,
				  IN va_list& a_args) asd_NoThrow;

	int vscprintf(IN const wchar_t* a_format,
				  IN va_list& a_args) asd_NoThrow;


	int fputs(IN const char* a_str,
			  IN FILE* a_fp) asd_NoThrow;

	int fputs(IN const wchar_t* a_str,
			  IN FILE* a_fp) asd_NoThrow;

	
	int puts(IN const char* a_str) asd_NoThrow;

	int puts(IN const wchar_t* a_str) asd_NoThrow;


	int strcmp(IN const char* a_str1, 
			   IN const char* a_str2,
			   IN bool a_ignoreCase = false) asd_NoThrow;

	int strcmp(IN const wchar_t* a_str1, 
			   IN const wchar_t* a_str2,
			   IN bool a_ignoreCase = false) asd_NoThrow;


	char* strcpy(OUT char* a_dst,
				 IN const char* a_src) asd_NoThrow;

	wchar_t* strcpy(OUT wchar_t* a_dst,
					IN const wchar_t* a_src) asd_NoThrow;


	// Ascii문자열만 사용 할 것.
	char* strcpy(OUT char* a_dst,
				 IN const wchar_t* a_src) asd_NoThrow;

	// Ascii문자열만 사용 할 것.
	wchar_t* strcpy(OUT wchar_t* a_dst,
					IN const char* a_src) asd_NoThrow;


	size_t strlen(IN const char* a_str) asd_NoThrow;

	size_t strlen(IN const wchar_t* a_str) asd_NoThrow;

	// SizeOfChar값 단위로 문자열 길이를 구한다.
	template<int SizeOfChar>
	size_t strlen(IN const void* a_str) asd_NoThrow
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
				  IN int a_sizeOfChar) asd_NoThrow;



	template<typename CHARTYPE>
	class BasicString
	{
		static_assert(std::is_same<CHARTYPE, char>::value ||
					  std::is_same<CHARTYPE, wchar_t>::value,
					  "CHARTYPE is not supported.");

	public:
		typedef CHARTYPE						CharType;

		typedef BasicString<CharType>			ThisType;

		typedef std::shared_ptr<CharType>		Buffer_ptr;

		typedef std::basic_string<CharType>		SupportType_StdString;

		static const bool IgnoreCase_Default = false;

	private:
		// 앞에서 sizeof(std::size_t) 만큼은 문자열 길이를 저장하는데 사용한다.
		static const int DataStartOffset = sizeof(std::size_t) / sizeof(CharType);
		

		inline static void Assert_TypeSizeCheck() asd_NoThrow 
		{
			assert(sizeof(std::size_t) >= sizeof(CharType));
			assert(sizeof(std::size_t) % sizeof(CharType) == 0);
		}


		inline static CharType* GetField_String(IN CharType* a_data) asd_NoThrow 
		{
			Assert_TypeSizeCheck();
			return a_data + DataStartOffset;
		}

		
		inline static std::size_t& GetField_Length(IN CharType* a_data) asd_NoThrow
		{
			Assert_TypeSizeCheck();
			return *((std::size_t*)a_data);
		}


		Buffer_ptr m_data;


	public:
		BasicString() asd_NoThrow
		{
		}



		template<typename... ARGS>
		BasicString(IN const CharType* a_format,
					IN ARGS&&... a_args) asd_NoThrow
		{
			Format(a_format, a_args...);
		}



		ThisType& Format(IN const CharType* a_format,
						 IN ...) asd_NoThrow
		{
			va_list args;
			va_start(args, a_format);
			FormatV(a_format, args);
			va_end(args);

			return *this;
		}



		inline ThisType& FormatV(IN const CharType* a_format,
								 IN va_list& a_args) asd_NoThrow
		{
			int length = vscprintf(a_format, a_args);
			assert(length > 0);

			CharType* newBuf = new CharType[length + DataStartOffset + 1];
			m_data = Buffer_ptr(newBuf, 
								std::default_delete<CharType[]>());

			auto vsprintf_ret = asd::vsprintf(GetField_String(newBuf),
											  length + 1,
											  a_format,
											  a_args);
			assert(vsprintf_ret >= 0);
			assert(vsprintf_ret == length);

			auto& lenField = GetField_Length(newBuf);
			lenField = length;
			assert(GetLength() >= 0);

			return *this;
		}



		// '\0'을 제외한 캐릭터 수를 리턴
		inline std::size_t GetLength() const asd_NoThrow
		{
			if (m_data == nullptr)
				return 0;

			auto ret = GetField_Length(m_data.get());
			assert(ret >= 0);
			assert(GetField_String(m_data.get())[ret] == '\0');

			return ret;
		}



		// 문자열의 시작 포인터를 리턴
		inline const CharType* GetData() const asd_NoThrow
		{
			CharType* ret;
			if (m_data == nullptr) {
				const static CharType NullChar = '\0';
				ret = (CharType*)&NullChar;
			}
			else {
				ret = GetField_String(m_data.get());
			}

			assert(ret[GetLength()] == '\0');
			assert(ret != nullptr);
			return ret;
		}



		inline operator const CharType*() const asd_NoThrow
		{
			return GetData();
		}



		inline operator const void*() const asd_NoThrow
		{
			return GetData();
		}



		inline operator SupportType_StdString() const asd_NoThrow
		{
			return SupportType_StdString(GetData(), GetLength());
		}



		// STL의 해시 기반 컨테이너에서 사용할 Functor
		struct Hash
		{
			inline std::size_t operator() (IN const CharType* a_src) const asd_NoThrow
			{
				Assert_TypeSizeCheck();
				const int cnt = sizeof(std::size_t) / sizeof(CharType);
				assert(cnt >= 1);

				if (a_src == nullptr)
					return 0;

				const CharType* p = a_src;
				std::size_t ret = 0;
				while (*p != '\0') {
					std::size_t block = 0;
					for (int i=0; i<cnt; ++i) {
						block |= *p;
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



		inline std::size_t GetHash() const asd_NoThrow
		{
			Hash functor;
			return functor(GetData());
		}



		// 현재 문자열 뒤에 인자로 받은 문자열을 적용한다.
		//  a_str  :  추가할 문자열
		//  a_len  :  a_str에서 '\0'을 제외한 원소 개수 (a_len = strlen(a_str))
		inline void Append(IN const CharType* a_str,
						   IN std::size_t a_len) asd_NoThrow
		{
			auto orgLen = GetLength();
			auto addLen = a_len;
			auto newLen = orgLen + addLen;
			if (newLen==orgLen || a_str==nullptr)
				return;

			assert(a_str[0] != 0);
			assert(addLen > 0);
			assert(newLen > 0);
			assert(newLen > orgLen);

			CharType* temp = new CharType[newLen + DataStartOffset + 1];

			if (orgLen > 0) {
				assert(m_data != nullptr);
				std::memcpy(GetField_String(temp),
							GetData(),
							sizeof(CharType)*orgLen);
			}
			std::memcpy(GetField_String(temp) + orgLen,
						a_str, 
						sizeof(CharType)*addLen);

			temp[DataStartOffset + newLen] = '\0';
			auto& len = GetField_Length(temp);
			len = newLen;

			m_data = Buffer_ptr(temp, 
								std::default_delete<CharType[]>());
			assert(GetLength() >= 0);
		}



		// CompareFunction를 사용해서 비교연산자들을 오버로딩하는 매크로.
#define asd_Define_CompareOperator(CompareFunction, TemplateType, IgnoreCase_DefaultVar)		\
																								\
		inline int Compare(IN TemplateType a_rval,												\
						   IN bool a_ignoreCase = IgnoreCase_DefaultVar) const asd_NoThrow		\
		{																						\
			return CompareFunction(GetData(), a_rval, a_ignoreCase);							\
		}																						\
																								\
		inline bool operator == (IN TemplateType a_rval) const asd_NoThrow						\
		{																						\
			return Compare(a_rval) == 0;														\
		}																						\
																								\
		inline bool operator != (IN TemplateType a_rval) const asd_NoThrow						\
		{																						\
			return Compare(a_rval) != 0;														\
		}																						\
																								\
		inline bool operator < (IN TemplateType a_rval) const asd_NoThrow						\
		{																						\
			return Compare(a_rval) < 0;															\
		}																						\
																								\
		inline bool operator <= (IN TemplateType a_rval) const asd_NoThrow						\
		{																						\
			return Compare(a_rval) <= 0;														\
		}																						\
																								\
		inline bool operator > (IN TemplateType a_rval) const asd_NoThrow						\
		{																						\
			return Compare(a_rval) > 0;															\
		}																						\
																								\
		inline bool operator >= (IN TemplateType a_rval) const asd_NoThrow						\
		{																						\
			return Compare(a_rval) >= 0;														\
		}																						\



		// static int Compare() 함수를 정의하면서 다른 비교연산자들까지 오버로딩하는 매크로.
#define asd_Define_CompareFunction(CharTypePointer_LeftVar,										\
								   TemplateType,												\
								   Template_RightVar,											\
								   IgnoreCaseVar,												\
								   IgnoreCase_DefaultVar)										\
																								\
		asd_Define_CompareOperator(Compare, TemplateType, IgnoreCase_DefaultVar)				\
																								\
		inline static int Compare(IN TemplateType a_templateVar,								\
								  IN const CharType* a_stringVar,								\
								  IN bool a_ignoreCase = IgnoreCase_DefaultVar) asd_NoThrow		\
		{																						\
			return Compare(a_stringVar, a_templateVar, a_ignoreCase);							\
		}																						\
																								\
		inline static int Compare(IN const CharType* CharTypePointer_LeftVar,					\
								  IN TemplateType Template_RightVar,							\
								  IN bool IgnoreCaseVar = IgnoreCase_DefaultVar) asd_NoThrow	\



		// 대입연산자(=)를 정의하면서 생성자까지 한번에 정의하는 매크로.
#define asd_Define_AssignmentOperator_Substitute(ArgType, ArgVar)								\
		BasicString(IN ArgType ArgVar) asd_NoThrow												\
		{																						\
			*this = ArgVar;																		\
		}																						\
																								\
		inline ThisType& operator = (IN ArgType ArgVar) asd_NoThrow								\



		// += 연산자를 정의하면서 << 와 + 연산자를 한번에 정의하는 매크로.
#define asd_Define_AssignmentOperator_Append(ArgType, ArgVar)									\
		inline ThisType operator + (IN ArgType ArgVar) const asd_NoThrow						\
		{																						\
			ThisType ret;																		\
			ret << *this << ArgVar;																\
			return ret;																			\
		}																						\
																								\
		inline ThisType& operator << (IN ArgType ArgVar) asd_NoThrow							\
		{																						\
			return *this += ArgVar;																\
		}																						\
																								\
		inline ThisType& operator += (IN ArgType ArgVar) asd_NoThrow							\



		// += 연산자를 기반으로 생성자와 기타 Assignment 연산자들을 한번에 정의하는 매크로.
#define asd_Define_AssignmentOperator(ArgType, ArgVar)											\
		asd_Define_AssignmentOperator_Substitute(ArgType, ArgVar)								\
		{																						\
			m_data = Buffer_ptr();																\
			return *this += ArgVar;																\
		}																						\
																								\
		asd_Define_AssignmentOperator_Append(ArgType, ArgVar)									\



		// asd_Define_AssignmentOperator를 정의하면서 
		// asd_Define_CompareFunction까지 정의하는 매크로.
#define asd_Define_AssignmentOperator_GenCompare(ArgType, ArgVar)								\
		asd_Define_CompareFunction(a_left, ArgType, a_right, a_ignoreCase, IgnoreCase_Default)	\
		{																						\
			return Compare(a_left, ThisType(a_right), a_ignoreCase);							\
		}																						\
																								\
		asd_Define_AssignmentOperator(ArgType, ArgVar)											\



		// CharType 관련.
		//   문자열 비교 함수.
		//   매크로에 의해 오버로딩된 대부분의 Compare함수들은 최종적으로 이것을 호출한다.
		//     return -  :  left <  right
		//     return +  :  left >  right
		//     return 0  :  left == right
		inline static int Compare(IN const CharType* a_left,
								  IN const CharType* a_right,
								  IN bool a_ignoreCase = IgnoreCase_Default) asd_NoThrow
		{
			return asd::strcmp(a_left,
							   a_right,
							   a_ignoreCase);
		}

		asd_Define_CompareOperator(Compare, const CharType*, IgnoreCase_Default)

		asd_Define_AssignmentOperator(const CharType*, a_rval)
		{
			Append(a_rval, asd::strlen(a_rval));
			return *this;
		}



		// ThisType 관련.
		//   ambiguous error를 피하기 위한 오버로딩.
		inline static int Compare(IN const CharType* a_left,
								  IN const ThisType& a_right,
								  IN bool a_ignoreCase = IgnoreCase_Default) asd_NoThrow
		{
			return Compare(a_left,
						   a_right.GetData(),
						   a_ignoreCase);
		}

		asd_Define_CompareOperator(Compare, const ThisType&, IgnoreCase_Default)

		asd_Define_AssignmentOperator_Substitute(REF const ThisType&, a_share)
		{
			// 공유
			m_data = a_share.m_data;
			return *this;
		}

		asd_Define_AssignmentOperator_Append(IN const ThisType&, a_rval)
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
		asd_Define_CompareFunction(a_left,
								   const SupportType_StdString&,
								   a_right,
								   a_ignoreCase,
								   IgnoreCase_Default)
		{
			return Compare(a_left,
						   a_right.data(),
						   a_ignoreCase);
		}

		asd_Define_AssignmentOperator(const SupportType_StdString&, a_rval)
		{
			Append(a_rval.data(), a_rval.length());
			return *this;
		}



		// 정수
		asd_Define_AssignmentOperator_GenCompare(const int32_t, a_rval)
		{
			const CharType format[] = {'%', 'd', 0};
			ThisType temp(format, a_rval);
			return *this += temp;
		}

		asd_Define_AssignmentOperator_GenCompare(const uint32_t, a_rval)
		{
			const CharType format[] = {'%', 'u', 0};
			ThisType temp(format, a_rval);
			return *this += temp;
		}

		asd_Define_AssignmentOperator_GenCompare(const int64_t, a_rval)
		{
			const CharType format[] = {'%', 'l', 'l', 'd', 0};
			ThisType temp(format, a_rval);
			return *this += temp;
		}

		asd_Define_AssignmentOperator_GenCompare(const uint64_t, a_rval)
		{
			const CharType format[] = {'%', 'l', 'l', 'u', 0};
			ThisType temp(format, a_rval);
			return *this += temp;
		}



		// 실수
		asd_Define_AssignmentOperator_GenCompare(const double, a_rval)
		{
			const CharType format[] = {'%', 'l', 'f', 0};
			ThisType temp(format, a_rval);
			return *this += temp;
		}



		// byte
		asd_Define_AssignmentOperator_GenCompare(const uint8_t, a_rval)
		{
			const CharType format[] = {'%', 'x', 0};
			ThisType temp(format, (uint32_t)a_rval);
			return *this += temp;
		}



		// pointer
		asd_Define_AssignmentOperator_GenCompare(const void*, a_rval)
		{
			const CharType format[] = {'%', 'p', 0};
			ThisType temp(format, a_rval);
			return *this += temp;
		}



		// bool
		asd_Define_AssignmentOperator_GenCompare(const bool, a_rval)
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
	};



	// MultiByte String
	typedef BasicString<char>		MString;

	// Wide String
	typedef BasicString<wchar_t>	WString;

	// Current String
#if !defined(UNICODE)
	typedef char		TChar;
	typedef MString		TString;
#else
	typedef wchar_t		TChar;
	typedef WString		TString;
#endif
}



namespace std
{
	template<typename CHARTYPE>
	struct hash<asd::BasicString<CHARTYPE> >
		: public asd::BasicString<CHARTYPE>::Hash
	{
	};
}
