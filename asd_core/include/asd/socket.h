﻿#pragma once
#include "asdbase.h"
#include "address.h"
#include <vector>



namespace asd
{
	class AsyncSocketNative;


	// Socket API 랩핑
	class Socket
	{
		friend class asd::AsyncSocketNative;

	public:
#if defined(asd_Platform_Windows)
		using Handle = intptr_t;
		using Error = int;
		static const Handle InvalidHandle = (Handle)~0;

#else
		using Handle = int;
		using Error = int;
		static const Handle InvalidHandle = -1;

#endif
		enum class Type : uint8_t
		{
			TCP,
			UDP,
		};
		static int ToNativeCode(IN Type a_type) asd_noexcept;
		static Type FromNativeCode(IN int a_type) asd_noexcept;

		struct IoResult
		{
			int		m_bytes;
			Error	m_error;
			inline IoResult(IN int a_bytes = 0,
							IN Error a_error = 0)
				// init
				: m_bytes(a_bytes)
				, m_error(a_error)
			{
			}
		};


		Socket(IN Socket::Type a_socketType = Type::TCP,
			   IN AddressFamily a_addressFamily = AddressFamily::IPv4) asd_noexcept;

		Socket(MOVE Socket&& a_rval) asd_noexcept;

		Socket(IN Handle a_nativeHandle) asd_noexcept;

		Socket(IN Handle a_nativeHandle,
			   IN Socket::Type a_socketType,
			   IN AddressFamily a_addressFamily) asd_noexcept;

		Socket& operator=(MOVE Socket&& a_rval) asd_noexcept;


		virtual ~Socket() asd_noexcept;


		Handle GetNativeHandle() const asd_noexcept;


		virtual void Close() asd_noexcept;


		// 실제 소켓을 생성하는 함수.
		// a_force가 true이면 무조건 기존에 열려있던 소켓을 닫고 새로 연다.
		// a_force가 false이면 소켓이 열려있지 않거나 a_addressFamily와 a_socketType가 기존과 다른 경우에만 새로 연다.
		Error Init(IN bool a_force = false) asd_noexcept;

		Error Init(IN Socket::Type a_socketType,
				   IN AddressFamily a_addressFamily,
				   IN bool a_force = false) asd_noexcept;


		AddressFamily GetAddressFamily() const asd_noexcept;

		Type GetSocektType() const asd_noexcept;


		// a_addr의 AddressFamily를 적용하여 초기화한다.
		Error Bind(IN const IpAddress& a_addr) asd_noexcept;


		// backlog 기본값 참고 : https://kldp.org/node/113987
		static constexpr int DefaultBacklog = 1024;
		Error Listen(IN int a_backlog = DefaultBacklog) asd_noexcept;


		Error Accept(OUT Socket& a_newbe,
					 OUT IpAddress& a_address) asd_noexcept;


		// a_dest의 AddressFamily를 적용하여 초기화한다.
		Error Connect(IN const IpAddress& a_dest) asd_noexcept;


		IoResult Send(IN const void* a_buffer,
					  IN int a_bufferSize,
					  IN int a_flags = 0) asd_noexcept;

		template <typename SizeType>
		inline IoResult Send(IN const void* a_buffer,
							 IN SizeType a_bufferSize,
							 IN int a_flags = 0) asd_noexcept
		{
			asd_ChkErrAndRetVal(a_bufferSize > std::numeric_limits<int>::max(),
								IoResult(0, -1),
								"size overflow, a_bufferSize:{}",
								a_bufferSize);
			return Send(a_buffer,
						(int)a_bufferSize,
						a_flags);
		}


		IoResult SendTo(IN const void* a_buffer,
						IN int a_bufferSize,
						IN const IpAddress& a_dest,
						IN int a_flags = 0) asd_noexcept;

		template <typename SizeType>
		inline IoResult SendTo(IN const void* a_buffer,
							   IN SizeType a_bufferSize,
							   IN const IpAddress& a_dest,
							   IN int a_flags = 0) asd_noexcept
		{
			asd_ChkErrAndRetVal(a_bufferSize > std::numeric_limits<int>::max(),
								IoResult(0, -1),
								"size overflow, a_bufferSize:{}",
								a_bufferSize);
			return SendTo(a_buffer,
						  (int)a_bufferSize,
						  a_dest,
						  a_flags);
		}


		IoResult Recv(OUT void* a_buffer,
					  IN int a_bufferSize,
					  IN int a_flags = 0) asd_noexcept;

		template <typename SizeType>
		inline IoResult Recv(OUT void* a_buffer,
							 IN SizeType a_bufferSize,
							 IN int a_flags = 0) asd_noexcept
		{
			asd_ChkErrAndRetVal(a_bufferSize > std::numeric_limits<int>::max(),
								IoResult(0, -1),
								"size overflow, a_bufferSize:{}",
								a_bufferSize);
			return Recv(a_buffer,
				(int)a_bufferSize,
						a_flags);
		}


		IoResult RecvFrom(OUT void* a_buffer,
						  IN int a_bufferSize,
						  OUT IpAddress& a_src,
						  IN int a_flags = 0) asd_noexcept;

		template <typename SizeType>
		inline IoResult RecvFrom(OUT void* a_buffer,
								 IN SizeType a_bufferSize,
								 OUT IpAddress& a_src,
								 IN int a_flags = 0) asd_noexcept
		{
			asd_ChkErrAndRetVal(a_bufferSize > std::numeric_limits<int>::max(),
								IoResult(0, -1),
								"size overflow, a_bufferSize:{}",
								a_bufferSize);
			return RecvFrom(a_buffer,
				(int)a_bufferSize,
							a_src,
							a_flags);
		}


		Error SetSockOpt(IN int a_level,
						 IN int a_optname,
						 IN const void* a_optval,
						 IN uint32_t a_optlen) asd_noexcept;
		Error GetSockOpt(IN int a_level,
						 IN int a_optname,
						 OUT void* a_optval,
						 INOUT uint32_t& a_optlen) const asd_noexcept;

		Error SetSockOpt_ReuseAddr(IN bool a_set) asd_noexcept;
		Error GetSockOpt_ReuseAddr(OUT bool& a_result) const asd_noexcept;

		Error SetSockOpt_UseNagle(IN bool a_set) asd_noexcept;
		Error GetSockOpt_UseNagle(OUT bool& a_result) const asd_noexcept;

		Error SetSockOpt_Linger(IN bool a_use,
								IN uint16_t a_sec) asd_noexcept;
		Error GetSockOpt_Linger(OUT bool& a_use,
								OUT uint16_t& a_sec) const asd_noexcept;


		Error SetSockOpt_RecvBufSize(IN int a_byte) asd_noexcept;
		Error GetSockOpt_RecvBufSize(OUT int& a_byte) const asd_noexcept;

		Error SetSockOpt_SendBufSize(IN int a_byte) asd_noexcept;
		Error GetSockOpt_SendBufSize(OUT int& a_byte) const asd_noexcept;

		Error GetSockOpt_Error(OUT int& a_error) const asd_noexcept;

		Error SetNonblock(IN bool a_nonblock) asd_noexcept;
		Error CheckNonblock(OUT bool& a_result) const asd_noexcept;

		Error GetSockName(OUT IpAddress& a_addr) const asd_noexcept;
		Error GetPeerName(OUT IpAddress& a_addr) const asd_noexcept;


	private:
		Handle m_handle = InvalidHandle;

		AddressFamily m_addressFamily = AddressFamily::IPv4;

		Type m_socketType = Type::TCP;

#if defined(asd_Platform_Windows)
		// Windows의 경우
		// 소켓의 논블럭 여부를 알아내는 API가 제공되지 않아서
		// 매개변수를 사용.
		// http://stackoverflow.com/questions/5489562/in-win32-is-there-a-way-to-test-if-a-socket-is-non-blocking
		bool m_nonblock;
#endif

		Socket(IN const Socket&) = delete;

		Socket& operator=(IN const Socket&) = delete;

	};
	typedef std::shared_ptr<Socket> Socket_ptr;



	// 성능을 고려하지 않고 예외처리만 한 소켓 클래스.
	// 예제나 실험용 소스에 사용.
	struct EasySocket
	{
		typedef std::vector<uint8_t> Buffer;
		static const int BufferSize = 2048;

		Socket_ptr m_socket;
		Socket::Type m_socketType;
		uint8_t m_recvBuffer[BufferSize];
		uint8_t m_sendBuffer[BufferSize];

		EasySocket(IN Socket::Type a_socketType = Socket::Type::TCP,
				   IN AddressFamily a_addressFamily = AddressFamily::IPv4);

		void Bind(IN const IpAddress& a_addr);

		void Listen(IN int a_backlog = 1024);

		void Accept(OUT Socket& a_newbe,
					OUT IpAddress& a_address);

		void Connect(IN const IpAddress& a_dest);

		void Send(IN const Buffer& a_buffer,
				  IN int a_flags = 0);

		void SendTo(IN const Buffer& a_buffer,
					IN const IpAddress& a_dest,
					IN int a_flags = 0);

		void Recv(OUT Buffer& a_buffer,
				  IN int a_flags = 0,
				  IN int a_recvComplete = -1);

		void RecvFrom(OUT Buffer& a_buffer,
					  OUT IpAddress& a_src,
					  IN int a_flags = 0);

		void Close();
	};
}
