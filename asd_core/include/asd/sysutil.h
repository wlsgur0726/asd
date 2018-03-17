#pragma once
#include "asdbase.h"


namespace asd
{
	enum class Endian : uint8_t
	{
		Little = 0,
		Big,
	};

	Endian GetNativeEndian();


	uint32_t GetCurrentProcessID();


	// 아무키나 입력을 기다리며 대기
	void Pause();
}