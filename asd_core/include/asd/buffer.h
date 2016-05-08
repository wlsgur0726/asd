#pragma once
#include "asd/asdbase.h"
#include "asd/classutil.h"
#include "asd/sysutil.h"
#include <array>

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
		virtual bool SetSize(IN size_t a_bytes) asd_noexcept = 0;

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

		virtual bool SetSize(IN size_t a_bytes) asd_noexcept override
		{
			if (a_bytes > Bytes) {
				assert(false);
				return false;
			}
			m_size = a_bytes;
			return true;
		}

		size_t m_size = 0;
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

		virtual bool SetSize(IN size_t a_bytes) asd_noexcept override
		{
			assert(false);
			return false;
		}
	};

#endif



	typedef std::unique_ptr<BufferInterface> Buffer_ptr;
	class BufferDeque final
	{
	public:
		struct iterator_base
		{
			BufferDeque* m_owner = nullptr;
			size_t m_index;

			inline Buffer_ptr& operator*() asd_noexcept
			{
				assert(m_owner != nullptr);
				return (*m_owner)[m_index];
			}

			inline bool operator!=(IN const iterator_base& a_cmp) asd_noexcept
			{
				assert(m_owner == a_cmp.m_owner);
				return m_index != a_cmp.m_index;
			}
		};

		struct iterator : public iterator_base
		{
			using iterator_base::operator*;
			using iterator_base::operator!=;

			inline iterator& operator++() asd_noexcept
			{
				assert(m_owner != nullptr);
				++m_index;
				return *this;
			}

			inline iterator& operator++(int) asd_noexcept
			{
				assert(m_owner != nullptr);
				iterator ret(*this);
				++m_index;
				return ret;
			}
		};

		struct reverse_iterator : public iterator_base
		{
			using iterator_base::operator*;
			using iterator_base::operator!=;

			inline reverse_iterator& operator++() asd_noexcept
			{
				assert(m_owner != nullptr);
				--m_index;
				return *this;
			}

			inline reverse_iterator& operator++(int) asd_noexcept
			{
				assert(m_owner != nullptr);
				reverse_iterator ret(*this);
				--m_index;
				return ret;
			}
		};


		inline void PushBack(MOVE Buffer_ptr&& a_new) asd_noexcept
		{
			m_back.push_back(std::move(a_new));
		}

		inline void PushBack(MOVE BufferDeque&& a_list) asd_noexcept
		{
			size_t i = m_back.size();
			m_back.resize(i + a_list.Count());

			for (auto it=a_list.m_front.rbegin(); it!=a_list.m_front.rend(); ++it)
				m_back[i++] = std::move(*it);
			for (auto it=a_list.m_back.begin(); it!=a_list.m_back.end(); ++it)
				m_back[i++] = std::move(*it);
		}

		inline void PushFront(MOVE Buffer_ptr&& a_new) asd_noexcept
		{
			m_front.push_back(std::move(a_new));
		}

		inline size_t Count() const asd_noexcept
		{
			return m_front.size() + m_back.size();
		}

		inline const Buffer_ptr& at(IN const size_t a_index) const asd_noexcept
		{
			auto idx = a_index;
			auto sz = m_front.size();
			if (idx < sz)
				return m_front[(sz-1) - idx];

			idx -= sz;
			sz = m_back.size();
			if (a_index < sz)
				return m_back[idx];

			thread_local Buffer_ptr t_null;
			t_null.reset(nullptr);
			return t_null;
		}

		inline const Buffer_ptr& operator[](IN const size_t a_index) const asd_noexcept
		{
			return at(a_index);
		}

		inline Buffer_ptr& operator[](IN const size_t a_index) asd_noexcept
		{
			return const_cast<Buffer_ptr&>(at(a_index));
		}

		inline void Clear() asd_noexcept
		{
			m_front.clear();
			m_back.clear();

			// 일정량 이하의 capacity는 보존한다.
			const size_t CapacityLimit = 512;
			if (m_front.capacity() > CapacityLimit)
				m_front.shrink_to_fit();
			if (m_back.capacity() > CapacityLimit)
				m_back.shrink_to_fit();
		}

		inline iterator begin() asd_noexcept
		{
			iterator ret;
			ret.m_owner = this;
			ret.m_index = 0;
			return ret;
		}

		inline iterator end() asd_noexcept
		{
			iterator ret;
			ret.m_owner = this;
			ret.m_index = Count();
			return ret;
		}

		inline reverse_iterator rbegin() asd_noexcept
		{
			reverse_iterator ret;
			ret.m_owner = this;
			ret.m_index = Count() - 1;
			return ret;
		}

		inline reverse_iterator rend() asd_noexcept
		{
			reverse_iterator ret;
			ret.m_owner = this;
			ret.m_index = (size_t(0) - size_t(1));
			return ret;
		}

	private:
		std::vector<Buffer_ptr> m_front;
		std::vector<Buffer_ptr> m_back;
	};



	class BufferList;
	struct BufferListDeleter
	{
		void* m_srcPool;
		void operator()(IN void* a_ptr) asd_noexcept;
	};
	typedef std::unique_ptr<BufferList, BufferListDeleter> BufferList_ptr;



	enum class BufferOperation
	{
		Write,
		Read,
	};



	class BufferList
	{
	public:
		template<BufferOperation Operation> friend class Transactional;
		static const size_t DefaultBufferSize;


	private:
		BufferDeque m_list;
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