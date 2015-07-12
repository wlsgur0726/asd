#include "stdafx.h"
#include "asd/tlsmanager.h"
#include <thread>
#include <atomic>

namespace asdtest_tlsmanager
{
	std::atomic<int> g_objCount;

	class TestClass
	{
	public:
		int m_data = 0;
		TestClass() {
			++g_objCount;
		}
		~TestClass() {
			--g_objCount;
		}
	};

	typedef std::shared_ptr<asd::TLSManager<TestClass>> TLSManager_ptr;

	TLSManager_ptr g_tlsManager;
	thread_local TestClass* t_objp = nullptr;

	void Init() {
		g_tlsManager = TLSManager_ptr(new asd::TLSManager<TestClass>);
		g_objCount = 0;
	}

	TEST(TLSManager, Case1)
	{
		// TLSManager에 등록한 쓰레드 전용 데이터가 제대로 delete되는지 테스트.
		Init();
		const int ThreadCount = 128;
		std::thread threads[ThreadCount];
		std::atomic<int> completeCount(0);

		{
			// 1. TLS Manager와 쓰레드 생성.
			//	본래 TLS Manager는 전역에 선언하여 사용하도록 디자인했으나
			//	여기서는 테스트를 위해 지역에 선언한다.
			asd::TLSManager<TestClass> tlsmanager;
			volatile bool run = true;
			for (int i=0; i<ThreadCount; ++i) {
				threads[i] = std::thread([&]() {
					// TLSManager에 등록한다.
					t_objp = new TestClass;
					tlsmanager.Register(t_objp);
					++completeCount;

					while (run) {
						std::this_thread::sleep_for(std::chrono::milliseconds(10));
					}
				});
			}

			// 2. 쓰레드가 생성되고 모든 객체들이 등록 완료되기를 기다린다.
			for (int loopCount=0; completeCount<ThreadCount; ++loopCount) {
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				if (loopCount > 1000)
					FAIL();
			}

			// 3. 개수만큼 제대로 생성 되었는지 체크
			ASSERT_EQ(ThreadCount, g_objCount);

			// 4. 쓰레드들을 종료시킨다.
			run = false;
			for (auto& t : threads) {
				t.join();
			}

			// Scope를 벗어나면서 tlsmanager가 제거되고
			// tlsmanager되었던 객체들도 모두 제거되어야 한다.
		}

		// 5. TLD가 모두 제거되었는지 확인
		ASSERT_EQ(0, g_objCount);
	}


	TEST(TLSManager, Case2)
	{
		// TLSManager.Delete 메소드가 정상 작동하는지 테스트.
		Init();
		t_objp = new TestClass;
		g_tlsManager->Register(t_objp);
		EXPECT_EQ(g_objCount, 1);

		g_tlsManager->Delete(t_objp);
		EXPECT_EQ(g_objCount, 0);
		EXPECT_EQ(t_objp, nullptr);
	}
}
