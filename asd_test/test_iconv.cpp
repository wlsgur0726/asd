#include "stdafx.h"
#include "asd/exception.h"
#include "asd/iconvwrap.h"
#include <unordered_set>
#include <unordered_map>
#include <functional>

namespace asdtest_iconv
{
	// abc 가나다
	const char TestStr_CP949[]   = {0x61, 0x62, 0x63, 0x20, 
									0xB0, 0xA1, 0xB3, 0xAA, 0xB4, 0xD9, 0x00};

	const char TestStr_UTF8[]    = {0x61, 0x62, 0x63, 0x20, 
									0xEA, 0xB0, 0x80, 0xEB, 0x82, 0x98, 0xEB, 0x8B, 0xA4, 0x00};

	const char TestStr_UTF16LE[] = {0x61, 0x00, 0x62, 0x00, 0x63, 0x00, 0x20, 0x00,
									0x00, 0xAC, 0x98, 0xB0, 0xE4, 0xB2, 0x00, 0x00};

	const char TestStr_UTF16BE[] = {0x00, 0x61, 0x00, 0x62, 0x00, 0x63, 0x00, 0x20,
									0xAC, 0x00, 0xB0, 0x98, 0xB2, 0xE4, 0x00, 0x00};

	const char TestStr_UTF32LE[] = {0x61, 0x00, 0x00, 0x00, 0x62, 0x00, 0x00, 0x00, 0x63, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
									0x00, 0xAC, 0x00, 0x00, 0x98, 0xB0, 0x00, 0x00, 0xE4, 0xB2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	
	const char TestStr_UTF32BE[] = {0x00, 0x00, 0x00, 0x61, 0x00, 0x00, 0x00, 0x62, 0x00, 0x00, 0x00, 0x63, 0x00, 0x00, 0x00, 0x20, 
									0x00, 0x00, 0xAC, 0x00, 0x00, 0x00, 0xB0, 0x98, 0x00, 0x00, 0xB2, 0xE4, 0x00, 0x00, 0x00, 0x00};


	struct TestSource
	{
		const char* buffer;
		const int size;
		TestSource(const char* buf, int sz)
			: buffer(buf), size(sz)
		{
			if (buf == nullptr || sz <= 0 )
				asd_RaiseException("invalid parameter");
		}
		
		bool Equal(const char* buf)
		{
			for (int i=0; i<size; ++i) {
				if (buffer[i] != buf[i])
					return false;
			}
			return true;
		}
	};


	void PrintBuffer(const char* buf, int sz)
	{
		asd::MString print;
		for (int i=0; i<sz; ++i) {
			uint8_t* b = (uint8_t*)buf+i;
			print << " " << *b;
		}
		asd::puts(print);
	}


	std::unordered_map<asd::Encoding, TestSource> g_TestStringMap;
	void Init()
	{
		g_TestStringMap.clear();

		g_TestStringMap.emplace(asd::Encoding::CP949, 
								TestSource(TestStr_CP949, sizeof(TestStr_CP949)));

		g_TestStringMap.emplace(asd::Encoding::UTF8, 
								TestSource(TestStr_UTF8, sizeof(TestStr_UTF8)));

		g_TestStringMap.emplace(asd::Encoding::UTF16LE, 
								TestSource(TestStr_UTF16LE, sizeof(TestStr_UTF16LE)));

		g_TestStringMap.emplace(asd::Encoding::UTF16BE,
								TestSource(TestStr_UTF16BE, sizeof(TestStr_UTF16BE)));

		g_TestStringMap.emplace(asd::Encoding::UTF32LE, 
								TestSource(TestStr_UTF32LE, sizeof(TestStr_UTF32LE)));

		g_TestStringMap.emplace(asd::Encoding::UTF32BE,
								TestSource(TestStr_UTF32BE, sizeof(TestStr_UTF32BE)));
	}



	void TestLoop(const char* description,
				  std::function< bool(asd::Encoding, const char*, TestSource,
									  asd::Encoding, const char*, TestSource) > testRoutine )
	{
		Init();
		for (auto it1 : g_TestStringMap) {
			for (auto it2 : g_TestStringMap) {
				auto encodig_enum1 = it1.first;
				auto encodig_str1 = asd::GetEncodingName(encodig_enum1);
				auto encodig_enum2 = it2.first;
				auto encodig_str2 = asd::GetEncodingName(encodig_enum2);
				ASSERT_NE(encodig_str1, nullptr);
				ASSERT_NE(encodig_str2, nullptr);

				// it1 -> it2
				bool success = testRoutine(encodig_enum1, encodig_str1, it1.second,
											encodig_enum2, encodig_str2, it2.second);
				EXPECT_TRUE(success);
				if (success == false) {
					printf("Fail - %s\n", description);
					printf(" it1  : %s\n", encodig_str1);
					printf(" it2  : %s\n", encodig_str2);
				}
			}
		}
	}



	TEST(IconvWrap, IconvWrap_Case1)
	{
		TestLoop("Iconv객체를 직접 생성 및 초기화하여 사용하는 예제",
				 [](asd::Encoding encodig_enum1, const char* encodig_str1, TestSource src1,
					asd::Encoding encodig_enum2, const char* encodig_str2, TestSource src2) -> bool
		{
			const int BufferSize = 512;
			int ret;
			
			// 생성 및 초기화
			asd::IconvWrap icv;
			ret = icv.Init(encodig_enum1, encodig_enum2);
			EXPECT_GE(ret, 0);
			if (ret < 0)
				return false;

			// 변환
			char outbuf[BufferSize] ={0xFF};
			size_t outsize = BufferSize;
			ret = icv.Convert(src1.buffer,
							  src1.size,
							  outbuf,
							  outsize);
			// 결과 확인 1
			auto e = errno;
			EXPECT_GE(ret, 0);
			if (ret < 0) {
				printf("ret : %d\n", ret);
				printf("errno : %d\n", e);
				return false;
			}

			// 결과 확인 2
			bool check_data = src2.Equal(outbuf);
			bool check_outsize = src2.size == BufferSize - outsize;
			EXPECT_TRUE(check_data);
			EXPECT_TRUE(check_outsize);

			return check_data && check_outsize;
		});
	}



	TEST(IconvWrap, IconvWrap_Case2)
	{
		TestLoop("전역함수로 Iconv객체를 얻어 사용하는 예제",
				 [](asd::Encoding encodig_enum1, const char* encodig_str1, TestSource src1,
					asd::Encoding encodig_enum2, const char* encodig_str2, TestSource src2) -> bool
		{
			// ICONV 객체를 얻어온다
			auto& icv = asd::GetConverter(encodig_enum1, encodig_enum2);

			// 변환
			size_t outsize;
			auto ret = icv.Convert(src1.buffer,
								   src1.size,
								   &outsize);

			// 결과 확인 1
			auto e = errno;
			EXPECT_NE(ret.get(), nullptr);
			if (ret == nullptr) {
				printf("errno : %d\n", e);
				return false;
			}

			// 결과 확인 2
			bool check_data = src2.Equal(ret.get());
			bool check_outsize = src2.size == outsize;
			EXPECT_TRUE(check_data);
			EXPECT_TRUE(check_outsize);

			return check_data && check_outsize;
		});
	}



	TEST(IconvWrap, IconvWrap_Case3)
	{
		TestLoop("StringConvert 함수들 테스트",
				 [](asd::Encoding encodig_enum1, const char* encodig_str1, TestSource src1,
					asd::Encoding encodig_enum2, const char* encodig_str2, TestSource src2) -> bool
		{
			asd::Encoding defaultEnc_M = asd::GetDefaultEncoding<char>();
			asd::Encoding defaultEnc_W = asd::GetDefaultEncoding<wchar_t>();
			EXPECT_NE(defaultEnc_M, asd::Encoding::Last);
			EXPECT_NE(defaultEnc_W, asd::Encoding::Last);
			if (defaultEnc_M == asd::Encoding::Last || defaultEnc_W == asd::Encoding::Last)
				return false;

			int srcUnitSize = asd::SizeOfCharUnit_Min(encodig_enum1);
			int dstUnitSize = asd::SizeOfCharUnit_Min(encodig_enum2);
			EXPECT_GT(srcUnitSize, 0);
			EXPECT_GT(dstUnitSize, 0);
			if (srcUnitSize <= 0 || dstUnitSize <= 0)
				return false;

			auto& goal = g_TestStringMap.find(encodig_enum2)->second;
			bool ret = true;

			// M to M
			if (srcUnitSize==1 && dstUnitSize==1) {
				asd::MString r = asd::ConvToM(src1.buffer,
											  encodig_enum1,
											  encodig_enum2);
				bool check = goal.Equal(r.GetData());
				EXPECT_TRUE(check);
				ret &= check;
			}

			// M to W
			if (srcUnitSize==1 && encodig_enum2==defaultEnc_W) {
				asd::WString r = asd::ConvToW(src1.buffer,
											  encodig_enum1);
				bool check = goal.Equal((const char*)r.GetData());
				EXPECT_TRUE(check);
				ret &= check;
			}

			// M to W (기본인자)
			if (encodig_enum1==defaultEnc_M && encodig_enum2==defaultEnc_W) {
				asd::WString r = asd::ConvToW(src1.buffer);
				bool check = goal.Equal((const char*)r.GetData());
				EXPECT_TRUE(check);
				ret &= check;
			}

			// W to M
			if (encodig_enum1==defaultEnc_W && dstUnitSize==1) {
				asd::MString r = asd::ConvToM((const wchar_t*)src1.buffer,
											  encodig_enum2);
				bool check = goal.Equal(r.GetData());
				EXPECT_TRUE(check);
				ret &= check;
			}

			// W to M (기본인자)
			if (encodig_enum1==defaultEnc_W && encodig_enum2==defaultEnc_M) {
				asd::MString r = asd::ConvToM((const wchar_t*)src1.buffer);
				bool check = goal.Equal(r.GetData());
				EXPECT_TRUE(check);
				ret &= check;
			}

			return ret;
		});
	}
}
