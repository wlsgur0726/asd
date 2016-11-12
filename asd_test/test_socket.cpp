#include "stdafx.h"
#include "asd/socket.h"
#include "asd/string.h"
#include "asd/semaphore.h"
#include "asd/threadutil.h"
#include <thread>
#include <array>

namespace asdtest_socket
{
	void TCP_Blocked(const char* ip_listen, const char* ip_connect)
	{
		uint32_t listener_seq;
		asd::Semaphore sync;
		asd::IpAddress addr;
		std::thread listener([&]()
		{
			listener_seq = asd::GetCurrentThreadSequence();

			asd::Socket sock;
			ASSERT_TRUE(0 == sock.Bind(asd::IpAddress(ip_listen, 0)));
			ASSERT_TRUE(0 == sock.GetSockName(addr));
			ASSERT_TRUE(0 == sock.Listen());
			sync.Post();

			asd::Socket acceptSock;
			asd::IpAddress acceptIp;
			ASSERT_TRUE(0 == sock.Accept(acceptSock, acceptIp));

			while (true) {
				std::array<uint8_t, 97> buf;
				auto r = acceptSock.Recv(buf.data(), buf.size());
				ASSERT_TRUE(r.m_error == 0);
				ASSERT_TRUE(r.m_bytes <= buf.size());
				if (r.m_bytes == 0)
					break;

				auto s = acceptSock.Send(buf.data(), r.m_bytes);
				ASSERT_TRUE(s.m_error == 0);
				ASSERT_TRUE(s.m_bytes == r.m_bytes);
			}
		});

		volatile bool fail = true;
		std::thread client([&]()
		{
			ASSERT_TRUE(sync.Wait(1000));

			asd::Socket sock;
			ASSERT_TRUE(0 == sock.Connect(asd::IpAddress(ip_connect, addr.GetPort())));
			for (size_t len=2; len<=(1024*1024); len*=2) {
				std::vector<uint8_t> sendbuf;
				std::vector<uint8_t> recvbuf;
				sendbuf.resize(len);
				recvbuf.resize(len);

				for (size_t i=0; i<len; ++i)
					sendbuf[i] = i;

				auto s = sock.Send(sendbuf.data(), sendbuf.size());
				ASSERT_TRUE(s.m_error == 0);
				ASSERT_TRUE(s.m_bytes == len);

				size_t recved = 0;
				while (recved < len) {
					auto r = sock.Recv(recvbuf.data()+recved, recvbuf.size()-recved);
					ASSERT_TRUE(r.m_error == 0);
					ASSERT_TRUE(r.m_bytes > 0);
					recved += r.m_bytes;
				}
				ASSERT_TRUE(recved == len);
				ASSERT_TRUE(0 == std::memcmp(sendbuf.data(), recvbuf.data(), len));
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
		TCP_Blocked("0.0.0.0", "127.0.0.1");
	}

	TEST(Socket, IPv6_TCP_Blocked)
	{
		TCP_Blocked("::0", "::1");
	}


}
