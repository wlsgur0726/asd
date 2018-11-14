#pragma once
#include "asdbase.h"
#include "string.h"

namespace asd
{
	enum class Encoding : int 
	{
		UTF8 = 0,
		UTF16LE,
		UTF16BE,
		UTF32LE,
		UTF32BE,
		CP949,
		Last
	};



	// char 문자열의 기본 인코딩값을 얻어온다.
	Encoding GetDefaultEncoding(const char*);

	// wchar_t 문자열의 기본 인코딩값을 얻어온다.
	Encoding GetDefaultEncoding(const wchar_t*);

	// char16_t 문자열의 기본 인코딩값을 얻어온다.
	Encoding GetDefaultEncoding(const char16_t*);

	// char32_t 문자열의 기본 인코딩값을 얻어온다.
	Encoding GetDefaultEncoding(const char32_t*);

	// CharType 문자열의 기본 인코딩값을 얻어온다.
	template<typename CharType>
	Encoding GetDefaultEncoding()
	{
		CharType t;
		return GetDefaultEncoding(&t);
	}



	// char 문자열의 기본 인코딩값을 지정한다.
	void SetDefaultEncoding(Encoding a_enc,
							const char*);

	// wchar_t 문자열의 기본 인코딩값을 지정한다.
	void SetDefaultEncoding(Encoding a_enc,
							const wchar_t*);

	// CharType 문자열의 기본 인코딩값을 지정한다.
	template<typename CharType>
	Encoding GetDefaultEncoding(Encoding a_enc)
	{
		CharType t;
		return SetDefaultEncoding(a_enc, &t);
	}



	// 인자로 받은 인코딩에서 문자 1개의 최소 크기를 리턴
	int SizeOfCharUnit_Min(Encoding a_enc);


	// 인자로 받은 인코딩에서 문자 1개의 평균 크기를 리턴
	float SizeOfCharUnit_Avg(Encoding a_enc);


	// 인자로 받은 인코딩에서 문자 1개의 최대 크기를 리턴
	int SizeOfCharUnit_Max(Encoding a_enc);


	// Encoding enum값으로 ICONV 초기화에 사용할 문자열을 리턴
	const char* GetEncodingName(Encoding a_enc);



	class IconvWrap final
	{
		void* m_icd;
		Encoding m_before = Encoding::Last;
		Encoding m_after = Encoding::Last;
		float m_ratio = 2;
		
		IconvWrap(const IconvWrap&) = delete;
		IconvWrap& operator = (const IconvWrap&) = delete;


	public:
		IconvWrap();

		IconvWrap(IconvWrap&& a_rval);

		IconvWrap& operator = (IconvWrap&& a_rval);


		// 성공  :  0 리턴,
		// 실패  :  errno 리턴
		int Init(Encoding a_before,
				 Encoding a_after);


		// 리턴 직후의 a_outSize_byte = (호출 당시 입력한 a_outSize_byte) - (a_outBuffer에 write한 byte 수 ('\0' 포함))
		// 리턴값이 asd_IconvWrap_ConvertError 이면 실패 (errno 확인요망)
		#define asd_IconvWrap_ConvertError ((size_t)-1)
		size_t Convert(const void* a_inBuffer,
					   size_t a_inBufSize_byte,
					   void* a_outBuffer  /*Out*/,
					   size_t& a_outSize_byte /*InOut*/) const;


		// a_outSize_byte  :  변환 후 결과물의 배열 길이 리턴
		// 리턴값 : 변환한 결과물. 실패 시 nullptr.
		std::shared_ptr<char> Convert(const void* a_inBuffer,
									  size_t a_inSize,
									  size_t* a_outSize_byte = nullptr /*Out*/) const;


		~IconvWrap();

	};


	const IconvWrap& GetConverter(Encoding a_srcEncoding,
								  Encoding a_dstEncoding);


	// To Multi-Byte
	MString ConvToM(const void* a_srcString,
					Encoding a_srcEncoding,
					Encoding a_dstEncoding);

	MString ConvToM(const wchar_t* a_srcString);

	MString ConvToM(const wchar_t* a_srcString,
					Encoding a_dstEncoding);

	MString ConvToM(const char16_t* a_srcString);

	MString ConvToM(const char16_t* a_srcString,
					Encoding a_dstEncoding);

	MString ConvToM(const char32_t* a_srcString);

	MString ConvToM(const char32_t* a_srcString,
					Encoding a_dstEncoding);


	// To Wide
	WString ConvToW(const void* a_srcString,
					Encoding a_srcEncoding);

	WString ConvToW(const char* a_srcString);

	WString ConvToW(const char16_t* a_srcString);

	WString ConvToW(const char32_t* a_srcString);



#if defined(UNICODE)
#	define ConvToT ConvToW
#
#else
#	define ConvToT ConvToM
#
#endif
}



namespace std
{
	template<>
	struct hash<asd::Encoding>
	{
		inline size_t operator()(asd::Encoding a_enc) const
		{
			return hash<int>()((int)a_enc);
		}
	};
}
