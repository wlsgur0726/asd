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
		size_t m_sampleCount = 10;

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
			asd_DAssert(0 <= a_usage && a_usage <= 100);
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

		void InitNative() asd_noexcept
		{
			::PdhOpenQuery(NULL, NULL, &m_cpuQuery);
			::PdhAddCounterW(m_cpuQuery, L"\\Processor(_Total)\\% Processor Time", NULL, &m_cpuTotal);
			::PdhCollectQueryData(m_cpuQuery);
		}

		double GetSample() asd_noexcept
		{
			PDH_FMT_COUNTERVALUE counterVal;
			::PdhCollectQueryData(m_cpuQuery);
			::PdhGetFormattedCounterValue(m_cpuTotal, PDH_FMT_DOUBLE, NULL, &counterVal);
			return counterVal.doubleValue;
		}

#else
		uint64_t m_lastTotalUser, m_lastTotalUserLow, m_lastTotalSys, m_lastTotalIdle;
		void InitNative() asd_noexcept
		{
			FILE* file = ::fopen("/proc/stat", "r");
			::fscanf(file,
					 "cpu %llu %llu %llu %llu",
					 &m_lastTotalUser,
					 &m_lastTotalUserLow,
					 &m_lastTotalSys,
					 &m_lastTotalIdle);
			::fclose(file);
		}

		double GetSample() asd_noexcept
		{
			double percent;
			FILE* file;
			uint64_t totalUser, totalUserLow, totalSys, totalIdle, total;

			file = ::fopen("/proc/stat", "r");
			::fscanf(file,
					 "cpu %llu %llu %llu %llu",
					 &totalUser,
					 &totalUserLow,
					 &totalSys,
					 &totalIdle);
			::fclose(file);

			bool overflow
				 = totalUser < m_lastTotalUser
				|| totalUserLow < m_lastTotalUserLow
				|| totalSys < m_lastTotalSys
				|| totalIdle < m_lastTotalIdle;
			if (overflow) {
				//오버플로우 detection
				percent = -1.0;
			}
			else {
				total = (totalUser - m_lastTotalUser) + (totalUserLow - m_lastTotalUserLow) +
					(totalSys - m_lastTotalSys);
				percent = total;
				total += (totalIdle - m_lastTotalIdle);
				percent /= total;
				percent *= 100;
			}

			m_lastTotalUser = totalUser;
			m_lastTotalUserLow = totalUserLow;
			m_lastTotalSys = totalSys;
			m_lastTotalIdle = totalIdle;

			return percent;
		}

#endif
	};



	double CpuUsage() asd_noexcept
	{
		return CPU::Instance().RecentAvg();
	}
}
