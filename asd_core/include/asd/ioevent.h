#pragma once
#include "asdbase.h"
#include "socket.h"
#include "buffer.h"
#include "lock.h"
#include "threadutil.h"


namespace asd
{
	class AsyncSocket;
	class IOEventInternal;
	class IOEventInternal_IOCP;
	class IOEventInternal_EPOLL;



	class IOEvent : public std::shared_ptr<IOEventInternal>
	{
	public:
		void Init(IN uint32_t a_threadCount = Get_HW_Concurrency());
		bool Register(REF AsyncSocket& a_sock) asd_noexcept;
		void Poll(IN uint32_t a_timeoutSec) asd_noexcept;
	};



	class AsyncSocket
	{
		friend class asd::IOEvent;
		friend class asd::IOEventInternal_IOCP;
		friend class asd::IOEventInternal_EPOLL;

		std::shared_ptr<AsyncSocketInternal> m_internal;


	public:
		AsyncSocket(IN AddressFamily a_addressFamily = AddressFamily::IPv4,
					IN Socket::Type a_socketType = Socket::Type::TCP) asd_noexcept;


		inline AsyncSocket(MOVE AsyncSocket&& a_move) asd_noexcept
		{
			Init(std::move(a_move));
		}
		inline AsyncSocket& operator=(MOVE AsyncSocket&& a_move) asd_noexcept
		{
			Init(std::move(a_move));
			return *this;
		}
		void Init(MOVE AsyncSocket&& a_move) asd_noexcept;


		Socket& CastToSocket() asd_noexcept;
		const Socket& CastToSocket() const asd_noexcept;


		uintptr_t GetID() const asd_noexcept;


		void Connect(IN const IpAddress& a_dest,
					 IN uint32_t a_timeoutMs = 10*1000) asd_noexcept;
		virtual void OnConnect(IN Socket::Error a_err) asd_noexcept
		{
		}


		bool Listen(IN const IpAddress& a_bind,
					IN int a_backlog = 1024) asd_noexcept;
		virtual void OnAccept(MOVE AsyncSocket&& a_newSock) asd_noexcept
		{
		}


		bool Send(MOVE std::deque<Buffer_ptr>&& a_data) asd_noexcept;
		inline bool Send(MOVE BufferList&& a_data) asd_noexcept
		{
			typename BufferList::BaseType& cast = a_data;
			if (Send(std::move(cast))) {
				a_data.Clear();
				return true;
			}
			return false;
		}
		virtual void OnRecv(MOVE Buffer_ptr&& a_data) asd_noexcept
		{
			// Sample
		}


		void Close() asd_noexcept;
		virtual void OnClose(IN Socket::Error a_err) asd_noexcept
		{
		}


		virtual ~AsyncSocket() asd_noexcept;
	};
}
