#include "stdafx.h"
#include "asd/socket.h"
#include "asd/exception.h"
#include <string>
#include <cstdio>
#include <algorithm>
#include <cassert>


# if defined(asd_Platform_Windows)
#	include <WinSock2.h>
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
	// 가독성과 일관성을 위해 
	// 각 플랫폼 별로 상이한 부분들을 랩핑
#if defined(asd_Platform_Windows)

	#define asd_RaiseSocketException(e) asd_RaiseException("WSAGetLastError:%#x", e);

	#define asd_SetNonblockFlag(SockObj, true_or_false) (SockObj).m_nonblock = true_or_false

	#define asd_SwapNonblockFlag(SockObj1, SockObj2) std::swap((SockObj1).m_nonblock, (SockObj2).m_nonblock)

	typedef char* SockOptValType;

	typedef char* SockBufType;

	inline Socket::Error GetErrorNumber() {
		return WSAGetLastError();
	}

	inline int CloseSocket(const Socket::Handle& socket) {
		return closesocket(socket);
	}

	// Winsock DLL 초기화를 위한 클래스
	struct WSAStartup_Auto
	{
		WSADATA m_wsa;
		WORD m_ver;

		WSAStartup_Auto() {
			m_ver = MAKEWORD(2, 0);
			auto err = WSAStartup(m_ver, &m_wsa);
			if ( err != 0 ) {
				asd_RaiseSocketException(err);
			}
		}

		~WSAStartup_Auto() {
			WSACleanup();
		}
	};

	WSAStartup_Auto g_wsa;

#else

	#define asd_RaiseSocketException(e) asd_RaiseException("errno:%#x", e);

	#define asd_SetNonblockFlag(SockObj, true_or_false)

	#define asd_SwapNonblockFlag(SockObj1, SockObj2)

	typedef void* SockOptValType;

	typedef void* SockBufType;

	inline Socket::Error GetErrorNumber() {
		return errno;
	}

	inline int CloseSocket(const Socket::Handle& socket) {
		return close(socket);
	}

#endif



	int Socket::ToNativeCode(Type a_type) asd_noexcept
	{
		switch (a_type) {
			case Socket::TCP:
				return SOCK_STREAM;
			case Socket::UDP:
				return SOCK_DGRAM;
		}
		assert(false);
		return -1;
	}



	Socket::Socket(IN IpAddress::Family a_addressFamily /*= IpAddress::IPv4*/,
				   IN Socket::Type a_socketType /*=TCP*/) asd_noexcept
	{
		m_addressFamily = a_addressFamily;
		m_socketType = a_socketType;
		asd_SetNonblockFlag(*this, false);
	}



	Socket::Socket(MOVE Socket&& a_rval) asd_noexcept
	{
		*this = std::move(a_rval);
	}



	Socket&
	Socket::operator = (MOVE Socket&& a_rval) asd_noexcept
	{
		std::swap(m_handle, a_rval.m_handle);
		std::swap(m_addressFamily, a_rval.m_addressFamily);
		std::swap(m_socketType, a_rval.m_socketType);
		asd_SwapNonblockFlag(*this, a_rval);
		return *this;
	}



	Socket::~Socket() asd_noexcept
	{
		Close();
	}



	Socket::Handle
	Socket::GetNativeHandle() const asd_noexcept
	{
		return m_handle;
	}



	void
	Socket::Close() asd_noexcept
	{
		if (m_handle == InvalidHandle)
			return;

		CloseSocket(m_handle);
		m_handle = InvalidHandle;
	}



	Socket::Error
	Socket::Init(IN IpAddress::Family a_addressFamily,
				 IN Socket::Type a_socketType,
				 IN bool a_force) asd_noexcept
	{
		bool need_init =   m_handle == InvalidHandle
						|| m_socketType != a_socketType
						|| m_addressFamily != a_addressFamily;

		Error ret = 0;
		if (need_init || a_force) {
			Close();
			m_handle = socket(IpAddress::ToNativeCode(a_addressFamily),
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



	Socket::Error
	Socket::SetSockOpt(IN int a_level,
					   IN int a_optname,
					   IN const void* a_optval,
					   IN uint32_t a_optlen) asd_noexcept
	{
		assert(m_handle != InvalidHandle);
		return setsockopt(m_handle,
						  a_level,
						  a_optname,
						  (const SockOptValType)a_optval,
						  (socklen_t)a_optlen);
	}



	Socket::Error
	Socket::GetSockOpt(IN int a_level,
					   IN int a_optname,
					   OUT void* a_optval,
					   INOUT uint32_t& a_optlen) const asd_noexcept
	{
		assert(m_handle != InvalidHandle);
		return getsockopt(m_handle, 
						  a_level,
						  a_optname,
						  (SockOptValType)a_optval,
						  (socklen_t*)&a_optlen);
	}



	Socket::Error
	Socket::SetSockOpt_ReuseAddr(IN bool a_set) asd_noexcept
	{
		int set = a_set;
		return SetSockOpt(SOL_SOCKET,
						  SO_REUSEADDR,
						  &set, 
						  sizeof(set));
	}



	Socket::Error
	Socket::GetSockOpt_ReuseAddr(OUT bool& a_result) const asd_noexcept
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



	Socket::Error
	Socket::SetSockOpt_UseNagle(IN bool a_set) asd_noexcept
	{
		assert(m_socketType == Type::TCP);

		int set = !a_set;
		return SetSockOpt(IPPROTO_TCP,
						  TCP_NODELAY,
						  &set,
						  sizeof(set));
	}



	Socket::Error
	Socket::GetSockOpt_UseNagle(OUT bool& a_result) const asd_noexcept
	{
		assert(m_socketType == Type::TCP);

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



	Socket::Error
	Socket::SetSockOpt_Linger(IN bool a_use,
							  IN uint16_t a_sec) asd_noexcept
	{
		linger val;
		val.l_onoff = a_use;
		val.l_linger = a_sec;
		return SetSockOpt(SOL_SOCKET,
						  SO_REUSEADDR,
						  &val,
						  sizeof(val));
	}



	Socket::Error
	Socket::GetSockOpt_Linger(OUT bool& a_use,
							  OUT uint16_t& a_sec) const asd_noexcept
	{
		linger val;
		uint32_t size = sizeof(val);
		auto ret = GetSockOpt(SOL_SOCKET,
							  SO_REUSEADDR,
							  &val,
							  size);
		if (ret == 0) {
			a_use = (val.l_onoff != 0);
			a_sec = (uint16_t)val.l_linger;
		}
		return ret;
	}



	Socket::Error 
	Socket::SetSockOpt_RecvBufSize(IN int a_byte) asd_noexcept
	{
		return SetSockOpt(SOL_SOCKET,
						  SO_RCVBUF,
						  &a_byte,
						  sizeof(a_byte));
	}



	Socket::Error 
	Socket::GetSockOpt_RecvBufSize(OUT int& a_byte) const asd_noexcept
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



	Socket::Error 
	Socket::SetSockOpt_SendBufSize(IN int a_byte) asd_noexcept
	{
		return SetSockOpt(SOL_SOCKET,
						  SO_SNDBUF,
						  &a_byte,
						  sizeof(a_byte));
	}



	Socket::Error 
	Socket::GetSockOpt_SendBufSize(OUT int& a_byte) const asd_noexcept
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



	Socket::Error 
	Socket::SetNonblock(IN bool a_nonblock) asd_noexcept
	{
		assert(m_handle != InvalidHandle);

#if defined(asd_Platform_Windows)
		Error ret = 0;
		u_long arg = a_nonblock;
		if (ioctlsocket(m_handle, FIONBIO, &arg) != 0)
			ret = WSAGetLastError();
		else
			m_nonblock = a_nonblock;

		return ret;
#else
		int val = fcntl(m_handle, F_GETFL, 0);
		if (val == -1) 
			return errno;

		if (a_nonblock)
			val |= O_NONBLOCK;
		else
			val &= ~O_NONBLOCK;

		if (fcntl(m_handle, F_SETFL, val) == -1) {
			return errno;
		}
		return 0;
#endif
	}



	Socket::Error 
	Socket::CheckNonblock(OUT bool& a_result) const asd_noexcept
	{
		assert(m_handle != InvalidHandle);

#if defined(asd_Platform_Windows)
		a_result = m_nonblock;
		return 0;
#else
		int val = fcntl(m_handle, F_GETFL, 0);
		if (val == -1)
			return errno;

		a_result = (val & O_NONBLOCK) != 0;
		return 0;
#endif
	}



	// getxxxxname 함수의 인자 타입이 플랫폼이나 컴파일러마다 너무 제각각이라서
	// 템플릿함수 대신 매크로함수를 사용
#define asd_GetIpAddress_Internal(getxxxxname, AddrType, a_sock, a_addr)					\
	{																						\
		AddrType addr;																		\
		socklen_t len = sizeof(AddrType);													\
		if (getxxxxname(a_sock, (sockaddr*)&addr, &len) != 0) {								\
			return GetErrorNumber();														\
		}																					\
		a_addr = addr;																		\
	}																						\
	
#define asd_GetIpAddress_Case(getxxxxname, a_sock, a_addrFam, a_addr)						\
	{																						\
		assert(a_sock != Socket::InvalidHandle);											\
																							\
		Socket::Error ret = 0;																\
		switch (a_addrFam) {																\
			case IpAddress::IPv4: {															\
				asd_GetIpAddress_Internal(getxxxxname,										\
										  sockaddr_in,										\
										  a_sock,											\
										  a_addr);											\
				break;																		\
			}																				\
			case IpAddress::IPv6: {															\
				asd_GetIpAddress_Internal(getxxxxname,										\
										  sockaddr_in6,										\
										  a_sock,											\
										  a_addr);											\
				break;																		\
			}																				\
			default: {																		\
				bool AF_Valid = false;														\
				assert(AF_Valid);															\
				break;																		\
			}																				\
		}																					\
		return ret;																			\
	}																						\



	Socket::Error 
	Socket::GetSockName(OUT IpAddress& a_addr) const asd_noexcept
	{
		asd_GetIpAddress_Case(getsockname,
							  m_handle,
							  m_addressFamily,
							  a_addr);
	}



	Socket::Error 
	Socket::GetPeerName(OUT IpAddress& a_addr) const asd_noexcept
	{
		asd_GetIpAddress_Case(getpeername,
							  m_handle,
							  m_addressFamily,
							  a_addr);
	}



	Socket::Error 
	Socket::Bind(IN const IpAddress& a_addr)  asd_noexcept
	{
		Error ret = 0;
		ret = Init(a_addr.GetAddressFamily(), m_socketType, false);
		if (ret != 0)
			return ret;

		if (bind(m_handle, a_addr, a_addr.GetAddrLen()) == -1)
			ret = GetErrorNumber();

		return ret;
	}



	Socket::Error 
	Socket::Listen(IN int a_backlog /*=1024*/) asd_noexcept
	{
		assert(m_handle != InvalidHandle);

		Error ret = 0;
		if (listen(m_handle, a_backlog) == -1)
			ret = GetErrorNumber();

		return ret;
	}



	template <typename SockAddrType>
	inline Socket::Error 
	Accept_Internal(IN Socket::Handle a_sock,
					IN IpAddress::Family a_addrFam,
					OUT Socket& a_newbe,
					OUT Socket::Handle& a_newSocket,
					OUT IpAddress& a_address)
	{
		Socket::Error ret = 0;
		SockAddrType addr;
		socklen_t addrLen = sizeof(addr);
		Socket::Handle h = accept(a_sock,
								  (sockaddr*)&addr,
								  &addrLen);
		if (h == -1)
			ret = GetErrorNumber();
		else {
			Socket newSock(a_addrFam, Socket::TCP);
			a_newbe = std::move(newSock);
			a_newSocket = h;
			a_address = addr;
		}

		return ret;
	}



	Socket::Error 
	Socket::Accept(OUT Socket& a_newbe,
				   OUT IpAddress& a_address) asd_noexcept
	{
		assert(m_handle != InvalidHandle);

		Error ret = 0;
		switch (m_addressFamily) {
			case IpAddress::IPv4: {
				ret = Accept_Internal<sockaddr_in>(m_handle,
												   m_addressFamily,
												   a_newbe,
												   a_newbe.m_handle,
												   a_address);
				break;
			}
			case IpAddress::IPv6: {
				ret = Accept_Internal<sockaddr_in6>(m_handle,
													m_addressFamily,
													a_newbe,
													a_newbe.m_handle,
													a_address);
				break;
			}
			default:
				assert(false);
				break;
		}

		return ret;
	}



	Socket::Error
	Socket::Connect(IN const IpAddress& a_dest) asd_noexcept
	{
		Error ret = 0;
		ret = Init(a_dest.GetAddressFamily(), m_socketType, false);
		if (ret != 0)
			return ret;

		if (connect(m_handle, a_dest, a_dest.GetAddrLen()) == -1)
			ret = GetErrorNumber();

		return ret;
	}



	Socket::IoResult
	Socket::Send(IN const void* a_buffer,
				 IN int a_bufferSize,
				 IN int a_flags /*= 0*/) asd_noexcept
	{
		assert(m_handle != InvalidHandle);

		IoResult ret;
		ret.m_bytes = send(m_handle,
						   (SockBufType)a_buffer,
						   a_bufferSize,
						   a_flags);
		if (ret.m_bytes < 0)
			ret.m_error = GetErrorNumber();

		return ret;
	}



	Socket::IoResult
	Socket::SendTo(IN const void* a_buffer,
				   IN int a_bufferSize,
				   IN const IpAddress& a_dest,
				   IN int a_flags /*= 0*/) asd_noexcept
	{
		IoResult ret;
		ret.m_error = Init(m_addressFamily, m_socketType, false);
		if (ret.m_error != 0)
			return ret;

		ret.m_bytes = sendto(m_handle, 
							 (SockBufType)a_buffer,
							 a_bufferSize,
							 a_flags, 
							 a_dest, 
							 a_dest.GetAddrLen());
		if (ret.m_bytes < 0)
			ret.m_error = GetErrorNumber();

		return ret;
	}



	Socket::IoResult
	Socket::Recv(OUT void* a_buffer,
				 IN int a_bufferSize,
				 IN int a_flags /*= 0*/) asd_noexcept
	{
		assert(m_handle != InvalidHandle);

		IoResult ret;
		ret.m_bytes = recv(m_handle, 
						   (SockBufType)a_buffer,
						   a_bufferSize,
						   a_flags);
		if (ret.m_bytes < 0)
			ret.m_error = GetErrorNumber();

		return ret;
	}



	template <typename SockAddrType>
	inline void
	RecvFrom_Internal(OUT Socket::IoResult& a_result,
					  IN Socket::Handle a_sock,
					  OUT void* a_buffer,
					  IN int a_bufferSize,
					  OUT IpAddress& a_src,
					  IN int a_flags)
	{
		SockAddrType addr;
		socklen_t addrLen = sizeof(addr);
		a_result.m_bytes = recvfrom(a_sock,
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



	Socket::IoResult
	Socket::RecvFrom(OUT void* a_buffer,
					 IN int a_bufferSize,
					 OUT IpAddress& a_src,
					 IN int a_flags /*= 0*/) asd_noexcept
	{
		IoResult ret;
		ret.m_error = Init(m_addressFamily, m_socketType, false);
		if (ret.m_error != 0)
			return ret;

		switch (m_addressFamily) {
			case asd::IpAddress::IPv4: {
				RecvFrom_Internal<sockaddr_in>(ret,
											   m_handle,
											   a_buffer,
											   a_bufferSize,
											   a_src,
											   a_flags);
				break;
			}
			case asd::IpAddress::IPv6: {
				RecvFrom_Internal<sockaddr_in6>(ret,
												m_handle,
												a_buffer,
												a_bufferSize,
												a_src,
												a_flags);
				break;
			}
			default:
				assert(false);
				break;
		}

		return ret;
	}



	EasySocket::EasySocket(IN IpAddress::Family a_addressFamily /*= IpAddress::IPv4*/,
						   IN Socket::Type a_socketType /*= Socket::Type::TCP*/)
	{
		m_socketType = a_socketType;
		m_socket = Socket_ptr(new Socket(a_addressFamily, m_socketType));
		memset(m_recvBuffer, 0, BufferSize);
		memset(m_sendBuffer, 0, BufferSize);
	}



	void
	EasySocket::Bind(IN const IpAddress& a_addr)
	{
		assert(m_socket != nullptr);

		auto e = m_socket->Bind(a_addr);
		if (e != 0) {
			asd_RaiseSocketException(e);
		}
	}



	void
	EasySocket::Listen(IN int a_backlog /*= 1024*/)
	{
		assert(m_socket != nullptr);

		auto e = m_socket->Listen(a_backlog);
		if (e != 0) {
			asd_RaiseSocketException(e);
		}
	}



	void
	EasySocket::Accept(OUT Socket& a_newbe,
					   OUT IpAddress& a_address)
	{
		assert(m_socket != nullptr);

		auto e = m_socket->Accept(a_newbe, a_address);
		if (e != 0) {
			asd_RaiseSocketException(e);
		}
	}



	void
	EasySocket::Connect(IN const IpAddress& a_dest)
	{
		assert(m_socket != nullptr);

		auto e = m_socket->Connect(a_dest);
		if (e != 0) {
			asd_RaiseSocketException(e);
		}
	}



	void
	EasySocket::Send(IN const Buffer& a_buffer,
					 IN int a_flags /*= 0*/)
	{
		assert(m_socket != nullptr);

		auto e = m_socket->Send(a_buffer.data(), 
								a_buffer.size(), 
								a_flags);
		if (e.m_error != 0) {
			asd_RaiseSocketException(e.m_error);
		}
	}



	void
	EasySocket::SendTo(IN const Buffer& a_buffer,
					   IN const IpAddress& a_dest,
					   IN int a_flags /*= 0*/)
	{
		assert(m_socket != nullptr);

		auto e = m_socket->SendTo(a_buffer.data(),
								  a_buffer.size(),
								  a_dest,
								  a_flags);

		if (e.m_error != 0) {
			asd_RaiseSocketException(e.m_error);
		}
	}



	void
	EasySocket::Recv(OUT Buffer& a_buffer,
					 IN int a_flags /*= 0*/)
	{
		assert(m_socket != nullptr);

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
				a_buffer.push_back(m_recvBuffer[i]);
			}

		} while (r.m_bytes < BufferSize);
	}



	void
	EasySocket::RecvFrom(OUT Buffer& a_buffer,
						 OUT IpAddress& a_src,
						 IN int a_flags /*= 0*/)
	{
		assert(m_socket != nullptr);

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
			a_buffer.push_back(m_recvBuffer[i]);
		}
	}



	void
	EasySocket::Close() 
	{
		m_socket = Socket_ptr(nullptr);
	}
}
