#include "stdafx.h"
#include "asd/serialize.h"
#include <memory>
#if !asd_Platform_Windows
#include <netinet/in.h>
#endif


namespace asdtest_serialize
{
	typedef asd::StaticBuffer<3>		SmallBuf;
	typedef asd::StaticBuffer<16*1024>	LargeBuf;



	template <typename T, typename Buf>
	void Test_PrimitiveType_Internal(const T in)
	{
		auto bufferList = asd::BufferList::New();
		for (int i=0; i<3; ++i)
			bufferList->ReserveBuffer(asd::Buffer_ptr(new Buf));

		size_t ret;
		ret = asd::Write(*bufferList, in);
		EXPECT_EQ(ret, sizeof(T));

		T out;
		ret = asd::Read(*bufferList, out);
		EXPECT_EQ(ret, sizeof(T));

		EXPECT_EQ(in, out);
	}

	template <typename T>
	void Test_PrimitiveType()
	{
		Test_PrimitiveType_Internal<T, SmallBuf>((T)0);
		Test_PrimitiveType_Internal<T, LargeBuf>((T)0);
		Test_PrimitiveType_Internal<T, SmallBuf>((T)123.123);
		Test_PrimitiveType_Internal<T, LargeBuf>((T)123.123);
		Test_PrimitiveType_Internal<T, SmallBuf>((T)-1);
		Test_PrimitiveType_Internal<T, LargeBuf>((T)-1);
	}

	TEST(Serialize, PrimitiveType)
	{
		Test_PrimitiveType<int8_t>();
		Test_PrimitiveType<uint8_t>();
		Test_PrimitiveType<int16_t>();
		Test_PrimitiveType<uint16_t>();
		Test_PrimitiveType<int32_t>();
		Test_PrimitiveType<uint32_t>();
		Test_PrimitiveType<int64_t>();
		Test_PrimitiveType<uint64_t>();
		Test_PrimitiveType<float>();
		Test_PrimitiveType<double>();
	}



	template <typename T>
	void Test_Endian_Reverse_1()
	{
		static_assert(sizeof(T)==2 || sizeof(T)==4, "unexpected size");

		const T src = (T)0xABCD;
		T asd = asd::Reverse(src);
		T libc;
		if (sizeof(T) == 2)
			libc = (T)htons(src);
		else if (sizeof(T) == 4)
			libc = (T)htonl(src);
		else
			return;

		EXPECT_EQ(0, std::memcmp(&asd, &libc, sizeof(T)));
	}

	template <typename T>
	void Test_Endian_Reverse_2()
	{
		const T src = (T)(0xABCDEF01020304);
		const T reverse1 = asd::Reverse(src);
		const T reverse2 = asd::Reverse(reverse1);
		const T reverse3 = asd::Reverse(reverse2);

		EXPECT_EQ(0, std::memcmp(&src, &reverse2, sizeof(T)));
		EXPECT_EQ(0, std::memcmp(&reverse1, &reverse3, sizeof(T)));
	}

	TEST(Serialize, Endian_Reverse)
	{
		Test_Endian_Reverse_1<int16_t>();
		Test_Endian_Reverse_1<uint16_t>();
		Test_Endian_Reverse_1<int32_t>();
		Test_Endian_Reverse_1<uint32_t>();

		Test_Endian_Reverse_2<int8_t>();
		Test_Endian_Reverse_2<uint8_t>();
		Test_Endian_Reverse_2<int16_t>();
		Test_Endian_Reverse_2<uint16_t>();
		Test_Endian_Reverse_2<int32_t>();
		Test_Endian_Reverse_2<uint32_t>();
		Test_Endian_Reverse_2<int64_t>();
		Test_Endian_Reverse_2<uint64_t>();
		Test_Endian_Reverse_2<float>();
		Test_Endian_Reverse_2<double>();
	}



	template <typename T, typename Buf, size_t ElemCnt, asd::Endian SrcEndian, asd::Endian DstEndian>
	void Test_Endian_Array_Internal(const T* src)
	{
		const size_t ExpectBytes = sizeof(T)*ElemCnt;
		const size_t BufCnt = 10;

		auto bufferList = asd::BufferList::New();
		for (auto i=BufCnt; i>0; --i)
			bufferList->ReserveBuffer(asd::Buffer_ptr(new Buf));

		size_t ret;
		ret = asd::Write_PrimitiveArray<SrcEndian, T>(*bufferList, src, ElemCnt);
		EXPECT_EQ(ret, ExpectBytes);

		T dst[ElemCnt];
		ret = asd::Read_PrimitiveArray<DstEndian, T>(*bufferList, dst, ElemCnt);
		EXPECT_EQ(ret, ExpectBytes);

		for (size_t i=0; i<ElemCnt; ++i) {
			if (SrcEndian == DstEndian) {
				EXPECT_EQ(src[i], dst[i]);
				if (src[i] != dst[i])
					break;
			}
			else {
				T reverse = asd::Reverse(dst[i]);
				EXPECT_EQ(src[i], reverse);
				if (src[i] != reverse)
					break;
			}
		}
	}

	template <typename T, typename Buf>
	void Test_Endian_Array_Internal()
	{
		const size_t ElemCnt = 10;
		T src[ElemCnt];
		for (size_t i=0; i<ElemCnt; ++i)
			src[i] = static_cast<T>(i+1);

		Test_Endian_Array_Internal<T, Buf, ElemCnt, asd::Endian::Little, asd::Endian::Little>(src);
		Test_Endian_Array_Internal<T, Buf, ElemCnt, asd::Endian::Little, asd::Endian::Big>(src);
		Test_Endian_Array_Internal<T, Buf, ElemCnt, asd::Endian::Big, asd::Endian::Little>(src);
		Test_Endian_Array_Internal<T, Buf, ElemCnt, asd::Endian::Big, asd::Endian::Big>(src);
	}

	template <typename T>
	void Test_Endian_Array()
	{
		Test_Endian_Array_Internal<T, SmallBuf>();
		Test_Endian_Array_Internal<T, LargeBuf>();
	}

	TEST(Serialize, Endian_Array)
	{
		Test_Endian_Array<int8_t>();
		Test_Endian_Array<uint8_t>();
		Test_Endian_Array<int16_t>();
		Test_Endian_Array<uint16_t>();
		Test_Endian_Array<int32_t>();
		Test_Endian_Array<uint32_t>();
		Test_Endian_Array<int64_t>();
		Test_Endian_Array<uint64_t>();
		Test_Endian_Array<float>();
		Test_Endian_Array<double>();
	}



	template <typename T, typename Buf>
	void Test_WriteRead(const T& src)
	{
		const size_t BufCnt = 1000;

		auto bufferList = asd::BufferList::New();
		for (auto i=BufCnt; i>0; --i)
			bufferList->ReserveBuffer(asd::Buffer_ptr(new Buf));

		size_t ret1 = asd::Write(*bufferList, src);
		
		T dst;
		size_t ret2 = asd::Read(*bufferList, dst);

		EXPECT_EQ(ret1, ret2);
		EXPECT_EQ(src, dst);

		bufferList->Flush();
		bufferList->ReserveBuffer(asd::Buffer_ptr(new Buf)); // 남은 버퍼가 없을 수도 있으므로
		EXPECT_EQ(bufferList->GetTotalSize(), bufferList->at(0)->GetSize());
	}

	TEST(Serialize, Tuple)
	{
		typedef std::tuple<
			int8_t,
			uint8_t,
			int16_t,
			uint16_t,
			int32_t,
			uint32_t,
			int64_t,
			uint64_t,
			float,
			double,
			const int> TestTuple;
		const TestTuple src(-8,
							8,
							-16,
							16,
							-32,
							32,
							-64,
							64,
							1.23f,
							4.56,
							100);
		Test_WriteRead<TestTuple, SmallBuf>(src);
		Test_WriteRead<TestTuple, LargeBuf>(src);
	}



	struct CustomStruct
	{
		int data1;
		double data2;
		std::vector<uint16_t> data3;
		std::unordered_map<int, std::map<int, double>> data4;

		inline size_t WriteTo(asd::BufferList& buffer) const
		{
			asd::Transactional<asd::BufOp::Write> tran(buffer);
			size_t sum = 0;
			size_t ret;

			ret = asd::Write(buffer, data1);
			if (ret == 0) {
				EXPECT_NE(ret, 0);
				return 0;
			}
			sum += ret;

			ret = asd::Write(buffer, data2);
			if (ret == 0) {
				EXPECT_NE(ret, 0);
				return 0;
			}
			sum += ret;

			ret = asd::Write(buffer, data3);
			if (ret == 0) {
				EXPECT_NE(ret, 0);
				return 0;
			}
			sum += ret;

			ret = asd::Write(buffer, data4);
			if (ret == 0) {
				EXPECT_NE(ret, 0);
				return 0;
			}
			sum += ret;

			return tran.SetResult(sum);
		}

		inline size_t ReadFrom(asd::BufferList& buffer)
		{
			asd::Transactional<asd::BufOp::Read> tran(buffer);
			size_t sum = 0;
			size_t ret;

			ret = asd::Read(buffer, data1);
			if (ret == 0) {
				EXPECT_NE(ret, 0);
				return 0;
			}
			sum += ret;

			ret = asd::Read(buffer, data2);
			if (ret == 0) {
				EXPECT_NE(ret, 0);
				return 0;
			}
			sum += ret;

			ret = asd::Read(buffer, data3);
			if (ret == 0) {
				EXPECT_NE(ret, 0);
				return 0;
			}
			sum += ret;

			ret = asd::Read(buffer, data4);
			if (ret == 0) {
				EXPECT_NE(ret, 0);
				return 0;
			}
			sum += ret;

			return tran.SetResult(sum);
		}

		inline bool operator == (const CustomStruct& r) const
		{
			if (data1 != r.data1)
				return false;
			if (data2 != r.data2)
				return false;
			if (data3.size() != r.data3.size())
				return false;
			if (0 != std::memcmp(data3.data(), r.data3.data(), sizeof(data3[0])*data3.size()))
				return false;
			if (data4.size() != r.data4.size())
				return false;
			for (const auto& it1 : data4) {
				const auto it2 = r.data4.find(it1.first);
				if (it2 == r.data4.end())
					return false;

				const auto& map1 = it1.second;
				const auto& map2 = it2->second;
				if (map1.size() != map2.size())
					return false;

				for (const auto& it11 : map1) {
					const auto it22 = map2.find(it11.first);
					if (it22 == map2.end())
						return false;
					if (it11.second != it22->second)
						return false;
				}
			}
			return true;
		}
	};

	TEST(Serialize, CustomType)
	{
		const CustomStruct src;
		{
			auto& setter = const_cast<CustomStruct&>(src);

			setter.data1 = 123;

			setter.data2 = 4.56;

			const char16_t str[] = u"Test!!";
			setter.data3.resize(sizeof(str));
			std::memcpy(setter.data3.data(), str, sizeof(str));

			for (int h=0; h<=9; ++h) {
				for (int w=0; w<=9; ++w)
					setter.data4[h][w] = h + w/10.0;
			}
		}
		Test_WriteRead<CustomStruct, SmallBuf>(src);
		Test_WriteRead<CustomStruct, LargeBuf>(src);
	}

	TEST(Serialize, Push_Buffer)
	{
		// 새로운 버퍼를 추가했을 때 Capacity 관리가 제대로 되는지 테스트
		const auto DefaultSize = asd_BufferList_DefaultWriteBufferSize;
		std::array<uint8_t, DefaultSize> data;
		asd::BufferList buffers;
		asd::Buffer_ptr buf;

		buf = asd::NewBuffer<DefaultSize>();
		buf->SetSize(DefaultSize / 2 - 1);
		buffers.PushBack(std::move(buf));

		buf = asd::NewBuffer<DefaultSize>();
		buf->SetSize(DefaultSize / 2 - 1);
		buffers.PushBack(std::move(buf));

		size_t r = asd::Write(buffers, data);
		EXPECT_EQ(r, DefaultSize);
		size_t total = (DefaultSize/2-1)*2 + DefaultSize;
		EXPECT_EQ(buffers.GetTotalSize(), total);

		buf = asd::NewBuffer<DefaultSize>();
		buf->SetSize(DefaultSize / 2 - 1);
		buffers.PushFront(std::move(buf));

		buf = asd::NewBuffer<DefaultSize>();
		buf->SetSize(DefaultSize / 2 - 1);
		buffers.PushBack(std::move(buf));

		r = asd::Write(buffers, data);
		EXPECT_EQ(r, DefaultSize);
		total += (DefaultSize/2-1)*2 + DefaultSize;
		EXPECT_EQ(buffers.GetTotalSize(), total);
	}
}

