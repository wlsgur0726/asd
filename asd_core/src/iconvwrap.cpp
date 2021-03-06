﻿#include "stdafx.h"
#include "asd/iconvwrap.h"
#include "asd/util.h"
#include <cwchar>

#if defined(asd_Platform_Windows)
#	include "../../../libiconv/include/iconv.h"
#
#else
#	// iconv가 기본적으로 설치되어 있는 플랫폼
#	include <iconv.h>
#
#endif



#define ICONV_InvalidDescriptor ((void*)-1)
namespace asd
{
	struct EncodingInfo
	{
		const Encoding m_encoding;
		const int m_minSize;
		const float m_avgSize;
		const int m_maxSize;
		const char* m_encodingName;

		EncodingInfo(const Encoding a_encoding,
					 const int a_minSize,
					 const float a_avgSize,
					 const int a_maxSize,
					 const char* a_encodingName)
					 : m_encoding(a_encoding)
					 , m_minSize(a_minSize)
					 , m_avgSize(a_avgSize)
					 , m_maxSize(a_maxSize)
					 , m_encodingName(a_encodingName)
		{
			asd_DAssert(a_minSize > 0);
			asd_DAssert(a_avgSize > 0);
			asd_DAssert(a_maxSize > 0);
		}
	};

	// enum Encoding의 순서와 맞춰야 함.
	// 프로세스 시작 시 초기화 후 오로지 조회 용도로만 사용.
	const EncodingInfo& GetEncodingInfo(Encoding a_enc)
	{
		static const EncodingInfo g_encodingInfo[(int)Encoding::Last] = {
			//              Enum             MinSize   AvgSize   MaxSize    Name
			EncodingInfo(Encoding::UTF8,       1,        2.5,      4,      "UTF-8"),
			EncodingInfo(Encoding::UTF16LE,    2,        2,        4,      "UTF-16LE"),
			EncodingInfo(Encoding::UTF16BE,    2,        2,        4,      "UTF-16BE"),
			EncodingInfo(Encoding::UTF32LE,    4,        4,        4,      "UTF-32LE"),
			EncodingInfo(Encoding::UTF32BE,    4,        4,        4,      "UTF-32BE"),
			EncodingInfo(Encoding::CP949,      1,        1.5,      2,      "CP949")
		};
		return g_encodingInfo[(int)a_enc];
	}



	inline bool IsValidEncoding(Encoding a_enc)
	{
		return (int)a_enc >= 0 && a_enc < Encoding::Last;
	}



	// GetEncodingInfo가 제대로 초기화 되었는지 확인하는 함수
	inline void Check_EncodingInfo_Table()
	{
#ifdef asd_Debug
		static bool g_checkedTable = false;
		if (g_checkedTable == false) {
			for (int i=0; i<(int)Encoding::Last; ++i) {
				asd_RAssert(GetEncodingInfo((Encoding)i).m_encoding == (Encoding)i, "encoding table error");
			}
			g_checkedTable = true;
		}
#endif
	}



	const char* GetEncodingName(Encoding a_enc)
	{
		Check_EncodingInfo_Table();
		if (IsValidEncoding(a_enc) == false) {
			asd_DAssert(false);
			return nullptr;
		}
		return GetEncodingInfo(a_enc).m_encodingName;
	}



	// 각 컴파일러 별 문자열 리터럴의 기본 인코딩값을 조사하여 기입 할 것.
	struct DefaultEncoding : public Global<DefaultEncoding>
	{
#define asd_Define_DefaultEncoding(CharType, DefaultEncoding) \
		Encoding m_ ## CharType = DefaultEncoding

#if defined(asd_Compiler_MSVC)
		asd_Define_DefaultEncoding(char,		Encoding::CP949);
		asd_Define_DefaultEncoding(wchar_t,		Encoding::UTF16LE);
		asd_Define_DefaultEncoding(char16_t,	Encoding::UTF16LE);
		asd_Define_DefaultEncoding(char32_t,	Encoding::UTF32LE);

#elif defined(asd_Compiler_GCC)
		asd_Define_DefaultEncoding(char,		Encoding::UTF8);
		asd_Define_DefaultEncoding(wchar_t,		Encoding::UTF32LE);
		asd_Define_DefaultEncoding(char16_t,	Encoding::UTF16LE);
		asd_Define_DefaultEncoding(char32_t,	Encoding::UTF32LE);

#else
		#error This compiler is not supported.

#endif
	};



#define asd_Define_GetDefaultEncoding(CharType)								\
	Encoding GetDefaultEncoding(const CharType*)							\
	{																		\
		Encoding ret = DefaultEncoding::GlobalInstance().m_ ## CharType;	\
		asd_DAssert(IsValidEncoding(ret));									\
		return ret;															\
	}																		\

	asd_Define_GetDefaultEncoding(char)

	asd_Define_GetDefaultEncoding(wchar_t)

	asd_Define_GetDefaultEncoding(char16_t)

	asd_Define_GetDefaultEncoding(char32_t)



#define asd_Define_SetDefaultEncoding(CharType)								\
	void SetDefaultEncoding(Encoding a_enc,								\
							const CharType*)								\
	{																		\
		asd_DAssert(IsValidEncoding(a_enc));								\
		DefaultEncoding::GlobalInstance().m_ ## CharType = a_enc;			\
	}																		\

	asd_Define_SetDefaultEncoding(char)

	asd_Define_SetDefaultEncoding(wchar_t)



	int SizeOfCharUnit_Min(Encoding a_enc)
	{
		Check_EncodingInfo_Table();
		if (IsValidEncoding(a_enc) == false)
			return -1;

		return GetEncodingInfo(a_enc).m_minSize;
	}



	float SizeOfCharUnit_Avg(Encoding a_enc)
	{
		Check_EncodingInfo_Table();
		if (IsValidEncoding(a_enc) == false)
			return -1;

		return GetEncodingInfo(a_enc).m_avgSize;
	}



	int SizeOfCharUnit_Max(Encoding a_enc)
	{
		Check_EncodingInfo_Table();
		if (IsValidEncoding(a_enc) == false)
			return -1;

		return GetEncodingInfo(a_enc).m_maxSize;
	}



	inline void Close(void*& a_icd /*InOut*/)
	{
		if (a_icd != ICONV_InvalidDescriptor) {
			iconv_close(a_icd);
			a_icd = ICONV_InvalidDescriptor;
		}
		asd_DAssert(a_icd == ICONV_InvalidDescriptor);
	}



	IconvWrap::IconvWrap()
	{
		m_icd = ICONV_InvalidDescriptor;
	}



	IconvWrap::IconvWrap(IconvWrap&& a_rval)
	{
		*this = std::move(a_rval);
	}



	IconvWrap& IconvWrap::operator = (IconvWrap&& a_rval)
	{
		Close(m_icd);

		m_before = a_rval.m_before;
		m_after = a_rval.m_after;

		m_icd = a_rval.m_icd;
		a_rval.m_icd = ICONV_InvalidDescriptor;

		return *this;
	}



	int IconvWrap::Init(Encoding a_before,
						Encoding a_after)
	{
		Close(m_icd);

		m_icd = iconv_open(GetEncodingName(a_after), 
						   GetEncodingName(a_before));
		if (m_icd == ICONV_InvalidDescriptor) {
			int err = errno;
			return err;
		}

		m_before = a_before;
		m_after = a_after;
		float ratio_min = (float)SizeOfCharUnit_Min(m_after) / SizeOfCharUnit_Min(m_before);
		float ratio_avg = (float)SizeOfCharUnit_Avg(m_after) / SizeOfCharUnit_Avg(m_before);
		float ratio_max = (float)SizeOfCharUnit_Max(m_after) / SizeOfCharUnit_Max(m_before);
		m_ratio = asd::max(ratio_min, ratio_avg, ratio_max);
		asd_DAssert(m_ratio > 0);
		return 0;
	}


	
	size_t IconvWrap::Convert(const void* a_inBuffer,
							  size_t a_inBufSize_byte,
							  void* a_outBuffer /*Out*/,
							  size_t& a_outSize_byte /*InOut*/) const
	{
		asd_DAssert(m_icd != ICONV_InvalidDescriptor);
		asd_DAssert(m_before != Encoding::Last);
		asd_DAssert(m_after != Encoding::Last);

#if asd_Platform_Windows
		const char* inBuf = (const char*)a_inBuffer;
#else
		char* inBuf = (char*)a_inBuffer;
#endif
		size_t inSiz = a_inBufSize_byte;
		char* outBuf = (char*)a_outBuffer;
		auto ret = iconv(m_icd,
						 &inBuf,
						 &inSiz,
						 &outBuf,
						 &a_outSize_byte);
		return ret;
	}



	std::shared_ptr<char> IconvWrap::Convert(const void* a_inBuffer,
											 size_t a_inSize,
											 size_t* a_outSize_byte /*= nullptr*/ /*Out*/) const
	{
		asd_DAssert(m_ratio > 0);
		size_t bufsize = (size_t)(a_inSize * m_ratio + 1);
		for(int i=0; i<3; ++i) {
			size_t outsize = bufsize;
			char* buf = new char[bufsize];

			if (Convert(a_inBuffer, a_inSize, buf, outsize) != asd_IconvWrap_ConvertError) {
				// 성공
				asd_DAssert(bufsize - outsize > 0);
				asd_DAssert(outsize >= 0);
				if (a_outSize_byte != nullptr)
					*a_outSize_byte = bufsize - outsize;
				return std::shared_ptr<char>(buf, std::default_delete<char[]>());
			}

			delete[] buf;
			bufsize *= 2;
		}

		asd_OnErr("fail Convert, before:{}, after:{}", GetEncodingName(m_before), GetEncodingName(m_after));
		return std::shared_ptr<char>(nullptr);
	}



	IconvWrap::~IconvWrap()
	{
		Close(m_icd);
	}



	const IconvWrap& GetConverter(Encoding a_srcEncoding,
								  Encoding a_dstEncoding)
	{
		typedef std::unique_ptr<IconvWrap> IconvWrap_ptr;
		thread_local IconvWrap_ptr t_converterTable[(int)Encoding::Last][(int)Encoding::Last];

		IconvWrap_ptr& ic = t_converterTable[(int)a_srcEncoding][(int)a_dstEncoding];
		if (ic == nullptr) {
			ic = IconvWrap_ptr(new IconvWrap);
			bool initSuccess = (0 == ic->Init(a_srcEncoding, a_dstEncoding));
			asd_DAssert(initSuccess);
		}
		asd_DAssert(ic != nullptr);
		return *ic;
	}



	inline bool IsWideEncoding(Encoding a_enc)
	{
		return a_enc == GetDefaultEncoding<wchar_t>();
	}



	template<typename ResultType>
	inline ResultType ConvTo_Internal(const void* a_srcString,
									  Encoding a_srcEncoding,
									  Encoding a_dstEncoding)
	{
		if (a_srcString == nullptr)
			return ResultType();

		if (a_srcEncoding == a_dstEncoding) {
			return (typename ResultType::CharType*)a_srcString;
		}

		int cu = SizeOfCharUnit_Min(a_srcEncoding);
		size_t len = asd::strlen(a_srcString, cu);
		if (len == 0)
			return ResultType();

		auto& ic = GetConverter(a_srcEncoding, a_dstEncoding);
		auto conv_result = ic.Convert(a_srcString,
									  (len + 1) * cu);
		ResultType ret = (typename ResultType::CharType*)conv_result.get();
		return ret;
	}



	// To Multi-Byte
#define asd_Define_ConvToM(CharType)														\
	MString ConvToM(const CharType* a_srcString)											\
	{																						\
		return ConvTo_Internal<MString>(a_srcString,										\
										GetDefaultEncoding<CharType>(),						\
										GetDefaultEncoding<char>());						\
	}																						\
																							\
	MString ConvToM(const CharType* a_srcString,											\
					Encoding a_dstEncoding)												\
	{																						\
		if (IsWideEncoding(a_dstEncoding)) {												\
			asd_PrintStdErr("invalid parameter (a_dstEncoding)");							\
			return MString();																\
		}																					\
		return ConvTo_Internal<MString>(a_srcString,										\
										GetDefaultEncoding<CharType>(),						\
										a_dstEncoding);										\
	}																						\


	asd_Define_ConvToM(wchar_t)

	asd_Define_ConvToM(char16_t)

	asd_Define_ConvToM(char32_t)

	MString ConvToM(const void* a_srcString,
					Encoding a_srcEncoding,
					Encoding a_dstEncoding)
	{
		if (IsWideEncoding(a_dstEncoding)) {
			asd_PrintStdErr("invalid parameter (a_dstEncoding)");
			return MString();
		}
		return ConvTo_Internal<MString>((const char*)a_srcString,
										a_srcEncoding,
										a_dstEncoding);
	}



	// To Wide
#define asd_Define_ConvToX(X, ReturnType, SrcCharType)										\
	ReturnType ConvTo ## X (const SrcCharType* a_srcString)								\
	{																						\
		return ConvTo_Internal<ReturnType>(a_srcString,										\
										   GetDefaultEncoding<SrcCharType>(),				\
										   GetDefaultEncoding<ReturnType::CharType>());		\
	}																						\

	asd_Define_ConvToX(W, WString, char)

	asd_Define_ConvToX(W, WString, char16_t)

	asd_Define_ConvToX(W, WString, char32_t)

	WString ConvToW(const void* a_srcString,
					Encoding a_srcEncoding)
	{
		return ConvTo_Internal<WString>((const char*)a_srcString,
										a_srcEncoding,
										GetDefaultEncoding<WString::CharType>());
	}
}
