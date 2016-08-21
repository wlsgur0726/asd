#pragma once

#include "asdbase.h"
#include "string.h"
#include "exception.h"

struct sockaddr;
struct sockaddr_in;
struct sockaddr_in6;

namespace asd
{
	class IpAddress
	{
	public:
		enum class Family : uint8_t
		{
			IPv4,
			IPv6,
		};
		static int ToNativeCode(Family a_family) asd_noexcept;

	private:
		Family m_addrFamily = Family::IPv4;
		sockaddr* m_addr = nullptr;
		int m_addrlen = 0;

	public:
		virtual ~IpAddress();

		IpAddress(IN Family a_addrFamily = Family::IPv4) asd_noexcept;

		IpAddress(IN const char* a_domain,
				  IN uint16_t a_port = 0);

		IpAddress(IN const IpAddress& a_cp) asd_noexcept;

		IpAddress(MOVE IpAddress&& a_rval) asd_noexcept;

		IpAddress(IN const sockaddr_in& a_native_ipv4) asd_noexcept;

		IpAddress(IN const sockaddr_in6& a_native_ipv6) asd_noexcept;

		IpAddress& operator = (IN const IpAddress& a_cp) asd_noexcept;

		IpAddress& operator = (MOVE IpAddress&& a_rval) asd_noexcept;

		IpAddress& operator = (IN const sockaddr_in& a_native_ipv4) asd_noexcept;

		IpAddress& operator = (IN const sockaddr_in6& a_native_ipv6) asd_noexcept;

		operator const sockaddr* () const asd_noexcept;

		int GetAddrLen() const asd_noexcept;

		Family GetAddressFamily() const asd_noexcept;

		void* GetIp(OUT int* a_len = nullptr) const asd_noexcept;

		uint16_t GetPort() const asd_noexcept;

		MString ToString() const asd_noexcept;

		static int Compare(IN const IpAddress& a_left,
						   IN const IpAddress& a_right) asd_noexcept;

		asd_Define_CompareOperator(Compare, IpAddress);

		// STL의 해시 기반 컨테이너에서 사용할 Functor
		struct Hash
		{
			size_t operator() (IN const IpAddress& a_addr) const asd_noexcept;
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
