#pragma once
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
		static int ToNativeCode(Type a_type);
		static Type FromNativeCode(int a_type);

		struct IoResult
		{
			int		m_bytes;
			Error	m_error;
			inline IoResult(int a_bytes = 0,
							Error a_error = 0)
				// init
				: m_bytes(a_bytes)
				, m_error(a_error)
			{
			}
		};


		Socket(Socket::Type a_socketType = Type::TCP,
			   AddressFamily a_addressFamily = AddressFamily::IPv4);

		Socket(Socket&& a_rval);

		Socket(Handle a_nativeHandle);

		Socket(Handle a_nativeHandle,
			   Socket::Type a_socketType,
			   AddressFamily a_addressFamily);

		Socket& operator=(Socket&& a_rval);


		virtual ~Socket();


		Handle GetNativeHandle() const;


		virtual void Close();


		// 실제 소켓을 생성하는 함수.
		// a_force가 true이면 무조건 기존에 열려있던 소켓을 닫고 새로 연다.
		// a_force가 false이면 소켓이 열려있지 않거나 a_addressFamily와 a_socketType가 기존과 다른 경우에만 새로 연다.
		Error Init(bool a_force = false);

		Error Init(Socket::Type a_socketType,
				   AddressFamily a_addressFamily,
				   bool a_force = false);


		AddressFamily GetAddressFamily() const;

		Type GetSocektType() const;


		// a_addr의 AddressFamily를 적용하여 초기화한다.
		Error Bind(const IpAddress& a_addr);


		// backlog 기본값 참고 : https://kldp.org/node/113987
		static constexpr int DefaultBacklog = 1024;
		Error Listen(int a_backlog = DefaultBacklog);


		Error Accept(Socket& a_newbe /*Out*/,
					 IpAddress& a_address /*Out*/);


		// a_dest의 AddressFamily를 적용하여 초기화한다.
		Error Connect(const IpAddress& a_dest);


		IoResult Send(const void* a_buffer,
					  int a_bufferSize,
					  int a_flags = 0);

		template <typename SizeType>
		inline IoResult Send(const void* a_buffer,
							 SizeType a_bufferSize,
							 int a_flags = 0)
		{
			if (a_bufferSize > std::numeric_limits<int>::max()) {
				asd_OnErr("size overflow, a_bufferSize:{}", a_bufferSize);
				return IoResult(0, -1);
			}
			return Send(a_buffer,
						(int)a_bufferSize,
						a_flags);
		}


		IoResult SendTo(const void* a_buffer,
						int a_bufferSize,
						const IpAddress& a_dest,
						int a_flags = 0);

		template <typename SizeType>
		inline IoResult SendTo(const void* a_buffer,
							   SizeType a_bufferSize,
							   const IpAddress& a_dest,
							   int a_flags = 0)
		{
			if (a_bufferSize > std::numeric_limits<int>::max()) {
				asd_OnErr("size overflow, a_bufferSize:{}", a_bufferSize);
				return IoResult(0, -1);
			}
			return SendTo(a_buffer,
						  (int)a_bufferSize,
						  a_dest,
						  a_flags);
		}


		IoResult Recv(void* a_buffer /*Out*/,
					  int a_bufferSize,
					  int a_flags = 0);

		template <typename SizeType>
		inline IoResult Recv(void* a_buffer /*Out*/,
							 SizeType a_bufferSize,
							 int a_flags = 0)
		{
			if (a_bufferSize > std::numeric_limits<int>::max()) {
				asd_OnErr("size overflow, a_bufferSize:{}", a_bufferSize);
				return IoResult(0, -1);
			}
			return Recv(a_buffer,
						(int)a_bufferSize,
						a_flags);
		}


		IoResult RecvFrom(void* a_buffer /*Out*/,
						  int a_bufferSize,
						  IpAddress& a_src /*Out*/,
						  int a_flags = 0);

		template <typename SizeType>
		inline IoResult RecvFrom(void* a_buffer /*Out*/,
								 SizeType a_bufferSize,
								 IpAddress& a_src /*Out*/,
								 int a_flags = 0)
		{
			if (a_bufferSize > std::numeric_limits<int>::max()) {
				asd_OnErr("size overflow, a_bufferSize:{}", a_bufferSize);
				return IoResult(0, -1);
			}
			return RecvFrom(a_buffer,
							(int)a_bufferSize,
							a_src,
							a_flags);
		}


		Error SetSockOpt(int a_level,
						 int a_optname,
						 const void* a_optval,
						 uint32_t a_optlen);
		Error GetSockOpt(int a_level,
						 int a_optname,
						 void* a_optval /*Out*/,
						 uint32_t& a_optlen /*InOut*/) const;

		Error SetSockOpt_ReuseAddr(bool a_set);
		Error GetSockOpt_ReuseAddr(bool& a_result /*Out*/) const;

		Error SetSockOpt_UseNagle(bool a_set);
		Error GetSockOpt_UseNagle(bool& a_result /*Out*/) const;

		Error SetSockOpt_Linger(bool a_use,
								uint16_t a_sec);
		Error GetSockOpt_Linger(bool& a_use /*Out*/,
								uint16_t& a_sec /*Out*/) const;


		Error SetSockOpt_RecvBufSize(int a_byte);
		Error GetSockOpt_RecvBufSize(int& a_byte /*Out*/) const;

		Error SetSockOpt_SendBufSize(int a_byte);
		Error GetSockOpt_SendBufSize(int& a_byte /*Out*/) const;

		Error GetSockOpt_Error(int& a_error /*Out*/) const;

		Error SetNonblock(bool a_nonblock);
		Error CheckNonblock(bool& a_result /*Out*/) const;

		Error GetSockName(IpAddress& a_addr /*Out*/) const;
		Error GetPeerName(IpAddress& a_addr /*Out*/) const;


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

		Socket(const Socket&) = delete;

		Socket& operator=(const Socket&) = delete;

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

		EasySocket(Socket::Type a_socketType = Socket::Type::TCP,
				   AddressFamily a_addressFamily = AddressFamily::IPv4);

		void Bind(const IpAddress& a_addr);

		void Listen(int a_backlog = 1024);

		void Accept(Socket& a_newbe /*Out*/,
					IpAddress& a_address /*Out*/);

		void Connect(const IpAddress& a_dest);

		void Send(const Buffer& a_buffer,
				  int a_flags = 0);

		void SendTo(const Buffer& a_buffer,
					const IpAddress& a_dest,
					int a_flags = 0);

		void Recv(Buffer& a_buffer /*Out*/,
				  int a_flags = 0,
				  int a_recvComplete = -1);

		void RecvFrom(Buffer& a_buffer /*Out*/,
					  IpAddress& a_src /*Out*/,
					  int a_flags = 0);

		void Close();
	};
}
