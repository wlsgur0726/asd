#include "stdafx.h"
#include "asd/ioservice.h"
#include "asd/objpool.h"
#include "asd/util.h"
#include <thread>
#include <vector>

#if defined(asd_Platform_Windows)
#include <Windows.h>

#elif defined(asd_Platform_Android) || defined(asd_Platform_Linux)

#else
#error "not support this platform"

#endif

namespace asd
{
	
	class IOServiceImpl
	{
	public:

		const uint32_t				m_threadCount;
		std::vector<std::thread>	m_threads;

		IOServiceImpl(IN uint32_t a_threadCount)
			: m_threadCount(a_threadCount)
		{
		}
	};
}
