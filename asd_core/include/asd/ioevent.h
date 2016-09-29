#pragma once
#include "asdbase.h"
#include "socket.h"
#include "buffer.h"
#include "lock.h"
#include "threadutil.h"


namespace asd
{
	class AsyncSocket;
	class IOEventInterface
	{
		friend class AsyncSocket;

	public:
		virtual ~IOEventInterface() asd_noexcept;
		virtual int Poll(IN uint32_t a_timeoutSec) asd_noexcept = 0;

	private:
		virtual void SendRequest(REF AsyncSocket* a_sock,
								 MOVE std::deque<Buffer_ptr>&& a_data) asd_noexcept = 0;
		virtual void Register(REF AsyncSocket* a_sock) asd_noexcept = 0;
		virtual void Unregister(IN AsyncSocket* a_sock) asd_noexcept = 0;
	};



	class IOEvent : public std::shared_ptr<IOEventInterface>
	{
	public:
		void Init(IN uint32_t a_threadCount = Get_HW_Concurrency());
	};



	struct IOEvCtx
	{
		MtxCtl_asdMutex& m_ioLock;
		MtxCtl_asdMutex& m_sockLock;
	};

	struct AsyncSocketData;
	class AsyncSocket : public Socket
	{
		friend class IOEventImpl;
		friend class Poller;
		std::unique_ptr<AsyncSocketData> m_data;


	public:
		AsyncSocket() asd_noexcept;

		virtual ~AsyncSocket() asd_noexcept;

		void Register(REF IOEvent& a_ev) asd_noexcept;

		void Unregister() asd_noexcept;

		void Connect(IN const IpAddress& a_dest,
					 IN uint32_t a_timeoutMs = 10*1000) asd_noexcept;
		virtual void OnConnect(IN Socket::Error a_err) asd_noexcept
		{
		}

		void Listen() asd_noexcept;
		virtual void OnAccept(IN Socket::Error a_err) asd_noexcept
		{
		}

		void Send(MOVE std::deque<Buffer_ptr>&& a_data) asd_noexcept;
		virtual void OnRecv(REF Buffer_ptr& a_data) asd_noexcept
		{
			// Sample
		}

		void Close() asd_noexcept;
		virtual void OnClose(IN Socket::Error a_err) asd_noexcept
		{
		}
	};
}