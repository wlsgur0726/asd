#pragma once
#include "asd/asdbase.h"
#include "asd/address.h"
#include <vector>

#if defined(asd_Platform_Windows)
	#define asd_PlatformTypeDef_Socket						\
		typedef intptr_t Handle;							\
		typedef int Error;									\
		static const Handle InvalidHandle = (Handle)~0;		\

#else
	#define asd_PlatformTypeDef_Socket						\
		typedef int Handle;									\
		typedef int Error;									\
		static const Handle InvalidHandle = -1;				\

#endif


namespace asd
{
	// 단순한 Socket API 랩핑
	class Socket
	{
	public:
		asd_PlatformTypeDef_Socket;

		enum Type {
			TCP,
			UDP,
		};
		static int ToNativeCode(Type a_type) asd_noexcept;

		struct IoResult {
			int		m_bytes;
			Error	m_error;
			IoResult(IN int a_bytes = 0,
					 IN Error a_error = 0)
					 // init
					 : m_bytes(a_bytes)
					 , m_error(a_error)
			{
			}
		};

	private:
		Handle m_handle = InvalidHandle;

		IpAddress::Family m_addressFamily = IpAddress::IPv4;

		Type m_socketType = Type::TCP;

#if defined(asd_Platform_Windows)
		// Windows의 경우
		// 소켓의 논블럭 여부를 알아내는 API가 제공되지 않아서
		// 매개변수를 사용.
		// http://stackoverflow.com/questions/5489562/in-win32-is-there-a-way-to-test-if-a-socket-is-non-blocking
		bool m_nonblock;
#endif

	public:
		Socket(IN IpAddress::Family a_addressFamily = IpAddress::IPv4,
			   IN Socket::Type a_socketType = TCP) asd_noexcept;


		Socket(MOVE Socket&& a_rval) asd_noexcept;


		virtual
		~Socket() asd_noexcept;


		Socket&
		operator = (MOVE Socket&& a_rval) asd_noexcept;


		Handle
		GetNativeHandle() const asd_noexcept;


		void
		Close() asd_noexcept;


		// 실제 소켓을 생성하는 함수.
		// 소켓이 이미 생성되어있고,
		// AddressFamily와 SocketType이 기존과 동일하고, 
		// a_force에 false를 주면 아무런 작업도 하지 않는다.
		Error
		Init(IN IpAddress::Family a_addressFamily,
			 IN Socket::Type a_socketType,
			 IN bool a_force) asd_noexcept;


		Error
		SetSockOpt(IN int a_level,
				   IN int a_optname,
				   IN const void* a_optval,
				   IN uint32_t a_optlen) asd_noexcept;


		Error
		GetSockOpt(IN int a_level,
				   IN int a_optname,
				   OUT void* a_optval,
				   INOUT uint32_t& a_optlen) const asd_noexcept;


		Error
		SetSockOpt_ReuseAddr(IN bool a_set) asd_noexcept;


		Error
		GetSockOpt_ReuseAddr(OUT bool& a_result) const asd_noexcept;


		Error
		SetSockOpt_UseNagle(IN bool a_set) asd_noexcept;


		Error
		GetSockOpt_UseNagle(OUT bool& a_result) const asd_noexcept;


		Error
		SetSockOpt_Linger(IN bool a_use,
						  IN uint16_t a_sec) asd_noexcept;


		Error
		GetSockOpt_Linger(OUT bool& a_use,
						  OUT uint16_t& a_sec) const asd_noexcept;


		Error
		SetSockOpt_RecvBufSize(IN int a_byte) asd_noexcept;


		Error
		GetSockOpt_RecvBufSize(OUT int& a_byte) const asd_noexcept;


		Error
		SetSockOpt_SendBufSize(IN int a_byte) asd_noexcept;


		Error
		GetSockOpt_SendBufSize(OUT int& a_byte) const asd_noexcept;


		Error
		SetNonblock(IN bool a_nonblock) asd_noexcept;


		Error
		CheckNonblock(OUT bool& a_result) const asd_noexcept;


		Error
		GetSockName(OUT IpAddress& a_addr) const asd_noexcept;


		Error
		GetPeerName(OUT IpAddress& a_addr) const asd_noexcept;


		// a_addr의 AddressFamily를 적용하여 초기화한다.
		Error
		Bind(IN const IpAddress& a_addr) asd_noexcept;


		// backlog 기본값 참고 : https://kldp.org/node/113987
		Error
		Listen(IN int a_backlog = 1024) asd_noexcept;


		Error
		Accept(OUT Socket& a_newbe,
			   OUT IpAddress& a_address) asd_noexcept;


		// a_dest의 AddressFamily를 적용하여 초기화한다.
		Error
		Connect(IN const IpAddress& a_dest) asd_noexcept;


		IoResult
		Send(IN const void* a_buffer,
			 IN int a_bufferSize,
			 IN int a_flags = 0) asd_noexcept;


		IoResult
		SendTo(IN const void* a_buffer,
			   IN int a_bufferSize,
			   IN const IpAddress& a_dest,
			   IN int a_flags = 0) asd_noexcept;


		IoResult
		Recv(OUT void* a_buffer,
			 IN int a_bufferSize,
			 IN int a_flags = 0) asd_noexcept;


		IoResult
		RecvFrom(OUT void* a_buffer,
				 IN int a_bufferSize,
				 OUT IpAddress& a_src,
				 IN int a_flags = 0) asd_noexcept;


	private:
		Socket(IN const Socket&) = delete;

		Socket&
		operator = (IN const Socket&) = delete;

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

		EasySocket(IN IpAddress::Family a_addressFamily = IpAddress::IPv4,
				   IN Socket::Type a_socketType = Socket::TCP);

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
				  IN int a_flags = 0);

		void RecvFrom(OUT Buffer& a_buffer,
					  OUT IpAddress& a_src,
					  IN int a_flags = 0);

		void Close();
	};
}
