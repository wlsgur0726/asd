#pragma once
#include "asdbase.h"
#include <random>

namespace asd
{
	namespace Random
	{
		template <typename T>
		std::mt19937_64& Genrator()
		{
			thread_local std::random_device g_rd;
			thread_local std::uniform_int_distribution<T> g_dist;
			thread_local std::mt19937_64 g_generator(g_dist(g_rd));
			return g_generator;
		}


		// min 이상, max 이하의 균등분포 랜덤값을 리턴하는 함수
		template <typename T>
		inline T Uniform(T min, T max)
		{
			if (min > max)
				std::swap(min, max);
			std::uniform_int_distribution<T> get(min, max);
			return get(Genrator<T>());
		}
	}
}
