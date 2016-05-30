#pragma once
#include "asd/asdbase.h"
#include "asd/socket.h"


namespace asd
{


	class IOServiceImpl;
	class IOService
	{
	public:
		typedef void(*OnSocketReceived)(Socket&);
		typedef void(*OnSocketSent)(Socket&);

		void Register(REF Socket& a_socket);
		void Unregister(IN Socket& a_socket);
		size_t Poll(IN uint32_t a_timeoutSec);

	private:
		std::unique_ptr<IOServiceImpl> m_impl;
	};
}