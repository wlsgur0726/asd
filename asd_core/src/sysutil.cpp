#include "stdafx.h"
#include "asd/sysutil.h"


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

				assert(false);
				asd_RaiseException("unsupported system");
			}
		};
		static const EndianCheck g_endian;
		return g_endian.endian;
	}
}
