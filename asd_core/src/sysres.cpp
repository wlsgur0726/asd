#include "asd_pch.h"
#include "asd/sysres.h"
#include "asd/classutil.h"
#include "asd/timer.h"

#if asd_Platform_Windows
#include <Pdh.h>
#endif

namespace asd
{
	class CPU final : public Global<CPU>
	{
		mutable Mutex m_lock;
		Timer::TimePoint m_timerOffset = Timer::Now();
		Task_ptr m_lastTimer;

		uint32_t m_periodMs = 1000;
		size_t m_sampleCount = 5;

		size_t m_offset = 0;
		std::vector<double> m_samples;

		double m_recentAvg = 0;
		double m_last = 0;

	public:
		CPU() asd_noexcept
		{
			InitNative();
			RegisterSamplingEvemt();
		}

		~CPU() asd_noexcept
		{
			if (m_lastTimer != nullptr)
				m_lastTimer->Cancel();
		}

		void RegisterSamplingEvemt() asd_noexcept
		{
			m_lastTimer = Timer::Instance().PushAt(m_timerOffset, [this]()
			{
				double sample = GetSample();
				auto lock = GetLock(m_lock);
				m_timerOffset += Timer::Millisec(m_periodMs);
				AddSample(sample);
				RegisterSamplingEvemt();
			});
		}

		void AddSample(IN double a_usage) asd_noexcept
		{
			asd_DAssert(0 <= a_usage && a_usage <= 1);
			auto lock = GetLock(m_lock);
			if (m_samples.size() != m_sampleCount) {
				auto org = m_samples.size();
				m_samples.resize(m_sampleCount);
				for (auto i=org; i<m_sampleCount; ++i)
					m_samples[i] = a_usage;
			}

			asd_DAssert(m_samples.size() == m_sampleCount);
			m_samples[++m_offset % m_sampleCount] = a_usage;
			m_last = a_usage;

			double sum = 0;
			for (auto& sample : m_samples)
				sum += sample;
			m_recentAvg = sum / m_sampleCount;
		}

		double RecentAvg() const asd_noexcept
		{
			return m_recentAvg;
		}

		double Last() const asd_noexcept
		{
			return m_last;
		}

		// http://hashcode.co.kr/questions/290/%ED%94%84%EB%A1%9C%EC%84%B8%EC%8A%A4-%EC%95%88%EC%97%90%EC%84%9C-cpu-memory-%EC%86%8C%EB%B9%84%EB%9F%89-%EC%95%8C%EC%95%84%EB%82%B4%EB%8A%94-%EB%B2%95
#if asd_Platform_Windows
		PDH_HQUERY m_cpuQuery;
		PDH_HCOUNTER m_cpuTotal;
		bool m_initNative = false;

		void InitNative() asd_noexcept
		{
			if (m_initNative)
				return;
			PDH_STATUS ret;
			ret = ::PdhOpenQuery(NULL, NULL, &m_cpuQuery);
			asd_ChkErrAndRet(ret != ERROR_SUCCESS, "fail PdhOpenQuery, ret:{}", ret);
			ret = ::PdhAddCounterW(m_cpuQuery, L"\\Processor(_Total)\\% Processor Time", NULL, &m_cpuTotal);
			asd_ChkErrAndRet(ret != ERROR_SUCCESS, "fail PdhAddCounter, ret:{}", ret);
			ret = ::PdhCollectQueryData(m_cpuQuery);
			asd_ChkErrAndRet(ret != ERROR_SUCCESS, "fail PdhCollectQueryData, ret:{}", ret);
			m_initNative = true;
		}

		double GetSample() asd_noexcept
		{
			InitNative();
			PDH_STATUS ret;
			PDH_FMT_COUNTERVALUE counterVal;
			ret = ::PdhCollectQueryData(m_cpuQuery);
			asd_ChkErrAndRetVal(ret != ERROR_SUCCESS, Last(), "fail PdhCollectQueryData, ret:{}", ret);
			ret = ::PdhGetFormattedCounterValue(m_cpuTotal, PDH_FMT_DOUBLE, NULL, &counterVal);
			asd_ChkErrAndRetVal(ret != ERROR_SUCCESS, Last(), "fail PdhGetFormattedCounterValue, ret:{}", ret);
			return counterVal.doubleValue / 100;
		}

#else
		struct Stat
		{
			uint64_t user;
			uint64_t nice;
			uint64_t system;
			uint64_t idle;
		};

		Stat m_lastStat;

		static bool GetStat(OUT Stat& a_stat) asd_noexcept
		{
			FILE* file = ::fopen("/proc/stat", "r");
			if (file == nullptr)
				return false;
			bool ok = 4 == ::fscanf(file,
									"cpu %llu %llu %llu %llu",
									&a_stat.user,
									&a_stat.nice,
									&a_stat.system,
									&a_stat.idle);
			::fclose(file);
			return ok;
		}

		void InitNative() asd_noexcept
		{
			if (GetStat(m_lastStat) == false)
				std::memset(&m_lastStat, 0, sizeof(m_lastStat));
		}

		double GetSample() asd_noexcept
		{
			Stat stat;
			asd_ChkErrAndRetVal(GetStat(stat) == false, Last(), "fail GetStat, errno:{}", errno);

			Stat delta;
			delta.user = stat.user - m_lastStat.user;
			delta.nice = stat.nice - m_lastStat.nice;
			delta.system = stat.system - m_lastStat.system;
			delta.idle = stat.idle - m_lastStat.idle;
			m_lastStat = stat;

			double useage = delta.user + delta.nice + delta.system;
			useage /= useage + delta.idle;
			return useage;
		}

#endif
	};



	double CpuUsage() asd_noexcept
	{
		return CPU::Instance().RecentAvg();
	}
}
