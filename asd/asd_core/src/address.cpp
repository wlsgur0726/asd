#include "stdafx.h"
#include "asd/address.h"

#if defined(asd_Platform_Windows)
#	include <WinSock2.h>
#	include <WS2tcpip.h>
#
#else
#	include <string.h>
#	include <netdb.h>
#	include <arpa/inet.h>
#
#endif

namespace asd
{
	struct AddrInfos
	{
		int m_error = 0;
		addrinfo* m_result = nullptr;

		AddrInfos(IN const char* a_domain)
		{
			addrinfo hints;
			memset(&hints, 0, sizeof(hints));

			m_error = getaddrinfo(a_domain, NULL, &hints, &m_result);
		}

		// 1순위 : IPv4,
		// 2순위 : IPv6,
		// 그 외 : nullptr
		addrinfo* Get()
		{
			assert(m_result != nullptr);
			assert(m_error == 0);

			addrinfo* ret = nullptr;
			for (auto p=m_result; p!=nullptr; p=p->ai_next) {
				if (p->ai_family == AF_INET)
					return p;
				else if (p->ai_family == AF_INET6) {
					if (ret == nullptr)
						ret = p;
				}
			}
			return ret;
		}

		~AddrInfos()
		{
			if (m_result != nullptr)
				freeaddrinfo(m_result);
		}
	};



	IpAddress::~IpAddress()
	{
		if (m_addr != nullptr) {
			uint8_t* p = (uint8_t*)m_addr;
			delete[] p;
			m_addr = nullptr;
			m_addrlen = 0;
			m_addrFamily = IPv4;
		}
	}



	IpAddress::IpAddress(IN Family a_addrFamily /*= IPv4*/) asd_NoThrow
	{
		m_addrFamily = a_addrFamily;
	}



	IpAddress::IpAddress(IN const char* a_domain,
						 IN uint16_t a_port /*= 0*/) asd_Throws(asd::Exception)
	{
		AddrInfos ais(a_domain);
		if (ais.m_error != 0) {
			asd_RaiseException("fail getaddrinfo(), error:0x%x", ais.m_error);
		}

		addrinfo* ai = ais.Get();
		if (ai == nullptr) {
			asd_RaiseException("[%s] does not exist!", a_domain);
		}

		switch (ai->ai_family) {
			case AF_INET: {
				auto& sin = *(sockaddr_in*)ai->ai_addr;
				sin.sin_port = htons(a_port);
				assert(sin.sin_family == AF_INET);
				*this = sin;
				break;
			}
			case AF_INET6: {
				auto& sin = *(sockaddr_in6*)ai->ai_addr;
				sin.sin6_port = htons(a_port);
				assert(sin.sin6_family == AF_INET6);
				*this = sin;
				break;
			}
			default:
				// 지원되지 않는 Address Family
				assert(false);
				break;
		}
	}



	IpAddress::IpAddress(IN const IpAddress& a_cp) asd_NoThrow
	{
		*this = a_cp;
	}



	IpAddress& IpAddress::operator = (IN const IpAddress& a_cp) asd_NoThrow
	{
		this->~IpAddress();
		m_addrFamily = a_cp.m_addrFamily;
		if (a_cp.m_addr != nullptr) {
			m_addrlen = a_cp.m_addrlen;
			m_addr = (sockaddr*)new uint8_t[m_addrlen];
			memcpy(m_addr, a_cp.m_addr, m_addrlen);
		}
		return *this;
	}



	IpAddress::IpAddress(MOVE IpAddress&& a_rval) asd_NoThrow
	{
		*this = std::move(a_rval);
	}



	IpAddress& IpAddress::operator = (MOVE IpAddress&& a_rval) asd_NoThrow
	{
		this->~IpAddress();
		m_addrFamily = a_rval.m_addrFamily;
		m_addr = a_rval.m_addr;
		m_addrlen = a_rval.m_addrlen;

		a_rval.m_addr = nullptr;
		return *this;
	}



#define asd_IpAddress_Define_Init(NativeType, Family)									\
	IpAddress::IpAddress(IN const NativeType& a_native) asd_NoThrow						\
	{																					\
		*this = a_native;																\
	}																					\
																						\
	IpAddress& IpAddress::operator = (IN const NativeType& a_native) asd_NoThrow		\
	{																					\
		this->~IpAddress();																\
		m_addrlen = sizeof(NativeType);													\
		m_addr = (sockaddr*)new uint8_t[m_addrlen];										\
		memcpy(m_addr, &a_native, m_addrlen);											\
		m_addrFamily = Family;															\
		return *this;																	\
	}																					\

	asd_IpAddress_Define_Init(sockaddr_in, IPv4);

	asd_IpAddress_Define_Init(sockaddr_in6, IPv6);



	IpAddress::operator const sockaddr* () const asd_NoThrow
	{
		return m_addr;
	}



	int IpAddress::GetAddrLen() const asd_NoThrow
	{
		return m_addrlen;
	}



	IpAddress::Family IpAddress::GetAddressFamily() const asd_NoThrow
	{
		return m_addrFamily;
	}



	void* IpAddress::GetIp(OUT int* a_len /*= nullptr*/) const asd_NoThrow
	{
		if (m_addr == nullptr)
			return nullptr;

		void* p = nullptr;
		int sz = 0;

		switch (m_addrFamily) {
			case IPv4: {
				auto cast = (sockaddr_in*)m_addr;
				p = &cast->sin_addr;
				sz = 4; // sizeof(cast->sin_addr);
				break;
			}
			case IPv6: {
				auto cast = (sockaddr_in6*)m_addr;
				p = &cast->sin6_addr;
				sz = 16; // izeof(cast->sin6_addr);
				break;
			}
			default:
				assert(false);
				break;
		}
		
		if ( a_len != nullptr )
			*a_len = sz;

		return p;
	}


	
	uint16_t IpAddress::GetPort() const asd_NoThrow
	{
		if (m_addr == nullptr)
			return 0;

		switch (m_addrFamily) {
			case IPv4: {
				auto cast = (sockaddr_in*)m_addr;
				return ntohs(cast->sin_port);
			}
			case IPv6: {
				auto cast = (sockaddr_in6*)m_addr;
				return ntohs(cast->sin6_port);
			}
			default:
				assert(false);
				break;
		}
		return 0;
	}



	MString IpAddress::ToString() const asd_NoThrow
	{
		if (m_addr == nullptr)
			return "";

		const int BufSize = 64;
		char ip[BufSize];
		ip[0] = '\0';
		
		inet_ntop(m_addrFamily, GetIp(), ip, BufSize);

		return MString("%s:%u", ip, (uint32_t)GetPort());
	}



	int IpAddress::Compare(IN const IpAddress& a_left,
						   IN const IpAddress& a_right) asd_NoThrow
	{
		if (a_left.m_addr == a_right.m_addr) {
			assert(a_left.m_addrFamily == a_right.m_addrFamily);
			return 0;
		}

		// 1. null 여부
		if (a_left.m_addr == nullptr)
			return -1;
		if (a_right.m_addr == nullptr)
			return 1;


		// 2. Family 비교
		if (a_left.m_addrFamily < a_right.m_addrFamily)
			return -1;
		else if (a_left.m_addrFamily > a_right.m_addrFamily)
			return 1;
		
		assert(a_left.m_addrFamily == a_right.m_addrFamily);
		assert(a_left.m_addrlen == a_right.m_addrlen);


		// 3. IP 비교
		int len;
		void* ip1 = a_left.GetIp(&len);
		void* ip2 = a_right.GetIp();
			
		int cmp = memcmp(ip1, ip2, len);
		if (cmp != 0)
			return cmp;


		// 4. Port 비교
		auto port1 = a_left.GetPort();
		auto port2 = a_right.GetPort();
		if (port1 < port2)
			return -1;
		else if (port1 > port2)
			return 1;
		
		return 0;
	}



	size_t IpAddress::Hash::operator () (IN const IpAddress& a_addr) const asd_NoThrow
	{
		if (a_addr.m_addr == nullptr)
			return 0;

		const int SizeOfSizeT = sizeof(size_t);
		size_t ret = 0;

		int len;
		void* ip = a_addr.GetIp(&len);

		if (len <= SizeOfSizeT) {
			// IPv4
			memcpy(&ret, ip, len);
		}
		else {
			// IPv6
			assert(len == 16);
			assert(len % SizeOfSizeT == 0);
			const int Count = len / SizeOfSizeT;
			size_t* arr = (size_t*)ip;
			for (int i=0; i<Count; ++i) {
				ret ^= arr[i];
			}
		}

		size_t port = a_addr.GetPort();
		port << (SizeOfSizeT - 2);
		ret ^= port;

		return ret;
	}
}
