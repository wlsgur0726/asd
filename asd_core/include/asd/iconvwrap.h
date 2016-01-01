﻿#pragma once
#include "asd/asdbase.h"
#include "asd/string.h"

namespace asd
{
	enum Encoding {
		Encoding_UTF8 = 0,
		Encoding_UTF16LE,
		Encoding_UTF16BE,
		Encoding_UTF32LE,
		Encoding_UTF32BE,
		Encoding_CP949,
		Encoding_Last
	};



	// char 문자열의 기본 인코딩값을 얻어온다.
	Encoding GetDefaultEncoding(IN const char*) noexcept;

	// wchar_t 문자열의 기본 인코딩값을 얻어온다.
	Encoding GetDefaultEncoding(IN const wchar_t*) noexcept;

	// char16_t 문자열의 기본 인코딩값을 얻어온다.
	Encoding GetDefaultEncoding(IN const char16_t*) noexcept;

	// char32_t 문자열의 기본 인코딩값을 얻어온다.
	Encoding GetDefaultEncoding(IN const char32_t*) noexcept;

	// CharType 문자열의 기본 인코딩값을 얻어온다.
	template<typename CharType>
	Encoding GetDefaultEncoding() noexcept
	{
		CharType t;
		return GetDefaultEncoding(&t);
	}



	// char 문자열의 기본 인코딩값을 지정한다.
	void SetDefaultEncoding(IN Encoding a_enc,
							IN const char*) noexcept;

	// wchar_t 문자열의 기본 인코딩값을 지정한다.
	void SetDefaultEncoding(IN Encoding a_enc,
							IN const wchar_t*) noexcept;

	// CharType 문자열의 기본 인코딩값을 지정한다.
	template<typename CharType>
	Encoding GetDefaultEncoding(IN Encoding a_enc) noexcept
	{
		CharType t;
		return SetDefaultEncoding(a_enc, &t);
	}



	// 인자로 받은 인코딩에서 문자 1개의 최소 크기를 리턴
	int SizeOfCharUnit_Min(IN Encoding a_enc) noexcept;


	// 인자로 받은 인코딩에서 문자 1개의 평균 크기를 리턴
	float SizeOfCharUnit_Avg(IN Encoding a_enc) noexcept;


	// 인자로 받은 인코딩에서 문자 1개의 최대 크기를 리턴
	int SizeOfCharUnit_Max(IN Encoding a_enc) noexcept;


	// Encoding enum값으로 ICONV 초기화에 사용할 문자열을 리턴
	const char* GetEncodingName(IN Encoding a_enc) noexcept;



	class IconvWrap final
	{
		void* m_icd;
		Encoding m_before = Encoding_Last;
		Encoding m_after = Encoding_Last;
		float m_ratio = 2;
		
		IconvWrap(IN const IconvWrap&) = delete;
		IconvWrap& operator = (IN const IconvWrap&) = delete;


	public:
		IconvWrap() noexcept;

		IconvWrap(MOVE IconvWrap&& a_rval) noexcept;

		IconvWrap& operator = (MOVE IconvWrap&& a_rval) noexcept;


		// 성공  :  0 리턴,
		// 실패  :  errno 리턴
		int Init(IN Encoding a_before,
				 IN Encoding a_after) noexcept;


		// 리턴 직후의 a_outSize_byte = (호출 당시 입력한 a_outSize_byte) - (a_outBuffer에 write한 byte 수 ('\0' 포함))
		// 리턴값이 음수이면 실패 (errno 확인요망)
		size_t Convert(IN const void* a_inBuffer,
					   IN size_t a_inBufSize_byte,
					   OUT void* a_outBuffer,
					   INOUT size_t& a_outSize_byte) const noexcept;


		// a_outSize_byte  :  변환 후 결과물의 배열 길이 리턴
		// 리턴값 : 변환한 결과물. 실패 시 nullptr.
		std::shared_ptr<char> Convert(IN const void* a_inBuffer,
									  IN size_t a_inSize,
									  OUT size_t* a_outSize_byte = nullptr) const noexcept;


		~IconvWrap() noexcept;

	};


	const IconvWrap& GetConverter(IN Encoding a_srcEncoding,
								  IN Encoding a_dstEncoding) noexcept;


	// To Multi-Byte
	MString ConvToM(IN const void* a_srcString,
					IN Encoding a_srcEncoding,
					IN Encoding a_dstEncoding) noexcept;

	MString ConvToM(IN const wchar_t* a_srcString) noexcept;

	MString ConvToM(IN const wchar_t* a_srcString,
					IN Encoding a_dstEncoding) noexcept;

	MString ConvToM(IN const char16_t* a_srcString) noexcept;

	MString ConvToM(IN const char16_t* a_srcString,
					IN Encoding a_dstEncoding) noexcept;

	MString ConvToM(IN const char32_t* a_srcString) noexcept;

	MString ConvToM(IN const char32_t* a_srcString,
					IN Encoding a_dstEncoding) noexcept;


	// To Wide
	WString ConvToW(IN const void* a_srcString,
					IN Encoding a_srcEncoding) noexcept;

	WString ConvToW(IN const char* a_srcString) noexcept;

	WString ConvToW(IN const char16_t* a_srcString) noexcept;

	WString ConvToW(IN const char32_t* a_srcString) noexcept;



#if defined(UNICODE)
#	define ConvToT ConvToW
#
#else
#	define ConvToT ConvToM
#
#endif
}