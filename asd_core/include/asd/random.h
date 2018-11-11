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
			thread_local std::random_device t_rd;
			thread_local std::uniform_int_distribution<T> t_dist;
			thread_local std::mt19937_64 t_generator(t_dist(t_rd));
			return t_generator;
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
