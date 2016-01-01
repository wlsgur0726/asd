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

		IpAddress(IN Family a_addrFamily = IPv4) noexcept;

		IpAddress(IN const char* a_domain,
				  IN uint16_t a_port = 0);

		IpAddress(IN const IpAddress& a_cp) noexcept;

		IpAddress(MOVE IpAddress&& a_rval) noexcept;

		IpAddress(IN const sockaddr_in& a_native_ipv4) noexcept;

		IpAddress(IN const sockaddr_in6& a_native_ipv6) noexcept;

		IpAddress& operator = (IN const IpAddress& a_cp) noexcept;

		IpAddress& operator = (MOVE IpAddress&& a_rval) noexcept;

		IpAddress& operator = (IN const sockaddr_in& a_native_ipv4) noexcept;

		IpAddress& operator = (IN const sockaddr_in6& a_native_ipv6) noexcept;

		operator const sockaddr* () const noexcept;

		int GetAddrLen() const noexcept;

		Family GetAddressFamily() const noexcept;

		void* GetIp(OUT int* a_len = nullptr) const noexcept;

		uint16_t GetPort() const noexcept;

		MString ToString() const noexcept;

		static int Compare(IN const IpAddress& a_left,
						   IN const IpAddress& a_right) noexcept;

		asd_Define_CompareOperator(Compare, IpAddress);

		// STL의 해시 기반 컨테이너에서 사용할 Functor
		struct Hash
		{
			size_t operator() (IN const IpAddress& a_addr) const noexcept;
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
