#include "stdafx.h"
#include "asd/buffer.h"
#include "asd/objpool.h"

namespace asd
{
#define asd_BufferListPool		ObjectPool2<BufferList, true>
#define asd_DefaultBufferPool	ObjectPool2<DefaultBuffer> // deque의 capacity를 보존하기 위해 소멸자 호출을 하지 않는다.
	const size_t BufferList::DefaultBufferSize = 16 * 1024;


	struct DefaultBuffer
		: public StaticBuffer<BufferList::DefaultBufferSize>
	{
		asd_DefaultBufferPool* m_srcPool = nullptr;
		inline static void operator delete(IN void* a_ptr)
		{
			auto cast = reinterpret_cast<DefaultBuffer*>(a_ptr);
			auto srcPool = cast->m_srcPool;

			if (srcPool == nullptr)
				::operator delete(a_ptr);
			else {
				cast->m_srcPool = nullptr;
				srcPool->Free(cast);
			}
		}
	};


	void BufferListDeleter::operator()(IN void* a_ptr) asd_noexcept
	{
		auto ptr = reinterpret_cast<BufferList*>(a_ptr);
		auto pool = reinterpret_cast<asd_BufferListPool*>(m_srcPool);
		if (pool == nullptr) {
			ptr->Clear();
			pool->Free(ptr);
		}
		else
			delete ptr;
	}



	Buffer_ptr BufferList::NewBuffer() asd_noexcept
	{
		static ObjectPoolShardSet<asd_DefaultBufferPool> t_bufferPool;

		auto& pool = t_bufferPool.GetShard();
		auto newBuf = pool.Alloc();
		newBuf->m_srcPool = &pool;

		return Buffer_ptr(newBuf);
	}


	BufferList_ptr BufferList::NewList() asd_noexcept
	{
		static ObjectPoolShardSet<asd_BufferListPool> t_bufferListPool;

		auto& pool = t_bufferListPool.GetShard();
		auto newList = pool.Alloc();
		newList->Clear();

		BufferListDeleter deleter;
		deleter.m_srcPool = &pool;

		return BufferList_ptr(newList, deleter);
	}


	BufferList::BufferList() asd_noexcept
	{
		Clear();
	}


	BufferList::~BufferList() asd_noexcept
	{
	}


	void BufferList::Clear() asd_noexcept
	{
		const size_t CapacityLimit = 1024;
		if (m_list.size() <= CapacityLimit)
			m_list.resize(0);
		else {
			m_list.~deque();
			new(&m_list) std::deque<Buffer_ptr>();
		}

		m_readOffset = Offset();
		m_writeOffset = 0;
		m_total_capacity = 0;
		m_total_write = 0;
		m_total_read = 0;
	}


	size_t BufferList::GetTotalSize() const asd_noexcept
	{
		return m_total_write;
	}


	void BufferList::ReserveBuffer(IN size_t a_bytes /*= DefaultBufferSize*/) asd_noexcept
	{
		assert(m_total_capacity >= m_total_write);
		assert(m_list.size() >= m_writeOffset);

		while (m_total_capacity - m_total_write < a_bytes) {
			m_total_capacity += DefaultBufferSize;
			m_list.push_back(NewBuffer());
		}
	}


	void BufferList::ReserveBuffer(MOVE Buffer_ptr&& a_buffer) asd_noexcept
	{
		assert(m_total_capacity >= m_total_write);
		assert(m_list.size() >= m_writeOffset);

		if (a_buffer == nullptr)
			return;

		const auto capacity = a_buffer->Capacity();
		if (capacity == 0)
			return;

		a_buffer->SetSize(0);
		m_total_capacity += capacity;
		m_list.push_back(std::move(a_buffer));
	}


	void BufferList::PushBack(MOVE Buffer_ptr&& a_buffer) asd_noexcept
	{
		assert(m_list.size() >= m_writeOffset);
		if (a_buffer == nullptr)
			return;

		const size_t capacity = a_buffer->Capacity();
		const size_t size = a_buffer->GetSize();

		assert(capacity > 0);
		assert(capacity >= size);
		m_total_capacity += capacity;
		m_list.push_back(std::move(a_buffer));

		if (size == 0)
			return; // 새로 추가하는 버퍼가 빈 버퍼이므로 여유버퍼로 취급
		m_total_write += size;

		// 오프셋 위치까지 쉬프트
		size_t writeOffset = m_list.size() - 1;
		auto newbe = m_list.rbegin();
		auto before = newbe;
		while (++before != m_list.rend()) {
			if ((*before)->GetSize() > 0)
				break;
			std::swap(*before, *newbe);
			++newbe;
			--writeOffset;
		}

		if (capacity > size)
			m_writeOffset = writeOffset;
		else
			m_writeOffset = writeOffset + 1;
	}


	void BufferList::PushBack(MOVE BufferList&& a_bufferList) asd_noexcept
	{
		for (auto& e : a_bufferList.m_list)
			PushBack(std::move(e));
		a_bufferList.Clear();
	}


	void BufferList::PushFront(MOVE Buffer_ptr&& a_buffer) asd_noexcept
	{
		assert(m_readOffset == Offset());
		assert(m_list.size() >= m_writeOffset);
		if (a_buffer == nullptr)
			return;

		const size_t capacity = a_buffer->Capacity();
		const size_t size = a_buffer->GetSize();

		assert(capacity > 0);
		assert(capacity >= size);
		m_total_capacity += capacity;

		if (size == 0) {
			// 새로 추가하는 버퍼가 빈 버퍼이므로 여유버퍼로 취급
			m_list.push_back(std::move(a_buffer));
			return;
		}
		m_total_write += size;

		if (m_list.size() > 0)
			++m_writeOffset;
		else {
			// 빈 상태에서 새로 추가된 경우
			assert(m_writeOffset == 0);
			if (capacity - size == 0)
				++m_writeOffset;
		}
		m_list.push_front(std::move(a_buffer));
	}


	void BufferList::PushFront(MOVE BufferList&& a_bufferList) asd_noexcept
	{
		auto& srcList = a_bufferList.m_list;
		for (auto it=srcList.rbegin(); it!=srcList.rend(); ++it) {
			m_list.push_front(std::move(*it));
		}
		a_bufferList.Clear();
	}


	bool BufferList::Readable(IN size_t a_bytes) const asd_noexcept
	{
		assert(m_list.size() >= m_readOffset.Row);
		assert(m_total_capacity >= m_total_write);
		assert(m_total_write >= m_total_read);

		if (a_bytes > m_total_write - m_total_read)
			return false;
		return true;
	}


	size_t BufferList::Write(IN const void* a_data,
							 IN const size_t a_bytes) asd_noexcept
	{
		ReserveBuffer(a_bytes);

		const uint8_t* src = reinterpret_cast<const uint8_t*>(a_data);
		for (size_t cp = 0;;) {
			BufferInterface* buf = m_list[m_writeOffset].get();
			assert(buf != nullptr);

			const size_t capacity = buf->Capacity();
			const size_t size = buf->GetSize();
			const size_t reserve = capacity - size;
			uint8_t* dst = buf->GetBuffer() + size;
			assert(capacity >= size);
			assert(dst != reinterpret_cast<uint8_t*>(size));

			const size_t remain = a_bytes - cp;
			if (reserve >= remain) {
				std::memcpy(dst, src+cp, remain);
				buf->SetSize(size + remain);
				break;
			}

			std::memcpy(dst, src+cp, reserve);
			cp += reserve;

			buf->SetSize(capacity);
			++m_writeOffset;
			assert(m_list.size() > m_writeOffset);
		}

		m_total_write += a_bytes;
		return a_bytes;
	}


	size_t BufferList::Read(OUT void* a_data,
							IN const size_t a_bytes) asd_noexcept
	{
		if (Readable(a_bytes) == false)
			return 0;

		uint8_t* dst = reinterpret_cast<uint8_t*>(a_data);
		for (size_t cp = 0;;) {
			BufferInterface* buf = m_list[m_readOffset.Row].get();
			assert(buf != nullptr);

			const size_t size = buf->GetSize() - m_readOffset.Col;
			const uint8_t* src = buf->GetBuffer() + m_readOffset.Col;
			assert(src != reinterpret_cast<uint8_t*>(m_readOffset.Col));

			const size_t remain = a_bytes - cp;
			if (size >= remain) {
				std::memcpy(dst+cp, src, remain);
				m_readOffset.Col += remain;
				break;
			}

			std::memcpy(dst+cp, src, size);
			cp += size;

			m_readOffset.Col = 0;
			++m_readOffset.Row;
			assert(m_list.size() > m_readOffset.Row);
		}

		m_total_read += a_bytes;
		return a_bytes;
	}



	template<>
	Transactional<BufferOperation::Write>::Transactional(REF BufferList& a_bufferList) asd_noexcept
		: m_bufferList(a_bufferList)
	{
		auto& rb = const_cast<Offset&>(m_rollbackPoint);
		rb.Row = m_bufferList.m_writeOffset;
		if (m_bufferList.m_list.size() > rb.Row)
			rb.Col = m_bufferList.m_list[rb.Row]->GetSize();
		else
			rb.Col = 0;
	}

	template<>
	Transactional<BufferOperation::Read>::Transactional(REF BufferList& a_bufferList) asd_noexcept
		: m_bufferList(a_bufferList)
		, m_rollbackPoint(a_bufferList.m_readOffset)
	{
	}

	template<>
	void Transactional<BufferOperation::Write>::Rollback() asd_noexcept
	{
		for (size_t i = 1 + m_rollbackPoint.Row;
			 i <= m_bufferList.m_writeOffset && i < m_bufferList.m_list.size();
			 ++i)
		{
			m_bufferList.m_list[i]->SetSize(0);
		}
		m_bufferList.m_list[m_rollbackPoint.Row]->SetSize(m_rollbackPoint.Col);
		m_bufferList.m_writeOffset = m_rollbackPoint.Row;
	}

	template<>
	void Transactional<BufferOperation::Read>::Rollback() asd_noexcept
	{
		m_bufferList.m_readOffset = m_rollbackPoint;
	}
}

