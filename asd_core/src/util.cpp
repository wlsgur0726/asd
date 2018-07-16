#include "stdafx.h"
#include "asd/util.h"
#include <math.h>

namespace asd
{
	const std::vector<uint16_t>& GetPrimeNumbers()
	{
		static std::once_flag s_init;
		static std::vector<uint16_t> s_list;
		std::call_once(s_init, []()
		{
			s_list.emplace_back(2);
			for (uint16_t num=3; num<std::numeric_limits<uint16_t>::max(); num+=2) {
				bool isPrime = true;
				uint16_t checkMax = 1 + (uint16_t)sqrt(num);
				for (uint16_t i=3; i<=checkMax; ++i) {
					isPrime = (num % i) != 0;
					if (!isPrime)
						break;
				}
				if (isPrime)
					s_list.emplace_back(num);
			}
		});
		return s_list;
	}
}
