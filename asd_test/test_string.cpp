#include "stdafx.h"
#include "asd/string.h"
#include <unordered_map>
#include <iostream>
#include <functional>

namespace asdtest_string
{
#define TEST_STRCMP_DEFAULT(CHECK, RVAL)					\
	CHECK(0, asd::strcmp(BufM, RVAL));						\
	CHECK(0, asd::strcmp(BufW, L ## RVAL));					\

#define TEST_STRCMP_CASE(CHECK, RVAL, CASE_SENSITIVE)		\
	CHECK(0, asd::strcmp(BufM, RVAL, CASE_SENSITIVE));		\
	CHECK(0, asd::strcmp(BufW, L ## RVAL, CASE_SENSITIVE));	\

	template <typename CharType>
	void GenTestArray(CharType* buf, int option)
	{
		int i = 0;
		if (option == 0) {
			for (char c='A'; c<='Z'; ++c)
				buf[i++] = c;
		}
		else if (option == 1) {
			for (char c='a'; c<='z'; ++c)
				buf[i++] = c;
		}
		else {
			std::srand(std::time(nullptr));
			for (i=0; i<26; ++i) {
				if (std::rand() % 2 == 0)
					buf[i] = 'A' + i;
				else
					buf[i] = 'a' + i;
			}
		}
		buf[26] = '\0';
	}

	template <typename CharType>
	void CaseConvertTest()
	{
		CharType ucase[27];
		CharType lcase[27];
		CharType testbuf[27];
		GenTestArray(ucase, 0);
		GenTestArray(lcase, 1);

		// toupper
		do {
			GenTestArray(testbuf, 2);
		} while (asd::strcmp(ucase, testbuf) == 0);
		EXPECT_STRNE(ucase, testbuf);
		for (int i=0; i<27; ++i)
			testbuf[i] = asd::toupper(testbuf[i]);
		EXPECT_STREQ(ucase, testbuf);

		// tolower
		do {
			GenTestArray(testbuf, 2);
		} while (asd::strcmp(lcase, testbuf) == 0);
		EXPECT_STRNE(lcase, testbuf);
		for (int i=0; i<27; ++i)
			testbuf[i] = asd::tolower(testbuf[i]);
		EXPECT_STREQ(lcase, testbuf);
	}

	TEST(String, GlobalFunction)
	{
		// strlen
		{
			const char*    teststr_m =  "0123456789";
			const wchar_t* teststr_w = L"0123456789";

			auto len_m = asd::strlen(teststr_m);
			auto len_w = asd::strlen(teststr_w);
			EXPECT_EQ(len_m, 10);
			EXPECT_EQ(len_m, len_w);
		}

		// sprintf, strcmp
		{
			const int BufSize = 512;
			char BufM[BufSize];
			memset(BufM, 0xff, sizeof(BufM));
			wchar_t BufW[BufSize];
			memset(BufW, 0xff, sizeof(BufW));

			asd::sprintf(BufM, BufSize, 
						 "%s %s %d", "abc", "가나다", 123);
			asd::sprintf(BufW, BufSize,
						 L"%ls %ls %d", L"abc", L"가나다", 123);

			TEST_STRCMP_DEFAULT(EXPECT_EQ, "abc 가나다 123");

			TEST_STRCMP_DEFAULT(EXPECT_NE, "ABC 가나다 123");

			TEST_STRCMP_CASE(EXPECT_EQ, "ABC 가나다 123", false);

			TEST_STRCMP_DEFAULT(EXPECT_LT, "abc 가나다 000");

			TEST_STRCMP_DEFAULT(EXPECT_LT, "abc 가나다");

			TEST_STRCMP_DEFAULT(EXPECT_GT, "abc 가나다 999");

			TEST_STRCMP_DEFAULT(EXPECT_GT, "abc 가나다 123 ");
		}

		// upper, lower
		{
			CaseConvertTest<char>();
			CaseConvertTest<wchar_t>();
		}
	}



	template <typename StringType_asd,
			  bool CaseSensitive>
	void TestCode_hash_and_equal(const typename StringType_asd::CharType* a_keyString)
	{
		const int BufSize = 512;
		typename StringType_asd::CharType key[BufSize];
		memset(key, 0xff, sizeof(key));
		asd::strcpy(key, a_keyString);

		std::unordered_map<StringType_asd, 
						   int,
						   asd::hash_String<typename StringType_asd::CharType, CaseSensitive>,
						   asd::equal_to_String<typename StringType_asd::CharType, CaseSensitive> > map;
		map.emplace(a_keyString, BufSize);

		// Case1. 삽입 당시의 포인터변수로 검색
		{
			auto it = map.find(a_keyString);
			ASSERT_NE(it, map.end());
			EXPECT_EQ(it->second, BufSize);
		}

		// Case2. 새로운 포인터변수로 검색
		{
			auto it = map.find(key);
			ASSERT_NE(it, map.end());
			EXPECT_EQ(it->second, BufSize);
		}

		// Case3. String객체로 검색
		{
			StringType_asd str;
			str = a_keyString;
			auto it = map.find(str);
			ASSERT_NE(it, map.end());
			EXPECT_EQ(it->second, BufSize);
		}

		// 다른 케이스 검색
		{

		}
	}

	TEST(String, hash_and_equal)
	{
		// MultiByte
		TestCode_hash_and_equal<asd::MString, true>("abc가나다123");
		TestCode_hash_and_equal<asd::MString, false>("abc가나다123");

		// Wide
		TestCode_hash_and_equal<asd::WString, true>(L"abc가나다123");
		TestCode_hash_and_equal<asd::WString, false>(L"abc가나다123");
	}



	template <typename StringType_asd, typename StringType_std>
	void TestCode_StringClass(const typename StringType_asd::CharType* a_testString)
	{
		typename StringType_asd::CharType c = 0;
		const StringType_std stdString1 = a_testString;

		auto Str = [](const char* str)
		{
			thread_local StringType_std t_format;
			const std::string format(str);
			t_format.assign(format.begin(), format.end());
			return t_format.c_str();
		};

		// 각종 생성자와 대입연산자 테스트
		StringType_asd s1(a_testString);
		StringType_asd s2(s1);
		StringType_asd s3(Str("{}"), a_testString);
		StringType_asd s4 = a_testString;
		StringType_asd s5 = s1;
		StringType_asd s6;
		s6 = a_testString;
		StringType_asd s7;
		s7 = s1;
		StringType_asd s8(stdString1);
		StringType_asd s9;
		const StringType_std stdString2 = s1;
		s9 = stdString2;

		EXPECT_EQ(s1, a_testString);
		EXPECT_EQ(s1, s2);
		EXPECT_EQ(s2, s3);
		EXPECT_EQ(s3, s4);
		EXPECT_EQ(s4, s5);
		EXPECT_EQ(s5, s6);
		EXPECT_EQ(s6, s7);
		EXPECT_EQ(s7, s8);
		EXPECT_EQ(s8, s9);


		// +와 +=연산자 테스트
		{
			s2 = s1;
			s2 += s3 + s4;

			// s3와 s4는 값의 변화가 없어야 한다.
			EXPECT_EQ(s1, s3);
			EXPECT_EQ(s1, s4);

			const int BufSize = 512;
			typename StringType_asd::CharType buf[BufSize];
			int offset = 0;
			asd::strcpy(buf+offset, s1);

			offset += s1.length();
			asd::strcpy(buf+offset, s3);

			offset += s3.length();
			asd::strcpy(buf+offset, s4);

			// s2는 뒤에 s3와 s4의 내용이 붙어있어야 한다.
			EXPECT_EQ(s2, buf);
		}


		// <<연산자 테스트
		{
			s2 = s1;
			s2 << s3 << s4;

			// s3와 s4는 값의 변화가 없어야 한다.
			EXPECT_EQ(s1, s3);
			EXPECT_EQ(s1, s4);

			const int BufSize = 512;
			typename StringType_asd::CharType buf[BufSize];
			int offset = 0;
			asd::strcpy(buf+offset, s1);

			offset += s1.length();
			asd::strcpy(buf+offset, s3);

			offset += s3.length();
			asd::strcpy(buf+offset, s4);

			// s2는 뒤에 s3와 s4의 내용이 붙어있어야 한다.
			EXPECT_EQ(s2, buf);
		}


		// 각종 타입들에 대한 연산자 오버로딩 테스트
		{
			void* p = &c;

			s2 = s1;
			s2 << 123 << 1.23 << true << p << stdString1;

			s3 = s1;
			s3 += 123;
			s3 += 1.23;
			s3 += true;
			s3 += p;
			s3 += stdString1;

			s4 = s1;
			s4 += Str("{}{}{}{}{}");
			s5.Format(s4, 123, 1.23, Str("true"), p, stdString1.data());

			// 값이 모두 같은지 확인.
			EXPECT_EQ(s2, s3);
			EXPECT_EQ(s3, s5);
		}


		// 비교연산 테스트
		{
			s2 = s1 + 0;
			s3 = s2 + 1;
			
			// '0'의 코드값이 '1'보다 작으므로 s2가 더 작다.
			EXPECT_LT(s2, s3);
			
			// 길이가 더 짧은 쪽이 더 작다.
			EXPECT_LT(s1, s2);

			// std string과 비교.
			EXPECT_EQ(s1, stdString1);
			EXPECT_GT(s2, stdString1);

			// 빈 문자열은 가장 작다.
			typename StringType_asd::CharType NullChar = '\0';
			s4 = StringType_asd();
			EXPECT_GT(s1, s4);
			EXPECT_GT(s1, &NullChar);
			EXPECT_EQ(s4, &NullChar);

			// nullptr은 빈 문자열로 취급한다.
			typename StringType_asd::CharType* NullPtr = nullptr;
			EXPECT_GT(s1, NullPtr);
			EXPECT_EQ(s4, NullPtr);
		}
	}

	TEST(String, StringClass)
	{
		// MultiByte
		TestCode_StringClass<asd::MString, std::string>("abc가나다123");

		// Wide
		TestCode_StringClass<asd::WString, std::wstring>(L"abc가나다123");
	}
}
