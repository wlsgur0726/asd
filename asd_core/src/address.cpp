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
	int IpAddress::ToNativeCode(AddressFamily a_family)
	{
		switch (a_family) {
			case AddressFamily::IPv4:
				return AF_INET;
			case AddressFamily::IPv6:
				return AF_INET6;
		}
		asd_OnErr("invalid AddressFamily");
		return -1;
	}



	IpAddress::~IpAddress()
	{
		if (m_addr != nullptr) {
			uint8_t* p = (uint8_t*)m_addr;
			delete[] p;
			m_addr = nullptr;
			m_addrlen = 0;
			m_addrFamily = AddressFamily::IPv4;
		}
	}



	IpAddress::IpAddress(IN AddressFamily a_addrFamily /*= IPv4*/)
	{
		m_addrFamily = a_addrFamily;
	}



	IpAddress::IpAddress(IN const char* a_ip,
						 IN uint16_t a_port /*= 0*/)
	{
		auto list = FindIP(a_ip);
		if (list.empty())
			return;
		*this = std::move(list[0]);
		SetPort(a_port);
	}



	IpAddress::IpAddress(IN const IpAddress& a_cp)
	{
		*this = a_cp;
	}



	IpAddress::IpAddress(MOVE IpAddress&& a_rval)
	{
		*this = std::move(a_rval);
	}



	IpAddress::IpAddress(IN const sockaddr_in& a_native)
	{
		*this = a_native;
	}



	IpAddress::IpAddress(IN const sockaddr_in6& a_native)
	{
		*this = a_native;
	}



	IpAddress& IpAddress::operator=(IN const IpAddress& a_cp)
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



	IpAddress& IpAddress::operator=(MOVE IpAddress&& a_rval)
	{
		std::swap(m_addr, a_rval.m_addr);
		std::swap(m_addrlen, a_rval.m_addrlen);
		std::swap(m_addrFamily, a_rval.m_addrFamily);
		return *this;
	}



	IpAddress& IpAddress::operator=(IN const sockaddr_in& a_native)
	{
		this->~IpAddress();
		m_addrlen = sizeof(sockaddr_in);
		m_addr = (sockaddr*)new uint8_t[m_addrlen];
		memcpy(m_addr, &a_native, m_addrlen);
		m_addrFamily = AddressFamily::IPv4;
		return *this;
	}



	IpAddress& IpAddress::operator=(IN const sockaddr_in6& a_native)
	{
		this->~IpAddress();
		m_addrlen = sizeof(sockaddr_in6);
		m_addr = (sockaddr*)new uint8_t[m_addrlen];
		memcpy(m_addr, &a_native, m_addrlen);
		m_addrFamily = AddressFamily::IPv6;
		return *this;
	}



	IpAddress::operator const sockaddr* () const
	{
		return m_addr;
	}



	int IpAddress::GetAddrLen() const
	{
		return m_addrlen;
	}



	AddressFamily IpAddress::GetAddressFamily() const
	{
		return m_addrFamily;
	}



	void* IpAddress::GetIp(OUT int* a_len /*= nullptr*/) const
	{
		if (m_addr == nullptr)
			return nullptr;

		void* p = nullptr;
		int sz = 0;

		switch (m_addrFamily) {
			case AddressFamily::IPv4: {
				auto cast = (sockaddr_in*)m_addr;
				p = &cast->sin_addr;
				sz = sizeof(cast->sin_addr);
				break;
			}
			case AddressFamily::IPv6: {
				auto cast = (sockaddr_in6*)m_addr;
				p = &cast->sin6_addr;
				sz = sizeof(cast->sin6_addr);
				break;
			}
			default:
				asd_OnErr("invalid AddressFamily");
				break;
		}
		
		if ( a_len != nullptr )
			*a_len = sz;

		return p;
	}



	uint16_t IpAddress::GetPort() const
	{
		if (m_addr == nullptr)
			return 0;

		switch (m_addrFamily) {
			case AddressFamily::IPv4:{
				auto cast = (sockaddr_in*)m_addr;
				return ntohs(cast->sin_port);
			}
			case AddressFamily::IPv6:{
				auto cast = (sockaddr_in6*)m_addr;
				return ntohs(cast->sin6_port);
			}
			default:
				asd_OnErr("invalid AddressFamily");
				break;
		}
		return 0;
	}



	void IpAddress::SetPort(IN uint16_t a_port)
	{
		if (m_addr == nullptr)
			return;

		switch (m_addrFamily) {
			case AddressFamily::IPv4:{
				auto cast = (sockaddr_in*)m_addr;
				cast->sin_port = htons(a_port);
				break;
			}
			case AddressFamily::IPv6:{
				auto cast = (sockaddr_in6*)m_addr;
				cast->sin6_port = htons(a_port);
				break;
			}
			default:
				asd_OnErr("invalid AddressFamily");
				break;
		}
	}



	MString IpAddress::ToString() const
	{
		if (m_addr == nullptr)
			return "";

		const int BufSize = 64;
		char ip[BufSize];
		ip[0] = '\0';
		
		inet_ntop(ToNativeCode(m_addrFamily),
				  GetIp(),
				  ip,
				  BufSize);

		switch (m_addrFamily) {
			case AddressFamily::IPv4:
				return MString::Format("{}:{}", ip, GetPort());
			case AddressFamily::IPv6:
				return MString::Format("[{}]:{}", ip, GetPort());
			default:
				asd_OnErr("invalid AddressFamily");
				break;
		}
		return "";
	}



	int IpAddress::Compare(IN const IpAddress& a_left,
						   IN const IpAddress& a_right)
	{
		if (a_left.m_addr == a_right.m_addr) {
			asd_DAssert(a_left.m_addrFamily == a_right.m_addrFamily);
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
		
		asd_DAssert(a_left.m_addrFamily == a_right.m_addrFamily);
		asd_DAssert(a_left.m_addrlen == a_right.m_addrlen);


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



	size_t IpAddress::Hash::operator()(IN const IpAddress& a_addr) const
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
			asd_DAssert(len == 16);
			asd_DAssert(len % SizeOfSizeT == 0);
			const int Count = len / SizeOfSizeT;
			size_t* arr = (size_t*)ip;
			for (int i=0; i<Count; ++i) {
				ret ^= arr[i];
			}
		}

		size_t port = a_addr.GetPort();
		port <<= (SizeOfSizeT - 2);
		ret ^= port;

		return ret;
	}



	std::vector<IpAddress> FindIP(IN const char* a_domain)
	{
		std::vector<IpAddress> ret;
		addrinfo* result;
		addrinfo hints;
		std::memset(&hints, 0, sizeof(hints));

		int err = getaddrinfo(a_domain, NULL, &hints, &result);
		if (err != 0)
			return ret;

		for (auto p=result; p!=nullptr; p=p->ai_next) {
			switch (p->ai_family) {
				case AF_INET:{
					ret.emplace_back(IpAddress(*(sockaddr_in*)p->ai_addr));
					break;
				}
				case AF_INET6:{
					ret.emplace_back(IpAddress(*(sockaddr_in6*)p->ai_addr));
					break;
				}
			}
		}

		freeaddrinfo(result);
		return ret;
	}
}
