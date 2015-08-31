#pragma once

#include "asd/asdbase.h"
#include "asd/string.h"
#include "asd/exception.h"

#ifndef asd_Platform_Windows
#	include <sys/socket.h>
#endif

struct sockaddr;
struct sockaddr_in;
struct sockaddr_in6;

namespace asd
{
	class IpAddress
	{
	public:
		enum Family {
			IPv4 = AF_INET,
			IPv6 = AF_INET6,
		};

	private:
		Family m_addrFamily = IPv4;
		sockaddr* m_addr = nullptr;
		int m_addrlen = 0;

	public:
		virtual ~IpAddress();

		IpAddress(IN Family a_addrFamily = IPv4) asd_NoThrow;

		IpAddress(IN const char* a_domain,
				  IN uint16_t a_port = 0) asd_Throws(asd::Exception);

		IpAddress(IN const IpAddress& a_cp) asd_NoThrow;

		IpAddress(MOVE IpAddress&& a_rval) asd_NoThrow;

		IpAddress(IN const sockaddr_in& a_native_ipv4) asd_NoThrow;

		IpAddress(IN const sockaddr_in6& a_native_ipv6) asd_NoThrow;

		IpAddress& operator = (IN const IpAddress& a_cp) asd_NoThrow;

		IpAddress& operator = (MOVE IpAddress&& a_rval) asd_NoThrow;

		IpAddress& operator = (IN const sockaddr_in& a_native_ipv4) asd_NoThrow;

		IpAddress& operator = (IN const sockaddr_in6& a_native_ipv6) asd_NoThrow;

		operator const sockaddr* () const asd_NoThrow;

		int GetAddrLen() const asd_NoThrow;

		Family GetAddressFamily() const asd_NoThrow;

		void* GetIp(OUT int* a_len = nullptr) const asd_NoThrow;

		uint16_t GetPort() const asd_NoThrow;

		MString ToString() const asd_NoThrow;

		static int Compare(IN const IpAddress& a_left,
						   IN const IpAddress& a_right) asd_NoThrow;

		asd_Define_CompareOperator(Compare, IpAddress);

		// STL의 해시 기반 컨테이너에서 사용할 Functor
		struct Hash
		{
			size_t operator() (IN const IpAddress& a_addr) const asd_NoThrow;
		};
	};
}

namespace std
{
	template<>
	struct hash<asd::IpAddress>
		: public asd::IpAddress::Hash
	{
	};
}
