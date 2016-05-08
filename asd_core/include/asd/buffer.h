#pragma once
#include "asd/asdbase.h"
#include "asd/classutil.h"
#include "asd/sysutil.h"
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
		size_t Row = 0;
		size_t Col = 0;

		inline static int Compare(IN const Offset& a_left,
								  IN const Offset& a_right) asd_noexcept
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


	class BufferInterface
	{
	public:
		virtual uint8_t* GetBuffer() const asd_noexcept = 0;
		virtual size_t Capacity() const asd_noexcept = 0;
		virtual size_t GetSize() const asd_noexcept = 0;
		virtual void SetSize(IN size_t a_bytes) asd_noexcept = 0;

		inline size_t GetReserve() const asd_noexcept
		{
			const auto capacity = Capacity();
			const auto size = GetSize();
			assert(capacity >= size);
			return capacity - size;
		}

		virtual ~BufferInterface() asd_noexcept
		{
		}
	};



	template <size_t Bytes>
	struct StaticBuffer
		: public BufferInterface
		, public std::array<uint8_t, Bytes>
	{
		static constexpr size_t Limit() asd_noexcept
		{
			return Bytes;
		}

		inline StaticBuffer() asd_noexcept
		{
		}

		virtual uint8_t* GetBuffer() const asd_noexcept override
		{
			return const_cast<uint8_t*>(data());
		}

		virtual size_t Capacity() const asd_noexcept override
		{
			return Limit();
		}

		virtual size_t GetSize() const asd_noexcept override
		{
			assert(m_size <= Bytes);
			return m_size;
		}

		virtual void SetSize(IN size_t a_bytes) asd_noexcept override
		{
			if (a_bytes > Bytes)
				assert(false);
			m_size = a_bytes;
		}

		size_t m_size = 0;
	};



	template <typename T>
	struct IsDirectSerializableType
	{
		static constexpr bool Value = false;
	};

	template<>
	struct IsDirectSerializableType<int8_t>
	{
		static constexpr bool Value = true;
	};

	template<>
	struct IsDirectSerializableType<uint8_t>
	{
		static constexpr bool Value = true;
	};

	template<>
	struct IsDirectSerializableType<int16_t>
	{
		static constexpr bool Value = true;
	};

	template<>
	struct IsDirectSerializableType<uint16_t>
	{
		static constexpr bool Value = true;
	};

	template<>
	struct IsDirectSerializableType<int32_t>
	{
		static constexpr bool Value = true;
	};

	template<>
	struct IsDirectSerializableType<uint32_t>
	{
		static constexpr bool Value = true;
	};

	template<>
	struct IsDirectSerializableType<int64_t>
	{
		static constexpr bool Value = true;
	};

	template<>
	struct IsDirectSerializableType<uint64_t>
	{
		static constexpr bool Value = true;
	};

	template<>
	struct IsDirectSerializableType<float>
	{
		static constexpr bool Value = true;
	};

	template<>
	struct IsDirectSerializableType<double>
	{
		static constexpr bool Value = true;
	};

	template<>
	struct IsDirectSerializableType<long double>
	{
		static constexpr bool Value = true;
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

		virtual uint8_t* GetBuffer() const asd_noexcept override
		{
			assert(size() > 0);
			return reinterpret_cast<uint8_t*>(data());
		}

		virtual size_t Capacity() const asd_noexcept override
		{
			assert(size() > 0);
			return GetSize();
		}

		virtual size_t GetSize() const asd_noexcept override
		{
			assert(size() > 0);
			return size() * sizeof(T);
		}

		virtual void SetSize(IN size_t a_bytes) asd_noexcept override
		{
			assert(false); // not use
		}
	};



#if asd_Support_FlatBuffers
	struct FlatBuffersData
		: public BufferInterface
	{
		flatbuffers::unique_ptr_t m_data;
		flatbuffers::uoffset_t m_bytes;

		inline FlatBuffersData()
		{
			m_bytes = 0;
		}

		inline FlatBuffersData(MOVE flatbuffers::FlatBufferBuilder& a_builder)
		{
			Set(a_builder);
		}

		void Set(MOVE flatbuffers::FlatBufferBuilder& a_builder)
		{
			m_bytes = a_builder.GetSize();
			m_data = std::move(a_builder.ReleaseBufferPointer());
		}

		virtual uint8_t* GetBuffer() const asd_noexcept override
		{
			assert(m_data != nullptr);
			return m_data.get();
		}

		virtual size_t Capacity() const asd_noexcept override
		{
			assert(m_data != nullptr);
			return m_bytes;
		}

		virtual size_t GetSize() const asd_noexcept override
		{
			assert(m_data != nullptr);
			return m_bytes;
		}

		virtual void SetSize(IN size_t a_bytes) asd_noexcept override
		{
			assert(false); // not use
		}
	};

#endif



	enum class BufferOperation
	{
		Write,
		Read,
	};

	struct BufferListDeleter
	{
		void* m_srcPool = nullptr;
		void operator()(IN void* a_ptr) asd_noexcept;
	};

	class BufferList;
	typedef std::unique_ptr<BufferList, BufferListDeleter> BufferList_ptr;
	typedef std::unique_ptr<BufferInterface> Buffer_ptr;


	class BufferList
	{
	public:
		template<BufferOperation Operation> friend class Transactional;
		static const size_t DefaultBufferSize;


	private:
		std::deque<Buffer_ptr> m_list;
		Offset m_readOffset;
		size_t m_writeOffset;
		size_t m_total_capacity;
		size_t m_total_write;
		size_t m_total_read;


	public:
		static Buffer_ptr NewBuffer() asd_noexcept;

		static BufferList_ptr NewList() asd_noexcept;

		BufferList() asd_noexcept;

		virtual ~BufferList() asd_noexcept;

		void Clear() asd_noexcept;

		size_t GetTotalSize() const asd_noexcept;

		void ReserveBuffer(IN size_t a_bytes = DefaultBufferSize) asd_noexcept;

		void ReserveBuffer(MOVE Buffer_ptr&& a_buffer) asd_noexcept;

		void PushBack(MOVE Buffer_ptr&& a_buffer) asd_noexcept;

		void PushBack(MOVE BufferList&& a_bufferList) asd_noexcept;

		void PushFront(MOVE Buffer_ptr&& a_buffer) asd_noexcept;

		void PushFront(MOVE BufferList&& a_bufferList) asd_noexcept;

		bool Readable(IN size_t a_bytes) const asd_noexcept;

		size_t Write(IN const void* a_data,
					 IN const size_t a_bytes) asd_noexcept;

		size_t Read(OUT void* a_data,
					IN const size_t a_bytes) asd_noexcept;

	};



	template <BufferOperation Operation>
	class Transactional final
	{
		static_assert(Operation==BufferOperation::Write || Operation==BufferOperation::Read,
					  "invalid Operation");

	private:
		BufferList&		m_bufferList;
		const Offset	m_rollbackPoint;
		size_t			m_result = 0;

	public:
		Transactional(REF BufferList& a_bufferList) asd_noexcept;

		void Rollback() asd_noexcept;

		inline size_t SetResult(IN const size_t a_result) asd_noexcept
		{
			m_result = a_result;
			return a_result;
		}

		inline ~Transactional() asd_noexcept
		{
			if (m_result == 0)
				Rollback();
		}
	};



}
