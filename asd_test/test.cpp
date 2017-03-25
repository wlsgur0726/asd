#include "stdafx.h"
#include "test.h"
#include "asd/ioevent.h"
#include "asd/semaphore.h"
#include "asd/serialize.h"
#include "asd/handle.h"
#include <thread>

const uint16_t Port = 23456;
#define PRINTF(...) ::printf(asd::MString::Format(__VA_ARGS__));


volatile bool g_run = true;
asd::Semaphore g_sync;


struct TestClient;

struct ClientEvent : public asd::IOEvent
{
	asd::Mutex m_lock;
	std::unordered_map<asd::AsyncSocketHandle, std::shared_ptr<TestClient>> m_clients;

	void Stop();

	void Sync(asd::AsyncSocket* a_sock);

	virtual void OnConnect(asd::AsyncSocket* a_sock,
						   asd::Socket::Error a_err) asd_noexcept override;

	virtual void OnRecv(asd::AsyncSocket* a_sock,
						asd::Buffer_ptr&& a_data) asd_noexcept override;

	virtual void OnClose(IN asd::AsyncSocket* a_sock,
						 IN asd::Socket::Error a_err) asd_noexcept override;
};
ClientEvent g_clientEvent;


struct ServerEvent : public asd::IOEvent
{
	asd::Mutex m_lock;
	std::unordered_set<asd::AsyncSocketHandle> m_clients;

	void Stop();

	virtual void OnAccept(asd::AsyncSocket* a_listener,
						  asd::AsyncSocket_ptr&& a_newSock) asd_noexcept;


	virtual void OnRecv(asd::AsyncSocket* a_sock,
						asd::Buffer_ptr&& a_data) asd_noexcept override;

	virtual void OnClose(asd::AsyncSocket* a_sock,
						 asd::Socket::Error a_err) asd_noexcept override;
};
ServerEvent g_serverEvent;


struct TestClient
{
	asd::AsyncSocketHandle m_socket;
	asd::Semaphore m_sync;
	TestClient()
	{
		auto sock = m_socket.Alloc();
		asd_RAssert(g_clientEvent.Register(sock), "");

		std::thread t([]()
		{
		});
		t.detach();
	}

	~TestClient()
	{
		m_socket.Free();
	}
};

struct TestServer
{
	asd::AsyncSocketHandle m_socket;
	std::thread m_thread;

	TestServer()
	{
		m_thread = std::thread([this]()
		{
			auto sock = m_socket.Alloc();
			asd_RAssert(g_serverEvent.Register(sock), "");

			asd_RAssert(0 == sock->SetSockOpt_ReuseAddr(true), "");

			asd_RAssert(sock->Listen(asd::IpAddress("0.0.0.0", Port), 1024), "");

			g_sync.Post();
			while (g_run)
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
		});
	}

	~TestServer()
	{
		m_thread.join();
		m_socket.Free();
	}
};


void ClientEvent::Sync(asd::AsyncSocket* a_sock)
{
	auto lock = asd::GetLock(m_lock);
	auto cli = m_clients[asd::AsyncSocketHandle::GetHandle(a_sock)];
	asd_RAssert(cli != nullptr, "");
	cli->m_sync.Post();
}

void ClientEvent::OnConnect(asd::AsyncSocket* a_sock,
							asd::Socket::Error a_err) asd_noexcept
{
	PRINTF("{}\n", asd_MakeDebugInfo("").ToString());
	Sync(a_sock);
}


void ClientEvent::OnRecv(asd::AsyncSocket* a_sock,
						 asd::Buffer_ptr&& a_data) asd_noexcept
{
	auto p = a_data->GetBuffer();
	auto sz = a_data->GetSize();
	PRINTF("{}\n", asd_MakeDebugInfo("").ToString());
	Sync(a_sock);
}

void ClientEvent::OnClose(IN asd::AsyncSocket* a_sock,
						  IN asd::Socket::Error a_err) asd_noexcept
{
	PRINTF("{}\n", asd_MakeDebugInfo("").ToString());
	auto handle = asd::AsyncSocketHandle::GetHandle(a_sock);
	auto lock = asd::GetLock(m_lock);
	m_clients.erase(handle);
	lock.unlock();
	handle.Free();
}

void ClientEvent::Stop()
{
	asd::IOEvent::Stop();

	auto lock = asd::GetLock(m_lock);
	while (m_clients.size() > 0) {
		auto it = m_clients.begin();
		auto handle = it->first;
		m_clients.erase(it);
		lock.unlock();
		//handle.Free();
	}
}


void ServerEvent::OnAccept(asd::AsyncSocket* a_listener,
						   asd::AsyncSocket_ptr&& a_newSock) asd_noexcept
{
	PRINTF("{}\n", asd_MakeDebugInfo("").ToString());
	asd_RAssert(Register(a_newSock), "");

	auto lock = asd::GetLock(m_lock);
	auto emplace = m_clients.emplace(asd::AsyncSocketHandle::GetHandle(a_newSock.get()));
	asd_RAssert(emplace.second, "");
}


void ServerEvent::OnRecv(asd::AsyncSocket* a_sock,
						 asd::Buffer_ptr&& a_data) asd_noexcept
{
	auto p = a_data->GetBuffer();
	auto sz = a_data->GetSize();
	PRINTF("{}\n", asd_MakeDebugInfo("").ToString());
	a_sock->Send(std::move(a_data));
}

void ServerEvent::OnClose(asd::AsyncSocket* a_sock,
						  asd::Socket::Error a_err) asd_noexcept
{
	PRINTF("{}\n", asd_MakeDebugInfo("").ToString());
	auto handle = asd::AsyncSocketHandle::GetHandle(a_sock);
	auto lock = asd::GetLock(m_lock);
	m_clients.erase(handle);
	lock.unlock();
	handle.Free();
}

void ServerEvent::Stop()
{
	asd::IOEvent::Stop();

	auto lock = asd::GetLock(m_lock);
	while (m_clients.size() > 0) {
		auto it = m_clients.begin();
		auto handle = *it;
		m_clients.erase(it);
		lock.unlock();
		//handle.Free();
	}
}


void Test()
{
	return;
	{
		PRINTF("start\n");
		g_clientEvent.Start();
		g_serverEvent.Start();

		TestServer server;

		g_sync.Wait();

		PRINTF("connect...\n");

		std::shared_ptr<TestClient> client(new TestClient);
		auto clisock = client->m_socket.GetObj();
		asd_RAssert(clisock != nullptr, "");
		{
			auto lock = asd::GetLock(g_clientEvent.m_lock);
			g_clientEvent.m_clients[client->m_socket] = client;
		}

		clisock->Connect(asd::IpAddress("127.0.0.1", Port));
		client->m_sync.Wait();

		uint8_t str[] = "TestData";
		asd::BufferList bufs;
		for (int i=0; i<sizeof(str); ++i)
			asd::Write(bufs, str[i]);

		clisock->Send(std::move(bufs));
		client->m_sync.Wait();

		g_run = false;

		PRINTF("stop\n");
		g_clientEvent.Stop();
		g_serverEvent.Stop();
		asd::AsyncSocketHandle::AllClear();
		PRINTF("end\n");
	}
	PRINTF("exit\n");
	exit(0);
}
