#include "stdafx.h"
#include "asd/ioevent.h"
#include "asd/string.h"
#include "asd/semaphore.h"
#include "asd/threadpool.h"
#include "asd/random.h"
#include <thread>
#include <array>
#include <unordered_map>

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
					sendbuf[i] = (uint8_t)i;

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
					sendbuf[i] = (uint8_t)i;

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



	struct PeerManager;
	struct Peer
	{
		asd::AsyncSocketHandle m_sock;
		PeerManager* m_owner = nullptr;
		std::atomic_bool m_onConnect;
		std::atomic_bool m_onClose;

		Peer()
		{
			m_onConnect = false;
			m_onClose = false;
		}
		virtual ~Peer()
		{
			auto sock = m_sock.Free();
			if (sock != nullptr)
				sock->Close();
		}
		virtual void OnAccept(IN asd::AsyncSocket_ptr& a_newSock) { FAIL(); }
		virtual void OnConnect(IN asd::Socket::Error a_err) { FAIL(); }
		virtual void OnRecv(MOVE asd::Buffer_ptr&& a_data) { FAIL(); }
		virtual void OnClose(IN asd::Socket::Error a_err) { FAIL(); }
	};

	struct PeerManager
	{
		asd::Mutex m_lock;
		std::unordered_map<asd::AsyncSocketHandle, std::shared_ptr<Peer>> m_peers;
		asd::ThreadPool m_threadPool = asd::ThreadPool(asd::ThreadPoolOption());

		bool Add(std::shared_ptr<Peer> peer)
		{
			auto lock = asd::GetLock(m_lock);
			bool ok = m_peers.emplace(peer->m_sock, peer).second;
			EXPECT_TRUE(ok);
			peer->m_owner = ok ? this : nullptr;
			return ok;
		}

		std::shared_ptr<Peer> Find(asd::AsyncSocketHandle handle)
		{
			auto lock = asd::GetLock(m_lock);
			return m_peers[handle];
		}

		std::shared_ptr<Peer> Del(asd::AsyncSocketHandle handle)
		{
			auto lock = asd::GetLock(m_lock);
			auto ret = std::move(m_peers[handle]);
			m_peers.erase(handle);
			return ret;
		}
	};

	void TCP_NonBlocked(asd::AddressFamily af)
	{
		static const size_t TotalDataSize = 1 * 1024 * 1024;
		static const size_t SendSize = TotalDataSize / 1024;
		static const size_t ClientCount = 1;

		static asd::Semaphore s_finish;
		static std::atomic<size_t> s_clientCount;

		asd::Reset(s_finish);
		s_clientCount = ClientCount;

		struct Client : public Peer
		{
			std::vector<uint8_t> m_expect;
			bool m_sendComplete = false;
			size_t m_offset = 0;
			bool m_finish = false;

			Client()
			{
				m_expect.reserve(TotalDataSize);
			}

			void Finish()
			{
				if (m_finish)
					return;
				m_finish = true;
				if (0 == --s_clientCount)
					s_finish.Post();
			}

			virtual void OnConnect(IN asd::Socket::Error a_err) override
			{
				bool b = false;
				EXPECT_TRUE(m_onConnect.compare_exchange_strong(b, true));
				EXPECT_EQ(0, a_err);
				if (0 != a_err) {
					Finish();
					m_owner->Del(m_sock);
					return;
				}

				auto sock = m_sock.GetObj();
				ASSERT_NE(sock, nullptr);
				for (size_t i=0; i<TotalDataSize/SendSize; ++i) {
					m_owner->m_threadPool.PushSeq(m_sock, [this, sock]()
					{
						auto buf = asd::NewBuffer<SendSize>();
						buf->SetSize(SendSize);
						uint8_t* p = buf->GetBuffer();
						for (size_t i=0; i<SendSize; ++i) {
							p[i] = (uint8_t)asd::Random::Uniform(0, 255);
							m_expect.push_back(p[i]);
						}
						sock->Send(std::move(buf));
					});
				}
				m_sendComplete = true;
			}

			virtual void OnRecv(MOVE asd::Buffer_ptr&& a_data) override
			{
				m_owner->m_threadPool.PushSeq(m_sock, [this, data=std::move(a_data)]()
				{
					uint8_t* exp = &m_expect[m_offset];
					uint8_t* cmp = data->GetBuffer();
					ASSERT_TRUE(m_expect.size() >= m_offset+data->GetSize());
					EXPECT_EQ(0, std::memcmp(exp, cmp, data->GetSize()));
					m_offset += data->GetSize();

					if (m_sendComplete && m_offset==m_expect.size()) {
						Finish();
						auto sock = m_sock.GetObj();
						if (sock != nullptr)
							sock->Close();
					}
				});
			}

			virtual void OnClose(IN asd::Socket::Error a_err) override
			{
				bool b = false;
				EXPECT_TRUE(m_onClose.compare_exchange_strong(b, true));
				EXPECT_EQ(0, a_err);
				Finish();
				m_owner->Del(m_sock);
			}
		};


		struct Server : public Peer
		{
			struct Client : public Peer
			{
				virtual void OnRecv(MOVE asd::Buffer_ptr&& a_data) override
				{
					auto sock = m_sock.GetObj();
					ASSERT_NE(sock, nullptr);
					sock->Send(std::move(a_data));
				}

				virtual void OnClose(IN asd::Socket::Error a_err) override
				{
					bool b = false;
					EXPECT_TRUE(m_onClose.compare_exchange_strong(b, true));
					EXPECT_EQ(0, a_err);
					m_owner->Del(m_sock);
				}
			};

			virtual void OnAccept(IN asd::AsyncSocket_ptr& a_newSock) override
			{
				auto handle = asd::AsyncSocketHandle::GetHandle(a_newSock.get());
				std::shared_ptr<Peer> peer(new Server::Client);
				peer->m_sock = handle;
				m_owner->Add(peer);
			}

			virtual void OnClose(IN asd::Socket::Error a_err) override
			{
				bool b = false;
				EXPECT_TRUE(m_onClose.compare_exchange_strong(b, true));
				EXPECT_EQ(0, a_err);
				m_owner->Del(m_sock);
			}
		};

		struct TestIO : public asd::IOEvent
		{
			PeerManager m_peerManager;

			virtual void OnAccept(IN asd::AsyncSocket* a_listener,
								  MOVE asd::AsyncSocket_ptr&& a_newSock) override
			{
				auto handle = asd::AsyncSocketHandle::GetHandle(a_listener);
				auto listener = m_peerManager.Find(handle);
				ASSERT_TRUE(listener != nullptr);
				listener->OnAccept(a_newSock);
				ASSERT_TRUE(Register(a_newSock));
			}

			virtual void OnConnect(IN asd::AsyncSocket* a_sock,
								   IN asd::Socket::Error a_err) override
			{
				auto handle = asd::AsyncSocketHandle::GetHandle(a_sock);
				auto peer = m_peerManager.Find(handle);
				ASSERT_TRUE(peer != nullptr);
				m_peerManager.m_threadPool.PushSeq(handle, [peer, a_err]() mutable
				{
					peer->OnConnect(a_err);
				});
			}

			virtual void OnRecv(IN asd::AsyncSocket* a_sock,
								MOVE asd::Buffer_ptr&& a_data) override
			{
				auto handle = asd::AsyncSocketHandle::GetHandle(a_sock);
				auto peer = m_peerManager.Find(handle);
				ASSERT_TRUE(peer != nullptr);
				m_peerManager.m_threadPool.PushSeq(handle, [peer, data=std::move(a_data)]() mutable
				{
					peer->OnRecv(std::move(data));
				});
			}

			virtual void OnClose(IN asd::AsyncSocket* a_sock,
								 IN asd::Socket::Error a_err) override
			{
				auto handle = asd::AsyncSocketHandle::GetHandle(a_sock);
				auto peer = m_peerManager.Find(handle);
				if (peer == nullptr)
					return;
				m_peerManager.m_threadPool.PushSeq(handle, [peer, a_err]() mutable
				{
					peer->OnClose(a_err);
				});
			}
		};

		asd::IpAddress addr;
		TestIO io;
		io.Start();
		io.m_peerManager.m_threadPool.Start();

		// start server
		{
			std::shared_ptr<Peer> listener(new Server);
			auto sock = listener->m_sock.Alloc();
			ASSERT_TRUE(io.m_peerManager.Add(listener));
			ASSERT_TRUE(io.RegisterListener(sock, asd::IpAddress(Addr_Any(af), 0), 1024));
			ASSERT_EQ(0, sock->SetSockOpt_ReuseAddr(true));
			ASSERT_EQ(0, sock->GetSockName(addr));
		}

		asd::puts(asd::MString::Format("addr = {}", addr.ToString()));

		// add client
		for (size_t i=0; i<ClientCount; ++i) {
			std::shared_ptr<Peer> client(new Client);
			auto sock = client->m_sock.Alloc();
			ASSERT_TRUE(io.m_peerManager.Add(client));
			ASSERT_TRUE(io.RegisterConnector(sock, asd::IpAddress(Addr_Loopback(af), addr.GetPort())));
		}

		// wait finish
		s_finish.Wait();

		auto lock = asd::GetLock(io.m_peerManager.m_lock);
		auto peers = std::move(io.m_peerManager.m_peers);
		lock.unlock();
		peers.clear();
	}

	TEST(Socket, IPv4_TCP_NonBlocked)
	{
		TCP_NonBlocked(asd::AddressFamily::IPv4);
	}

	TEST(Socket, IPv6_TCP_NonBlocked)
	{
		TCP_NonBlocked(asd::AddressFamily::IPv6);
	}

	void UDP_NonBlocked(asd::AddressFamily af)
	{

	}
}
