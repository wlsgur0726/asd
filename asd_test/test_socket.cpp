#include "stdafx.h"
#include "asd/socket.h"
#include "asd/string.h"
#include "asd/semaphore.h"
#include "asd/threadutil.h"
#include <thread>
#include <array>

namespace asdtest_socket
{
	const char* Addr_Any(asd::AddressFamily af)
	{
		switch (af) {
			case asd::AddressFamily::IPv4:
				return "0.0.0.0";
			case asd::AddressFamily::IPv6:
				return "::";
		}
		return nullptr;
	}

	const char* Addr_Loopback(asd::AddressFamily af)
	{
		switch (af) {
			case asd::AddressFamily::IPv4:
				return "127.0.0.1";
			case asd::AddressFamily::IPv6:
				return "::1";
		}
		return nullptr;
	}

	size_t UDP_Payload_Limit(asd::AddressFamily af)
	{
		switch (af) {
			case asd::AddressFamily::IPv4:
				// 0xffff - (sizeof(IPv4헤더) + sizeof(UDP헤더))
				return 0xffff - (20+8);
			case asd::AddressFamily::IPv6:
				// 0xffff - sizeof(UDP헤더)
				return 0xffff - 8;
		}
		return 0;
	}



	void TCP_Blocked(asd::AddressFamily af)
	{
		uint32_t listener_seq;
		asd::Semaphore sync;
		asd::IpAddress addr;
		std::thread listener([&]()
		{
			listener_seq = asd::GetCurrentThreadSequence();

			asd::Socket sock;
			ASSERT_EQ(0, sock.Bind(asd::IpAddress(Addr_Any(af), 0)));
			ASSERT_EQ(0, sock.GetSockName(addr));
			ASSERT_EQ(0, sock.Listen());
			sync.Post();

			asd::Socket acceptSock;
			asd::IpAddress acceptAddr;
			ASSERT_EQ(0, sock.Accept(acceptSock, acceptAddr));

			while (true) {
				std::array<uint8_t, 97> buf;
				auto r = acceptSock.Recv(buf.data(), buf.size());
				ASSERT_EQ(r.m_error, 0);
				ASSERT_LE(r.m_bytes, buf.size());
				if (r.m_bytes == 0)
					break;

				auto s = acceptSock.Send(buf.data(), r.m_bytes);
				ASSERT_EQ(s.m_error, 0);
				ASSERT_EQ(s.m_bytes, r.m_bytes);
			}
		});

		volatile bool fail = true;
		std::thread client([&]()
		{
			ASSERT_TRUE(sync.Wait(1000));
			asd::Socket sock;

			ASSERT_EQ(0, sock.Connect(asd::IpAddress(Addr_Loopback(af), addr.GetPort())));
			for (size_t len=2; len<=(1*1024*1024); len*=2) {
				std::vector<uint8_t> sendbuf;
				std::vector<uint8_t> recvbuf;
				sendbuf.resize(len);
				recvbuf.resize(len);

				for (size_t i=0; i<len; ++i)
					sendbuf[i] = i;

				auto s = sock.Send(sendbuf.data(), sendbuf.size());
				ASSERT_EQ(s.m_error, 0);
				ASSERT_EQ(s.m_bytes, len);

				size_t recved = 0;
				while (recved < len) {
					auto r = sock.Recv(recvbuf.data()+recved, recvbuf.size()-recved);
					ASSERT_EQ(r.m_error, 0);
					ASSERT_GT(r.m_bytes, 0);
					recved += r.m_bytes;
				}
				ASSERT_EQ(recved, len);
				ASSERT_EQ(0, std::memcmp(sendbuf.data(), recvbuf.data(), len));
			}
			fail = false;
		});

		client.join();
		if (fail)
			asd::KillThread(listener_seq);
		listener.join();
	}

	TEST(Socket, IPv4_TCP_Blocked)
	{
		TCP_Blocked(asd::AddressFamily::IPv4);
	}

	TEST(Socket, IPv6_TCP_Blocked)
	{
		TCP_Blocked(asd::AddressFamily::IPv6);
	}



	void UDP_Blocked(asd::AddressFamily af)
	{
		const size_t Payload_Limit = UDP_Payload_Limit(af);
		uint32_t listener_seq;
		asd::Semaphore sync;
		asd::IpAddress addr;
		std::thread listener([&]()
		{
			listener_seq = asd::GetCurrentThreadSequence();

			asd::Socket sock(asd::Socket::Type::UDP);
			ASSERT_EQ(0, sock.Bind(asd::IpAddress(Addr_Any(af), 0)));
			ASSERT_EQ(0, sock.GetSockName(addr));
			sync.Post();

			asd::IpAddress peerAddr;
			std::vector<uint8_t> buf;
			buf.resize(Payload_Limit);
			while (true) {
				auto r = sock.RecvFrom(buf.data(), buf.size(), peerAddr);
				ASSERT_EQ(r.m_error, 0);
				ASSERT_LE(r.m_bytes, buf.size());
				if (r.m_bytes == 0)
					break;

				auto s = sock.SendTo(buf.data(), r.m_bytes, peerAddr);
				ASSERT_EQ(s.m_error, 0);
				ASSERT_EQ(s.m_bytes, r.m_bytes);
			}
		});

		volatile bool fail = true;
		std::thread client([&]()
		{
			ASSERT_TRUE(sync.Wait(1000));
			asd::Socket sock(asd::Socket::Type::UDP);
			asd::IpAddress dst(Addr_Loopback(af), addr.GetPort());

			for (size_t len=2; len<=0x10000; len*=2) {
				const size_t len2 = len<=Payload_Limit ? len : Payload_Limit;
				std::vector<uint8_t> sendbuf;
				std::vector<uint8_t> recvbuf;
				sendbuf.resize(len2);
				recvbuf.resize(len2);

				for (size_t i=0; i<len2; ++i)
					sendbuf[i] = i;

				auto s = sock.SendTo(sendbuf.data(), sendbuf.size(), dst);
				ASSERT_EQ(s.m_error, 0);
				ASSERT_EQ(s.m_bytes, len2);

				asd::IpAddress peer;
				auto r = sock.RecvFrom(recvbuf.data(), recvbuf.size(), peer);
				ASSERT_EQ(r.m_error, 0);
				ASSERT_EQ(r.m_bytes, len2);
				ASSERT_EQ(peer, dst);
				ASSERT_EQ(0, std::memcmp(sendbuf.data(), recvbuf.data(), len2));
			}
			auto s = sock.SendTo("", 0, dst);
			ASSERT_EQ(s.m_error, 0);
			ASSERT_EQ(s.m_bytes, 0);
			fail = false;
		});

		client.join();
		if (fail)
			asd::KillThread(listener_seq);
		listener.join();
	}

	TEST(Socket, IPv4_UDP_Blocked)
	{
		UDP_Blocked(asd::AddressFamily::IPv4);
	}

	TEST(Socket, IPv6_UDP_Blocked)
	{
		UDP_Blocked(asd::AddressFamily::IPv6);
	}
}
