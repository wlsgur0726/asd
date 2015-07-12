#include "stdafx.h"
#include "asd/string.h"
#include <unordered_map>
#include <iostream>

namespace asdtest_string
{
#define TEST_STRCMP_DEFAULT(CHECK, RVAL)					\
	CHECK(0, asd::strcmp(BufM, RVAL));						\
	CHECK(0, asd::strcmp(BufW, L ## RVAL));					\

#define TEST_STRCMP_CASE(CHECK, RVAL, IGNORE_CASE)			\
	CHECK(0, asd::strcmp(BufM, RVAL, IGNORE_CASE));			\
	CHECK(0, asd::strcmp(BufW, L ## RVAL, IGNORE_CASE));	\

	TEST(String, GlobalFunction)
	{
		// strlen
		{
			char*	teststr_m =  "0123456789";
			wchar_t* teststr_w = L"0123456789";

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
			memset(BufW, 0xff, sizeof(BufM));

			asd::sprintf(BufM, BufSize, 
						 "%s %s %d", "abc", "가나다", 123);
			asd::sprintf(BufW, BufSize,
						 L"%ls %ls %d", L"abc", L"가나다", 123);

			TEST_STRCMP_DEFAULT(EXPECT_EQ, "abc 가나다 123");

			TEST_STRCMP_DEFAULT(EXPECT_NE, "ABC 가나다 123");

			TEST_STRCMP_CASE(EXPECT_EQ, "ABC 가나다 123", true);

			TEST_STRCMP_DEFAULT(EXPECT_LT, "abc 가나다 000");

			TEST_STRCMP_DEFAULT(EXPECT_LT, "abc 가나다");

			TEST_STRCMP_DEFAULT(EXPECT_GT, "abc 가나다 999");

			TEST_STRCMP_DEFAULT(EXPECT_GT, "abc 가나다 123 ");
		}
	}



	template <typename StringType_asd>
	void TestCode_hash_and_equal(const typename StringType_asd::CharType* a_keyString)
	{
		const int BufSize = 512;
		typename StringType_asd::CharType key[BufSize];
		memset(key, 0xff, sizeof(key));
		asd::strcpy(key, a_keyString);

		std::unordered_map<StringType_asd,
						   int,
						   typename StringType_asd::Hash> map;

		map.emplace(a_keyString, BufSize);

		{
			auto it = map.find(a_keyString);
			ASSERT_NE(it, map.end());
			EXPECT_EQ(it->second, BufSize);
		}
		{
			auto it = map.find(key);
			ASSERT_NE(it, map.end());
			EXPECT_EQ(it->second, BufSize);
		}
		{
			StringType_asd str;
			str = a_keyString;
			auto it = map.find(str);
			ASSERT_NE(it, map.end());
			EXPECT_EQ(it->second, BufSize);
		}
	}

	TEST(String, hash_and_equal)
	{
		// MultiByte
		TestCode_hash_and_equal<asd::MString>("abc가나다123");

		// Wide
		TestCode_hash_and_equal<asd::WString>(L"abc가나다123");
	}


	const char* FormatString_String(char) {
		return "%s";
	}
	const wchar_t* FormatString_String(wchar_t) {
		return L"%ls";
	}
#define FormatString(TYPE, FORMAT)						\
	const char* FormatString_##TYPE(char) {				\
		return FORMAT;									\
	}													\
	const wchar_t* FormatString_##TYPE(wchar_t) {		\
		return L ## FORMAT;								\
	}													\

	FormatString(Int32, "%d");

	FormatString(Int64, "%lld");

	FormatString(Double, "%lf");

	FormatString(Pointer, "%p");

	FormatString(True, "true");

	FormatString(False, "false");


	template <typename StringType_asd, typename StringType_std>
	void TestCode_StringClass(const typename StringType_asd::CharType* a_testString)
	{
		typename StringType_asd::CharType c = 0;
		const StringType_std stdString = a_testString;

		// 각종 생성자와 대입연산자 테스트
		const StringType_asd s1(a_testString);
		StringType_asd s2(s1);
		StringType_asd s3(FormatString_String(c), a_testString);
		StringType_asd s4 = a_testString;
		StringType_asd s5 = s1;
		StringType_asd s6;
		s6 = a_testString;
		StringType_asd s7;
		s7 = s1;
		StringType_asd s8(stdString);
		StringType_asd s9;
		s9 = stdString;

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
			typename StringType_asd::CharType buf[BufSize] ={0xFF};
			int offset = 0;
			asd::strcpy(buf+offset, s1);

			offset += s1.GetLength();
			asd::strcpy(buf+offset, s3);

			offset += s3.GetLength();
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
			typename StringType_asd::CharType buf[BufSize] ={0xFF};
			int offset = 0;
			asd::strcpy(buf+offset, s1);

			offset += s1.GetLength();
			asd::strcpy(buf+offset, s3);

			offset += s3.GetLength();
			asd::strcpy(buf+offset, s4);

			// s2는 뒤에 s3와 s4의 내용이 붙어있어야 한다.
			EXPECT_EQ(s2, buf);
		}


		// 각종 타입들에 대한 연산자 오버로딩 테스트
		{
			void* p = &c;

			s2 = s1;
			s2 << 123 << 1.23 << true << p << stdString;

			s3 = s1;
			s3 += 123;
			s3 += 1.23;
			s3 += true;
			s3 += p;
			s3 += stdString;

			s4 =  s1 
				+ FormatString_Int32(c)
				+ FormatString_Double(c)
				+ FormatString_String(c)
				+ FormatString_Pointer(c)
				+ FormatString_String(c);
			s5.Format(s4, 123, 1.23, FormatString_True(c), p, stdString.data());

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
			EXPECT_EQ(s1, stdString);
			EXPECT_GT(s2, stdString);

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
