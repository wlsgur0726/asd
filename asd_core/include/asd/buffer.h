#pragma once
#include "asdbase.h"
#include "classutil.h"
#include "objpool.h"
#include "util.h"
#include <array>
#include <vector>
#include <deque>

#define asd_Support_FlatBuffers 1
#if asd_Support_FlatBuffers
#include "../../../../flatbuffers/include/flatbuffers/flatbuffers.h"
#endif

namespace asd
{
	struct Offset
	{
		size_t Row = 0;	// 버퍼의 인덱스
		size_t Col = 0;	// 버퍼 내에서의 오프셋

		inline static int Compare(const Offset& a_left,
								  const Offset& a_right)
		{
			if (a_left.Row < a_right.Row)
				return -1;
			else if (a_left.Row > a_right.Row)
				return 1;

			if (a_left.Col < a_left.Col)
				return -1;
			else if (a_left.Col > a_left.Col)
				return 1;

			return 0;
		}

		asd_Define_CompareOperator(Compare, Offset);
	};



	template <typename T>
	struct IsDirectSerializableType { static constexpr bool Value = false; };

	template<>
	struct IsDirectSerializableType<int8_t> { static constexpr bool Value = true; };

	template<>
	struct IsDirectSerializableType<uint8_t> { static constexpr bool Value = true; };

	template<>
	struct IsDirectSerializableType<int16_t> { static constexpr bool Value = true; };

	template<>
	struct IsDirectSerializableType<uint16_t> { static constexpr bool Value = true; };

	template<>
	struct IsDirectSerializableType<int32_t> { static constexpr bool Value = true; };

	template<>
	struct IsDirectSerializableType<uint32_t> { static constexpr bool Value = true; };

	template<>
	struct IsDirectSerializableType<int64_t> { static constexpr bool Value = true; };

	template<>
	struct IsDirectSerializableType<uint64_t> { static constexpr bool Value = true; };

	template<>
	struct IsDirectSerializableType<float> { static constexpr bool Value = true; };

	template<>
	struct IsDirectSerializableType<double> { static constexpr bool Value = true; };

	template<>
	struct IsDirectSerializableType<long double> { static constexpr bool Value = true; };



	class BufferInterface
	{
	public:
		virtual uint8_t* GetBuffer() const = 0;
		virtual size_t Capacity() const = 0;
		virtual size_t GetSize() const = 0;
		virtual bool SetSize(size_t a_bytes) = 0;

		inline size_t GetReserve() const
		{
			const auto capacity = Capacity();
			const auto size = GetSize();
			asd_DAssert(capacity >= size);
			return capacity - size;
		}

		virtual ~BufferInterface()
		{
		}
	};



	template <size_t Bytes>
	struct StaticBuffer
		: public BufferInterface
		, public std::array<uint8_t, Bytes>
	{
		static constexpr size_t Limit()
		{
			return Bytes;
		}

		inline StaticBuffer()
		{
		}

		virtual uint8_t* GetBuffer() const override
		{
			return const_cast<uint8_t*>(this->data());
		}

		virtual size_t Capacity() const override
		{
			return Limit();
		}

		virtual size_t GetSize() const override
		{
			asd_DAssert(m_size <= Bytes);
			return m_size;
		}

		virtual bool SetSize(size_t a_bytes) override
		{
			if (a_bytes > Bytes)
				return false;
			m_size = a_bytes;
			return true;
		}

		size_t m_size = 0;
	};



	template <
		typename T,
		typename... Args
	> struct VectorBasedBuffer
		: public BufferInterface
		, public std::vector<T, Args...>
	{
		static_assert(IsDirectSerializableType<T>::Value, "invalid type");
		typedef std::vector<T, Args...> BaseClass;
		using BaseClass::BaseClass;

		virtual uint8_t* GetBuffer() const override
		{
			asd_DAssert(this->size() > 0);
			return reinterpret_cast<uint8_t*>(this->data());
		}

		virtual size_t Capacity() const override
		{
			asd_DAssert(this->size() > 0);
			return GetSize();
		}

		virtual size_t GetSize() const override
		{
			asd_DAssert(this->size() > 0);
			return this->size() * sizeof(T);
		}

		virtual bool SetSize(size_t a_bytes) override
		{
			return false; // not use
		}
	};



#if asd_Support_FlatBuffers
	struct FlatBuffersData
		: public BufferInterface
	{
		flatbuffers::unique_ptr_t m_data;
		flatbuffers::uoffset_t m_bytes = 0;

		inline FlatBuffersData(flatbuffers::FlatBufferBuilder& a_builder)
		{
			Set(a_builder);
		}

		void Set(flatbuffers::FlatBufferBuilder& a_builder)
		{
			m_bytes = a_builder.GetSize();
			m_data = std::move(a_builder.ReleaseBufferPointer());
		}

		virtual uint8_t* GetBuffer() const override
		{
			asd_DAssert(m_data != nullptr);
			return m_data.get();
		}

		virtual size_t Capacity() const override
		{
			asd_DAssert(m_data != nullptr);
			return m_bytes;
		}

		virtual size_t GetSize() const override
		{
			asd_DAssert(m_data != nullptr);
			return m_bytes;
		}

		virtual bool SetSize(size_t a_bytes) override
		{
			asd_OnErr("banned call (send only)");
			return false; // not use
		}
	};

#endif



	enum class BufOp
	{
		Write,
		Read,
	};


	typedef UniquePtr<BufferInterface> Buffer_ptr;


	template<size_t BYTES>
	Buffer_ptr NewBuffer()
	{
		using Buffer = StaticBuffer<BYTES>;
		using PoolType = ObjectPoolShardSet< ObjectPool2<Buffer> >;
		using Pool = Global<PoolType>;

		auto newBuf = Pool::Instance().Alloc();
		return Buffer_ptr(newBuf, [](BufferInterface* a_ptr)
		{
			auto cast = reinterpret_cast<Buffer*>(a_ptr);
			Pool::Instance().Free(cast);
		});
	}



	class BufferList;
	typedef UniquePtr<BufferList> BufferList_ptr;

	// gather-scatter를 고려한 버퍼 목록
	class BufferList
		: protected std::deque<Buffer_ptr>
	{
	public:
		typedef std::deque<Buffer_ptr> BaseType;
		template<BufOp Operation> friend class Transactional;
		friend class AsyncSocket;
		#define asd_BufferList_DefaultWriteBufferSize	( 16 * 1024 )
		#define asd_BufferList_DefaultReadBufferSize	(  2 * 1024 )


	private:
		Offset m_readOffset;		// 읽기 오프셋
		size_t m_writeOffset;		// 쓰기 Row 오프셋, (BufferInterface.GetSize()가 Col 오프셋)
		size_t m_total_capacity;	// 여분을 포함한 버퍼 용량 총합 (bytes)
		size_t m_total_write;		// 여분을 제외하고 쓰여진 버퍼 크기 총합 (bytes)
		size_t m_total_read;		// 현재까지 읽은 바이트 수 (bytes)


	public:
		inline static Buffer_ptr NewWriteBuffer()
		{
			return NewBuffer<asd_BufferList_DefaultWriteBufferSize>();
		}

		static BufferList_ptr New();

		BufferList();

		virtual ~BufferList();

		void Clear();

		size_t GetTotalSize() const;

		void ReserveBuffer(size_t a_bytes = asd_BufferList_DefaultWriteBufferSize);

		void ReserveBuffer(Buffer_ptr&& a_buffer);

		void PushBack(Buffer_ptr&& a_buffer);

		void PushBack(BufferList&& a_bufferList);

		void PushBack(BaseType&& a_bufferList);

		void PushFront(Buffer_ptr&& a_buffer);

		void PushFront(BufferList&& a_bufferList);

		void PushFront(BaseType&& a_bufferList);

		void Flush();

		bool Readable(size_t a_bytes) const;

		size_t Write(const void* a_data,
					 const size_t a_bytes);

		size_t Read(void* a_data /*Out*/,
					const size_t a_bytes);

		using BaseType::begin;
		using BaseType::end;
		using BaseType::rbegin;
		using BaseType::rend;
		using BaseType::at;
		using BaseType::operator[];
	};


	template <BufOp Operation>
	class Transactional final
	{
		static_assert(Operation==BufOp::Write || Operation==BufOp::Read,
					  "invalid Operation");

	private:
		BufferList&		m_bufferList;
		const Offset	m_rollbackPoint;
		size_t			m_result = 0;

	public:
		Transactional(BufferList& a_bufferList);

		void Rollback();

		inline size_t SetResult(const size_t a_result)
		{
			m_result = a_result;
			return a_result;
		}

		inline ~Transactional()
		{
			if (m_result == 0)
				Rollback();
		}
	};



}
