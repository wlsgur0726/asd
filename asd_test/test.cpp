#include "stdafx.h"
#include "test.h"
#include "asd/ioevent.h"
#include "asd/semaphore.h"
#include "asd/serialize.h"
#include "asd/handle.h"
#include <thread>

const uint16_t Port = 23456;
#define PRINTF(...) ::printf(asd::MString::Format(__VA_ARGS__));

asd::IOEvent g_ev;
bool g_run = true;
asd::Mutex g_lock;
struct Client;
std::unordered_map<uintptr_t, std::unique_ptr<Client>> g_sockets;

struct Sock : public asd::AsyncSocket
{
	asd::Semaphore m_connect;
	asd::Semaphore m_recv;

	virtual void OnConnect(IN asd::Socket::Error a_err) asd_noexcept override
	{
		printf("%s\n", __FUNCTION__);
		m_connect.Post();
	}

	virtual void OnAccept(MOVE asd::AsyncSocket&& a_newSock) asd_noexcept override
	{
		printf("%s\n", __FUNCTION__);
	}

	virtual void OnRecv(MOVE asd::Buffer_ptr&& a_data) asd_noexcept override
	{
		auto p = a_data->GetBuffer();
		auto sz = a_data->GetSize();
		m_recv.Post();
		printf("%s\n", __FUNCTION__);
	}

	virtual void OnClose(IN asd::Socket::Error a_err) asd_noexcept override
	{
		printf("%s\n", __FUNCTION__);
	}
};


struct Client : public asd::AsyncSocket
{
	typedef asd::AsyncSocket Base;

	Client(Base&& a_sock)
		: Base(std::move(a_sock))
	{
	}

	virtual void OnRecv(MOVE asd::Buffer_ptr&& a_data) asd_noexcept override
	{
		printf("%s\n", __FUNCTION__);
		asd::BufferList packets;
		packets.PushBack(std::move(a_data));
		Send(std::move(packets));
	}

	virtual void OnClose(IN asd::Socket::Error a_err) asd_noexcept override
	{
		printf("%s\n", __FUNCTION__);
		auto lock = asd::GetLock(g_lock);
		g_sockets.erase(GetID());
	}
};

struct ListenSocket : public asd::AsyncSocket
{
	virtual void OnAccept(MOVE asd::AsyncSocket&& a_newSock) asd_noexcept override
	{
		printf("%s\n", __FUNCTION__);

		std::unique_ptr<Client> sock(new Client(std::move(a_newSock)));

		if (g_ev.Register(*sock) == false) {
			PRINTF("fail Register\n");
			std::terminate();
		}

		auto lock = asd::GetLock(g_lock);
		auto emplace = g_sockets.emplace(sock->GetID(), std::move(sock));
		if (emplace.second == false) {
			PRINTF("fail emplace\n");
			std::terminate();
		}
	}

	virtual void OnClose(IN asd::Socket::Error a_err) asd_noexcept override
	{
		printf("%s\n", __FUNCTION__);
	}
};

struct TestServer
{
	asd::Semaphore m_sync;
	std::thread m_thread;
	TestServer()
	{
		m_thread = std::thread([this]()
		{
			ListenSocket listeningSock;

			if (g_ev.Register(listeningSock) == false) {
				PRINTF("fail Register\n");
				std::terminate();
			}

			//auto e = listeningSock.CastToSocket().SetSockOpt_ReuseAddr(true);
			//if (e != 0) {
			//	PRINTF("fail SetSockOpt_ReuseAddr\n");
			//	std::terminate();
			//}

			if (listeningSock.Listen(asd::IpAddress("0.0.0.0", Port), 1024) == false) {
				PRINTF("fail Listen\n");
				std::terminate();
			}

			m_sync.Post();
			while (g_run) 
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
		});
		m_thread.detach();
	}

};

struct MyObj
{
	MyObj()
	{
		printf("[%p] %s\n", this, __FUNCTION__);
	}
	~MyObj()
	{
		printf("[%p] %s\n", this, __FUNCTION__);
	}
	int data;
};

void Func1()
{
	typedef asd::Handle<MyObj> MyObjHandle;

	MyObjHandle h1;
	h1.Alloc();
	printf("%p\n", h1.GetObj().get());

	MyObjHandle h2;
	h2 = h1;
	h2.Free();
	printf("-------------\n");
	h2.Alloc();
	printf("%p\n", h2.GetObj().get());
	h1.Free();
	printf("end\n");
}
void Test()
{
	//Func1();
	//exit(0);
	return;
	{
		g_ev.Init();
		TestServer server;

		server.m_sync.Wait();

		PRINTF("Connect...\n");
		Sock conn;

		if (g_ev.Register(conn) == false) {
			PRINTF("fail Register\n");
			std::terminate();
		}

		conn.Connect(asd::IpAddress("127.0.0.1", Port));
		conn.m_connect.Wait();

		uint8_t str[] = "TestData";
		asd::BufferList bufs;
		for (int i=0; i<sizeof(str); ++i)
			asd::Write(bufs, str[i]);

		conn.Send(std::move(bufs));
		conn.m_recv.Wait();
	}
	exit(0);
}
