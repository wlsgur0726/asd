#include "asd_pch.h"
#include "asd/sysutil.h"
#include <cstdlib>

#if !asd_Platform_Windows
#	include <sys/types.h>
#	include <unistd.h>
#
#endif

namespace asd
{
	Endian GetNativeEndian() asd_noexcept
	{
		struct EndianCheck
		{
			Endian endian;
			EndianCheck()
			{
				const uint32_t check = 0xAABBCCDD;
				const uint8_t* p = (uint8_t*)&check;

				if (p[0]==0xDD && p[1]==0xCC && p[2]==0xBB && p[3]==0xAA) {
					endian = Endian::Little;
					return;
				}

				if (p[0]==0xAA && p[1]==0xBB && p[2]==0xCC && p[3]==0xDD) {
					endian = Endian::Big;
					return;
				}

				asd_RaiseException("unsupported system");
			}
		};
		static const EndianCheck g_endian;
		return g_endian.endian;
	}



	uint32_t GetCurrentProcessID() asd_noexcept
	{
		static uint32_t g_pid = 0;
		if (g_pid == 0) {
#if asd_Platform_Windows
			g_pid = ::GetCurrentProcessId();
#else
			g_pid = ::getpid();
#endif
		}
		return g_pid;
	}



	void Pause() asd_noexcept
	{
#if asd_Platform_Windows
		system("pause");

#elif asd_Platform_Linux
		asd::puts("press any key to continue...");
		getchar();

#else
		asd_RaiseException("unsupported system");

#endif
	}
}
