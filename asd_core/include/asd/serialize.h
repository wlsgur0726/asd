﻿#pragma once
#include "asdbase.h"
#include "sysutil.h"
#include "buffer.h"
#include <cstring>
#include <utility>
#include <array>
#include <vector>
#include <list>
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>


namespace asd
{
	typedef uint16_t			DefaultCountType;
#define asd_Default_LimitCount	((DefaultCountType)std::numeric_limits<DefaultCountType>::max())
#define asd_Default_Endian		asd::Endian::Little


	inline bool InvalidCount(size_t a_count)
	{
		const auto Limit = asd_Default_LimitCount;
		static_assert(Limit > 0, "invalid Limit value");
		if (a_count > Limit || a_count < 0) {
			asd_DAssert(false);
			return true;
		}
		return false;
	}


	inline void BufferDeficiency()
	{
		// Write  :  버퍼의 여유공간이 부족하여 쓸 수 없음
		// Read   :  버퍼에 들어있는 데이터가 온전하지 않아 읽을 수 없음
		asd_DAssert(false);
	}


	template<
		Endian BufferEndian,
		typename DataType
	> inline constexpr bool EndianFree()
	{
		return (sizeof(DataType) == 1 || GetNativeEndian() == BufferEndian);
	}



	template <typename T>
	inline T Reverse(T a_src)
	{
		auto src = (const uint8_t*)(&a_src);
		uint8_t dst[sizeof(T)];
		for (size_t i=0; i<sizeof(T); ++i)
			dst[i] = src[(sizeof(T)-1) - i];
		return *((T*)dst);
	}



	// PrimitiveType
	template <
		Endian BufferEndian,
		typename DataType
	> inline size_t Write_PrimitiveType(BufferList& a_buffer,
										DataType a_data)
	{
		static_assert(IsDirectSerializableType<DataType>::Value, "invalid type");

		if (EndianFree<BufferEndian, DataType>() == false)
			a_data = Reverse(a_data);
		return a_buffer.Write(&a_data, sizeof(DataType));
	}


	template <
		Endian BufferEndian,
		typename DataType
	> inline size_t Read_PrimitiveType(BufferList& a_buffer,
									   DataType& a_data /*Out*/)
	{
		static_assert(IsDirectSerializableType<DataType>::Value, "invalid type");

		size_t ret = a_buffer.Read(&a_data,
								   sizeof(DataType));
		if (ret > 0) {
			if (EndianFree<BufferEndian, DataType>() == false)
				a_data = Reverse(a_data);
			asd_DAssert(sizeof(DataType) == ret);
		}
		return ret;
	}



	template <
		Endian BufferEndian,
		typename DataType
	> inline size_t Write_PrimitiveArray(BufferList& a_buffer,
										 const DataType* a_data,
										 const size_t a_count)
	{
		static_assert(IsDirectSerializableType<DataType>::Value, "invalid type");

		const size_t TotalBytes = sizeof(DataType) * a_count;
		if (EndianFree<BufferEndian, DataType>())
			return a_buffer.Write(a_data, TotalBytes);

		a_buffer.ReserveBuffer(TotalBytes);
		size_t ret = 0;
		for (size_t i=0; i<a_count; ++i) {
			DataType reverse = Reverse(a_data[i]);
			ret += a_buffer.Write(&reverse, sizeof(DataType));
		}
		asd_DAssert(TotalBytes == ret);
		return TotalBytes;
	}


	template <
		Endian BufferEndian,
		typename DataType
	> inline size_t Read_PrimitiveArray(BufferList& a_buffer,
										DataType* a_data /*Out*/,
										const size_t a_count)
	{
		static_assert(IsDirectSerializableType<DataType>::Value, "invalid type");

		const size_t TotalBytes = sizeof(DataType) * a_count;
		size_t ret = a_buffer.Read(a_data, TotalBytes);
		if (ret > 0) {
			if (EndianFree<BufferEndian, DataType>() == false) {
				for (size_t i=0; i<a_count; ++i)
					a_data[i] = Reverse(a_data[i]);
			}
			asd_DAssert(ret == TotalBytes);
		}
		return ret;
	}



	template <
		Endian BufferEndian,
		typename DataType,
		typename... Args
	> inline size_t Write_PrimitiveVector(BufferList& a_buffer,
										  const std::vector<DataType, Args...>& a_data)
	{
		static_assert(IsDirectSerializableType<DataType>::Value, "invalid type");

		const auto count = a_data.size();
		if (InvalidCount(count))
			return 0;

		Transactional<BufOp::Write> tran(a_buffer);

		size_t ret1 = Write_PrimitiveType<BufferEndian, DefaultCountType>(a_buffer,
																		  static_cast<DefaultCountType>(count));
		if (ret1 == 0)
			return 0;

		if (count == 0)
			return tran.SetResult(ret1);

		size_t ret2 = Write_PrimitiveArray<BufferEndian, DataType>(a_buffer,
																   a_data.data(),
																   a_data.size());
		if (ret2 == 0)
			return 0;

		return tran.SetResult(ret1 + ret2);
	}


	template <
		Endian BufferEndian,
		typename DataType,
		typename... Args
	> inline size_t Read_PrimitiveVector(BufferList& a_buffer,
										 std::vector<DataType, Args...>& a_data /*Out*/)
	{
		static_assert(IsDirectSerializableType<DataType>::Value, "invalid type");

		Transactional<BufOp::Read> tran(a_buffer);

		DefaultCountType count;
		size_t ret1 = Read_PrimitiveType<BufferEndian, DefaultCountType>(a_buffer,
																		 count);
		if (ret1 == 0)
			return 0;

		if (InvalidCount(count))
			return 0;

		if (count == 0)
			return tran.SetResult(ret1);

		const auto OrgCount = a_data.size();
		a_data.resize(OrgCount + count);
		size_t ret2 = Read_PrimitiveArray<BufferEndian, DataType>(a_buffer,
																  &a_data[OrgCount],
																  count);
		if (ret2 == 0) {
			// rollback
			a_data.resize(OrgCount);
			return 0;
		}

		return tran.SetResult(ret1 + ret2);
	}


#define asd_Define_Write_And_Read_PrimitiveType(Type)									\
	inline size_t Write(BufferList& a_buffer,											\
						const Type a_data)												\
	{																					\
		return Write_PrimitiveType<asd_Default_Endian, Type>(a_buffer,					\
															 a_data);					\
	}																					\
																						\
	inline size_t Write(BufferList& a_buffer,											\
						const Type* a_data,												\
						const size_t a_count)											\
	{																					\
		return Write_PrimitiveArray<asd_Default_Endian, Type>(a_buffer,					\
															  a_data,					\
															  a_count);					\
	}																					\
																						\
	template <size_t Count>																\
	inline size_t Write(BufferList& a_buffer,											\
						const std::array<Type, Count>& a_data)							\
	{																					\
		return Write(a_buffer,															\
					 a_data.data(),														\
					 Count);															\
	}																					\
																						\
	template <typename... Args>															\
	inline size_t Write(BufferList& a_buffer,											\
						const std::vector<Type, Args...>& a_data)						\
	{																					\
		return Write_PrimitiveVector<asd_Default_Endian, Type, Args...>(a_buffer,		\
																		a_data);		\
	}																					\
																						\
	inline size_t Read(BufferList& a_buffer,											\
					   Type& a_data /*Out*/)											\
	{																					\
		return Read_PrimitiveType<asd_Default_Endian, Type>(a_buffer,					\
															a_data);					\
	}																					\
																						\
	inline size_t Read(BufferList& a_buffer,											\
					   Type* a_data /*Out*/,											\
					   const size_t a_count)											\
	{																					\
		return Read_PrimitiveArray<asd_Default_Endian, Type>(a_buffer,					\
															 a_data,					\
															 a_count);					\
	}																					\
																						\
	template <size_t Count>																\
	inline size_t Read(BufferList& a_buffer,											\
					   std::array<Type, Count>& a_data /*Out*/)							\
	{																					\
		return Read(a_buffer,															\
					a_data.data(),														\
					Count);																\
	}																					\
																						\
	template <typename... Args>															\
	inline size_t Read(BufferList& a_buffer,											\
					   std::vector<Type, Args...>& a_data /*Out*/)						\
	{																					\
		return Read_PrimitiveVector<asd_Default_Endian, Type, Args...>(a_buffer,		\
																	   a_data);			\
	}																					\


	asd_Define_Write_And_Read_PrimitiveType(int8_t);
	asd_Define_Write_And_Read_PrimitiveType(uint8_t);
	asd_Define_Write_And_Read_PrimitiveType(int16_t);
	asd_Define_Write_And_Read_PrimitiveType(uint16_t);
	asd_Define_Write_And_Read_PrimitiveType(int32_t);
	asd_Define_Write_And_Read_PrimitiveType(uint32_t);
	asd_Define_Write_And_Read_PrimitiveType(int64_t);
	asd_Define_Write_And_Read_PrimitiveType(uint64_t);
	asd_Define_Write_And_Read_PrimitiveType(float);
	asd_Define_Write_And_Read_PrimitiveType(double);



	// Default
	template <typename CustomSerializableType>
	inline size_t Write(BufferList& a_buffer,
						const CustomSerializableType& a_data)
	{
		return a_data.WriteTo(a_buffer);
	}


	template <typename CustomSerializableType>
	inline size_t Read(BufferList& a_buffer,
					   CustomSerializableType& a_data /*Out*/)
	{
		return a_data.ReadFrom(a_buffer);
	}



	// Read to const type
	template <typename DataType>
	inline size_t Read(BufferList& a_buffer,
					   const DataType& a_data /*Out*/)
	{
		return Read(a_buffer,
					const_cast<DataType&>(a_data));
	}



	// pair
	template <
		typename First,
		typename Second
	> size_t Write(BufferList& a_buffer,
				   const std::pair<First, Second>& a_data)
	{
		Transactional<BufOp::Write> tran(a_buffer);

		size_t ret1 = Write(a_buffer,
							a_data.first);
		if (ret1 == 0)
			return 0;

		size_t ret2 = Write(a_buffer,
							a_data.second);
		if (ret2 == 0)
			return 0;

		return tran.SetResult(ret1 + ret2);
	}


	template <
		typename First,
		typename Second
	> size_t Read(BufferList& a_buffer,
				  std::pair<First, Second>& a_data /*Out*/)
	{
		Transactional<BufOp::Read> tran(a_buffer);

		size_t ret1 = Read(a_buffer,
						   a_data.first);
		if (ret1 == 0)
			return 0;

		size_t ret2 = Read(a_buffer,
						   a_data.second);
		if (ret2 == 0)
			return 0;

		return tran.SetResult(ret1 + ret2);
	}



	// tuple
	template <
		typename Tuple,
		size_t Count,
		size_t Index
	> inline size_t Write_TupleElem(BufferList& a_buffer,
									const Tuple& a_data)
	{
		const bool Nullity = Count <= Index;
		if (Nullity)
			return 0;

		const size_t I = Nullity ? 0 : Index;	// static_assert 방지
		const auto& data = std::get<I>(a_data);
		const size_t cur = Write(a_buffer, data);
		if (cur == 0)
			return 0;

		const bool IsLastElem = Count <= Index+1;
		const size_t NextIndex = IsLastElem ? Count : Index+1;
		const size_t next = Write_TupleElem<Tuple, Count, NextIndex>(a_buffer,
																	 a_data);
		if (next == 0 && IsLastElem == false)
			return 0;

		return cur + next;
	}


	template <
		typename Tuple,
		size_t Count,
		size_t Index
	> inline size_t Read_TupleElem(BufferList& a_buffer,
								   Tuple& a_data /*Out*/)
	{
		const bool Nullity = Count <= Index;
		if (Nullity)
			return 0;

		const size_t I = Nullity ? 0 : Index;	// static_assert 방지
		auto& data = std::get<I>(a_data);
		const size_t cur = Read(a_buffer, data);
		if (cur == 0)
			return 0;

		const bool IsLastElem = Index + 1 >= Count;
		const size_t NextIndex = IsLastElem ? Count : Index+1;
		const size_t next = Read_TupleElem<Tuple, Count, NextIndex>(a_buffer,
																	a_data);
		if (next == 0 && IsLastElem == false)
			return 0;

		return cur + next;
	}


	template <typename... Args>
	inline size_t Write(BufferList& a_buffer,
						const std::tuple<Args...>& a_data)
	{
		Transactional<BufOp::Write> tran(a_buffer);

		typedef std::tuple<Args...> Tuple;
		const size_t Count = sizeof...(Args);
		size_t ret = Write_TupleElem<Tuple, Count, 0>(a_buffer,
													  a_data);
		return tran.SetResult(ret);
	}



	template <typename... Args>
	inline size_t Read(BufferList& a_buffer,
					   std::tuple<Args...>& a_data /*Out*/)
	{
		Transactional<BufOp::Read> tran(a_buffer);

		typedef std::tuple<Args...> Tuple;
		const size_t Count = sizeof...(Args);
		size_t ret = Read_TupleElem<Tuple, Count, 0>(a_buffer,
													 a_data);
		return tran.SetResult(ret);
	}



	// std container
	template <typename Container>
	inline size_t Write_StdContainer(BufferList& a_buffer,
									 const Container& a_data)
	{
		const auto count = a_data.size();
		if (InvalidCount(count))
			return 0;

		Transactional<BufOp::Write> tran(a_buffer);

		size_t ret = Write(a_buffer,
						   static_cast<DefaultCountType>(count));
		if (ret == 0)
			return 0;

		for (const auto& elem : a_data) {
			size_t r = Write(a_buffer, elem);
			if (r == 0)
				return 0;
			ret += r;
		}
		return tran.SetResult(ret);
	}

#define asd_Define_Write_And_Read_StdContainer(Container)						\
	template <typename... Args>													\
	inline size_t Write(BufferList& a_buffer,									\
						const Container<Args...>& a_data)						\
	{																			\
		return Write_StdContainer(a_buffer, a_data);							\
	}																			\
																				\
	template <typename... Args>													\
	inline size_t Read(BufferList& a_buffer,									\
					   Container<Args...>& a_data /*Out*/)						\

	asd_Define_Write_And_Read_StdContainer(std::vector)
	{
		Transactional<BufOp::Read> tran(a_buffer);

		DefaultCountType count;
		size_t ret = Read(a_buffer, count);
		if (ret == 0)
			return 0;

		if (InvalidCount(count))
			return 0;

		if (count == 0)
			return tran.SetResult(ret);

		const auto OrgCount = a_data.size();
		const auto NewCount = OrgCount + count;
		a_data.resize(NewCount);
		for (auto i=OrgCount; i<NewCount; ++i) {
			size_t r = Read(a_buffer, a_data[i]);
			if (r == 0) {
				// rollback
				a_data.resize(OrgCount);
				return r;
			}
			ret += r;
		}
		return tran.SetResult(ret);
	}

	asd_Define_Write_And_Read_StdContainer(std::list)
	{
		Transactional<BufOp::Read> tran(a_buffer);

		DefaultCountType count;
		size_t ret = Read(a_buffer, count);
		if (ret == 0)
			return 0;

		if (InvalidCount(count))
			return 0;

		typedef typename std::set<Args...>::value_type T;
		const auto OrgCount = a_data.size();
		for (DefaultCountType i=0; i<count; ++i) {
			a_data.emplace_back(T());
			T& data = *a_data.rbegin();
			size_t r = Read(a_buffer, data);
			if (r == 0) {
				// rollback
				a_data.resize(OrgCount);
				return 0;
			}
			ret += r;
		}
		return ret;
	}

	asd_Define_Write_And_Read_StdContainer(std::set)
	{
		Transactional<BufOp::Read> tran(a_buffer);

		DefaultCountType count;
		size_t ret = Read(a_buffer, count);
		if (ret == 0)
			return 0;

		if (InvalidCount(count))
			return 0;

		typedef typename std::set<Args...>::value_type T;
		for (DefaultCountType i=0; i<count; ++i) {
			T data;
			size_t r = Read(a_buffer, data);
			if (r == 0) {
				// rollback
				tran.Rollback();
				for (DefaultCountType j=0; j<i; ++j) {
					T data2;
					size_t r2 = Read(a_buffer, data2);
					asd_DAssert(r2 != 0);
					a_data.erase(data2);
				}
				return 0;
			}
			a_data.emplace(std::move(data));
			ret += r;
		}
		return tran.SetResult(ret);
	}

	asd_Define_Write_And_Read_StdContainer(std::unordered_set)
	{
		Transactional<BufOp::Read> tran(a_buffer);

		DefaultCountType count;
		size_t ret = Read(a_buffer, count);
		if (ret == 0)
			return 0;

		if (InvalidCount(count))
			return 0;

		typedef typename std::unordered_set<Args...>::value_type T;
		for (DefaultCountType i=0; i<count; ++i) {
			T data;
			size_t r = Read(a_buffer, data);
			if (r == 0) {
				// rollback
				tran.Rollback();
				for (DefaultCountType j=0; j<i; ++j) {
					T data2;
					size_t r2 = Read(a_buffer, data2);
					asd_DAssert(r2 != 0);
					a_data.erase(data2);
				}
				return 0;
			}
			a_data.emplace(std::move(data));
			ret += r;
		}
		return tran.SetResult(ret);
	}

#if asd_Compiler_MSVC
	template <
		typename Key,
		typename Value,
		typename... Args
	> inline size_t Write(BufferList& a_buffer,
						  const std::map<Key, Value, Args...>& a_data)
	{
		return Write_StdContainer<std::map<Key, Value, Args...>>(a_buffer, a_data);
	}

	template <
		typename Key,
		typename Value,
		typename... Args
	> inline size_t Read(BufferList& a_buffer,
						 std::map<Key, Value, Args...>& a_data /*Out*/)
	{
		Transactional<BufOp::Read> tran(a_buffer);

		DefaultCountType count;
		size_t ret = Read(a_buffer, count);
		if (ret == 0)
			return 0;

		if (InvalidCount(count))
			return 0;

		typedef typename std::map<Key, Value, Args...>::value_type T;
		for (DefaultCountType i=0; i<count; ++i) {
			T data;
			size_t r = Read(a_buffer, data);
			if (r == 0) {
				// rollback
				tran.Rollback();
				for (DefaultCountType j=0; j<i; ++j) {
					T data2;
					size_t r2 = Read(a_buffer, data2);
					asd_DAssert(r2 != 0);
					a_data.erase(data2.first);
				}
				return 0;
			}
			a_data.emplace(std::move(data));
			ret += r;
		}
		return tran.SetResult(ret);
	}

	template <
		typename Key,
		typename Value,
		typename... Args
	> inline size_t Write(BufferList& a_buffer,
						  const std::unordered_map<Key, Value, Args...>& a_data)
	{
		return Write_StdContainer<std::unordered_map<Key, Value, Args...>>(a_buffer, a_data);
	}

	template <
		typename Key,
		typename Value,
		typename... Args
	> inline size_t Read(BufferList& a_buffer,
						 std::unordered_map<Key, Value, Args...>& a_data /*Out*/)
	{
		Transactional<BufOp::Read> tran(a_buffer);

		DefaultCountType count;
		size_t ret = Read(a_buffer, count);
		if (ret == 0)
			return 0;

		if (InvalidCount(count))
			return 0;

		typedef typename std::unordered_map<Key, Value, Args...>::value_type T;
		for (DefaultCountType i=0; i<count; ++i) {
			T data;
			size_t r = Read(a_buffer, data);
			if (r == 0) {
				// rollback
				tran.Rollback();
				for (DefaultCountType j=0; j<i; ++j) {
					T data2;
					size_t r2 = Read(a_buffer, data2);
					asd_DAssert(r2 != 0);
					a_data.erase(data2.first);
				}
				return 0;
			}
			a_data.emplace(std::move(data));
			ret += r;
		}
		return tran.SetResult(ret);
	}

#else
	asd_Define_Write_And_Read_StdContainer(std::map)
	{
		Transactional<BufOp::Read> tran(a_buffer);

		DefaultCountType count;
		size_t ret = Read(a_buffer, count);
		if (ret == 0)
			return 0;

		if (InvalidCount(count))
			return 0;

		typedef typename std::map<Args...>::value_type T;
		for (DefaultCountType i=0; i<count; ++i) {
			T data;
			size_t r = Read(a_buffer, data);
			if (r == 0) {
				// rollback
				tran.Rollback();
				for (DefaultCountType j=0; j<i; ++j) {
					T data2;
					size_t r2 = Read(a_buffer, data2);
					asd_DAssert(r2 != 0);
					a_data.erase(data2.first);
				}
				return 0;
			}
			a_data.emplace(std::move(data));
			ret += r;
		}
		return tran.SetResult(ret);
	}

	asd_Define_Write_And_Read_StdContainer(std::unordered_map)
	{
		Transactional<BufOp::Read> tran(a_buffer);

		DefaultCountType count;
		size_t ret = Read(a_buffer, count);
		if (ret == 0)
			return 0;

		if (InvalidCount(count))
			return 0;

		typedef typename std::unordered_map<Args...>::value_type T;
		for (DefaultCountType i=0; i<count; ++i) {
			T data;
			size_t r = Read(a_buffer, data);
			if (r == 0) {
				// rollback
				tran.Rollback();
				for (DefaultCountType j=0; j<i; ++j) {
					T data2;
					size_t r2 = Read(a_buffer, data2);
					asd_DAssert(r2 != 0);
					a_data.erase(data2.first);
				}
				return 0;
			}
			a_data.emplace(std::move(data));
			ret += r;
		}
		return tran.SetResult(ret);
	}

#endif


	// TODO string

}

