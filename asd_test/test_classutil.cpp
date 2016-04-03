#include "stdafx.h"
#include "asd/string.h"
#include "asd/classutil.h"
#include <thread>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace asdtest_classutil
{
	struct Counter
	{
		size_t val = 0;
	};

	struct
	{
		std::unordered_map<void*, Counter> UseCounter;
		std::mutex Mutex;
		std::atomic<int> Seq;

		void Init(int seq = 0)
		{
			Mutex.lock();
			Seq = seq;
			UseCounter.clear();
			Mutex.unlock();
		}

		size_t PopUseCount(void* ptr)
		{
			Mutex.lock();
			auto ret = UseCounter[ptr].val;
			UseCounter.erase(ptr);
			Mutex.unlock();
			return ret;
		}
	} TestManager;


	class TestClass
		: public asd::Global<TestClass>
		, public asd::ThreadLocal<TestClass>
	{
		int m_data;

	public:
		TestClass()
		{
			m_data = TestManager.Seq++;
			TestManager.Mutex.lock();
			TestManager.UseCounter[this].val++;
			TestManager.Mutex.unlock();
		}

		int GetData() const 
		{
			return m_data;
		}
	};



	TEST(ClassUtil, GlobalInstance)
	{
		const int DataValue = 123;
		TestManager.Init(DataValue);

		const size_t ThreadCount = 4;
		std::thread threads[ThreadCount];
		
		std::mutex lock;
		std::unordered_map<std::thread::id, void*> results;

		for (size_t i=0; i<ThreadCount; ++i) {
			threads[i] = std::thread([&]()
			{
				auto& globalInstance = TestClass::GlobalInstance();
				EXPECT_EQ(globalInstance.GetData(), DataValue);
				lock.lock();
				results[std::this_thread::get_id()] = &globalInstance;
				lock.unlock();
			});
		}

		for (auto& t : threads)
			t.join();

		size_t useCount = 0;
		auto& globalInstance = TestClass::GlobalInstance();
		for (auto it : results) {
			useCount += TestManager.PopUseCount(it.second);
			EXPECT_EQ(&globalInstance, it.second);
		}
		EXPECT_EQ(useCount, 1);
	}



	TEST(ClassUtil, ThreadLocalInstance)
	{
		TestManager.Init();

		const size_t ThreadCount = 100;
		std::thread threads[ThreadCount];

		std::mutex lock;
		typedef std::pair<void*, int> Result;
		std::unordered_map<std::thread::id, Result> results;

		for (size_t i=0; i<ThreadCount; ++i) {
			threads[i] = std::thread([&]()
			{
				auto& tlinst = TestClass::ThreadLocalInstance();
				lock.lock();
				results[std::this_thread::get_id()] = Result(&tlinst, tlinst.GetData());
				lock.unlock();
			});
		}

		for (auto& t : threads)
			t.join();

		size_t useCount = 0;
		std::unordered_set<int> valset;
		for (auto it : results) {
			useCount += TestManager.PopUseCount(it.second.first);
			valset.insert(it.second.second);
		}

		EXPECT_EQ(ThreadCount, useCount);
		EXPECT_EQ(ThreadCount, valset.size());
	}
}
