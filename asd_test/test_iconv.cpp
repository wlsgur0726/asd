﻿#include "stdafx.h"
#include "asd/exception.h"
#include "asd/iconvwrap.h"
#include <unordered_set>
#include <unordered_map>
#include <functional>

namespace asdtest_iconv
{
#define HX(code) ((char)0x ## code)

	// abc 가나다
	const char TestStr_CP949[]   = {HX(61), HX(62), HX(63), HX(20),
									HX(B0), HX(A1), HX(B3), HX(AA), HX(B4), HX(D9), HX(00) };

	const char TestStr_UTF8[]    = {HX(61), HX(62), HX(63), HX(20),
									HX(EA), HX(B0), HX(80), HX(EB), HX(82), HX(98), HX(EB), HX(8B), HX(A4), HX(00) };

	const char TestStr_UTF16LE[] = {HX(61), HX(00), HX(62), HX(00), HX(63), HX(00), HX(20), HX(00),
									HX(00), HX(AC), HX(98), HX(B0), HX(E4), HX(B2), HX(00), HX(00) };

	const char TestStr_UTF16BE[] = {HX(00), HX(61), HX(00), HX(62), HX(00), HX(63), HX(00), HX(20),
									HX(AC), HX(00), HX(B0), HX(98), HX(B2), HX(E4), HX(00), HX(00) };

	const char TestStr_UTF32LE[] = {HX(61), HX(00), HX(00), HX(00), HX(62), HX(00), HX(00), HX(00), HX(63), HX(00), HX(00), HX(00), HX(20), HX(00), HX(00), HX(00),
									HX(00), HX(AC), HX(00), HX(00), HX(98), HX(B0), HX(00), HX(00), HX(E4), HX(B2), HX(00), HX(00), HX(00), HX(00), HX(00), HX(00) };
	
	const char TestStr_UTF32BE[] = {HX(00), HX(00), HX(00), HX(61), HX(00), HX(00), HX(00), HX(62), HX(00), HX(00), HX(00), HX(63), HX(00), HX(00), HX(00), HX(20),
									HX(00), HX(00), HX(AC), HX(00), HX(00), HX(00), HX(B0), HX(98), HX(00), HX(00), HX(B2), HX(E4), HX(00), HX(00), HX(00), HX(00) };


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
		auto descriptionW = asd::ConvToW(description);
		auto descriptionU8 = asd::ConvToM(descriptionW, asd::Encoding::UTF8);
		auto description2 = asd::ConvToM(descriptionU8, asd::Encoding::UTF8, asd::GetDefaultEncoding<char>());
		ASSERT_TRUE(asd::MString(description) == description2);

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
			
			// 생성 및 초기화
			asd::IconvWrap icv;
			int r1 = icv.Init(encodig_enum1, encodig_enum2);
			EXPECT_EQ(r1, 0);
			if (r1 < 0)
				return false;

			// 변환
			char outbuf[BufferSize] = { HX(FF) };
			size_t outsize = BufferSize;
			size_t r2 = icv.Convert(src1.buffer,
									src1.size,
									outbuf,
									outsize);
			// 결과 확인 1
			EXPECT_NE(r2, asd_IconvWrap_ConvertError);
			if (r2 == asd_IconvWrap_ConvertError) {
				int e = errno;
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
				bool check = goal.Equal(r.data());
				EXPECT_TRUE(check);
				ret &= check;
			}

			// M to W
			if (srcUnitSize==1 && encodig_enum2==defaultEnc_W) {
				asd::WString r = asd::ConvToW(src1.buffer,
											  encodig_enum1);
				bool check = goal.Equal((const char*)r.data());
				EXPECT_TRUE(check);
				ret &= check;
			}

			// M to W (기본인자)
			if (encodig_enum1==defaultEnc_M && encodig_enum2==defaultEnc_W) {
				asd::WString r = asd::ConvToW(src1.buffer);
				bool check = goal.Equal((const char*)r.data());
				EXPECT_TRUE(check);
				ret &= check;
			}

			// W to M
			if (encodig_enum1==defaultEnc_W && dstUnitSize==1) {
				asd::MString r = asd::ConvToM((const wchar_t*)src1.buffer,
											  encodig_enum2);
				bool check = goal.Equal(r.data());
				EXPECT_TRUE(check);
				ret &= check;
			}

			// W to M (기본인자)
			if (encodig_enum1==defaultEnc_W && encodig_enum2==defaultEnc_M) {
				asd::MString r = asd::ConvToM((const wchar_t*)src1.buffer);
				bool check = goal.Equal(r.data());
				EXPECT_TRUE(check);
				ret &= check;
			}

			return ret;
		});
	}
}
