#pragma once
#include "asdbase.h"
#include "socket.h"
#include "buffer.h"
#include "lock.h"
#include "threadutil.h"
#include "handle.h"

namespace asd
{
	class IOEvent;
	class IOEventInternal;
	class IOEventInternal_IOCP;
	class IOEventInternal_EPOLL;

	class AsyncSocket;
	using AsyncSocketHandle = Handle<AsyncSocket, uintptr_t>;
	using AsyncSocket_ptr = std::shared_ptr<AsyncSocket>;

	class AsyncSocket : public Socket
	{
		friend class asd::IOEvent;
		friend class asd::IOEventInternal;
		friend class asd::IOEventInternal_IOCP;
		friend class asd::IOEventInternal_EPOLL;

		enum class State : uint8_t
		{
			None,
			Connecting,
			Connected,
			Listening,
			Closing,
			Closed,
		};

		// 이 소켓의 데이터들을 보호하는 락
		mutable Mutex m_sockLock;

		// 이 소켓이 등록되어있는 IOEvent 객체
		std::shared_ptr<IOEventInternal> m_event;

		// 상태
		State m_state = State::None;

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

		// OS별 특수 데이터
		static std::shared_ptr<AsyncSocketNative> InitNative();
		std::shared_ptr<AsyncSocketNative> m_native = InitNative();


	public:
		using Socket::Socket;

		bool Send(MOVE std::deque<Buffer_ptr>&& a_data);

		inline bool Send(MOVE BufferList&& a_data)
		{
			std::deque<Buffer_ptr>& cast = a_data;
			if (Send(std::move(cast))) {
				a_data.Clear();
				return true;
			}
			return false;
		}

		inline bool Send(MOVE Buffer_ptr&& a_data)
		{
			std::deque<Buffer_ptr> list;
			list.emplace_back(std::move(a_data));
			return Send(std::move(list));
		}


		virtual void Close() override;


		virtual ~AsyncSocket();
	};



	class IOEvent : public std::shared_ptr<IOEventInternal>
	{
	protected:
		mutable Mutex				m_ioLock;
		std::atomic_bool			m_run;
		std::vector<std::thread>	m_threads;

	public:
		virtual ~IOEvent();

		void Start(IN uint32_t a_threadCount = Get_HW_Concurrency());

		void Stop();


		bool RegisterListener(IN AsyncSocket_ptr& a_sock,
							  IN const IpAddress& a_bind,
							  IN int a_backlog = Socket::DefaultBacklog);

		inline bool RegisterListener(IN AsyncSocketHandle a_sockHandle,
									 IN const IpAddress& a_bind,
									 IN int a_backlog = Socket::DefaultBacklog)
		{
			auto sock = a_sockHandle.GetObj();
			return RegisterListener(sock, a_bind, a_backlog);
		}


		bool RegisterConnector(IN AsyncSocket_ptr& a_sock,
							   IN const IpAddress& a_dst);

		inline bool RegisterConnector(IN AsyncSocketHandle a_sockHandle,
									  IN const IpAddress& a_dst)
		{
			auto sock = a_sockHandle.GetObj();
			return RegisterConnector(sock, a_dst);
		}


		bool Register(IN AsyncSocket_ptr& a_sock);

		inline bool Register(IN AsyncSocketHandle a_sockHandle)
		{
			auto sock = a_sockHandle.GetObj();
			return Register(sock);
		}


		void Poll(IN uint32_t a_timeoutSec);


		virtual void OnAccept(IN AsyncSocket* a_listener,
							  MOVE AsyncSocket_ptr&& a_newSock)
		{
			auto handle = AsyncSocketHandle::GetHandle(a_listener);
			asd_DAssert(handle.IsValid());
			asd_RAssert(Register(a_newSock), "fail Register");
		}

		virtual void OnConnect(IN AsyncSocket* a_sock,
							   IN Socket::Error a_err)
		{
			auto handle = AsyncSocketHandle::GetHandle(a_sock);
			asd_DAssert(handle.IsValid());
		}

		virtual void OnRecv(IN AsyncSocket* a_sock,
							MOVE Buffer_ptr&& a_data)
		{
			auto handle = AsyncSocketHandle::GetHandle(a_sock);
			asd_DAssert(handle.IsValid());
		}

		virtual void OnClose(IN AsyncSocket* a_sock, 
							 IN Socket::Error a_err)
		{
			auto handle = AsyncSocketHandle::GetHandle(a_sock);
			asd_DAssert(handle.IsValid());
		}
	};
}
