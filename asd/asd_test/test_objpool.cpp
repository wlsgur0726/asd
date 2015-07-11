#include "stdafx.h"
#include "../asd/include/objpool.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <cstdlib>
#include <ctime>

namespace asdtest_objpool
{
	std::atomic<int> g_objCount;
	std::atomic<int> g_conCount_default;
	std::atomic<int> g_conCount_param;
	std::atomic<int> g_desCount;

	void Init()
	{
		std::srand(std::time(nullptr));
		g_objCount = 0;
		g_conCount_default = 0;
		g_desCount = 0;
		g_conCount_param = 0;
	}
	

	struct TestClass
	{
		char m_data[100];

		TestClass() {
			++g_objCount;
			++g_conCount_default;
		}

		TestClass(int p1, void* p2) {
			++g_objCount;
			++g_conCount_param;
		}

		~TestClass() {
			--g_objCount;
			++g_desCount;
		}
	};


	const int TestCount = 100;
	template<typename ObjPool, int ThreadCount>
	void TestObjPool()
	{
		// 1. AddCount 테스트
		Init();
		{
			const int ObjCount = TestCount;
			ObjPool objPool;
			EXPECT_EQ(objPool.GetCount(), 0);
			EXPECT_EQ(g_objCount, 0);
			EXPECT_EQ(g_conCount_default, 0);

			objPool.AddCount(ObjCount);
			EXPECT_EQ(objPool.GetCount(), ObjCount);
			EXPECT_EQ(g_objCount, 0);
			EXPECT_EQ(g_conCount_default, 0);
		}


		// 2. ObjectPool 생성자의 인자 테스트
		Init();
		{
			const int LimitCount = TestCount;
			const int InitCount = TestCount + 123; // 사용자가 실수로 많이 넣는 경우

			ObjPool objPool(LimitCount, InitCount);
			EXPECT_EQ(objPool.GetCount(), LimitCount);
			EXPECT_EQ(g_objCount, 0);
			EXPECT_EQ(g_conCount_default, 0);

			std::vector<TestClass*> objs;

			// 2-1. initCount로 풀링되어있는 개수만큼 
			//      기본생성자로 Get을 하면서 풀에 남아있는 객체 수를 검사
			const int FirstGetCount = objPool.GetCount();
			for (int i=1; i<=FirstGetCount; ++i) {
				objs.push_back(objPool.Get());
				EXPECT_EQ(g_objCount, i);
				EXPECT_EQ(g_conCount_default, i);
				EXPECT_EQ(g_conCount_param, 0);
				EXPECT_EQ(objPool.GetCount(), FirstGetCount - i);
			}

			// 2-2. 기존에 풀링되어있는 개수를 초과하여 Get,
			//      더불어 생성하는 객체의 다른 생성자가 동작하는지 여부도 테스트
			const int SecondGetCount = TestCount;
			for (int i=1; i<=SecondGetCount; ++i) {
				objs.push_back(objPool.Get(123, &objPool));
				EXPECT_EQ(g_objCount, FirstGetCount + i);
				EXPECT_EQ(g_conCount_default, FirstGetCount);
				EXPECT_EQ(g_conCount_param, i);
				EXPECT_EQ(objPool.GetCount(), 0);
			}

			// 2-3. 생성자 횟수와 풀링된 객체 수 검사
			EXPECT_EQ(objPool.GetCount(), 0);
			ASSERT_EQ(g_objCount, FirstGetCount + SecondGetCount);
			ASSERT_EQ(g_objCount, objs.size());
			ASSERT_EQ(g_conCount_default, FirstGetCount);
			ASSERT_EQ(g_conCount_param, SecondGetCount);


			// 2-4. 모두 릴리즈. LimitCount보다 많이 풀링되어선 안된다.
			ASSERT_GT(objs.size(), LimitCount);
			const int ObjCount = g_objCount;
			for (int i=1; i<=objs.size(); ++i) {
				objPool.Release(objs[i-1]);
				EXPECT_EQ(g_desCount, i);
				EXPECT_EQ(g_objCount, ObjCount - i);

				if (i < LimitCount)
					EXPECT_EQ(objPool.GetCount(), i);
				else
					EXPECT_EQ(objPool.GetCount(), LimitCount);
			}

			EXPECT_EQ(objPool.GetCount(), LimitCount);
			EXPECT_EQ(g_desCount, ObjCount);
			EXPECT_EQ(g_objCount, 0);
		}


		// 3. 멀티쓰레드 테스트 (락이 있는 경우만)
		Init();
		if (ThreadCount > 1)
		{
			std::thread threads[ThreadCount];
			ObjPool objPool;
			
			volatile bool start = false;
			for (auto& t : threads) {
				t = std::thread([&]() 
				{
					while (start == false);

					for (int i=0; i<TestCount; ++i) {
						TestClass* objs[TestCount];

						// 3-1. 풀에서부터 할당
						for (int i=0; i<TestCount; ++i) {
							objs[i] = objPool.Get();
						}

						// 3-2. 할당받았던 것들을 풀에 반납
						for (int i=0; i<TestCount; ++i) {
							objPool.Release(objs[i]);
						}

						// 3-3. 랜덤하게 Clear나 AddCount등을 호출하여 
						//      Thread Safe 여부를 테스트
						switch (std::rand() % 4) {
							case 0:
								objPool.Clear();
								break;
							case 1:
								objPool.AddCount(TestCount/10);
								break;
							default:
								break;
						}
					}
				});
			}
			
			start = true;
			for (auto& t : threads)
				t.join();

			const int CheckCount = ThreadCount * TestCount * TestCount;
			EXPECT_EQ(g_conCount_default, CheckCount);
			EXPECT_EQ(g_desCount, CheckCount);
			EXPECT_EQ(g_objCount, 0);
		}
	}


	TEST(ObjectPool, Default)
	{
		TestObjPool<typename asd::ObjectPool<TestClass>, 1>();
	}

	TEST(ObjectPool, WithStdMutex)
	{
		TestObjPool<typename asd::ObjectPool<TestClass, std::mutex>, 4>();
	}

	TEST(ObjectPool, WithSpinMutex)
	{
		TestObjPool<typename asd::ObjectPool<TestClass, asd::SpinMutex>, 4>();
	}

	TEST(ObjectPool, WithAsdMutex)
	{
		TestObjPool<typename asd::ObjectPool<TestClass, asd::Mutex>, 4>();
	}
}
