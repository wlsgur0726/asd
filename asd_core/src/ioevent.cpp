#include "asd_pch.h"
#include "asd/ioevent.h"
#include "asd/objpool.h"
#include <vector>
#include <unordered_map>


namespace asd
{
	IOEventInterface::~IOEventInterface() asd_noexcept
	{
	}



	inline uintptr_t NewAsyncSocketID() asd_noexcept
	{
		// 소켓값이나 객체의 포인터값을 키로 사용하지 않는 이유는
		// 새로운 객체가 할당될 때
		// 방금전에 제거된 객체의 값이 재사용되어 발생할 수 있는 하이젠버그를
		// 최대한 회피하기 위함이다.
		static std::atomic<uintptr_t> g_id;
		uintptr_t ret;
		do {
			ret = g_id++;
		} while (ret == 0);
		return ret;
	}

	struct AsyncSocketData
	{
		const uintptr_t m_id = NewAsyncSocketID();

		IOEvent m_event; // 등록되어있는 이벤트풀
		mutable Mutex m_sockLock; // 이 소켓의 데이터들을 보호하는 락

		std::deque<Buffer_ptr> m_sendQueue; // 송신 큐
		Buffer_ptr m_recvBuffer; // 수신 버퍼

		Socket::Error m_lastError = 0; // 마지막에 발생한 소켓에러

#if defined(asd_Platform_Windows)
		// N-Send
		ObjectPool<WSAOVERLAPPED, true> m_sendov_pool = ObjectPool<WSAOVERLAPPED, true>(100);
		std::unordered_map<WSAOVERLAPPED*, std::deque<Buffer_ptr>> m_sendProgress;

		// 1-Recv
		WSAOVERLAPPED m_recvov;
#endif
	};



#if defined(asd_Platform_Windows)
	struct PollResult
	{
		DWORD m_error;
		bool m_timeout;
		uintptr_t m_id;
		uint32_t m_transBytes;
		LPOVERLAPPED m_overlapped;
		static_assert(IsEqualType<uintptr_t, ULONG_PTR>::Value, "type missmatch");
		static_assert(IsEqualType<uint32_t, DWORD>::Value, "type missmatch");
	};

	class Poller final
	{
	public:
		// IOCP
		HANDLE m_iocp = NULL;

		Poller(IN uint32_t a_threadCount)
		{
			if (a_threadCount == 0)
				a_threadCount = 1;
			m_iocp = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, a_threadCount);
			if (m_iocp == NULL) {
				auto e = GetLastError();
				asd_Assert(false, "fail CreateIoCompletionPort, GetLastError:{}", e);
				return;
			}
		}


		void Register(REF AsyncSocket* a_sock) asd_noexcept
		{
			auto r = ::CreateIoCompletionPort((HANDLE)a_sock->m_handle,
											  m_iocp,
											  (ULONG_PTR)a_sock->m_data->m_id,
											  0);
			if (r == NULL) {
				a_sock->m_data->m_lastError = (Socket::Error)GetLastError();
				asd_Assert(false, "fail CreateIoCompletionPort, GetLastError:{}", a_sock->m_data->m_lastError);
				return;
			}

			a_sock->m_data->m_lastError = WSARecv(a_sock);
			if (0 != a_sock->m_data->m_lastError) {
				a_sock->Close();
				return;
			}
		}


		int WSARecv(REF AsyncSocket* a_sock) asd_noexcept
		{
			asd_Assert(a_sock->m_data->m_recvBuffer == nullptr, "unknown logic error");
			a_sock->m_data->m_recvBuffer = NewBuffer<asd_BufferList_DefaultReadBufferSize>();

			WSABUF wsabuf;
			wsabuf.buf = (CHAR*)a_sock->m_data->m_recvBuffer->GetBuffer();
			wsabuf.len = (ULONG)a_sock->m_data->m_recvBuffer->Capacity();

			DWORD flags = 0;

			memset(&a_sock->m_data->m_recvov, 0, sizeof(a_sock->m_data->m_recvov));

			int r = ::WSARecv(a_sock->m_handle,
							  &wsabuf,
							  1,
							  NULL,
							  &flags,
							  &a_sock->m_data->m_recvov,
							  NULL);
			if (r != 0) {
				auto e = WSAGetLastError();
				switch (e) {
					case WSA_IO_PENDING:
						break; // 정상
					case WSAENETRESET:
					case WSAESHUTDOWN:
					case WSAECONNABORTED:
					case WSAECONNRESET:
						return e; // 소켓 닫힘, assert 까지 할 필요는 없음
					default:
						asd_Assert(false, "fail WSARecv, WSAGetLastError:{}", e);
						a_sock->Close();
						return e;
				}
			}
			return 0;
		}


		void Unregister(REF AsyncSocket* a_sock) asd_noexcept
		{
			// 소켓을 닫지 않는 이상 IOCP에서 등록해제 시킬 방법이 없다.
			// 어짜피 App단에서 관리하는 목록에서는 빠졌으니 이벤트가 발생하더라도 무시될 것이고,
			// 곧 삭제될 소켓이니 별다른 처리는 하지 않는다.

			//::SetFileCompletionNotificationModes((HANDLE)a_sock->m_handle,
			//									 FILE_SKIP_COMPLETION_PORT_ON_SUCCESS | FILE_SKIP_SET_EVENT_ON_HANDLE);
		}


		void Poll(IN uint32_t a_timeoutMs,
				  OUT PollResult& a_result) asd_noexcept
		{
			auto r = ::GetQueuedCompletionStatus(m_iocp,
												 (LPDWORD)&a_result.m_transBytes,
												 (PULONG_PTR)&a_result.m_id,
												 &a_result.m_overlapped,
												 a_timeoutMs);
			if (r == FALSE) {
				a_result.m_error = GetLastError();
				a_result.m_timeout = a_result.m_overlapped == NULL;
				if (a_result.m_timeout == false)
					asd_Assert(false, "fail GetQueuedCompletionStatus, GetLastError:{}", a_result.m_error);
				return;
			}

			a_result.m_timeout = false;
			a_result.m_error = 0;
		}


		void ProcEvent(IN const PollResult& a_pollResult,
					   REF MtxCtl_asdMutex& a_sockLock,
					   IN AsyncSocket* a_sock) asd_noexcept
		{
			assert(a_pollResult.m_error == 0);

			// close case
			if (a_pollResult.m_transBytes == 0) {
				a_sock->Close();
				a_sock->OnClose(a_sock->m_data->m_lastError);
				return;
			}

			// recv comp case
			if (a_pollResult.m_overlapped == &a_sock->m_data->m_recvov) {
				auto recvedData = std::move(a_sock->m_data->m_recvBuffer);
				recvedData->SetSize(a_pollResult.m_transBytes);

				asd_Assert(a_sockLock.m_recursionCount == 1,
						   "unknown logic error, a_sockLock.m_recursionCount:{}",
						   a_sockLock.m_recursionCount);
				a_sockLock.unlock();
				a_sock->OnRecv(recvedData);

				a_sockLock.lock();
				a_sock->m_data->m_lastError = WSARecv(a_sock);
				if (0 != a_sock->m_data->m_lastError) {
					a_sock->Close();
					a_sock->OnClose(a_sock->m_data->m_lastError);
				}
				return;
			}

			// send comp case
			if (a_sock->m_data->m_sendProgress.erase(a_pollResult.m_overlapped) == 1) {
				a_sock->m_data->m_sendov_pool.Free(a_pollResult.m_overlapped);
				return;
			}
		}


		int Send(REF AsyncSocket* a_sock) asd_noexcept
		{
			if (a_sock->m_data->m_sendQueue.empty())
				return 0;

			WSAOVERLAPPED* ov = a_sock->m_data->m_sendov_pool.Alloc();
			memset(ov, 0, sizeof(*ov));
			auto& buffer = a_sock->m_data->m_sendProgress[ov];
			if (buffer.empty() == false) {
				asd_Assert(false, "unknown logic error");
				return -1;
			}
			buffer = std::move(a_sock->m_data->m_sendQueue);

			thread_local std::vector<WSABUF> t_wsabufs;
			t_wsabufs.clear();
			for (auto& buf : buffer) {
				const auto sz = buf->GetSize();
				if (sz == 0)
					continue;
				WSABUF wsabuf;
				wsabuf.buf = (CHAR*)buf->GetBuffer();
				wsabuf.len = (ULONG)sz;
				asd_Assert(sz == wsabuf.len,
						   "overflow error {}->{}",
						   sz,
						   wsabuf.len);
				t_wsabufs.push_back(wsabuf);
			}
			const DWORD wsaBufCount = (DWORD)t_wsabufs.size();
			asd_Assert(t_wsabufs.size() == wsaBufCount,
					   "overflow error {}->{}",
					   t_wsabufs.size(),
					   wsaBufCount);

			int r = ::WSASend(a_sock->m_handle,
							  t_wsabufs.data(),
							  wsaBufCount,
							  NULL,
							  0,
							  ov,
							  NULL);
			if (r != 0) {
				auto e = WSAGetLastError();
				switch (e) {
					case WSA_IO_PENDING:
						break; // 정상
					case WSAENETRESET:
					case WSAESHUTDOWN:
					case WSAECONNABORTED:
					case WSAECONNRESET:
						return e; // 소켓 닫힘, assert 까지 할 필요는 없음
					case WSAENOBUFS:
					case WSAEWOULDBLOCK:
						// 과도한 send로 버퍼 부족
						asd_Assert(false, "fail WSASend, excessive requests, WSAGetLastError:{}", e);
						return e;
					default:
						asd_Assert(false, "fail WSASend, WSAGetLastError:{}", e);
						a_sock->Close();
						return e;
				}
			}
			return 0;
		}


		bool PostSignal() asd_noexcept
		{
			auto r = ::PostQueuedCompletionStatus(m_iocp, 0, NULL, NULL);
			if (r == FALSE) {
				auto e = GetLastError();
				asd_Assert(false, "fail PostQueuedCompletionStatus, GetLastError:{}", e);
				return false;
			}
			return true;
		}


		~Poller() asd_noexcept
		{
			if (m_iocp != NULL)
				CloseHandle(m_iocp);
		}
	};

#elif defined(asd_Platform_Linux) || defined(asd_Platform_Android)

	struct PollResult
	{
		int m_error;
		bool m_timeout;
		uintptr_t m_id;
	};

	class Poller final
	{
	public:
		// epoll
		Poller(IN uint32_t a_threadCount)
		{
		}
		void Register(REF AsyncSocket* a_sock) asd_noexcept
		{
		}
		void Unregister(REF AsyncSocket* a_sock) asd_noexcept
		{
		}
		void Poll(IN uint32_t a_timeoutMs,
				  OUT PollResult& a_result) asd_noexcept
		{
		}
		void ProcEvent(IN const PollResult& a_pollResult,
					   REF MtxCtl_asdMutex& a_sockLock,
					   IN AsyncSocket* a_sock) asd_noexcept
		{
		}
		int Send(REF AsyncSocket* a_sock) asd_noexcept
		{
			return 0;
		}
		bool PostSignal() asd_noexcept
		{
			return true;
		}
	};

#else
#error This platform is not supported.


#endif



	class IOEventImpl : public IOEventInterface
	{
		friend class AsyncSocket;

	public:
		mutable Mutex								m_ioLock;
		std::atomic_bool							m_run;
		const uint32_t								m_threadCount;
		std::vector<std::thread>					m_threads;
		Poller										m_poller;
		std::unordered_map<uintptr_t, AsyncSocket*>	m_sockets;



		IOEventImpl(IN uint32_t a_threadCount)
			: m_threadCount(a_threadCount)
			, m_poller(a_threadCount)
		{
			m_run = true;
			for (uint32_t i=0; i<a_threadCount; ++i) {
				m_threads.push_back(std::thread([this]()
				{
					while (m_run) {
						int err = Poll(100);
						asd_Assert(err == 0, "fail Poll, err:{}", err);
					}
				}));
			}
		}



		virtual ~IOEventImpl() asd_noexcept
		{
			MtxCtl_asdMutex lock(m_ioLock);
			for (auto sock : m_sockets)
				Unregister(sock.second);
			lock.unlock();

			m_run = false;
			for (auto cnt=m_threads.size(); cnt!=0; --cnt)
				m_poller.PostSignal();
			for (auto& thread : m_threads)
				thread.join();
		}



		virtual int Poll(IN uint32_t a_timeoutSec) asd_noexcept override
		{
			// Wait
			PollResult result;
			m_poller.Poll(a_timeoutSec, result);
			if (result.m_timeout)
				return 0;
			if (result.m_error != 0)
				return (int)result.m_error;

			// Event 처리
			MtxCtl_asdMutex ioLock(m_ioLock);
			auto it = m_sockets.find(result.m_id);
			if (it != m_sockets.end()) {
				AsyncSocket* sock = it->second;
				MtxCtl_asdMutex sockLock(sock->m_data->m_sockLock);
				ioLock.unlock();
				m_poller.ProcEvent(result, sockLock, sock);
			}
			return 0;
		}



		virtual void SendRequest(REF AsyncSocket* a_sock,
								 MOVE std::deque<Buffer_ptr>&& a_data) asd_noexcept override
		{
			MtxCtl_asdMutex sockLock(a_sock->m_data->m_sockLock);

			for (auto& it : a_data)
				a_sock->m_data->m_sendQueue.push_back(std::move(it));

			int r = m_poller.Send(a_sock);
			if (r != 0)
				a_sock->Close();
		}



		virtual void Register(REF AsyncSocket* a_sock) asd_noexcept override
		{
			MtxCtl_asdMutex ioLock(m_ioLock);
			MtxCtl_asdMutex sockLock(a_sock->m_data->m_sockLock);
			if (m_sockets.emplace(a_sock->m_data->m_id, a_sock).second == false) {
				asd_Assert(false, "duplicate Register call");
				return;
			}
			ioLock.unlock();

			// iocp/epoll proc
			m_poller.Register(a_sock);
		}



		virtual void Unregister(IN AsyncSocket* a_sock) asd_noexcept override
		{
			MtxCtl_asdMutex ioLock(m_ioLock);
			MtxCtl_asdMutex sockLock(a_sock->m_data->m_sockLock);
			m_sockets.erase(a_sock->m_data->m_id);

			// iocp/epoll proc
			m_poller.Unregister(a_sock);
		}
	};



	void IOEvent::Init(IN uint32_t a_threadCount /*= Get_HW_Concurrency()*/)
	{
		reset(new IOEventImpl(a_threadCount));
	}



	AsyncSocket::AsyncSocket() asd_noexcept
	{
		m_data.reset(new AsyncSocketData);
	}


	AsyncSocket::~AsyncSocket() asd_noexcept
	{
		if (m_data != nullptr)
			Unregister();
	}


	void AsyncSocket::Register(REF IOEvent& a_ev) asd_noexcept
	{
		Unregister();
		if (a_ev == nullptr)
			return;
		std::atomic_store(&m_data->m_event, a_ev);
		a_ev->Register(this);
	}


	void AsyncSocket::Unregister() asd_noexcept
	{
		auto ev = std::atomic_exchange(&m_data->m_event, IOEvent());
		if (ev == nullptr)
			return;
		ev->Unregister(this);
	}


	void AsyncSocket::Connect(IN const IpAddress& a_dest,
							  IN uint32_t a_timeoutMs /*= 10*1000*/) asd_noexcept
	{
	}


	void AsyncSocket::Listen() asd_noexcept
	{
	}


	void AsyncSocket::Send(MOVE std::deque<Buffer_ptr>&& a_data) asd_noexcept
	{
		auto ev = std::atomic_load(&m_data->m_event);
		if (ev == nullptr)
			return;
		ev->SendRequest(this, std::move(a_data));
	}


	void Close() asd_noexcept
	{

	}
}
