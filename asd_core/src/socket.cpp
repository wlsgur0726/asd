#include "stdafx.h"
#include "asd/socket.h"
#include "asd/exception.h"
#include <string>
#include <cstdio>
#include <algorithm>
#include <cassert>


# if defined(asd_Platform_Windows)
#	include <ws2tcpip.h>
#
# else
#	if defined(asd_Platform_Android)
#		include <unistd.h>
#	else
#		include <sys/unistd.h>
#	endif
#
#	include <cerrno>
#	include <sys/socket.h>
#	include <arpa/inet.h>
#	include <string.h>
#	include <fcntl.h>
#	include <netinet/tcp.h>
#
# endif



namespace asd
{
#if defined(asd_Platform_Windows)

	#define asd_RaiseSocketException(e) asd_RaiseException("WSAGetLastError:{}", e);

	#define asd_SetNonblockFlag(SockObj, true_or_false) (SockObj).m_nonblock = true_or_false

	#define asd_MoveNonblockFlag(SockObj1, SockObj2)		\
		do {												\
			(SockObj1).m_nonblock = (SockObj2).m_nonblock;	\
			(SockObj2).m_nonblock = false;					\
		} while (false)

	typedef char* SockOptValType;

	typedef char* SockBufType;

	inline Socket::Error GetErrorNumber() {
		return ::WSAGetLastError();
	}

	inline void CloseSocket(const Socket::Handle& socket) {
		while (0 != ::closesocket(socket)) {
			auto e = GetErrorNumber();
			if (e == WSAEWOULDBLOCK) {
				::Sleep(0);
				continue;
			}
			asd_OnErr("fail closesocket, WSAGetLastError:{}", e);
			break;
		}
	}

	// Winsock DLL 초기화를 위한 클래스
	struct WSAStartup_Auto
	{
		WSADATA m_wsa;
		WORD m_ver;

		WSAStartup_Auto() {
			m_ver = MAKEWORD(2, 0);
			auto err = ::WSAStartup(m_ver, &m_wsa);
			if ( err != 0 ) {
				asd_RaiseSocketException(err);
			}
		}

		~WSAStartup_Auto() {
			::WSACleanup();
		}
	};
	WSAStartup_Auto g_wsa;

#else

	#define asd_RaiseSocketException(e) asd_RaiseException("errno:{}", e);

	#define asd_SetNonblockFlag(SockObj, true_or_false)

	#define asd_MoveNonblockFlag(SockObj1, SockObj2)

	typedef void* SockOptValType;

	typedef void* SockBufType;

	inline Socket::Error GetErrorNumber() {
		return errno;
	}

	inline void CloseSocket(const Socket::Handle& socket) {
		if (0 != ::close(socket)) {
			auto e = GetErrorNumber();
			asd_OnErr("fail close, errno:{}", e);
		}
	}

#endif



	int Socket::ToNativeCode(Type a_type)
	{
		switch (a_type) {
			case Socket::Type::TCP:
				return SOCK_STREAM;
			case Socket::Type::UDP:
				return SOCK_DGRAM;
		}
		asd_DAssert(false);
		return -1;
	}



	Socket::Type Socket::FromNativeCode(int a_type)
	{
		switch (a_type) {
			case SOCK_STREAM:
				return Socket::Type::TCP;
			case SOCK_DGRAM:
				return Socket::Type::UDP;
		}
		asd_DAssert(false);
		return Socket::Type::TCP;
	}



	Socket::Socket(Socket::Type a_socketType /*=Type::TCP*/,
				   AddressFamily a_addressFamily /*= AddressFamily::IPv4*/)
	{
		m_socketType = a_socketType;
		m_addressFamily = a_addressFamily;
		asd_SetNonblockFlag(*this, false);
	}



	Socket::Socket(Socket&& a_rval)
	{
		*this = std::move(a_rval);
	}


	Socket::Socket(Handle a_nativeHandle)
	{
		m_handle = a_nativeHandle;
		if (m_handle == InvalidHandle)
			return;

		int result;
		uint32_t size = sizeof(result);
		if (0 == GetSockOpt(SOL_SOCKET, SO_TYPE, &result, size))
			m_socketType = FromNativeCode(result);

		IpAddress ip;
		if (0 == GetSockName(ip))
			m_addressFamily = ip.GetAddressFamily();

		asd_SetNonblockFlag(*this, false);
	}


	Socket::Socket(Handle a_nativeHandle,
				   Socket::Type a_socketType,
				   AddressFamily a_addressFamily)
		: Socket(a_socketType, a_addressFamily)
	{
		m_handle = a_nativeHandle;
	}



	Socket& Socket::operator=(Socket&& a_rval)
	{
		Close();
		std::swap(m_handle, a_rval.m_handle);
		m_socketType = a_rval.m_socketType;
		m_addressFamily = a_rval.m_addressFamily;
		asd_MoveNonblockFlag(*this, a_rval);
		return *this;
	}



	Socket::~Socket()
	{
		Close();
	}



	Socket::Handle Socket::GetNativeHandle() const
	{
		return m_handle;
	}



	void Socket::Close()
	{
		if (m_handle == InvalidHandle)
			return;
		CloseSocket(m_handle);
		m_handle = InvalidHandle;
	}


	Socket::Error Socket::Init(bool a_force /*= false*/)
	{
		return Init(m_socketType, m_addressFamily, a_force);
	}


	Socket::Error Socket::Init(Socket::Type a_socketType,
							   AddressFamily a_addressFamily,
							   bool a_force /*= false*/)
	{
		bool need_init =   m_handle == InvalidHandle
						|| m_socketType != a_socketType
						|| m_addressFamily != a_addressFamily;

		Error ret = 0;
		if (need_init || a_force) {
			Close();
			m_handle = ::socket(IpAddress::ToNativeCode(a_addressFamily),
								Socket::ToNativeCode(a_socketType),
								0);
			if (m_handle == InvalidHandle) {
				ret = GetErrorNumber();
			}
			else {
				m_addressFamily = a_addressFamily;
				m_socketType = a_socketType;
				asd_SetNonblockFlag(*this, false);
			}
		}
		return ret;
	}



	AddressFamily Socket::GetAddressFamily() const
	{
		return m_addressFamily;
	}



	Socket::Type Socket::GetSocektType() const
	{
		return m_socketType;
	}



	Socket::Error Socket::Bind(const IpAddress& a_addr) 
	{
		Error ret = 0;
		ret = Init(m_socketType, a_addr.GetAddressFamily(), false);
		if (ret != 0)
			return ret;

		if (::bind(m_handle, a_addr, a_addr.GetAddrLen()) == -1)
			ret = GetErrorNumber();

		return ret;
	}



	Socket::Error Socket::Listen(int a_backlog /*=1024*/)
	{
		asd_DAssert(m_handle != InvalidHandle);

		Error ret = 0;
		if (::listen(m_handle, a_backlog) == -1)
			ret = GetErrorNumber();

		return ret;
	}



	template <typename SockAddrType>
	inline Socket::Error Accept_Internal(Socket::Handle a_sock,
										 AddressFamily a_addrFam,
										 Socket& a_newbe /*Out*/,
										 Socket::Handle& a_newSocket /*Out*/,
										 IpAddress& a_address /*Out*/)
	{
		Socket::Error ret = 0;
		SockAddrType addr;
		socklen_t addrLen = sizeof(addr);
		Socket::Handle h = ::accept(a_sock,
									(sockaddr*)&addr,
									&addrLen);
		if (h == -1)
			ret = GetErrorNumber();
		else {
			Socket newSock(Socket::Type::TCP, a_addrFam);
			a_newbe = std::move(newSock);
			a_newSocket = h;
			a_address = addr;
		}

		return ret;
	}



	Socket::Error Socket::Accept(Socket& a_newbe /*Out*/,
								 IpAddress& a_address /*Out*/)
	{
		asd_DAssert(m_handle != InvalidHandle);

		Error ret = 0;
		switch (m_addressFamily) {
			case AddressFamily::IPv4: {
				ret = Accept_Internal<sockaddr_in>(m_handle,
												   m_addressFamily,
												   a_newbe,
												   a_newbe.m_handle,
												   a_address);
				break;
			}
			case AddressFamily::IPv6: {
				ret = Accept_Internal<sockaddr_in6>(m_handle,
													m_addressFamily,
													a_newbe,
													a_newbe.m_handle,
													a_address);
				break;
			}
			default:
				asd_DAssert(false);
				break;
		}

		return ret;
	}



	Socket::Error Socket::Connect(const IpAddress& a_dest)
	{
		Error ret = 0;
		ret = Init(m_socketType, a_dest.GetAddressFamily(), false);
		if (ret != 0)
			return ret;

		if (::connect(m_handle, a_dest, a_dest.GetAddrLen()) == -1)
			ret = GetErrorNumber();

		return ret;
	}



	Socket::IoResult Socket::Send(const void* a_buffer,
								  int a_bufferSize,
								  int a_flags /*= 0*/)
	{
		asd_DAssert(m_handle != InvalidHandle);

		IoResult ret;
		ret.m_bytes = ::send(m_handle,
							 (SockBufType)a_buffer,
							 a_bufferSize,
							 a_flags);
		if (ret.m_bytes < 0)
			ret.m_error = GetErrorNumber();

		return ret;
	}



	Socket::IoResult Socket::SendTo(const void* a_buffer,
									int a_bufferSize,
									const IpAddress& a_dest,
									int a_flags /*= 0*/)
	{
		IoResult ret;
		ret.m_error = Init(m_socketType, a_dest.GetAddressFamily(), false);
		if (ret.m_error != 0)
			return ret;

		ret.m_bytes = ::sendto(m_handle,
							   (SockBufType)a_buffer,
							   a_bufferSize,
							   a_flags, 
							   a_dest, 
							   a_dest.GetAddrLen());
		if (ret.m_bytes < 0)
			ret.m_error = GetErrorNumber();

		return ret;
	}



	Socket::IoResult Socket::Recv(void* a_buffer /*Out*/,
								  int a_bufferSize,
								  int a_flags /*= 0*/)
	{
		asd_DAssert(m_handle != InvalidHandle);

		IoResult ret;
		ret.m_bytes = ::recv(m_handle,
							 (SockBufType)a_buffer,
							 a_bufferSize,
							 a_flags);
		if (ret.m_bytes < 0)
			ret.m_error = GetErrorNumber();

		return ret;
	}



	template <typename SockAddrType>
	inline void RecvFrom_Internal(Socket::IoResult& a_result /*Out*/,
								  Socket::Handle a_sock,
								  void* a_buffer /*Out*/,
								  int a_bufferSize,
								  IpAddress& a_src /*Out*/,
								  int a_flags)
	{
		SockAddrType addr;
		socklen_t addrLen = sizeof(addr);
		a_result.m_bytes = ::recvfrom(a_sock,
									  (SockBufType)a_buffer,
									  a_bufferSize,
									  a_flags,
									  (sockaddr*)&addr,
									  &addrLen);
		if (a_result.m_bytes < 0)
			a_result.m_error = GetErrorNumber();
		else
			a_src = addr;
	}



	Socket::IoResult Socket::RecvFrom(void* a_buffer /*Out*/,
									  int a_bufferSize,
									  IpAddress& a_src /*Out*/,
									  int a_flags /*= 0*/)
	{
		IoResult ret;
		ret.m_error = Init(m_socketType, m_addressFamily, false);
		if (ret.m_error != 0)
			return ret;

		switch (m_addressFamily) {
			case asd::AddressFamily::IPv4: {
				RecvFrom_Internal<sockaddr_in>(ret,
											   m_handle,
											   a_buffer,
											   a_bufferSize,
											   a_src,
											   a_flags);
				break;
			}
			case asd::AddressFamily::IPv6: {
				RecvFrom_Internal<sockaddr_in6>(ret,
												m_handle,
												a_buffer,
												a_bufferSize,
												a_src,
												a_flags);
				break;
			}
			default:
				asd_DAssert(false);
				break;
		}

		return ret;
	}



	Socket::Error Socket::SetSockOpt(int a_level,
									 int a_optname,
									 const void* a_optval,
									 uint32_t a_optlen)
	{
		asd_DAssert(m_handle != InvalidHandle);
		auto r = ::setsockopt(m_handle,
							  a_level,
							  a_optname,
							  (const SockOptValType)a_optval,
							  (socklen_t)a_optlen);
		if (r != 0)
			return GetErrorNumber();
		return 0;
	}

	Socket::Error Socket::GetSockOpt(int a_level,
									 int a_optname,
									 void* a_optval /*Out*/,
									 uint32_t& a_optlen /*InOut*/) const
	{
		asd_DAssert(m_handle != InvalidHandle);
		auto r = ::getsockopt(m_handle,
							  a_level,
							  a_optname,
							  (SockOptValType)a_optval,
							  (socklen_t*)&a_optlen);
		if (r != 0)
			return GetErrorNumber();
		return 0;
	}



	Socket::Error Socket::SetSockOpt_ReuseAddr(bool a_set)
	{
		int set = a_set;
		return SetSockOpt(SOL_SOCKET,
						  SO_REUSEADDR,
						  &set, 
						  sizeof(set));
	}

	Socket::Error Socket::GetSockOpt_ReuseAddr(bool& a_result /*Out*/) const
	{
		int result;
		uint32_t size = sizeof(result);
		auto ret = GetSockOpt(SOL_SOCKET,
							  SO_REUSEADDR,
							  &result,
							  size);
		if (ret == 0) {
			a_result = result != 0;
		}
		return ret;
	}



	Socket::Error Socket::SetSockOpt_UseNagle(bool a_set)
	{
		asd_DAssert(m_socketType == Type::TCP);

		int set = !a_set;
		return SetSockOpt(IPPROTO_TCP,
						  TCP_NODELAY,
						  &set,
						  sizeof(set));
	}

	Socket::Error Socket::GetSockOpt_UseNagle(bool& a_result /*Out*/) const
	{
		asd_DAssert(m_socketType == Type::TCP);

		int result;
		uint32_t size = sizeof(result);
		auto ret = GetSockOpt(IPPROTO_TCP,
							  TCP_NODELAY,
							  &result,
							  size);
		if (ret == 0) {
			a_result = result == 0;
		}
		return ret;
	}



	Socket::Error Socket::SetSockOpt_Linger(bool a_use,
											uint16_t a_sec)
	{
		linger val;
		val.l_onoff = a_use;
		val.l_linger = a_sec;
		return SetSockOpt(SOL_SOCKET,
						  SO_LINGER,
						  &val,
						  sizeof(val));
	}

	Socket::Error Socket::GetSockOpt_Linger(bool& a_use /*Out*/,
											uint16_t& a_sec /*Out*/) const
	{
		linger val;
		uint32_t size = sizeof(val);
		auto ret = GetSockOpt(SOL_SOCKET,
							  SO_LINGER,
							  &val,
							  size);
		if (ret == 0) {
			a_use = (val.l_onoff != 0);
			a_sec = (uint16_t)val.l_linger;
		}
		return ret;
	}



	Socket::Error Socket::SetSockOpt_RecvBufSize(int a_byte)
	{
		return SetSockOpt(SOL_SOCKET,
						  SO_RCVBUF,
						  &a_byte,
						  sizeof(a_byte));
	}

	Socket::Error Socket::GetSockOpt_RecvBufSize(int& a_byte /*Out*/) const
	{
		int result;
		uint32_t size = sizeof(result);
		auto ret = GetSockOpt(SOL_SOCKET,
							  SO_RCVBUF,
							  &result,
							  size);
		if (ret == 0) {
			a_byte = result;
		}
		return ret;
	}



	Socket::Error Socket::SetSockOpt_SendBufSize(int a_byte)
	{
		return SetSockOpt(SOL_SOCKET,
						  SO_SNDBUF,
						  &a_byte,
						  sizeof(a_byte));
	}

	Socket::Error Socket::GetSockOpt_SendBufSize(int& a_byte /*Out*/) const
	{
		int result;
		uint32_t size = sizeof(result);
		auto ret = GetSockOpt(SOL_SOCKET,
							  SO_SNDBUF,
							  &result,
							  size);
		if (ret == 0) {
			a_byte = result;
		}
		return ret;
	}



	Socket::Error Socket::GetSockOpt_Error(int& a_error /*Out*/) const
	{
		int result;
		uint32_t size = sizeof(result);
		auto ret = GetSockOpt(SOL_SOCKET,
							  SO_ERROR,
							  &result,
							  size);
		if (ret == 0) {
			a_error = result;
		}
		return ret;
	}



	Socket::Error Socket::SetNonblock(bool a_nonblock)
	{
		asd_DAssert(m_handle != InvalidHandle);

#if defined(asd_Platform_Windows)
		Error ret = 0;
		u_long arg = a_nonblock;
		if (::ioctlsocket(m_handle, FIONBIO, &arg) != 0)
			ret = ::WSAGetLastError();
		else
			m_nonblock = a_nonblock;

		return ret;
#else
		int val = ::fcntl(m_handle, F_GETFL, 0);
		if (val == -1) 
			return errno;

		if (a_nonblock)
			val |= O_NONBLOCK;
		else
			val &= ~O_NONBLOCK;

		if (::fcntl(m_handle, F_SETFL, val) == -1) {
			return errno;
		}
		return 0;
#endif
	}

	Socket::Error Socket::CheckNonblock(bool& a_result /*Out*/) const
	{
		asd_DAssert(m_handle != InvalidHandle);

#if defined(asd_Platform_Windows)
		a_result = m_nonblock;
		return 0;
#else
		int val = ::fcntl(m_handle, F_GETFL, 0);
		if (val == -1)
			return errno;

		a_result = (val & O_NONBLOCK) != 0;
		return 0;
#endif
	}


	template <typename Func>
	Socket::Error GetIpAddress(Func a_getxxxxname,
							   Socket::Handle a_sock,
							   IpAddress& a_addr /*Out*/)
	{
		asd_DAssert(a_sock != Socket::InvalidHandle);

		uint8_t buf[sizeof(sockaddr_in6) * 2];
		sockaddr* addr = (sockaddr*)buf;
		socklen_t len = sizeof(buf);
		if (0 != a_getxxxxname(a_sock, addr, &len))
			return GetErrorNumber();

		switch (addr->sa_family) {
			case AF_INET: {
				auto cast = (sockaddr_in*)addr;
				a_addr = *cast;
				break;
			}
			case AF_INET6: {
				auto cast = (sockaddr_in6*)addr;
				a_addr = *cast;
				break;
			}
			default:
				asd_OnErr("unknown address family, {}", addr->sa_family);
				return -1;
		}
		return 0;
	}

	Socket::Error Socket::GetSockName(IpAddress& a_addr /*Out*/) const
	{
		return GetIpAddress(::getsockname, m_handle, a_addr);
	}

	Socket::Error Socket::GetPeerName(IpAddress& a_addr /*Out*/) const
	{
		return GetIpAddress(::getpeername, m_handle, a_addr);
	}



	EasySocket::EasySocket(Socket::Type a_socketType /*= Socket::Type::TCP*/,
						   AddressFamily a_addressFamily /*= AddressFamily::IPv4*/)
	{
		m_socketType = a_socketType;
		m_socket = Socket_ptr(new Socket(m_socketType, a_addressFamily));
		memset(m_recvBuffer, 0, BufferSize);
		memset(m_sendBuffer, 0, BufferSize);
	}



	void EasySocket::Bind(const IpAddress& a_addr)
	{
		asd_DAssert(m_socket != nullptr);

		auto e = m_socket->Bind(a_addr);
		if (e != 0) {
			asd_RaiseSocketException(e);
		}
	}



	void EasySocket::Listen(int a_backlog /*= 1024*/)
	{
		asd_DAssert(m_socket != nullptr);

		auto e = m_socket->Listen(a_backlog);
		if (e != 0) {
			asd_RaiseSocketException(e);
		}
	}



	void EasySocket::Accept(Socket& a_newbe /*Out*/,
							IpAddress& a_address /*Out*/)
	{
		asd_DAssert(m_socket != nullptr);

		auto e = m_socket->Accept(a_newbe, a_address);
		if (e != 0) {
			asd_RaiseSocketException(e);
		}
	}



	void EasySocket::Connect(const IpAddress& a_dest)
	{
		asd_DAssert(m_socket != nullptr);

		auto e = m_socket->Connect(a_dest);
		if (e != 0) {
			asd_RaiseSocketException(e);
		}
	}



	void EasySocket::Send(const Buffer& a_buffer,
						  int a_flags /*= 0*/)
	{
		asd_DAssert(m_socket != nullptr);

		auto e = m_socket->Send(a_buffer.data(), 
								a_buffer.size(), 
								a_flags);
		if (e.m_error != 0) {
			asd_RaiseSocketException(e.m_error);
		}
	}



	void EasySocket::SendTo(const Buffer& a_buffer,
							const IpAddress& a_dest,
							int a_flags /*= 0*/)
	{
		asd_DAssert(m_socket != nullptr);

		auto e = m_socket->SendTo(a_buffer.data(),
								  a_buffer.size(),
								  a_dest,
								  a_flags);

		if (e.m_error != 0) {
			asd_RaiseSocketException(e.m_error);
		}
	}



	void EasySocket::Recv(Buffer& a_buffer /*Out*/,
						  int a_flags /*= 0*/,
						  int a_recvComplete /*= -1*/)
	{
		asd_DAssert(m_socket != nullptr);

		Socket::IoResult r;
		a_buffer.clear();
		do {
			r = m_socket->Recv(m_recvBuffer, 
							   BufferSize, 
							   a_flags);

			if (r.m_error != 0) {
				asd_RaiseSocketException(r.m_error);
			}

			for (int i=0; i<r.m_bytes; ++i) {
				a_buffer.emplace_back(m_recvBuffer[i]);
			}

		} while (0 <= a_recvComplete && r.m_bytes < a_recvComplete);
	}



	void EasySocket::RecvFrom(Buffer& a_buffer /*Out*/,
							  IpAddress& a_src /*Out*/,
							  int a_flags /*= 0*/)
	{
		asd_DAssert(m_socket != nullptr);

		Socket::IoResult r;
		a_buffer.clear();
		r = m_socket->RecvFrom(m_recvBuffer,
								BufferSize,
								a_src,
								a_flags);

		if (r.m_error != 0) {
			asd_RaiseSocketException(r.m_error);
		}

		for (int i=0; i<r.m_bytes; ++i) {
			a_buffer.emplace_back(m_recvBuffer[i]);
		}
	}



	void EasySocket::Close() 
	{
		m_socket = Socket_ptr(nullptr);
	}
}
