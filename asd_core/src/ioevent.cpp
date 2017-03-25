#include "asd_pch.h"
#include "asd/ioevent.h"
#include "asd/objpool.h"
#include "asd/trace.h"
#include <vector>
#include <unordered_map>
#include <bitset>


#if defined(asd_Platform_Windows)
#	include <MSWSock.h>
#	include <ws2ipdef.h>
#
#elif defined(asd_Platform_Linux)
#	include <sys/epoll.h>
#	include <sys/types.h>
#	include <sys/socket.h>
#	include <sys/uio.h>
#	include <limits.h>
#	include <sys/eventfd.h>
#
#endif


namespace asd
{
#if defined(asd_Platform_Windows)
	class AsyncSocketNative final
	{
	public:
		static_assert(IsEqualType<AsyncSocketHandle::ID, ULONG_PTR>::Value, "SocketID-CompletionKey type missmatch");

		// N-Send
		ObjectPool<WSAOVERLAPPED, NoLock, true> m_sendov_pool = ObjectPool<WSAOVERLAPPED, NoLock, true>(100);
		std::unordered_map<WSAOVERLAPPED*, std::deque<Buffer_ptr>> m_sendProgress;

		// 1-Recv
		WSAOVERLAPPED m_recvov;

		// Listen소켓에서 AcceptEx를 위해 사용
		struct Listening final
		{
			SOCKET m_acceptSock = INVALID_SOCKET;
			DWORD m_bytes = 0;
			uint8_t m_buffer[(sizeof(sockaddr_in6)+16)*2];

			~Listening()
			{
				if (m_acceptSock != INVALID_SOCKET)
					::closesocket(m_acceptSock);
			}
		};
		std::unique_ptr<Listening> m_listening;

		bool ReadyAcceptSocket(IN const Socket& a_sock) asd_noexcept
		{
			m_listening->m_acceptSock = ::socket(IpAddress::ToNativeCode(a_sock.GetAddressFamily()),
												 Socket::ToNativeCode(a_sock.GetSocektType()),
												 0);
			return m_listening->m_acceptSock != INVALID_SOCKET;
		}

		SOCKET AcceptNewSocket() asd_noexcept
		{
			SOCKET newSock = m_listening->m_acceptSock;;
			m_listening->m_acceptSock = INVALID_SOCKET;
			return newSock;
		}
	};


	UniquePtr<AsyncSocketNative> AsyncSocket::InitNative() asd_noexcept
	{
		return UniquePtr<AsyncSocketNative>(new AsyncSocketNative);
	}


#else
	class AsyncSocketNative final
	{
	};

	UniquePtr<AsyncSocketNative> AsyncSocket::InitNative() asd_noexcept
	{
		return nullptr;
	}


#endif



	struct EventInfo final
	{
		AsyncSocket_ptr m_socket;
		bool m_timeout = false;
		bool m_onEvent = false;
		bool m_onSignal = false;

#if defined(asd_Platform_Windows)
		DWORD m_error = 0;
		DWORD m_transBytes = 0;
		LPOVERLAPPED m_overlapped = nullptr;

#elif defined(asd_Platform_Linux) || defined(asd_Platform_Android)
		int m_error = 0;
		epoll_event m_epollEvent;

#else
	#error This platform is not supported.

#endif
	};



	class IOEventInternal
	{
	public:
		mutable Mutex				m_ioLock;
		std::atomic_bool			m_run;
		std::vector<std::thread>	m_threads;
		IOEvent*					m_event;

		IOEventInternal(IN uint32_t a_threadCount,
						REF IOEvent* a_event)
		{
			m_threads.resize(a_threadCount);
			m_event = a_event;
			asd_DAssert(m_event != nullptr);
		}

		virtual ~IOEventInternal() asd_noexcept
		{
			StopThread();
		}

		void StartThread() asd_noexcept
		{
			m_run = true;
			for (auto& t : m_threads) {
				t = std::thread([this]()
				{
					while (m_run)
						Poll(std::numeric_limits<uint32_t>::max());
				});
			}
		}

		void StopThread() asd_noexcept
		{
			if (m_run.exchange(false)) {
				for (auto cnt=m_threads.size(); cnt>0; --cnt)
					PostSignal(nullptr);
				for (auto& thread : m_threads)
					thread.join();
			}
		}

		void Poll(IN uint32_t a_timeoutMs) asd_noexcept
		{
			// Wait
			EventInfo event;
			if (false == Wait(a_timeoutMs, event))
				return;
			if (event.m_timeout)
				return;

			AsyncSocket* sock = event.m_socket.get();
			if (sock == nullptr)
				return;

			// 처리
			auto sockLock = GetLock(sock->m_sockLock);
			if (event.m_onEvent) {
				// IO event
				ProcEvent(event);
			}
			if (event.m_onSignal) {
				// send signal
				auto sendLock = GetLock(sock->m_sendLock);
				sock->m_sendSignal = false;
				switch (sock->m_state) {
					case AsyncSocket::State::Connected:
					case AsyncSocket::State::Closing:
						sock->m_lastError = Send(sock);
						if (sock->m_lastError != 0)
							CloseSocket(sock, true);
						break;
				}
			}
			Poll_Finally(sock);
		}

		virtual bool Register(REF AsyncSocket* a_sock) asd_noexcept
		{
			asd_RAssert(false, "not impl");
			return false;
		}

		virtual bool PostSignal(IN AsyncSocket* a_sock) asd_noexcept
		{
			asd_RAssert(false, "not impl");
			return false;
		}

		virtual bool Wait(IN uint32_t a_timeoutMs,
						  OUT EventInfo& a_event) asd_noexcept
		{
			asd_RAssert(false, "not impl");
			return false;
		}

		virtual void ProcEvent(REF EventInfo& a_event) asd_noexcept
		{
			asd_RAssert(false, "not impl");
		}

		virtual int Connect(REF AsyncSocket* a_sock,
							IN const IpAddress& a_dest) asd_noexcept
		{
			asd_RAssert(false, "not impl");
			return -1;
		}

		virtual int Listen(REF AsyncSocket* a_sock) asd_noexcept
		{
			asd_RAssert(false, "not impl");
			return -1;
		}

		virtual int Send(REF AsyncSocket* a_sock) asd_noexcept
		{
			asd_RAssert(false, "not impl");
			return -1;
		}

		virtual void CloseSocket(REF AsyncSocket* a_sock,
								 IN bool a_hard = false) asd_noexcept
		{
			asd_RAssert(false, "not impl");
		}

		virtual void Poll_Finally(REF AsyncSocket* a_sock) asd_noexcept
		{
			asd_RAssert(false, "not impl");
		}
	};



#if defined(asd_Platform_Windows)
	class IOEventInternal_IOCP final
		: public IOEventInternal
	{
	public:
		HANDLE m_iocp = NULL;


		IOEventInternal_IOCP(IN uint32_t a_threadCount,
							 REF IOEvent* a_event)
			: IOEventInternal(a_threadCount, a_event)
		{
			m_iocp = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE,
											  NULL,
											  NULL,
											  a_threadCount + 1);
			if (m_iocp == NULL) {
				auto e = ::GetLastError();
				asd_RaiseException("fail CreateIoCompletionPort, GetLastError:{}", e);
			}
			StartThread();
		}



		virtual ~IOEventInternal_IOCP() asd_noexcept
		{
			StopThread();
			if (m_iocp != NULL)
				::CloseHandle(m_iocp);
		}



		virtual bool Register(REF AsyncSocket* a_sock) asd_noexcept override
		{
			if (a_sock->m_native == nullptr) {
				a_sock->m_lastError = -1;
				asd_RAssert(false, "empty native data");
				return false;
			}

			auto r = ::CreateIoCompletionPort((HANDLE)a_sock->GetNativeHandle(),
											  m_iocp,
											  (ULONG_PTR)AsyncSocketHandle::GetID(a_sock),
											  0);
			if (r == NULL) {
				a_sock->m_lastError = (Socket::Error)::GetLastError();
				asd_RAssert(false, "fail CreateIoCompletionPort(Register), GetLastError:{}", a_sock->m_lastError);
				return false;
			}
			if (a_sock->m_state == AsyncSocket::State::Connected) {
				a_sock->m_lastError = WSARecv(a_sock);
				if (0 != a_sock->m_lastError) {
					CloseSocket(a_sock);
					return false;
				}
			}
			return true;
		}



		virtual bool PostSignal(IN AsyncSocket* a_sock) asd_noexcept override
		{
			ULONG_PTR id = a_sock != nullptr ? AsyncSocketHandle::GetID(a_sock) : AsyncSocketHandle::Null;
			auto r = ::PostQueuedCompletionStatus(m_iocp, 0, id, nullptr);
			if (r == FALSE) {
				auto e = ::GetLastError();
				asd_RAssert(false, "fail PostQueuedCompletionStatus, GetLastError:{}", e);
				return false;
			}
			return true;
		}



		int WSARecv(REF AsyncSocket* a_sock) asd_noexcept
		{
			asd_RAssert(a_sock->m_recvBuffer == nullptr, "unknown logic error");
			a_sock->m_recvBuffer = NewBuffer<asd_BufferList_DefaultReadBufferSize>();

			WSABUF wsabuf;
			wsabuf.buf = (CHAR*)a_sock->m_recvBuffer->GetBuffer();
			wsabuf.len = (ULONG)a_sock->m_recvBuffer->Capacity();

			DWORD flags = 0;

			auto& ov = a_sock->m_native->m_recvov;
			std::memset(&ov, 0, sizeof(ov));

			int r = ::WSARecv(a_sock->GetNativeHandle(),
							  &wsabuf,
							  1,
							  NULL,
							  &flags,
							  &ov,
							  NULL);
			if (r != 0) {
				auto e = ::WSAGetLastError();
				switch (e) {
					case WSA_IO_PENDING:
						break; // 정상
					case WSAENETRESET:
					case WSAESHUTDOWN:
					case WSAECONNABORTED:
					case WSAECONNRESET:
						return e; // 소켓 닫힘, assert 까지 할 필요는 없음
					default:
						asd_RAssert(false, "fail WSARecv, WSAGetLastError:{}", e);
						return e;
				}
			}
			return 0;
		}



		virtual bool Wait(IN uint32_t a_timeoutMs,
						  OUT EventInfo& a_event) asd_noexcept override
		{
			AsyncSocketHandle::ID id = AsyncSocketHandle::Null;
			auto r = ::GetQueuedCompletionStatus(m_iocp,
												 (LPDWORD)&a_event.m_transBytes,
												 (PULONG_PTR)&id,
												 &a_event.m_overlapped,
												 a_timeoutMs);
			if (r == FALSE) {
				a_event.m_error = ::GetLastError();
				switch (a_event.m_error) {
					case WAIT_TIMEOUT:
						a_event.m_timeout = true;
						return true;
					case ERROR_CONNECTION_REFUSED:
					case ERROR_NETNAME_DELETED:
					case ERROR_SEM_TIMEOUT:
						if (id!=AsyncSocketHandle::Null && a_event.m_overlapped!=nullptr)
							break; // socket error
					default:
						asd_RAssert(false, "polling error, GetLastError:{}", a_event.m_error);
						return false;
				}
			}

			a_event.m_socket = AsyncSocketHandle(id).GetObj();
			if (a_event.m_socket == nullptr)
				return true;

			a_event.m_onEvent = a_event.m_overlapped != nullptr;
			a_event.m_onSignal = a_event.m_overlapped == nullptr;
			return true;
		}



		virtual void ProcEvent(REF EventInfo& a_event) asd_noexcept override
		{
			AsyncSocket* sock = a_event.m_socket.get();

			// error
			if (a_event.m_error != 0) {
				bool sendError = sock->m_native->m_sendProgress.erase(a_event.m_overlapped) == 1;
				bool closed = false;
				switch (a_event.m_error) {
					case ERROR_CONNECTION_REFUSED:
					case ERROR_NETNAME_DELETED:
					case ERROR_SEM_TIMEOUT: {
						DWORD t, f;
						::WSAGetOverlappedResult(sock->GetNativeHandle(),
												 a_event.m_overlapped,
												 &t,
												 FALSE,
												 &f);
						auto e = ::WSAGetLastError();
						switch (e) {
							case WSAETIMEDOUT: // connection timeout
							case WSAECONNREFUSED: // 서버에서 connection 거부 (listen backlog 부족)
								if (sock->m_state == AsyncSocket::State::Closing)
									break;
								asd_RAssert(sock->m_state == AsyncSocket::State::Connecting,
											"invaild socket state : {}",
											(uint8_t)sock->m_state);
								sock->m_lastError = e;
								sock->m_state = AsyncSocket::State::None;
								m_event->OnConnect(sock, sock->m_lastError);
								return; // 지금은 닫지 않는다.
							case WSAECONNABORTED: // 로컬에서 끊음
							case WSAECONNRESET: // 상대방이 끊음 (RST)
								closed = true;
								if (sock->m_state != AsyncSocket::State::Closing)
									sock->m_lastError = e;
								break;
							default:
								asd_RAssert(false, "unknown socket error : {}", e);
								sock->m_lastError = e;
								break;
						}
						break;
					}
					default:
						asd_RAssert(false, "unknown error : {}", a_event.m_error);
						sock->m_lastError = a_event.m_error;
						break;
				}
				if (sendError)
					sock->m_native->m_sendov_pool.Free(a_event.m_overlapped);
				CloseSocket(sock, sendError || closed);
				return;
			}

			// recv complete
			if (a_event.m_overlapped == &sock->m_native->m_recvov) {
				sock->m_lastError = 0;
				switch (sock->m_state) {
					case AsyncSocket::State::Connecting:
						sock->m_state = AsyncSocket::State::Connected;
						m_event->OnConnect(sock, 0);
						break;

					case AsyncSocket::State::Connected:
					case AsyncSocket::State::Closing:
						if (a_event.m_transBytes == 0) {
							// fin
							CloseSocket(sock, true);
						}
						else {
							auto recvedData = std::move(sock->m_recvBuffer);
							asd_RAssert(recvedData->SetSize(a_event.m_transBytes),
										"fail recvedData->SetSize({})",
										a_event.m_transBytes);
							m_event->OnRecv(sock, std::move(recvedData));
						}
						break;

					case AsyncSocket::State::Listening:{
						auto newSock = AsyncSocketHandle().Alloc(sock->m_native->AcceptNewSocket(),
																 sock->GetSocektType(),
																 sock->GetAddressFamily());
						newSock->m_state = AsyncSocket::State::Connected;
						m_event->OnAccept(sock, std::move(newSock));
						break;
					}
				}

				// 유저 콜백 호출 후
				switch (sock->m_state) {
					case AsyncSocket::State::Connected:{
						sock->m_lastError = WSARecv(sock);
						if (0 != sock->m_lastError)
							CloseSocket(sock);
						break;
					}
					case AsyncSocket::State::Listening:{
						sock->m_lastError = AcceptEx(sock);
						if (sock->m_lastError != 0) {
							CloseSocket(sock);
							asd_RAssert(false, "Fatal Error : listening socket had closed!");
						}
						break;
					}
				}
				return;
			}

			// send complete
			if (sock->m_native->m_sendProgress.erase(a_event.m_overlapped) == 1)
				sock->m_native->m_sendov_pool.Free(a_event.m_overlapped);
		}



		virtual int Connect(REF AsyncSocket* a_sock,
							IN const IpAddress& a_dest) asd_noexcept override
		{
			static LPFN_CONNECTEX s_connectEx = nullptr;
			if (s_connectEx == nullptr) {
				LPFN_CONNECTEX connectEx;
				SOCKET sock = ::WSASocket(AF_INET, SOCK_STREAM, 0, NULL, NULL, WSA_FLAG_OVERLAPPED);
				if (sock == INVALID_SOCKET) {
					auto e = ::WSAGetLastError();
					asd_RAssert(false, "fail WSASocket, WSAGetLastError:{}", e);
					return e;
				}
				GUID guid = WSAID_CONNECTEX;
				DWORD bytes = 0;
				int r = ::WSAIoctl(sock,
								   SIO_GET_EXTENSION_FUNCTION_POINTER,
								   &guid,
								   sizeof(guid),
								   &connectEx,
								   sizeof(connectEx),
								   &bytes,
								   NULL,
								   NULL);
				if (r != 0) {
					auto e = ::WSAGetLastError();
					asd_RAssert(false, "fail WSAIoctl, WSAGetLastError:{}", e);
					::closesocket(sock);
					return e;
				}
				::closesocket(sock);
				s_connectEx = connectEx;
			}

			IpAddress bindAddr;
			switch (a_dest.GetAddressFamily()) {
				case AddressFamily::IPv4:{
					sockaddr_in t;
					t.sin_family = PF_INET;
					t.sin_addr.s_addr = INADDR_ANY;
					t.sin_port = 0;
					bindAddr = t;
					break;
				}
				case AddressFamily::IPv6:{
					sockaddr_in6 t;
					t.sin6_family = PF_INET6;
					t.sin6_addr = in6addr_any;
					t.sin6_port = 0;
					t.sin6_scope_id = 0; // TODO
					bindAddr = t;
					break;
				}
				default:
					asd_RAssert(false, "invalid a_dest:{}", a_dest.ToString());
					return -1;
			}
			auto e = a_sock->Bind(bindAddr);
			if (e != 0) {
				asd_RAssert(false, "fail Bind, WSAGetLastError:{}", e);
				return e;
			}

			auto addr = (const sockaddr*)a_dest;
			auto& ov = a_sock->m_native->m_recvov;
			std::memset(&ov, 0, sizeof(ov));
			BOOL r = s_connectEx(a_sock->GetNativeHandle(),
								 addr,
								 a_dest.GetAddrLen(),
								 NULL,
								 0,
								 NULL,
								 &ov);
			if (r == FALSE) {
				auto e = ::WSAGetLastError();
				switch (e) {
					case WSA_IO_PENDING:
						break; // 정상
					default:
						asd_RAssert(false, "fail WSASend, WSAGetLastError:{}", e);
						return e;
				}
			}
			return 0;
		}



		virtual int Listen(REF AsyncSocket* a_sock) asd_noexcept override
		{
			if (a_sock->m_native->m_listening != nullptr) {
				asd_RAssert(false, "already listenig");
				return -1;
			}
			a_sock->m_native->m_listening.reset(new AsyncSocketNative::Listening);
			return AcceptEx(a_sock);
		}



		int AcceptEx(REF AsyncSocket* a_sock) asd_noexcept
		{
			static LPFN_ACCEPTEX s_acceptEx = nullptr;
			if (s_acceptEx == nullptr) {
				LPFN_ACCEPTEX acceptEx;
				SOCKET sock = ::WSASocket(AF_INET, SOCK_STREAM, 0, NULL, NULL, WSA_FLAG_OVERLAPPED);
				if (sock == INVALID_SOCKET) {
					auto e = ::WSAGetLastError();
					asd_RAssert(false, "fail WSASocket, WSAGetLastError:{}", e);
					return e;
				}

				GUID guid = WSAID_ACCEPTEX;
				DWORD bytes = 0;
				int r = ::WSAIoctl(sock,
								   SIO_GET_EXTENSION_FUNCTION_POINTER,
								   &guid,
								   sizeof(guid),
								   &acceptEx,
								   sizeof(acceptEx),
								   &bytes,
								   NULL,
								   NULL);
				if (r != 0) {
					auto e = ::WSAGetLastError();
					asd_RAssert(false, "fail WSAIoctl, WSAGetLastError:{}", e);
					::closesocket(sock);
					return e;
				}
				::closesocket(sock);
				s_acceptEx = acceptEx;
			}

			if (a_sock->m_native->ReadyAcceptSocket(*a_sock) == false) {
				auto e = ::WSAGetLastError();
				asd_RAssert(false, "fail ReadyAcceptSocket, WSAGetLastError:{}", e);
				return e;
			}

			auto& listening = *a_sock->m_native->m_listening;
			auto& ov = a_sock->m_native->m_recvov;
			std::memset(&ov, 0, sizeof(ov));
			BOOL r = s_acceptEx(a_sock->GetNativeHandle(),
								listening.m_acceptSock,
								listening.m_buffer,
								0,
								sizeof(listening.m_buffer) / 2,
								sizeof(listening.m_buffer) / 2,
								&listening.m_bytes,
								&ov);
			if (r == FALSE) {
				auto e = ::WSAGetLastError();
				switch (e) {
					case WSA_IO_PENDING:
						break; // 정상
					default:
						asd_RAssert(false, "fail AcceptEx, WSAGetLastError:{}", e);
						return e;
				}
			}
			return 0;
		}



		virtual int Send(REF AsyncSocket* a_sock) asd_noexcept override
		{
			if (a_sock->m_sendQueue.empty())
				return 0;

			WSAOVERLAPPED* ov = a_sock->m_native->m_sendov_pool.Alloc();
			std::memset(ov, 0, sizeof(*ov));
			auto& progress = a_sock->m_native->m_sendProgress[ov];
			if (progress.empty() == false) {
				asd_RAssert(false, "unknown logic error");
				return -1;
			}
			progress = std::move(a_sock->m_sendQueue);

			thread_local std::vector<WSABUF> t_wsabufs;
			t_wsabufs.resize(progress.size());
			size_t i = 0;
			for (auto& buf : progress) {
				WSABUF& wsabuf = t_wsabufs[i++];
				const auto sz = buf->GetSize();
				wsabuf.buf = (CHAR*)buf->GetBuffer();
				wsabuf.len = (ULONG)sz;
				asd_RAssert(wsabuf.len == sz,
						   "overflow error {}->{}",
						   sz,
						   wsabuf.len);
			}
			const DWORD wsaBufCount = (DWORD)t_wsabufs.size();
			asd_RAssert(wsaBufCount == t_wsabufs.size(),
					   "overflow error {}->{}",
					   t_wsabufs.size(),
					   wsaBufCount);

			while (true) {
				int r = ::WSASend(a_sock->GetNativeHandle(),
								  t_wsabufs.data(),
								  wsaBufCount,
								  NULL,
								  0,
								  ov,
								  NULL);
				if (r != 0) {
					auto e = ::WSAGetLastError();
					if (e == WSA_IO_PENDING)
						goto SUCCESS; // 정상
					switch (e) {
						case WSAENETRESET:
						case WSAESHUTDOWN:
						case WSAECONNABORTED:
						case WSAECONNRESET:
							break; // 소켓 닫힘, assert 까지 할 필요는 없음
						case WSAEWOULDBLOCK:
							// 과도한 send로 버퍼 부족
							a_sock->m_sendSignal = true;
							a_sock->m_sendQueue.emplace_front(std::move(*progress.rbegin()));
							progress.pop_back();
							t_wsabufs.pop_back();
							if (t_wsabufs.empty()) {
								a_sock->m_native->m_sendProgress.erase(ov);
								a_sock->m_native->m_sendov_pool.Free(ov);
								::Sleep(0);
								goto SUCCESS;
							}
							continue;
						default:
							asd_RAssert(false, "fail WSASend, WSAGetLastError:{}", e);
							break;
					}
					a_sock->m_native->m_sendProgress.erase(ov);
					a_sock->m_native->m_sendov_pool.Free(ov);
					return e;
				}
				goto SUCCESS;
			}

		SUCCESS:
			if (a_sock->m_sendSignal)
				PostSignal(a_sock);
			return 0;
		}



		virtual void CloseSocket(REF AsyncSocket* a_sock,
								 IN bool a_hard = false) asd_noexcept override
		{
			if (a_sock->m_state == AsyncSocket::State::Closed)
				return;

			if (a_hard == false) {
				auto sendLock = GetLock(a_sock->m_sendLock);
				if (a_sock->m_state != AsyncSocket::State::Closing)
					::shutdown(a_sock->GetNativeHandle(), SD_RECEIVE);

				if (a_sock->m_sendQueue.size()>0 || a_sock->m_native->m_sendProgress.size()>0) {
					a_sock->m_state = AsyncSocket::State::Closing;
					return;
				}
			}

			a_sock->Socket::Close();
			a_sock->m_state = AsyncSocket::State::Closed;
			m_event->OnClose(a_sock, a_sock->m_lastError);

			auto handle = AsyncSocketHandle::GetHandle(a_sock);
			handle.Free();
		}



		virtual void Poll_Finally(REF AsyncSocket* a_sock) asd_noexcept override
		{
			if (a_sock->m_state == AsyncSocket::State::Closing)
				CloseSocket(a_sock);
		}
	};
	typedef IOEventInternal_IOCP IOEventInternal_NATIVE;



#elif defined(asd_Platform_Linux) || defined(asd_Platform_Android)
	class IOEventInternal_EPOLL final
		: public IOEventInternal
	{
	public:
		static const int ObjCntPerPoll = 1;
		static const uint32_t DefaultPollOptions = EPOLLONESHOT;
		int m_epoll = -1;
		int m_eventfd = -1;


		IOEventInternal_EPOLL(IN uint32_t a_threadCount,
							  REF IOEvent* a_event)
			: IOEventInternal(a_threadCount, a_event)
		{
			m_epoll = ::epoll_create(ObjCntPerPoll);
			if (m_epoll == -1) {
				auto e = errno;
				asd_RaiseException("fail epoll_create, errno:{}", e);
			}

			m_eventfd = ::eventfd(0, EFD_SEMAPHORE | EFD_NONBLOCK);
			if (m_eventfd == -1) {
				auto e = errno;
				asd_RaiseException("fail eventfd, errno:{}", e);
			}

			if (ProcEventfd<true>() == false)
				return;

			StartThread();
		}



		virtual ~IOEventInternal_EPOLL()
		{
			StopThread();
			if (m_epoll >= 0)
				::close(m_epoll);
			if (m_eventfd >= 0)
				::close(m_eventfd);
		}



		virtual bool Register(REF AsyncSocket* a_sock) asd_noexcept override
		{
			epoll_event ev;
			ev.data.ptr = (void*)AsyncSocketHandle::GetID(a_sock);
			ev.events = DefaultPollOptions | EPOLLIN;
			auto r = ::epoll_ctl(m_epoll,
								 EPOLL_CTL_ADD,
								 a_sock->GetNativeHandle(),
								 &ev);
			if (r != 0) {
				auto e = errno;
				asd_RAssert(false, "fail epoll_ctl, errno:{}", e);
				a_sock->m_lastError = e;
				return false;
			}

			int e = a_sock->SetNonblock(true);
			if (e != 0) {
				asd_RAssert(false, "fail SetNonblock, errno:{}", e);
				a_sock->m_lastError = e;
				return false;
			}

			return true;
		}



		virtual bool PostSignal(IN AsyncSocket* a_sock) asd_noexcept override
		{
			if (a_sock == nullptr) {
				ssize_t r;
				uint64_t wakeup = 1;
				while (sizeof(wakeup) != (r=::write(m_eventfd, &wakeup, sizeof(wakeup)))) {
					asd_ChkErrAndRetVal(r>=0, false, "unexpected result, r:{}", r);
					auto e = errno;
					switch (e) {
						case EAGAIN:
							return true;
						case EINTR:
							continue;
					}
					asd_RAssert(false, "fail write to m_eventfd, errno:{}", e);
					return false;
				}
				return true;
			}

			epoll_event ev;
			ev.data.ptr = (void*)AsyncSocketHandle::GetID(a_sock);
			ev.events = DefaultPollOptions | EPOLLOUT;
			auto r = ::epoll_ctl(m_epoll,
								 EPOLL_CTL_MOD,
								 a_sock->GetNativeHandle(),
								 &ev);
			if (r != 0) {
				auto e = errno;
				asd_RAssert(false, "fail epoll_ctl, errno:{}", e);
				return false;
			}
			return true;
		}



		template <bool IS_FIRST>
		bool ProcEventfd()
		{
			bool fail = false;
			if (IS_FIRST == false) {
				ssize_t r;
				uint64_t wakeup;
				while (sizeof(wakeup) != (r=::read(m_eventfd, &wakeup, sizeof(wakeup)))) {
					if (r >= 0)
						asd_RAssert(false, "unexpected result, r:{}", r);
					else {
						auto e = errno;
						if (e == EINTR)
							continue;
						asd_RAssert(false, "fail read from m_eventfd, errno:{}", e);
					}
					fail = true;
					break;
				}
			}

			epoll_event ev;
			ev.data.ptr = (void*)AsyncSocketHandle::Null;
			ev.events = EPOLLONESHOT | EPOLLIN;
			auto r = ::epoll_ctl(m_epoll,
								 IS_FIRST ? EPOLL_CTL_ADD : EPOLL_CTL_MOD,
								 m_eventfd,
								 &ev);
			if (r != 0) {
				auto e = errno;
				if (IS_FIRST)
					asd_RaiseException("fail epoll_ctl, errno:{}", e);
				else
					asd_RAssert(false, "fail epoll_ctl, errno:{}", e);
				return false;
			}

			return !fail;
		}



		virtual bool Wait(IN uint32_t a_timeoutMs,
						  OUT EventInfo& a_event) asd_noexcept override
		{
			auto r = ::epoll_wait(m_epoll,
								  &a_event.m_epollEvent,
								  1,
								  a_timeoutMs);
			if (r > 0) {
				auto id = (AsyncSocketHandle::ID)a_event.m_epollEvent.data.ptr;
				if (id == AsyncSocketHandle::Null)
					return ProcEventfd<false>();
				a_event.m_socket = AsyncSocketHandle(id).GetObj();
				if (a_event.m_socket == nullptr)
					return true;
				if (a_event.m_socket->m_state == AsyncSocket::State::Connecting)
					a_event.m_onEvent = a_event.m_epollEvent.events & EPOLLOUT;
				else {
					a_event.m_onEvent = a_event.m_epollEvent.events & EPOLLIN;
					a_event.m_onSignal = a_event.m_epollEvent.events & EPOLLOUT;
				}
				return true;
			}
			else if (r == 0) {
				a_event.m_timeout = true;
				return true;
			}

			a_event.m_error = errno;
			if (a_event.m_error == EINTR)
				return true;
			asd_RAssert(false, "polling error, errno:{}", a_event.m_error);
			return false;
		}



		int GetSocketError(IN AsyncSocket* a_sock) asd_noexcept
		{
			int err = -1;
			a_sock->GetSockOpt_Error(err);
			return err;
		}



		virtual void ProcEvent(REF EventInfo& a_event) asd_noexcept override
		{
			AsyncSocket* sock = a_event.m_socket.get();

			// error
			if (EPOLLERR & a_event.m_epollEvent.events) {
				int e = GetSocketError(sock);
				switch (e) {
					default:
						asd_RAssert(false, "unknown socket error, errno:{}", e);
						break;
				}
				if (sock->m_state != AsyncSocket::State::Closing)
					sock->m_lastError = e;
				CloseSocket(sock);
				return;
			}

			// connected
			if (a_event.m_socket->m_state == AsyncSocket::State::Connecting) {
				asd_RAssert(EPOLLOUT & a_event.m_epollEvent.events, "unknown logic error");
				int e = GetSocketError(sock);
				if (e == 0) {
					a_event.m_socket->m_state = AsyncSocket::State::Connected;
					sock->m_recvBuffer = NewBuffer<asd_BufferList_DefaultReadBufferSize>();
					m_event->OnConnect(sock, 0);
				}
				else {
					switch (e) {
						default:
							asd_RAssert(false, "unknown socket error, errno:{}", e);
							break;
					}
					sock->m_lastError = e;
					sock->m_state = AsyncSocket::State::None;
					m_event->OnConnect(sock, e);
				}
				return;
			}

			// recv
			sock->m_lastError = 0;
			while (sock->m_state == AsyncSocket::State::Connected) {
				if (sock->m_recvBuffer == nullptr) {
					asd_RAssert(false, "unknown logic error");
					sock->m_lastError = -1;
					CloseSocket(sock, true);
					return;
				}
				auto r = ::recv(sock->GetNativeHandle(),
								sock->m_recvBuffer->GetBuffer(),
								sock->m_recvBuffer->Capacity(),
								0);
				if (r > 0) {
					// success
					auto recvedData = std::move(sock->m_recvBuffer);
					asd_RAssert(recvedData->SetSize(r), "fail recvedData->SetSize({})", r);
					m_event->OnRecv(sock, std::move(recvedData));

					// 유저 콜백 호출 후
					if (sock->m_state == AsyncSocket::State::Connected)
						sock->m_recvBuffer = NewBuffer<asd_BufferList_DefaultReadBufferSize>();
					return;
				}
				else if (r == 0) {
					// fin
					CloseSocket(sock, true);
					return;
				}
				else {
					// error
					auto e = errno;
					switch (e) {
						case EAGAIN: // 전부 읽었음
							return;
						case EINTR: // 인터럽트
							continue;
						case ECONNABORTED:
						case ECONNRESET: // 상대방이 끊음 (RST)
							CloseSocket(sock, true);
							return;
						default:
							asd_RAssert(false, "fail recv, errno:{}", e);
							sock->m_lastError = e;
							CloseSocket(sock);
							return;
					}
				}
				return;
			}

			// listen
			while (sock->m_state == AsyncSocket::State::Listening) {
				auto newSock = AsyncSocketHandle().Alloc();
				IpAddress addr;
				auto e = sock->Accept(*newSock, addr);
				switch (e) {
					case 0:
						newSock->m_state = AsyncSocket::State::Connected;
						newSock->m_recvBuffer = NewBuffer<asd_BufferList_DefaultReadBufferSize>();
						m_event->OnAccept(sock, std::move(newSock));
						return;
					case EAGAIN:
						return;
					case EINTR: // 인터럽트
						continue;
					default:
						asd_RAssert(false, "fail Accept, errno:{}", e);
						sock->m_lastError = e;
						CloseSocket(sock);
						return;
				}
				return;
			}
		}



		virtual int Connect(REF AsyncSocket* a_sock,
							IN const IpAddress& a_dest) asd_noexcept override
		{
			if (PostSignal(a_sock) == false)
				return -1;

			int e = a_sock->Socket::Connect(a_dest);
			switch (e) {
				case 0:
					asd_RAssert(false, "??");
				case EINPROGRESS:
					return 0;
			}
			asd_RAssert(false, "fail Connect, errno:{}", e);
			return e;
		}



		virtual int Listen(REF AsyncSocket* a_sock) asd_noexcept override
		{
			// 딱히 할게 없음
			return 0;
		}



		virtual int Send(REF AsyncSocket* a_sock) asd_noexcept override
		{
			thread_local std::vector<iovec> t_iovec;
			const size_t count = std::min(a_sock->m_sendQueue.size(), (size_t)IOV_MAX);
			t_iovec.resize(count);
			size_t i = 0;
			for (auto& data : a_sock->m_sendQueue) {
				auto& iov = t_iovec[i++];
				iov.iov_base = data->GetBuffer();
				iov.iov_len = data->GetSize();
			}

			while (t_iovec.size() > 0) {
				auto r = writev(a_sock->GetNativeHandle(),
								t_iovec.data(),
								t_iovec.size());
				if (r == -1) {
					auto e = errno;
					switch (e) {
						case EINTR: // 인터럽트
							continue;
						case EAGAIN: // 과도한 Send로 인해 송신버퍼 부족
							a_sock->m_sendSignal = true;
							t_iovec.pop_back();
							continue;
						case EPIPE: // 상대방 연결 끊김
							break;
						default:
							asd_RAssert(false, "fail writev, errno:{}", e);
							break;
					}
					return e;
				}
				break;
			}

			for (i=0; i<t_iovec.size(); ++i)
				a_sock->m_sendQueue.pop_front();

			return 0;
		}



		virtual void CloseSocket(REF AsyncSocket* a_sock,
								 IN bool a_hard = false) asd_noexcept override
		{
			if (a_sock->m_state == AsyncSocket::State::Closed)
				return;

			if (a_hard == false) {
				auto sendLock = GetLock(a_sock->m_sendLock);
				if (a_sock->m_state != AsyncSocket::State::Closing)
					::shutdown(a_sock->GetNativeHandle(), SHUT_RD);

				if (a_sock->m_sendQueue.size() > 0) {
					a_sock->m_state = AsyncSocket::State::Closing;
					return;
				}
			}

			a_sock->Socket::Close();
			a_sock->m_state = AsyncSocket::State::Closed;
			m_event->OnClose(a_sock, a_sock->m_lastError);

			auto handle = AsyncSocketHandle::GetHandle(a_sock);
			handle.Free();
		}



		virtual void Poll_Finally(REF AsyncSocket* a_sock) asd_noexcept override
		{
			if (a_sock->m_state == AsyncSocket::State::Closed)
				return;

			if (DefaultPollOptions & EPOLLONESHOT) {
				epoll_event ev;
				ev.data.ptr = (void*)AsyncSocketHandle::GetID(a_sock);
				ev.events = DefaultPollOptions | EPOLLIN;
				if (a_sock->m_sendSignal)
					ev.events |= EPOLLOUT;
				auto r = ::epoll_ctl(m_epoll,
									 EPOLL_CTL_MOD,
									 a_sock->GetNativeHandle(),
									 &ev);
				if (r != 0) {
					auto e  = errno;
					asd_RAssert(false, "fail epoll_ctl, errno:{}", e);
					a_sock->m_lastError = e;
				}
			}

			if (a_sock->m_state == AsyncSocket::State::Closing)
				CloseSocket(a_sock);
		}
	};
	typedef IOEventInternal_EPOLL IOEventInternal_NATIVE;



#else
	#error This platform is not supported.

#endif



	IOEvent::~IOEvent() asd_noexcept
	{
		Stop();
	}


	void IOEvent::Start(IN uint32_t a_threadCount /*= Get_HW_Concurrency()*/)
	{
		reset(new IOEventInternal_NATIVE(a_threadCount, this));
	}


	void IOEvent::Stop() asd_noexcept
	{
		std::shared_ptr<IOEventInternal> internal = std::move(*this);
		if (internal != nullptr)
			internal->StopThread();
	}


	bool IOEvent::Register(IN AsyncSocket_ptr& a_sock) asd_noexcept
	{
		auto internal = get();
		if (internal == nullptr)
			return false;
		if (a_sock == nullptr)
			return false;

		auto sockLock = GetLock(a_sock->m_sockLock);

		auto e = a_sock->Init();
		if (e != 0) {
			asd_RAssert(false, "fail socket init, e:{}", e);
			return false;
		}

		std::shared_ptr<IOEventInternal> null;
		bool set = std::atomic_compare_exchange_strong(&a_sock->m_event, &null, *this);
		if (set == false)
			return false;

		if (internal->Register(a_sock.get()) == false) {
			a_sock->m_event.reset();
			return false;
		}
		return true;
	}


	void IOEvent::Poll(IN uint32_t a_timeoutSec) asd_noexcept
	{
		auto internal = get();
		if (internal != nullptr)
			internal->Poll(a_timeoutSec);
	}



	void AsyncSocket::Connect(IN const IpAddress& a_dest,
							  IN uint32_t a_timeoutMs /*= 10*1000*/) asd_noexcept
	{
		auto ev = std::atomic_load(&m_event);
		if (ev == nullptr) {
			asd_RAssert(false, "not registered socket");
			return;
		}

		auto sockLock = GetLock(m_sockLock);

		if (m_state != AsyncSocket::State::None) {
			asd_RAssert(false, "invalid socket state : {}", (uint8_t)m_state);
			ev->m_event->OnConnect(this, -1);
			return;
		}

		m_state = AsyncSocket::State::Connecting;
		m_lastError = ev->Connect(this, a_dest);
		if (m_lastError != 0) {
			m_state = AsyncSocket::State::None;
			ev->m_event->OnConnect(this, m_lastError);
			return;
		}
	}


	bool AsyncSocket::Listen(IN const IpAddress& a_bind,
							 IN int a_backlog /*= 1024*/) asd_noexcept
	{
		auto ev = std::atomic_load(&m_event);
		if (ev == nullptr) {
			asd_RAssert(false, "not registered socket");
			return false;
		}

		auto sockLock = GetLock(m_sockLock);

		if (m_state != AsyncSocket::State::None) {
			asd_RAssert(false, "invalid socket state : {}", (uint8_t)m_state);
			return false;
		}

		auto e = Bind(a_bind);
		if (e != 0) {
			asd_RAssert(false, "fail Socket::Bind({}), e:{}", a_bind.ToString(), e);
			return false;
		}

		e = Socket::Listen(a_backlog);
		if (e != 0) {
			asd_RAssert(false, "fail Socket::Listen({}), e:{}", a_backlog, e);
			return false;
		}

		e = ev->Listen(this);
		if (0 != 0) {
			asd_RAssert(false, "fail IOEventInternal::Listen(), e:{}", e);
			return false;
		}

		m_state = AsyncSocket::State::Listening;
		return true;
	}


	bool AsyncSocket::Send(MOVE std::deque<Buffer_ptr>&& a_data) asd_noexcept
	{
		auto ev = std::atomic_load(&m_event);
		if (ev == nullptr) {
			asd_RAssert(false, "not registered socket");
			return false;
		}

		auto sendLock = GetLock(m_sendLock);
		if (m_state != AsyncSocket::State::Connected)
			return false;

		size_t count = 0;
		for (auto& it : a_data) {
			if (it == nullptr)
				continue;
			m_sendQueue.emplace_back(std::move(it));
			++count;
		}
		a_data.clear();

		if (count == 0)
			return true;

		if (m_sendSignal == false) {
			if (ev->PostSignal(this))
				m_sendSignal = true;
			else
				asd_RAssert(false, "fail PostSignal, ID:{}", AsyncSocketHandle::GetID(this));
		}
		return true;
	}


	void AsyncSocket::Close() asd_noexcept
	{
		std::shared_ptr<IOEventInternal> null;
		auto ev = std::atomic_exchange(&m_event, null);

		auto sockLock = GetLock(m_sockLock);
		m_lastError = 0;
		if (ev != nullptr)
			ev->CloseSocket(this, true);
		else
			Socket::Close();
	}


	AsyncSocket::~AsyncSocket() asd_noexcept
	{
		Close();
	}
}
