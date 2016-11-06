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
#
#endif


namespace asd
{
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


	enum class SockState : uint8_t
	{
		None,
		Connecting,
		Connected,
		Listening,
		Closing,
		Closed,
	};

	class AsyncSocketInternal : public Socket
	{
	public:
		inline AsyncSocketInternal(IN AddressFamily a_addressFamily,
								   IN Socket::Type a_socketType)
			: Socket(a_addressFamily, a_socketType) {}

		// 이 소켓의 데이터들을 보호하는 락
		mutable Mutex m_sockLock;

		// 콜백 호출을 위한 포인터
		AsyncSocket* m_interface = nullptr;

		// 유니크하고 재사용주기가 긴 ID
		const uintptr_t m_id = NewAsyncSocketID();

		// 상태
		SockState m_state = SockState::None;

		// 이 소켓이 등록되어있는 이벤트풀
		IOEvent m_event;

		// 수신 버퍼
		Buffer_ptr m_recvBuffer;

		// 마지막에 발생한 소켓에러
		Socket::Error m_lastError = 0;

		// m_sendQueue와 m_sendSignal을 보호하는 락
		mutable Mutex m_sendLock;

		// 송신 큐
		std::deque<Buffer_ptr> m_sendQueue;

		// IO 쓰레드에게 송신 요청 전달하는 동안 true로 셋팅 (중복요청 방지를 위함)
		bool m_sendSignal = false;

#if defined(asd_Platform_Windows)
		// N-Send
		ObjectPool<WSAOVERLAPPED, true> m_sendov_pool = ObjectPool<WSAOVERLAPPED, true>(100);
		std::unordered_map<WSAOVERLAPPED*, std::deque<Buffer_ptr>> m_sendProgress;

		// 1-Recv
		WSAOVERLAPPED m_recvov;

		// Listen소켓에서 AcceptEx를 위해 사용
		struct Listening
		{
			SOCKET m_acceptSock = INVALID_SOCKET;
			DWORD m_bytes = 0;
			uint8_t m_buffer[(sizeof(sockaddr_in6)+16)*2];
		};
		std::unique_ptr<Listening> m_listening;

		bool ReadyAcceptSocket() asd_noexcept
		{
			m_listening->m_acceptSock = ::socket(IpAddress::ToNativeCode(m_addressFamily),
												 Socket::ToNativeCode(m_socketType),
												 0);
			return m_listening->m_acceptSock != INVALID_SOCKET;
		}

		Socket AcceptNewSocket() asd_noexcept
		{
			Socket s(m_addressFamily, m_socketType);
			s.m_handle = m_listening->m_acceptSock;
			m_listening->m_acceptSock = INVALID_SOCKET;
			return s;
		}
#endif
	};
	typedef std::shared_ptr<AsyncSocketInternal> AsyncSocketInternal_ptr;



	struct EventInfo final
	{
		AsyncSocketInternal_ptr m_socket = nullptr;
		bool m_timeout = false;
		bool m_onEvent = false;
		bool m_onSignal = false;

#if defined(asd_Platform_Windows)
		static_assert(IsEqualType<uintptr_t, ULONG_PTR>::Value, "SocketID-CompletionKey type missmatch");
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
		mutable Mutex											m_ioLock;
		std::atomic_bool										m_run;
		std::vector<std::thread>								m_threads;
		std::unordered_map<uintptr_t, AsyncSocketInternal_ptr>	m_sockets;

		IOEventInternal(IN uint32_t a_threadCount)
		{
			m_threads.resize(a_threadCount);
		}

		virtual ~IOEventInternal() asd_noexcept
		{
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
			auto ioLock = GetLock(m_ioLock);
			for (auto it : m_sockets)
				it.second->Socket::Close();
			m_sockets.clear();
			ioLock.unlock();

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

			AsyncSocketInternal* sock = event.m_socket.get();
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
					case SockState::Connected:
					case SockState::Closing:
						sock->m_lastError = Send(sock);
						if (sock->m_lastError != 0)
							CloseSocket(sock, true);
						break;
				}
			}
			Poll_Finally(sock);
		}

		virtual bool Register(REF AsyncSocketInternal* a_sock) asd_noexcept
		{
			asd_Assert(false, "not impl");
			return false;
		}

		virtual bool PostSignal(IN AsyncSocketInternal* a_sock) asd_noexcept
		{
			asd_Assert(false, "not impl");
			return false;
		}

		virtual bool Wait(IN uint32_t a_timeoutMs,
						  OUT EventInfo& a_event) asd_noexcept
		{
			asd_Assert(false, "not impl");
			return false;
		}

		virtual void ProcEvent(REF EventInfo& a_event) asd_noexcept
		{
			asd_Assert(false, "not impl");
		}

		virtual int Connect(REF AsyncSocketInternal* a_sock,
							IN const IpAddress& a_dest) asd_noexcept
		{
			asd_Assert(false, "not impl");
			return -1;
		}

		virtual int Listen(REF AsyncSocketInternal* a_sock) asd_noexcept
		{
			asd_Assert(false, "not impl");
			return -1;
		}

		virtual int Send(REF AsyncSocketInternal* a_sock) asd_noexcept
		{
			asd_Assert(false, "not impl");
			return -1;
		}

		virtual void CloseSocket(REF AsyncSocketInternal* a_sock,
								 IN bool a_hard = false) asd_noexcept
		{
			asd_Assert(false, "not impl");
		}

		virtual void Poll_Finally(REF AsyncSocketInternal* a_sock) asd_noexcept
		{
			asd_Assert(false, "not impl");
		}
	};



#if defined(asd_Platform_Windows)
	class IOEventInternal_IOCP final
		: public IOEventInternal
	{
	public:
		HANDLE m_iocp = NULL;


		IOEventInternal_IOCP(IN uint32_t a_threadCount)
			: IOEventInternal(a_threadCount)
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



		virtual bool Register(REF AsyncSocketInternal* a_sock) asd_noexcept override
		{
			auto r = ::CreateIoCompletionPort((HANDLE)a_sock->GetNativeHandle(),
											  m_iocp,
											  (ULONG_PTR)a_sock->m_id,
											  0);
			if (r == NULL) {
				a_sock->m_lastError = (Socket::Error)::GetLastError();
				asd_Assert(false, "fail CreateIoCompletionPort(Register), GetLastError:{}", a_sock->m_lastError);
				return false;
			}
			if (a_sock->m_state == SockState::Connected) {
				a_sock->m_lastError = WSARecv(a_sock);
				if (0 != a_sock->m_lastError) {
					CloseSocket(a_sock);
					return false;
				}
			}
			return true;
		}



		virtual bool PostSignal(IN AsyncSocketInternal* a_sock) asd_noexcept override
		{
			auto r = ::PostQueuedCompletionStatus(m_iocp,
												  0,
												  a_sock!=nullptr ? (ULONG_PTR)a_sock->m_id : 0,
												  nullptr);
			if (r == FALSE) {
				auto e = GetLastError();
				asd_Assert(false, "fail PostQueuedCompletionStatus, GetLastError:{}", e);
				return false;
			}
			return true;
		}



		int WSARecv(REF AsyncSocketInternal* a_sock) asd_noexcept
		{
			asd_Assert(a_sock->m_recvBuffer == nullptr, "unknown logic error");
			a_sock->m_recvBuffer = NewBuffer<asd_BufferList_DefaultReadBufferSize>();

			WSABUF wsabuf;
			wsabuf.buf = (CHAR*)a_sock->m_recvBuffer->GetBuffer();
			wsabuf.len = (ULONG)a_sock->m_recvBuffer->Capacity();

			DWORD flags = 0;

			memset(&a_sock->m_recvov, 0, sizeof(a_sock->m_recvov));

			int r = ::WSARecv(a_sock->GetNativeHandle(),
							  &wsabuf,
							  1,
							  NULL,
							  &flags,
							  &a_sock->m_recvov,
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
						asd_Assert(false, "fail WSARecv, WSAGetLastError:{}", e);
						return e;
				}
			}
			return 0;
		}



		virtual bool Wait(IN uint32_t a_timeoutMs,
						  OUT EventInfo& a_event) asd_noexcept override
		{
			uintptr_t id = 0;
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
						if (id!=0 && a_event.m_overlapped!=nullptr)
							break; // socket error
					default:
						asd_Assert(false, "polling error, GetLastError:{}", a_event.m_error);
						return false;
				}
			}

			auto ioLock = GetLock(m_ioLock);
			auto it = m_sockets.find(id);
			if (it == m_sockets.end())
				return true;
			a_event.m_socket = it->second;
			ioLock.unlock();
			a_event.m_onEvent = a_event.m_overlapped != nullptr;
			a_event.m_onSignal = a_event.m_overlapped == nullptr;
			return true;
		}



		virtual void ProcEvent(REF EventInfo& a_event) asd_noexcept override
		{
			AsyncSocketInternal* sock = a_event.m_socket.get();

			// error
			if (a_event.m_error != 0) {
				bool sendError = sock->m_sendProgress.erase(a_event.m_overlapped) == 1;
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
								if (sock->m_state == SockState::Closing)
									break;
								asd_Assert(sock->m_state == SockState::Connecting,
										   "invaild socket state : {}",
										   (uint8_t)sock->m_state);
								sock->m_lastError = e;
								sock->m_state = SockState::None;
								if (sock->m_interface != nullptr)
									sock->m_interface->OnConnect(sock->m_lastError);
								return; // 지금은 닫지 않는다.
							case WSAECONNABORTED: // 로컬에서 끊음
							case WSAECONNRESET: // 상대방이 끊음 (RST)
								closed = true;
								if (sock->m_state != SockState::Closing)
									sock->m_lastError = e;
								break;
							default:
								asd_Assert(false, "unknown socket error : {}", e);
								sock->m_lastError = e;
								break;
						}
						break;
					}
					default:
						asd_Assert(false, "unknown error : {}", a_event.m_error);
						sock->m_lastError = a_event.m_error;
						break;
				}
				if (sendError)
					sock->m_sendov_pool.Free(a_event.m_overlapped);
				CloseSocket(sock, sendError || closed);
				return;
			}

			// recv complete
			if (a_event.m_overlapped == &sock->m_recvov) {
				sock->m_lastError = 0;
				switch (sock->m_state) {
					case SockState::Connecting:
						sock->m_state = SockState::Connected;
						if (sock->m_interface != nullptr)
							sock->m_interface->OnConnect(0);
						break;

					case SockState::Connected:
					case SockState::Closing:
						if (a_event.m_transBytes == 0) {
							// fin
							CloseSocket(sock, true);
						}
						else {
							auto recvedData = std::move(sock->m_recvBuffer);
							asd_Assert(recvedData->SetSize(a_event.m_transBytes),
									   "fail recvedData->SetSize({})",
									   a_event.m_transBytes);
							if (sock->m_interface != nullptr)
								sock->m_interface->OnRecv(std::move(recvedData));
						}
						break;

					case SockState::Listening:{
						AsyncSocket newSock;
						auto& cast = newSock.CastToSocket();
						cast = std::move(sock->AcceptNewSocket());
						newSock.m_internal->m_state = SockState::Connected;
						if (sock->m_interface != nullptr)
							sock->m_interface->OnAccept(std::move(newSock));
						break;
					}
				}

				// 유저 콜백 호출 후
				switch (sock->m_state) {
					case SockState::Connected:
						sock->m_lastError = WSARecv(sock);
						if (0 != sock->m_lastError)
							CloseSocket(sock);
						break;

					case SockState::Listening:
						sock->m_lastError = AcceptEx(sock);
						if (sock->m_lastError != 0) {
							CloseSocket(sock);
							asd_Assert(false, "Fatal Error : listening socket had closed!");
						}
						break;
				}
				return;
			}

			// send complete
			if (sock->m_sendProgress.erase(a_event.m_overlapped) == 1)
				sock->m_sendov_pool.Free(a_event.m_overlapped);
		}



		virtual int Connect(REF AsyncSocketInternal* a_sock,
							IN const IpAddress& a_dest) asd_noexcept override
		{
			static LPFN_CONNECTEX ConnectEx = nullptr;
			if (ConnectEx == nullptr) {
				SOCKET sock = ::WSASocket(AF_INET, SOCK_STREAM, 0, NULL, NULL, WSA_FLAG_OVERLAPPED);
				if (sock == INVALID_SOCKET) {
					auto e = ::WSAGetLastError();
					asd_Assert(false, "fail WSASocket, WSAGetLastError:{}", e);
					return e;
				}
				GUID  guid = WSAID_CONNECTEX;
				DWORD bytes = 0;
				int r = ::WSAIoctl(sock,
								   SIO_GET_EXTENSION_FUNCTION_POINTER,
								   &guid,
								   sizeof(guid),
								   &ConnectEx,
								   sizeof(ConnectEx),
								   &bytes,
								   NULL,
								   NULL);
				if (r != 0) {
					auto e = ::WSAGetLastError();
					asd_Assert(false, "fail WSAIoctl, WSAGetLastError:{}", e);
					::closesocket(sock);
					return e;
				}
				::closesocket(sock);
				if (ConnectEx == nullptr) {
					asd_Assert(false, "ConnectEx pointer is null");
					return -1;
				}
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
					asd_Assert(false, "invalid a_dest:{}", a_dest.ToString());
					return -1;
			}
			auto e = a_sock->Bind(bindAddr);
			if (e != 0) {
				asd_Assert(false, "fail Bind, WSAGetLastError:{}", e);
				return e;
			}

			auto addr = (const sockaddr*)a_dest;
			memset(&a_sock->m_recvov, 0, sizeof(a_sock->m_recvov));
			BOOL r = ConnectEx(a_sock->GetNativeHandle(),
							   addr,
							   a_dest.GetAddrLen(),
							   NULL,
							   0,
							   NULL,
							   &a_sock->m_recvov);
			if (r == FALSE) {
				auto e = ::WSAGetLastError();
				switch (e) {
					case WSA_IO_PENDING:
						break; // 정상
					default:
						asd_Assert(false, "fail WSASend, WSAGetLastError:{}", e);
						return e;
				}
			}
			return 0;
		}



		virtual int Listen(REF AsyncSocketInternal* a_sock) asd_noexcept override
		{
			if (a_sock->m_listening != nullptr) {
				asd_Assert(false, "already listenig");
				return -1;
			}
			a_sock->m_listening.reset(new AsyncSocketInternal::Listening);
			return AcceptEx(a_sock);
		}



		int AcceptEx(REF AsyncSocketInternal* a_sock) asd_noexcept
		{
			static LPFN_ACCEPTEX AcceptEx = nullptr;
			if (AcceptEx == nullptr) {
				SOCKET sock = ::WSASocket(AF_INET, SOCK_STREAM, 0, NULL, NULL, WSA_FLAG_OVERLAPPED);
				if (sock == INVALID_SOCKET) {
					auto e = ::WSAGetLastError();
					asd_Assert(false, "fail WSASocket, WSAGetLastError:{}", e);
					return e;
				}

				GUID  guid = WSAID_ACCEPTEX;
				DWORD bytes = 0;

				int r = ::WSAIoctl(sock,
								   SIO_GET_EXTENSION_FUNCTION_POINTER,
								   &guid,
								   sizeof(guid),
								   &AcceptEx,
								   sizeof(AcceptEx),
								   &bytes,
								   NULL,
								   NULL);
				if (r != 0) {
					auto e = ::WSAGetLastError();
					asd_Assert(false, "fail WSAIoctl, WSAGetLastError:{}", e);
					::closesocket(sock);
					return e;
				}
				::closesocket(sock);
				if (AcceptEx == nullptr) {
					asd_Assert(false, "AcceptEx pointer is null");
					return -1;
				}
			}

			if (a_sock->ReadyAcceptSocket() == false) {
				auto e = ::WSAGetLastError();
				asd_Assert(false, "fail ReadyAcceptSocket, WSAGetLastError:{}", e);
				return e;
			}

			memset(&a_sock->m_recvov, 0, sizeof(a_sock->m_recvov));
			BOOL r = AcceptEx(a_sock->GetNativeHandle(),
							  a_sock->m_listening->m_acceptSock,
							  a_sock->m_listening->m_buffer,
							  0,
							  sizeof(a_sock->m_listening->m_buffer) / 2,
							  sizeof(a_sock->m_listening->m_buffer) / 2,
							  &a_sock->m_listening->m_bytes,
							  &a_sock->m_recvov);
			if (r == FALSE) {
				auto e = ::WSAGetLastError();
				switch (e) {
					case WSA_IO_PENDING:
						break; // 정상
					default:
						asd_Assert(false, "fail AcceptEx, WSAGetLastError:{}", e);
						return e;
				}
			}
			return 0;
		}



		virtual int Send(REF AsyncSocketInternal* a_sock) asd_noexcept override
		{
			if (a_sock->m_sendQueue.empty())
				return 0;

			WSAOVERLAPPED* ov = a_sock->m_sendov_pool.Alloc();
			memset(ov, 0, sizeof(*ov));
			auto& progress = a_sock->m_sendProgress[ov];
			if (progress.empty() == false) {
				asd_Assert(false, "unknown logic error");
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
				asd_Assert(wsabuf.len == sz,
						   "overflow error {}->{}",
						   sz,
						   wsabuf.len);
			}
			const DWORD wsaBufCount = (DWORD)t_wsabufs.size();
			asd_Assert(wsaBufCount == t_wsabufs.size(),
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
							a_sock->m_sendQueue.push_front(std::move(*progress.rbegin()));
							progress.pop_back();
							t_wsabufs.pop_back();
							if (t_wsabufs.empty()) {
								a_sock->m_sendProgress.erase(ov);
								a_sock->m_sendov_pool.Free(ov);
								::Sleep(0);
								goto SUCCESS;
							}
							continue;
						default:
							asd_Assert(false, "fail WSASend, WSAGetLastError:{}", e);
							break;
					}
					a_sock->m_sendProgress.erase(ov);
					a_sock->m_sendov_pool.Free(ov);
					return e;
				}
				goto SUCCESS;
			}

		SUCCESS:
			if (a_sock->m_sendSignal)
				PostSignal(a_sock);
			return 0;
		}



		virtual void CloseSocket(REF AsyncSocketInternal* a_sock,
								 IN bool a_hard = false) asd_noexcept override
		{
			if (a_sock->m_state == SockState::Closed)
				return;

			if (a_hard == false) {
				auto sendLock = GetLock(a_sock->m_sendLock);
				if (a_sock->m_state != SockState::Closing)
					::shutdown(a_sock->GetNativeHandle(), SD_RECEIVE);

				if (a_sock->m_sendQueue.size()>0 || a_sock->m_sendProgress.size()>0) {
					a_sock->m_state = SockState::Closing;
					return;
				}
			}

			a_sock->Close();
			a_sock->m_state = SockState::Closed;
			if (a_sock->m_interface != nullptr) {
				a_sock->m_interface->OnClose(a_sock->m_lastError);
				a_sock->m_interface = nullptr;
			}

			auto ioLock = GetLock(m_ioLock);
			m_sockets.erase(a_sock->m_id);
		}



		virtual void Poll_Finally(REF AsyncSocketInternal* a_sock) asd_noexcept override
		{
			if (a_sock->m_state == SockState::Closing)
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


		IOEventInternal_EPOLL(IN uint32_t a_threadCount)
			: IOEventInternal(a_threadCount)
		{
			m_epoll = ::epoll_create(ObjCntPerPoll);
			if (0 > m_epoll) {
				auto e = errno;
				asd_RaiseException("fail epoll_create, errno:{}", e);
			}
			StartThread();
		}



		virtual ~IOEventInternal_EPOLL()
		{
			StopThread();
			if (m_epoll >= 0)
				::close(m_epoll);
		}



		virtual bool Register(REF AsyncSocketInternal* a_sock) asd_noexcept override
		{
			epoll_event ev;
			ev.data.ptr = (void*)a_sock->m_id;
			ev.events = DefaultPollOptions | EPOLLIN;
			auto r = ::epoll_ctl(m_epoll,
								 EPOLL_CTL_ADD,
								 a_sock->GetNativeHandle(),
								 &ev);
			if (r != 0) {
				auto e = errno;
				asd_Assert(false, "fail epoll_ctl, errno:{}", e);
				a_sock->m_lastError = e;
				return false;
			}

			int e = a_sock->SetNonblock(true);
			if (e != 0) {
				asd_Assert(false, "fail SetNonblock, errno:{}", e);
				a_sock->m_lastError = e;
				return false;
			}

			return true;
		}



		virtual bool PostSignal(IN AsyncSocketInternal* a_sock) asd_noexcept override
		{
			if (a_sock == nullptr)
				return false;

			epoll_event ev;
			ev.data.ptr = (void*)a_sock->m_id;
			ev.events = DefaultPollOptions | EPOLLOUT;
			auto r = ::epoll_ctl(m_epoll,
								 EPOLL_CTL_MOD,
								 a_sock->GetNativeHandle(),
								 &ev);
			if (r != 0) {
				auto e = errno;
				asd_Assert(false, "fail epoll_ctl, errno:{}", e);
				return false;
			}
			return true;
		}



		virtual bool Wait(IN uint32_t a_timeoutMs,
						  OUT EventInfo& a_event) asd_noexcept override
		{
			auto r = ::epoll_wait(m_epoll,
								  &a_event.m_epollEvent,
								  1,
								  a_timeoutMs);
			if (r > 0) {
				auto id = (uintptr_t)a_event.m_epollEvent.data.ptr;
				auto ioLock = GetLock(m_ioLock);
				auto it = m_sockets.find(id);
				if (it == m_sockets.end())
					return true;
				a_event.m_socket = it->second;
				ioLock.unlock();
				if (a_event.m_socket->m_state == SockState::Connecting)
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
			asd_Assert(false, "polling error, errno:{}", a_event.m_error);
			return false;
		}



		bool GetSocketError(IN AsyncSocketInternal* sock,
							OUT int& a_err) asd_noexcept
		{
			a_err = 0;
			socklen_t errlen = sizeof(a_err);
			int r = ::getsockopt(sock->GetNativeHandle(),
								 SOL_SOCKET,
								 SO_ERROR,
								 &a_err,
								 &errlen);
			if (r != 0)
				a_err = errno;
			return r == 0;
		}



		virtual void ProcEvent(REF EventInfo& a_event) asd_noexcept override
		{
			AsyncSocketInternal* sock = a_event.m_socket.get();

			// error
			if (EPOLLERR & a_event.m_epollEvent.events) {
				int e;
				bool r = GetSocketError(sock, e);
				switch (e) {
					default:
						asd_Assert(false, "unknown socket error, errno:{}", e);
						break;
				}
				if (sock->m_state != SockState::Closing)
					sock->m_lastError = e;
				CloseSocket(sock);
				return;
			}

			// connected
			if (a_event.m_socket->m_state == SockState::Connecting) {
				asd_Assert(EPOLLOUT & a_event.m_epollEvent.events, "unknown logic error");
				int e;
				bool r = GetSocketError(sock, e);
				if (e == 0) {
					a_event.m_socket->m_state = SockState::Connected;
					sock->m_recvBuffer = NewBuffer<asd_BufferList_DefaultReadBufferSize>();
					if (sock->m_interface != nullptr)
						sock->m_interface->OnConnect(0);
				}
				else {
					switch (e) {
						default:
							asd_Assert(false, "unknown socket error, errno:{}", e);
							break;
					}
					sock->m_lastError = e;
					sock->m_state = SockState::None;
					if (sock->m_interface != nullptr)
						sock->m_interface->OnConnect(e);
				}
				return;
			}

			// recv
			sock->m_lastError = 0;
			while (sock->m_state == SockState::Connected) {
				auto r = ::recv(sock->GetNativeHandle(),
								sock->m_recvBuffer->GetBuffer(),
								sock->m_recvBuffer->Capacity(),
								0);
				if (r > 0) {
					// success
					auto recvedData = std::move(sock->m_recvBuffer);
					asd_Assert(recvedData->SetSize(r), "fail recvedData->SetSize({})", r);
					if (sock->m_interface != nullptr)
						sock->m_interface->OnRecv(std::move(recvedData));
					// 유저 콜백 호출 후
					if (sock->m_state == SockState::Connected)
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
							asd_Assert(false, "fail recv, errno:{}", e);
							sock->m_lastError = e;
							CloseSocket(sock);
							return;
					}
				}
				return;
			}

			// listen
			while (sock->m_state == SockState::Listening) {
				AsyncSocket newSock;
				IpAddress addr;
				auto e = sock->Accept(newSock.CastToSocket(), addr);
				switch (e) {
					case 0:
						newSock.m_internal->m_state = SockState::Connected;
						newSock.m_internal->m_recvBuffer = NewBuffer<asd_BufferList_DefaultReadBufferSize>();
						if (sock->m_interface != nullptr)
							sock->m_interface->OnAccept(std::move(newSock));
						return;
					case EAGAIN:
						return;
					case EINTR: // 인터럽트
						continue;
					default:
						asd_Assert(false, "fail Accept, errno:{}", e);
						sock->m_lastError = e;
						CloseSocket(sock);
						return;
				}
				return;
			}

		}



		virtual int Connect(REF AsyncSocketInternal* a_sock,
							IN const IpAddress& a_dest) asd_noexcept override
		{
			if (PostSignal(a_sock) == false)
				return -1;

			int e = a_sock->Connect(a_dest);
			switch (e) {
				case 0:
					asd_Assert(false, "??");
				case EINPROGRESS:
					return 0;
			}
			asd_Assert(false, "fail Connect, errno:{}", e);
			return e;
		}



		virtual int Listen(REF AsyncSocketInternal* a_sock) asd_noexcept override
		{
			// 딱히 할게 없음
			return 0;
		}



		virtual int Send(REF AsyncSocketInternal* a_sock) asd_noexcept override
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
							asd_Assert(false, "fail writev, errno:{}", e);
							break;
					}
					return e;
				}
				break;
			}

			for (i=t_iovec.size(); i>0; --i)
				a_sock->m_sendQueue.pop_front();

			return 0;
		}



		virtual void CloseSocket(REF AsyncSocketInternal* a_sock,
								 IN bool a_hard = false) asd_noexcept override
		{
			if (a_sock->m_state == SockState::Closed)
				return;

			if (a_hard == false) {
				auto sendLock = GetLock(a_sock->m_sendLock);
				if (a_sock->m_state != SockState::Closing)
					::shutdown(a_sock->GetNativeHandle(), SHUT_RD);

				if (a_sock->m_sendQueue.size() > 0) {
					a_sock->m_state = SockState::Closing;
					return;
				}
			}

			a_sock->Close();
			a_sock->m_state = SockState::Closed;
			if (a_sock->m_interface != nullptr) {
				a_sock->m_interface->OnClose(a_sock->m_lastError);
				a_sock->m_interface = nullptr;
			}

			auto ioLock = GetLock(m_ioLock);
			m_sockets.erase(a_sock->m_id);
		}



		virtual void Poll_Finally(REF AsyncSocketInternal* a_sock) asd_noexcept override
		{
			if (a_sock->m_state == SockState::Closed)
				return;

			if (DefaultPollOptions & EPOLLONESHOT) {
				epoll_event ev;
				ev.data.ptr = (void*)a_sock->m_id;
				ev.events = DefaultPollOptions | EPOLLIN;
				if (a_sock->m_sendSignal)
					ev.events |= EPOLLOUT;
				auto r = ::epoll_ctl(m_epoll,
									 EPOLL_CTL_MOD,
									 a_sock->GetNativeHandle(),
									 &ev);
				if (r != 0) {
					auto e  = errno;
					asd_Assert(false, "fail epoll_ctl, errno:{}", e);
					a_sock->m_lastError = e;
				}
			}

			if (a_sock->m_state == SockState::Closing)
				CloseSocket(a_sock);
		}
	};
	typedef IOEventInternal_EPOLL IOEventInternal_NATIVE;



#else
	#error This platform is not supported.

#endif



	void IOEvent::Init(IN uint32_t a_threadCount /*= Get_HW_Concurrency()*/)
	{
		reset(new IOEventInternal_NATIVE(a_threadCount));
	}


	bool IOEvent::Register(REF AsyncSocket& a_sock) asd_noexcept
	{
		auto internal = get();
		if (internal == nullptr)
			return false;
		if (a_sock.m_internal == nullptr)
			return false;

		auto sockLock = GetLock(a_sock.m_internal->m_sockLock);

		auto e = a_sock.m_internal->Init();
		if (e != 0) {
			asd_Assert(false, "fail socket init, e:{}", e);
			return false;
		}

		auto ioLock = GetLock(internal->m_ioLock);

		std::shared_ptr<IOEventInternal> null;
		bool set = std::atomic_compare_exchange_strong(&a_sock.m_internal->m_event, &null, *this);
		if (set == false)
			return false;
		auto emplace = internal->m_sockets.emplace(a_sock.m_internal->m_id,
												   a_sock.m_internal);
		if (emplace.second == false) {
			asd_Assert(false, "duplicate socket id : {}", a_sock.m_internal->m_id);
			return false;
		}
		ioLock.unlock();

		if (internal->Register(a_sock.m_internal.get()) == false) {
			ioLock.lock();
			internal->m_sockets.erase(emplace.first);
			a_sock.m_internal->m_event.reset();
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



	AsyncSocket::AsyncSocket(IN AddressFamily a_addressFamily /*= AddressFamily::IPv4*/,
							 IN Socket::Type a_socketType /*= Socket::Type::TCP*/) asd_noexcept
	{
		m_internal.reset(new AsyncSocketInternal(a_addressFamily, a_socketType));
		m_internal->m_interface = this;
	}


	void AsyncSocket::Init(MOVE AsyncSocket&& a_move) asd_noexcept
	{
		Mutex* lock[2] = {nullptr, nullptr};

		if (m_internal != nullptr)
			lock[0] = &m_internal->m_sockLock;
		if (a_move.m_internal != nullptr)
			lock[1] = &a_move.m_internal->m_sockLock;

		if (lock[0] > lock[1])
			std::swap(lock[0], lock[1]);

		if (lock[0] != nullptr)
			lock[0]->lock();
		if (lock[1] != nullptr)
			lock[1]->lock();

		m_internal.swap(a_move.m_internal);
		if (m_internal != nullptr)
			m_internal->m_interface = this;
		if (a_move.m_internal != nullptr)
			a_move.m_internal->m_interface = &a_move;

		if (lock[0] != nullptr)
			lock[0]->unlock();
		if (lock[1] != nullptr)
			lock[1]->unlock();
	}


	Socket& AsyncSocket::CastToSocket() asd_noexcept
	{
		return *m_internal;
	}


	const Socket& AsyncSocket::CastToSocket() const asd_noexcept
	{
		return *m_internal;
	}


	uintptr_t AsyncSocket::GetID() const asd_noexcept
	{
		return m_internal->m_id;
	}


	void AsyncSocket::Connect(IN const IpAddress& a_dest,
							  IN uint32_t a_timeoutMs /*= 10*1000*/) asd_noexcept
	{
		auto ev = std::atomic_load(&m_internal->m_event);
		if (ev == nullptr) {
			asd_Assert(false, "not registered socket");
			OnConnect(-1);
			return;
		}

		auto sockLock = GetLock(m_internal->m_sockLock);

		if (m_internal->m_state != SockState::None) {
			asd_Assert(false, "invalid socket state : {}", (uint8_t)m_internal->m_state);
			OnConnect(-1);
			return;
		}

		m_internal->m_state = SockState::Connecting;
		m_internal->m_lastError = ev->Connect(m_internal.get(), a_dest);
		if (m_internal->m_lastError != 0) {
			m_internal->m_state = SockState::None;
			OnConnect(m_internal->m_lastError);
		}
	}


	bool AsyncSocket::Listen(IN const IpAddress& a_bind,
							 IN int a_backlog /*= 1024*/) asd_noexcept
	{
		auto ev = std::atomic_load(&m_internal->m_event);
		if (ev == nullptr) {
			asd_Assert(false, "not registered socket");
			return false;
		}

		auto sockLock = GetLock(m_internal->m_sockLock);

		if (m_internal->m_state != SockState::None) {
			asd_Assert(false, "invalid socket state : {}", (uint8_t)m_internal->m_state);
			return false;
		}

		auto e = m_internal->Bind(a_bind);
		if (e != 0) {
			asd_Assert(false, "fail Bind({}), e:{}", a_bind.ToString(), e);
			return false;
		}

		e = m_internal->Listen(a_backlog);
		if (e != 0) {
			asd_Assert(false, "fail Listen({}), e:{}", a_backlog, e);
			return false;
		}

		if (0 != ev->Listen(m_internal.get()))
			return false;

		m_internal->m_state = SockState::Listening;
		return true;
	}


	bool AsyncSocket::Send(MOVE std::deque<Buffer_ptr>&& a_data) asd_noexcept
	{
		auto ev = std::atomic_load(&m_internal->m_event);
		if (ev == nullptr) {
			asd_Assert(false, "not registered socket");
			return false;
		}

		auto sendLock = GetLock(m_internal->m_sendLock);
		if (m_internal->m_state != SockState::Connected)
			return false;

		for (auto& it : a_data)
			m_internal->m_sendQueue.push_back(std::move(it));
		a_data.clear();

		if (m_internal->m_sendSignal == false) {
			if (ev->PostSignal(m_internal.get()))
				m_internal->m_sendSignal = true;
			else
				asd_Assert(false, "fail PostSignal, ID:{}", m_internal->m_id);
		}
		return true;
	}


	void AsyncSocket::Close() asd_noexcept
	{
		std::shared_ptr<IOEventInternal> null;
		auto ev = std::atomic_exchange(&m_internal->m_event, null);

		auto sockLock = GetLock(m_internal->m_sockLock);
		m_internal->m_lastError = 0;
		if (ev != nullptr)
			ev->CloseSocket(m_internal.get(),
							m_internal->m_interface == nullptr);
	}


	AsyncSocket::~AsyncSocket() asd_noexcept
	{
		if (m_internal == nullptr)
			return;

		auto sockLock = GetLock(m_internal->m_sockLock);
		m_internal->m_interface = nullptr;
		Close();
		OnClose(0);
	}
}
