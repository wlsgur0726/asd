﻿#include "asd_pch.h"
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
	int IpAddress::ToNativeCode(AddressFamily a_family) asd_noexcept
	{
		switch (a_family) {
			case AddressFamily::IPv4:
				return AF_INET;
			case AddressFamily::IPv6:
				return AF_INET6;
		}
		asd_Assert(false, "invalid AddressFamily");
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



	IpAddress::IpAddress(IN AddressFamily a_addrFamily /*= IPv4*/) asd_noexcept
	{
		m_addrFamily = a_addrFamily;
	}



	IpAddress::IpAddress(IN const char* a_ip,
						 IN uint16_t a_port /*= 0*/) asd_noexcept
	{
		SetPort(a_port);
	}



	IpAddress::IpAddress(IN const IpAddress& a_cp) asd_noexcept
	{
		*this = a_cp;
	}



	IpAddress& IpAddress::operator = (IN const IpAddress& a_cp) asd_noexcept
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



	IpAddress::IpAddress(MOVE IpAddress&& a_rval) asd_noexcept
	{
		*this = std::move(a_rval);
	}



	IpAddress& IpAddress::operator = (MOVE IpAddress&& a_rval) asd_noexcept
	{
		this->~IpAddress();
		m_addrFamily = a_rval.m_addrFamily;
		m_addr = a_rval.m_addr;
		m_addrlen = a_rval.m_addrlen;

		a_rval.m_addr = nullptr;
		return *this;
	}



#define asd_IpAddress_Define_Init(NativeType, AddressFamily)							\
	IpAddress::IpAddress(IN const NativeType& a_native) asd_noexcept					\
	{																					\
		*this = a_native;																\
	}																					\
																						\
	IpAddress& IpAddress::operator = (IN const NativeType& a_native) asd_noexcept		\
	{																					\
		this->~IpAddress();																\
		m_addrlen = sizeof(NativeType);													\
		m_addr = (sockaddr*)new uint8_t[m_addrlen];										\
		memcpy(m_addr, &a_native, m_addrlen);											\
		m_addrFamily = AddressFamily;													\
		return *this;																	\
	}																					\

	asd_IpAddress_Define_Init(sockaddr_in, AddressFamily::IPv4);

	asd_IpAddress_Define_Init(sockaddr_in6, AddressFamily::IPv6);



	IpAddress::operator const sockaddr* () const asd_noexcept
	{
		return m_addr;
	}



	int IpAddress::GetAddrLen() const asd_noexcept
	{
		return m_addrlen;
	}



	AddressFamily IpAddress::GetAddressFamily() const asd_noexcept
	{
		return m_addrFamily;
	}



	void* IpAddress::GetIp(OUT int* a_len /*= nullptr*/) const asd_noexcept
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
				asd_Assert(false, "invalid AddressFamily");
				break;
		}
		
		if ( a_len != nullptr )
			*a_len = sz;

		return p;
	}



	uint16_t IpAddress::GetPort() const asd_noexcept
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
				asd_Assert(false, "invalid AddressFamily");
				break;
		}
		return 0;
	}



	void IpAddress::SetPort(IN uint16_t a_port) asd_noexcept
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
				asd_Assert(false, "invalid AddressFamily");
				break;
		}
	}



	MString IpAddress::ToString() const asd_noexcept
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
				asd_Assert(false, "invalid AddressFamily");
				break;
		}
		return "";
	}



	int IpAddress::Compare(IN const IpAddress& a_left,
						   IN const IpAddress& a_right) asd_noexcept
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



	size_t IpAddress::Hash::operator () (IN const IpAddress& a_addr) const asd_noexcept
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
		port <<= (SizeOfSizeT - 2);
		ret ^= port;

		return ret;
	}



	std::vector<IpAddress> FindIP(IN const char* a_domain) asd_noexcept
	{
		std::vector<IpAddress> ret;
		addrinfo* result;
		addrinfo hints;
		memset(&hints, 0, sizeof(hints));

		int err = getaddrinfo(a_domain, NULL, &hints, &result);
		if (err != 0)
			return ret;

		for (auto p=result; p!=nullptr; p=p->ai_next) {
			switch (p->ai_family) {
				case AF_INET:{
					ret.push_back(*(sockaddr_in*)p->ai_addr);
					break;
				}
				case AF_INET6:{
					ret.push_back(*(sockaddr_in6*)p->ai_addr);
					break;
				}
			}
		}

		freeaddrinfo(result);
		return ret;
	}
}
