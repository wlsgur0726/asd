#pragma once
#include "asd/asdbase.h"


namespace asd
{
	enum class Endian : uint8_t
	{
		Little = 0,
		Big,
	};

	Endian GetNativeEndian() asd_noexcept;
}