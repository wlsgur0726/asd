#include "stdafx.h"
#include "asd/sysres.h"
#include "asd/classutil.h"
#include "asd/timer.h"

#if asd_Platform_Windows
#	include <winternl.h>
#	include <unordered_map>
#	include <unordered_set>
#
#endif

#define asd_CpuUsage_Enable_HighAccuracy 1

namespace asd
{
#if asd_Platform_Windows && asd_CpuUsage_Enable_HighAccuracy
#	include "SYSTEM_INFORMATION_CLASS.h"
#endif

	class CPU final : public Global<CPU>
	{
		mutable Mutex m_lock;
		Timer::TimePoint m_timerOffset = Timer::Now();
		Task_ptr m_lastTimer;

		Timer::Millisec m_interval = Timer::Millisec(1000);
		size_t m_sampleCount = 5;

		size_t m_offset = 0;
		std::vector<double> m_samples;

		double m_recentAvg = 0;
		double m_last = 0;

		int m_errorCount = 0;
		bool m_run = true;

	public:
		CPU()
		{
			InitNative();
			RegisterSamplingEvemt();
		}

		~CPU()
		{
			auto lock = GetLock(m_lock);
			m_run = false;
			if (m_lastTimer != nullptr) {
				m_lastTimer->Cancel();
				m_lastTimer = nullptr;
			}
		}

		void RegisterSamplingEvemt()
		{
			if (!m_run)
				return;

			m_lastTimer = Timer::Instance().PushAt(m_timerOffset, [this]()
			{
				double sample = GetSample();

				auto lock = GetLock(m_lock);
				m_timerOffset += m_interval;
				if (sample >= 0)
					AddSample(sample);
				RegisterSamplingEvemt();
			});
		}

		void AddSample(IN double a_usage)
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

		Timer::Millisec SetInterval(IN Timer::Millisec a_interval)
		{
			auto lock = GetLock(m_lock);
			auto org = m_interval;
			m_interval = a_interval;
			return org;
		}

		Timer::Millisec GetInterval() const
		{
			return m_interval;
		}

		double RecentAvg() const
		{
			return m_recentAvg;
		}

		double Last() const
		{
			return m_last;
		}

#if asd_Platform_Windows

	#if asd_CpuUsage_Enable_HighAccuracy
		struct Stat
		{
			uint64_t idle;
			uint64_t system;
		};

		Stat m_lastStat;

		Mutex m_lockGetSample;
		std::vector<_SYSTEM_PROCESSOR_IDLE_CYCLE_TIME_INFORMATION> m_spicti;
		std::vector<_SYSTEM_PROCESSOR_CYCLE_TIME_INFORMATION> m_spci;
		std::vector<uint8_t> m_spiBuffer;

		using PID = decltype(SYSTEM_PROCESS_INFORMATION::UniqueProcessId);
		struct ProcessInfo
		{
			uint64_t cycle = 0;
		};
		std::unordered_map<PID, ProcessInfo> m_processMap;

		void InitNative()
		{
			std::memset(&m_lastStat, 0, sizeof(m_lastStat));
			m_spicti.resize(Get_HW_Concurrency());
			m_spci.resize(Get_HW_Concurrency());
		}

		double GetSample()
		{
			auto lock = GetLock(m_lockGetSample);

			uint64_t idleCycle;
			{
				ULONG expLen = (ULONG)(sizeof(m_spicti[0]) * m_spicti.size());
				ULONG retLen;
				auto ret = ::NtQuerySystemInformation((SYSTEM_INFORMATION_CLASS)SystemProcessorIdleCycleTimeInformation,
													  m_spicti.data(),
													  expLen,
													  &retLen);
				if (ret != STATUS_SUCCESS) {
					asd_OnErr("fail NtQuerySystemInformation, ret:{}", ret);
					return -1;
				}
				if (expLen != retLen) {
					asd_OnErr("unexpected length, expLen:{}, retLen:{}", expLen, retLen);
					return -1;
				}

				uint64_t cur = 0;
				for (auto& spicti : m_spicti)
					cur += spicti.CycleTime;

				idleCycle = cur - m_lastStat.idle;
				m_lastStat.idle = cur;
			}

			uint64_t systemCycle;
			{

				ULONG expLen = (ULONG)(sizeof(m_spci[0]) * m_spci.size());
				ULONG retLen;
				auto ret = ::NtQuerySystemInformation((SYSTEM_INFORMATION_CLASS)SystemProcessorCycleTimeInformation,
													  m_spci.data(),
													  expLen,
													  &retLen);
				if (ret != STATUS_SUCCESS) {
					asd_OnErr("fail NtQuerySystemInformation, ret:{}", ret);
					return -1;
				}
				if (expLen != retLen) {
					asd_OnErr("unexpected length, expLen:{}, retLen:{}", expLen, retLen);
					return -1;
				}

				uint64_t cur = 0;
				for (auto& spci : m_spci)
					cur += spci.CycleTime;

				systemCycle = cur - m_lastStat.system;
				m_lastStat.system = cur;
			}

			uint64_t processCycle = 0;
			{
				auto ret = [&](){
					for(;;) {
						ULONG expLen = (ULONG)m_spiBuffer.size();
						ULONG retLen = 0;
						auto ret = ::NtQuerySystemInformation(::SystemProcessInformation, m_spiBuffer.data(), expLen, &retLen);
						switch (ret) {
							case STATUS_BUFFER_TOO_SMALL:
							case STATUS_INFO_LENGTH_MISMATCH:
								m_spiBuffer.resize(retLen);
								continue;
						}
						return ret;
					}
				}();
				if (ret != STATUS_SUCCESS) {
					asd_OnErr("fail NtQuerySystemInformation, ret:{}", ret);
					return -1;
				}
				if (m_spiBuffer.empty()) {
					asd_OnErr("unknown error");
					return -1;
				}

				uint64_t newCycle = 0;
				std::unordered_map<PID, ProcessInfo> processMap;
				auto spi = reinterpret_cast<SYSTEM_PROCESS_INFORMATION*>(m_spiBuffer.data());
				for (auto next=spi; next; spi=next) {
					if (spi->NextEntryOffset == 0)
						next = nullptr;
					else
						next = reinterpret_cast<SYSTEM_PROCESS_INFORMATION*>(&((uint8_t*)spi)[spi->NextEntryOffset]);

					if (spi->UniqueProcessId == 0)
						continue; // IDLE

					auto& pi = processMap[spi->UniqueProcessId];
					pi.cycle = spi->CycleTime;
					newCycle += pi.cycle;
				}

				uint64_t oldCycle = 0;
				for (auto& it : m_processMap) {
					if (processMap.find(it.first) == processMap.end()) {
						// dead process
					}
					else {
						oldCycle += it.second.cycle;
					}
				}

				processCycle = newCycle - oldCycle;
				std::swap(m_processMap, processMap);
			}

			double totalCycle = (double)(idleCycle + systemCycle + processCycle);
			if (totalCycle == 0)
				return 0;
			return 1.0 - (idleCycle / totalCycle);
		}

	#else
		struct Stat
		{
			uint64_t kernel;
			uint64_t user;
			uint64_t idle;
		};

		Stat m_lastStat;

		void InitNative()
		{
			std::memset(&m_lastStat, 0, sizeof(m_lastStat));
		}

		double GetSample()
		{
			FILETIME idle, kernel, user;
			if (::GetSystemTimes(&idle, &kernel, &user) == FALSE) {
				auto e = ::GetLastError();
				asd_OnErr("fail GetSystemTimes, GetLastError:{}", e);
				return -1;
			}

			auto conv = [](const FILETIME& ft) -> uint64_t
			{
				uint64_t ret = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
				return ret;
			};

			Stat stat;
			stat.idle = conv(idle);
			stat.kernel = conv(kernel) - stat.idle;
			stat.user = conv(user);

			auto dIdle = stat.idle - m_lastStat.idle;
			auto dKernel = stat.kernel - m_lastStat.kernel;
			auto dUser = stat.user - m_lastStat.user;
			m_lastStat = stat;

			auto total = (double)(dIdle + dKernel + dUser);
			if (total == 0)
				return 0;
			return (dKernel + dUser) / total;
		}

	#endif // asd_CpuUsage_Enable_HighAccuracy

#else
		struct Stat
		{
			unsigned long long user = 0;
			unsigned long long nice = 0;
			unsigned long long system = 0;
			unsigned long long idle = 0;
		};

		Stat m_lastStat;

		static bool GetStat(OUT Stat& a_stat)
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

		void InitNative()
		{
			if (GetStat(m_lastStat) == false)
				std::memset(&m_lastStat, 0, sizeof(m_lastStat));
		}

		double GetSample()
		{
			Stat stat;
			if (GetStat(stat) == false) {
				asd_OnErr("fail GetStat, errno:{}", errno);
				return -1;
			}

			auto dUser = stat.user - m_lastStat.user;
			auto dNice = stat.nice - m_lastStat.nice;
			auto dSystem = stat.system - m_lastStat.system;
			auto dIdle = stat.idle - m_lastStat.idle;
			auto total = dUser + dNice + dSystem + dIdle;
			m_lastStat = stat;

			if (total == 0)
				return 0;
			return (double)(dUser + dNice + dSystem) / total;
		}

#endif
	};



	double CpuUsage()
	{
		return CPU::Instance().RecentAvg();
	}

	Timer::Millisec GetCpuUsageCheckInterval()
	{
		return CPU::Instance().GetInterval();
	}

	Timer::Millisec SetCpuUsageCheckInterval(IN Timer::Millisec a_cycle)
	{
		return CPU::Instance().SetInterval(a_cycle);
	}
}
