#pragma once
#include "asdbase.h"
#include "string.h"
#include "exception.h"

struct sockaddr;
struct sockaddr_in;
struct sockaddr_in6;

namespace asd
{
	enum class AddressFamily : uint8_t
	{
		IPv4,
		IPv6,
	};


	class IpAddress
	{
	public:
		static int ToNativeCode(AddressFamily a_family);

	private:
		AddressFamily m_addrFamily = AddressFamily::IPv4;
		sockaddr* m_addr = nullptr;
		int m_addrlen = 0;

	public:
		virtual ~IpAddress();

		IpAddress(IN AddressFamily a_addrFamily = AddressFamily::IPv4);

		IpAddress(IN const char* a_ip,
				  IN uint16_t a_port = 0);

		IpAddress(IN const IpAddress& a_cp);

		IpAddress(MOVE IpAddress&& a_rval);

		IpAddress(IN const sockaddr_in& a_native_ipv4);

		IpAddress(IN const sockaddr_in6& a_native_ipv6);

		IpAddress& operator=(IN const IpAddress& a_cp);

		IpAddress& operator=(MOVE IpAddress&& a_rval);

		IpAddress& operator=(IN const sockaddr_in& a_native_ipv4);

		IpAddress& operator=(IN const sockaddr_in6& a_native_ipv6);

		operator const sockaddr* () const;

		int GetAddrLen() const;

		AddressFamily GetAddressFamily() const;

		void* GetIp(OUT int* a_len = nullptr) const;

		uint16_t GetPort() const;

		void SetPort(IN uint16_t a_port);

		MString ToString() const;

		static int Compare(IN const IpAddress& a_left,
						   IN const IpAddress& a_right);

		asd_Define_CompareOperator(Compare, IpAddress);

		// STL의 해시 기반 컨테이너에서 사용할 Functor
		struct Hash
		{
			size_t operator()(IN const IpAddress& a_addr) const;
		};
	};



	std::vector<IpAddress> FindIP(IN const char* a_domain);
}

namespace std
{
	template<>
	struct hash<asd::IpAddress>
		: public asd::IpAddress::Hash
	{
	};
}
