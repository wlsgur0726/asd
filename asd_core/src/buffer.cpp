#include "stdafx.h"
#include "asd/buffer.h"

namespace asd
{
	BufferList_ptr BufferList::New()
	{
		typedef ObjectPool2<BufferList, true> BufferListPool; // deque의 capacity를 보존하기 위해 소멸자 호출을 하지 않는다.
		static ObjectPoolShardSet<BufferListPool> g_bufferListPool;

		auto newList = g_bufferListPool.Alloc();
		newList->Clear();

		return BufferList_ptr(newList, [](BufferList* a_ptr)
		{
			a_ptr->Clear();
			g_bufferListPool.Free(a_ptr);
		});
	}


	BufferList::BufferList()
	{
		Clear();
	}


	BufferList::~BufferList()
	{
	}


	void BufferList::Clear()
	{
		const size_t CapacityLimit = 1024;
		if (size() <= CapacityLimit)
			resize(0);
		else {
			std::deque<Buffer_ptr>* _this = this;
			Reset(*_this);
		}

		m_readOffset = Offset();
		m_writeOffset = 0;
		m_total_capacity = 0;
		m_total_write = 0;
		m_total_read = 0;
	}


	size_t BufferList::GetTotalSize() const
	{
		return m_total_write;
	}


	void BufferList::ReserveBuffer(size_t a_bytes /*= asd_BufferList_DefaultWriteBufferSize*/)
	{
		asd_DAssert(m_total_capacity >= m_total_write);
		asd_DAssert(size() >= m_writeOffset);

		while (m_total_capacity - m_total_write < a_bytes) {
			m_total_capacity += asd_BufferList_DefaultWriteBufferSize;
			emplace_back(NewWriteBuffer());
		}
	}


	void BufferList::ReserveBuffer(Buffer_ptr&& a_buffer)
	{
		asd_DAssert(m_total_capacity >= m_total_write);
		asd_DAssert(size() >= m_writeOffset);

		if (a_buffer == nullptr)
			return;

		const auto capacity = a_buffer->Capacity();
		if (capacity == 0)
			return;

		if (a_buffer->SetSize(0) == false)
			return;

		m_total_capacity += capacity;
		emplace_back(std::move(a_buffer));
	}


	void BufferList::PushBack(Buffer_ptr&& a_buffer)
	{
		asd_DAssert(size() >= m_writeOffset);
		if (a_buffer == nullptr)
			return;

		const size_t capacity = a_buffer->Capacity();
		const size_t sz = a_buffer->GetSize();

		asd_DAssert(capacity > 0);
		asd_DAssert(capacity >= sz);
		m_total_capacity += capacity;
		emplace_back(std::move(a_buffer));

		if (sz == 0)
			return; // 새로 추가하는 버퍼가 빈 버퍼이므로 여유버퍼로 취급
		m_total_write += sz;

		// 오프셋 위치까지 쉬프트
		size_t writeOffset = size() - 1;
		auto newbe = rbegin();
		auto before = newbe;
		size_t sz_before;
		while (++before != rend()) {
			sz_before = (*before)->GetSize();
			if (sz_before > 0) {
				asd_DAssert(writeOffset > 0);
				break;
			}
			std::swap(*before, *newbe);
			++newbe;
			--writeOffset;
		}

		asd_DAssert(before==rend() ? writeOffset==0 : true);
		if (writeOffset > 0) {
			// 마지막 버퍼에 남은 여분을 사용할 수 없게 되므로 총 Capacity가 감소한다.
			const size_t remain = (*before)->Capacity() - sz_before;
			m_total_capacity -= remain;
		}

		if (capacity > sz)
			m_writeOffset = writeOffset;
		else
			m_writeOffset = writeOffset + 1;
	}


	void BufferList::PushBack(BufferList&& a_bufferList)
	{
		for (auto& e : a_bufferList)
			PushBack(std::move(e));
		a_bufferList.Clear();
	}


	void BufferList::PushBack(BaseType&& a_bufferList)
	{
		for (auto& e : a_bufferList)
			PushBack(std::move(e));
		a_bufferList.clear();
	}


	void BufferList::PushFront(Buffer_ptr&& a_buffer)
	{
		asd_DAssert(m_readOffset == Offset());
		asd_DAssert(size() >= m_writeOffset);
		if (a_buffer == nullptr)
			return;

		const size_t capacity = a_buffer->Capacity();
		const size_t sz = a_buffer->GetSize();

		asd_DAssert(capacity > 0);
		asd_DAssert(capacity >= sz);
		m_total_capacity += capacity;

		if (sz == 0) {
			// 새로 추가하는 버퍼가 빈 버퍼이므로 여유버퍼로 취급
			emplace_back(std::move(a_buffer));
			return;
		}

		const size_t remain = capacity - sz;
		if (m_total_write == 0) {
			// 빈 상태에서 새로 추가된 경우
			asd_DAssert(m_writeOffset == 0);
			if (remain == 0)
				++m_writeOffset;
		}
		else {
			// Write가 진행중인 버퍼 앞에 추가시키는 경우
			asd_DAssert(size() > 0);
			asd_DAssert(at(0)->GetSize() > 0);
			++m_writeOffset;
			m_total_capacity -= remain;
		}
		m_total_write += sz;
		emplace_front(std::move(a_buffer));
	}


	void BufferList::PushFront(BufferList&& a_bufferList)
	{
		for (auto it=a_bufferList.rbegin(); it!=a_bufferList.rend(); ++it)
			PushFront(std::move(*it));
		a_bufferList.Clear();
	}


	void BufferList::PushFront(BaseType&& a_bufferList)
	{
		for (auto it=a_bufferList.rbegin(); it!=a_bufferList.rend(); ++it)
			PushFront(std::move(*it));
		a_bufferList.clear();
	}


	void BufferList::Flush()
	{
		asd_DAssert(size() >= m_readOffset.Row);
		for (; m_readOffset.Row!=0; --m_readOffset.Row) {
			const auto sz = at(0)->GetSize();
			m_total_read -= sz;
			m_total_write -= min(m_total_write, sz);
			pop_front();
		}
		asd_DAssert(m_readOffset.Row == 0);
		asd_DAssert(m_readOffset.Col == m_total_read);
	}


	bool BufferList::Readable(size_t a_bytes) const
	{
		asd_DAssert(size() >= m_readOffset.Row);
		asd_DAssert(m_total_capacity >= m_total_write);
		asd_DAssert(m_total_write >= m_total_read);
#if asd_Debug
		{
			size_t total_read = 0;
			for (size_t i=0; i<m_readOffset.Row; ++i) {
				size_t sz = at(i)->GetSize();
				asd_DAssert(sz > 0);
				total_read += sz;
			}
			if (size() > m_readOffset.Row)
				asd_DAssert(m_readOffset.Col <= at(m_readOffset.Row)->GetSize());
			else
				asd_DAssert(m_readOffset.Col == 0);
			total_read += m_readOffset.Col;
			asd_DAssert(total_read == m_total_read);
		}
#endif
		return a_bytes <= m_total_write - m_total_read;
	}


	size_t BufferList::Write(const void* a_data,
							 const size_t a_bytes)
	{
		ReserveBuffer(a_bytes);

		const uint8_t* src = reinterpret_cast<const uint8_t*>(a_data);
		for (size_t cp = 0;;) {
			BufferInterface* buf = at(m_writeOffset).get();
			asd_DAssert(buf != nullptr);

			const size_t capacity = buf->Capacity();
			const size_t sz = buf->GetSize();
			const size_t reserve = capacity - sz;
			uint8_t* dst = buf->GetBuffer() + sz;
			asd_DAssert(capacity >= sz);
			asd_DAssert(dst != reinterpret_cast<uint8_t*>(sz)); // buf->GetBuffer() null check

			const size_t remain = a_bytes - cp;
			if (reserve >= remain) {
				std::memcpy(dst, src+cp, remain);
				asd_RAssert(buf->SetSize(sz + remain), "fail SetSize({})", sz + remain);
				break;
			}

			std::memcpy(dst, src+cp, reserve);
			cp += reserve;

			asd_RAssert(buf->SetSize(capacity), "SetSize({})", capacity);

			++m_writeOffset;
			asd_DAssert(size() > m_writeOffset);
		}

		m_total_write += a_bytes;
		return a_bytes;
	}


	size_t BufferList::Read(void* a_data /*Out*/,
							const size_t a_bytes)
	{
		if (Readable(a_bytes) == false)
			return 0;

		uint8_t* dst = reinterpret_cast<uint8_t*>(a_data);
		for (size_t cp = 0;;) {
			BufferInterface* buf = at(m_readOffset.Row).get();
			asd_DAssert(buf != nullptr);

			const size_t sz = buf->GetSize() - m_readOffset.Col;
			const uint8_t* src = buf->GetBuffer() + m_readOffset.Col;
			asd_DAssert(src != reinterpret_cast<uint8_t*>(m_readOffset.Col));

			const size_t remain = a_bytes - cp;
			if (sz >= remain) {
				std::memcpy(dst+cp, src, remain);
				m_readOffset.Col += remain;
				break;
			}

			std::memcpy(dst+cp, src, sz);
			cp += sz;

			m_readOffset.Col = 0;
			++m_readOffset.Row;
			asd_DAssert(size() > m_readOffset.Row);
		}

		m_total_read += a_bytes;
		return a_bytes;
	}



	template<>
	Transactional<BufOp::Write>::Transactional(BufferList& a_bufferList)
		: m_bufferList(a_bufferList)
	{
		auto& rb = const_cast<Offset&>(m_rollbackPoint);
		rb.Row = m_bufferList.m_writeOffset;
		if (m_bufferList.size() > rb.Row)
			rb.Col = m_bufferList[rb.Row]->GetSize();
		else
			rb.Col = 0;
	}

	template<>
	Transactional<BufOp::Read>::Transactional(BufferList& a_bufferList)
		: m_bufferList(a_bufferList)
		, m_rollbackPoint(a_bufferList.m_readOffset)
	{
	}

	template<>
	void Transactional<BufOp::Write>::Rollback()
	{
		for (size_t i = 1 + m_rollbackPoint.Row;
			 i <= m_bufferList.m_writeOffset && i < m_bufferList.size();
			 ++i)
		{
			asd_RAssert(m_bufferList[i]->SetSize(0), "fail SetSize(0)");
		}

		bool set = m_bufferList[m_rollbackPoint.Row]->SetSize(m_rollbackPoint.Col);
		asd_RAssert(set, "fail SetSize({})", m_rollbackPoint.Col);
		m_bufferList.m_writeOffset = m_rollbackPoint.Row;
	}

	template<>
	void Transactional<BufOp::Read>::Rollback()
	{
		m_bufferList.m_readOffset = m_rollbackPoint;
	}
}
