#include "stdafx.h"
#include "asd/iconvwrap.h"
#include "asd/tlsmanager.h"
#include <cwchar>

#if defined(asd_Platform_Windows)
#	include "../../../libiconv-1.14/include/iconv.h"
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

		EncodingInfo(IN const Encoding a_encoding,
					 IN const int a_minSize,
					 IN const float a_avgSize,
					 IN const int a_maxSize,
					 IN const char* a_encodingName)
					 : m_encoding(a_encoding)
					 , m_minSize(a_minSize)
					 , m_avgSize(a_avgSize)
					 , m_maxSize(a_maxSize)
					 , m_encodingName(a_encodingName)
		{
		}
	};



	// enum Encoding의 순서와 맞춰야 함.
	// 프로세스 시작 시 초기화 후 오로지 조회 용도로만 사용.
	const EncodingInfo g_encodingInfo[Encoding_Last] = {
		EncodingInfo(Encoding_UTF8,		1,	2.5,	4,	"UTF-8"),
		EncodingInfo(Encoding_UTF16LE,	2,	2,		4,	"UTF-16LE"),
		EncodingInfo(Encoding_UTF16BE,	2,	2,		4,	"UTF-16BE"),
		EncodingInfo(Encoding_UTF32LE,	4,	4,		4,	"UTF-32LE"),
		EncodingInfo(Encoding_UTF32BE,	4,	4,		4,	"UTF-32BE"),
		EncodingInfo(Encoding_CP949,	1,	1.5,	2,	"CP949")
	};



	inline bool IsValidEncoding(IN Encoding a_enc) asd_NoThrow
	{
		return a_enc >= 0 && a_enc < Encoding_Last;
	}



	bool g_checkedTable = false;
	inline void Check_EncodingInfo_Table()
	{
#ifdef asd_Debug
		if (g_checkedTable == false) {
			for (int i=0; i<Encoding_Last; ++i) {
				assert(g_encodingInfo[i].m_encoding == (Encoding)i);
			}
			g_checkedTable = true;
		}
#endif
	}



	const char* GetEncodingName(IN Encoding a_enc) asd_NoThrow
	{
		Check_EncodingInfo_Table();
		if (IsValidEncoding(a_enc) == false) {
			assert(false);
			return nullptr;
		}
		return g_encodingInfo[a_enc].m_encodingName;
	}



	// 각 컴파일러 별 문자열 리터럴의 기본 인코딩값을 조사하여 기입 할 것.
#if defined(asd_Compiler_MSVC)
	Encoding DefaultEncoding_char = Encoding_CP949;
	Encoding DefaultEncoding_wchar_t = Encoding_UTF16LE;
#elif defined(asd_Compiler_GCC)
	Encoding DefaultEncoding_char = Encoding_UTF8;
	Encoding DefaultEncoding_wchar_t = Encoding_UTF32LE;
#else
	#error This Compiler is not supported.
#endif


	Encoding GetDefaultEncoding_char() asd_NoThrow
	{
		auto ret = DefaultEncoding_char;
		assert(IsValidEncoding(ret));
		return ret;
	}


	Encoding GetDefaultEncoding_wchar_t() asd_NoThrow
	{
		auto ret = DefaultEncoding_wchar_t;
		assert(IsValidEncoding(ret));
		return ret;
	}


	void SetDefaultEncoding_char(IN Encoding a_enc) asd_NoThrow
	{
		DefaultEncoding_char = a_enc;
	}


	void SetDefaultEncoding_wchar_t(IN Encoding a_enc) asd_NoThrow
	{
		DefaultEncoding_wchar_t = a_enc;
	}


	int SizeOfCharUnit_Min(IN Encoding a_enc) asd_NoThrow
	{
		Check_EncodingInfo_Table();
		if (IsValidEncoding(a_enc) == false)
			return -1;

		return g_encodingInfo[a_enc].m_minSize;
	}



	float SizeOfCharUnit_Avg(IN Encoding a_enc) asd_NoThrow
	{
		Check_EncodingInfo_Table();
		if (IsValidEncoding(a_enc) == false)
			return -1;

		return g_encodingInfo[a_enc].m_avgSize;
	}



	int SizeOfCharUnit_Max(IN Encoding a_enc) asd_NoThrow
	{
		Check_EncodingInfo_Table();
		if (IsValidEncoding(a_enc) == false)
			return -1;

		return g_encodingInfo[a_enc].m_maxSize;
	}


		
	inline void Close(INOUT void*& a_icd)
	{
		if (a_icd != ICONV_InvalidDescriptor) {
			iconv_close(a_icd);
			a_icd = ICONV_InvalidDescriptor;
		}
		assert(a_icd == ICONV_InvalidDescriptor);
	}



	IconvWrap::IconvWrap() asd_NoThrow
	{
		m_icd = ICONV_InvalidDescriptor;
	}



	IconvWrap::IconvWrap(MOVE IconvWrap&& a_rval) asd_NoThrow
	{
		*this = std::move(a_rval);
	}



	IconvWrap& IconvWrap::operator = (MOVE IconvWrap&& a_rval) asd_NoThrow
	{
		Close(m_icd);

		m_before = a_rval.m_before;
		m_after = a_rval.m_after;

		m_icd = a_rval.m_icd;
		a_rval.m_icd = ICONV_InvalidDescriptor;

		return *this;
	}



	int IconvWrap::Init(IN Encoding a_before,
						IN Encoding a_after) asd_NoThrow
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
		m_ratio = std::max<float>(std::max<float>(ratio_min, 
												  ratio_avg),
												  ratio_max);
		assert(m_ratio > 0);
		return 0;
	}


	
	int IconvWrap::Convert(IN const char* a_inBuffer,
						   IN size_t a_inBufSize_byte,
						   OUT char* a_outBuffer,
						   INOUT size_t& a_outSize_byte) const asd_NoThrow
	{
		assert(m_icd != ICONV_InvalidDescriptor);
		assert(m_before != Encoding_Last);
		assert(m_after != Encoding_Last);

		int ret;
		const char* inBuf = a_inBuffer;
		size_t inSiz = a_inBufSize_byte;
		char* outBuf = a_outBuffer;
		ret = iconv(m_icd,
					&inBuf,
					&inSiz,
					&outBuf,
					&a_outSize_byte);
		if (ret < 0)
			ret = errno;

		return ret;
	}



	std::shared_ptr<char> IconvWrap::Convert(IN const char* a_inBuffer,
											 IN size_t a_inSize,
											 OUT size_t* a_outSize_byte /*= nullptr*/) const asd_NoThrow
	{
		assert(m_ratio > 0);
		size_t bufsize = a_inSize * m_ratio + 1;
		for(int i=0; i<3; ++i) {
			size_t outsize = bufsize;
			char* buf = new char[bufsize];

			int r = Convert(a_inBuffer, a_inSize, buf, outsize);
			if (r == 0) {
				// 성공
				assert(bufsize - outsize > 0);
				assert(outsize >= 0);
				if (a_outSize_byte != nullptr)
					*a_outSize_byte = bufsize - outsize;
				return std::shared_ptr<char>(buf, std::default_delete<char[]>());
			}

			delete[] buf;
			bufsize *= 2;
		}

		return std::shared_ptr<char>(nullptr);
	}



	IconvWrap::~IconvWrap() asd_NoThrow
	{
		Close(m_icd);
	}



	TLSManager<IconvWrap> g_converterManager[Encoding_Last][Encoding_Last];

	thread_local IconvWrap* t_converterTable[Encoding_Last][Encoding_Last] = {nullptr};

	const IconvWrap& GetConverter(IN Encoding a_srcEncoding,
								  IN Encoding a_dstEncoding) asd_NoThrow
	{
		IconvWrap*& ic = t_converterTable[a_srcEncoding][a_dstEncoding];
		if (ic == nullptr) {
			ic = new IconvWrap();
			bool initSuccess = 0 == ic->Init(a_srcEncoding, a_dstEncoding);
			assert(initSuccess);
			g_converterManager[a_srcEncoding][a_dstEncoding].Register(ic);
		}
		assert(ic != nullptr);
		return *ic;
	}



	inline bool IsWideEncoding(IN Encoding a_enc) asd_NoThrow
	{
		return a_enc == Encoding_UTF32LE
			|| a_enc == Encoding_UTF32BE
			|| a_enc == Encoding_UTF16LE
			|| a_enc == Encoding_UTF16BE;
	}



	template<typename SrcType, typename ResultType>
	inline ResultType StringConvert_Internal(IN const SrcType* a_srcString,
											 IN Encoding a_srcEncoding,
											 IN Encoding a_dstEncoding) asd_NoThrow
	{
		const int sizeOfDstChar = sizeof(typename ResultType::CharType);
#ifdef asd_Debug
		{
			if (a_dstEncoding==Encoding_UTF16LE || a_dstEncoding==Encoding_UTF16BE)
				assert(sizeOfDstChar == 2);
			else if (a_dstEncoding==Encoding_UTF32LE || a_dstEncoding==Encoding_UTF32BE)
				assert(sizeOfDstChar == 4);
			else
				assert(sizeOfDstChar == 1);
		}
#endif

		if (a_srcString == nullptr || a_srcString[0] == '\0')
			return ResultType();

		if (a_srcEncoding == a_dstEncoding) {
			assert(sizeof(SrcType) == sizeOfDstChar);
			return a_srcString;
		}

		auto& ic = GetConverter(a_srcEncoding, a_dstEncoding);

		auto conv_result = ic.Convert((char*)a_srcString,
									  (asd::strlen(a_srcString)+1) * sizeof(SrcType));
		ResultType ret = (typename ResultType::CharType*)conv_result.get();
		return ret;
	}



	MString StringConvertM(IN const wchar_t* a_srcString) asd_NoThrow
	{
		return StringConvert_Internal<wchar_t, MString>(a_srcString,
														GetDefaultEncoding_wchar_t(),
														GetDefaultEncoding_char());
	}



	MString StringConvertM(IN const wchar_t* a_srcString,
						   IN Encoding a_dstEncoding) asd_NoThrow
	{
		if (IsWideEncoding(a_dstEncoding)) {
			asd_PrintStdErr("invalid parameter (a_dstEncoding)");
			return MString();
		}
		return StringConvert_Internal<wchar_t, MString>(a_srcString,
														GetDefaultEncoding_wchar_t(),
														a_dstEncoding);
	}



	MString StringConvertM(IN const char* a_srcString,
						   IN Encoding a_srcEncoding,
						   IN Encoding a_dstEncoding) asd_NoThrow
	{
		if (IsWideEncoding(a_srcEncoding)) {
			asd_PrintStdErr("invalid parameter (a_srcEncoding)");
			return MString();
		}
		if (IsWideEncoding(a_dstEncoding)) {
			asd_PrintStdErr("invalid parameter (a_dstEncoding)");
			return MString();
		}
		return StringConvert_Internal<char, MString>(a_srcString,
													 a_srcEncoding,
													 a_dstEncoding);
	}



	WString StringConvertW(IN const char* a_srcString) asd_NoThrow
	{
		return StringConvert_Internal<char, WString>(a_srcString,
													 GetDefaultEncoding_char(),
													 GetDefaultEncoding_wchar_t());
	}



	WString StringConvertW(IN const char* a_srcString,
						   IN Encoding a_srcEncoding) asd_NoThrow
	{
		if (IsWideEncoding(a_srcEncoding)) {
			asd_PrintStdErr("invalid parameter (a_srcEncoding)");
			return WString();
		}
		return StringConvert_Internal<char, WString>(a_srcString,
													 a_srcEncoding,
													 GetDefaultEncoding_wchar_t());
	}

}
