#include "CppUnitTest.h"

#include "../reuse/reuse.h"

#include <thread>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace reuse
{
	std::atomic<int> test_class_count = 0;

	class test_class : public reusable
	{
	public:
		test_class(const std::wstring& initializer, bool cleanInBackground)
			: reusable(initializer)
			, m_cleanInBackground(cleanInBackground)
		{
			test_class_count.fetch_add(1);
		}

		~test_class()
		{
			clean();
			test_class_count.fetch_add(-1);
		}

		virtual void clean()
		{
			data = "";
		}
		virtual bool cleanInBackground() const { return m_cleanInBackground; }

		void process()
		{
			data = "914";
		}

		std::string data;

	private:
		bool m_cleanInBackground;
	};

	TEST_CLASS(reusetests)
	{
	public:
		TEST_METHOD(TestReuse)
		{
			// Test cleaning in the foreground and background
			for (int run = 1; run <= 4; ++run)
			{
				std::vector<bool> should_clean_in_bgs{ true, false };
				for (bool should_clean_in_bg : should_clean_in_bgs)
				{
					// Create our pool with a constructor function
					// that passes in how to do the cleaning
					pool<test_class>
						pool
						(
							[&](const std::wstring& initializer)
							{
								return new test_class(initializer, should_clean_in_bg);
							}
					);

					// NOTE: We'll hold onto a pool pointer in p so after we're done using the object
					//		 we can inspect the object as it sits in the pool's inventory
					test_class* p = nullptr;
					{
						// Use the pool to get a test_class object
						auto use = pool.use(L"init");

						// Access the test_class object inside the reuse object
						test_class& obj = use.get();
						p = &obj; // stash off the pointer for later

						// See that the object is clean
						Assert::AreEqual(std::string(), obj.data);
						Assert::AreEqual(std::wstring(L"init"), obj.initializer());

						// Use the object
						obj.process();
						Assert::AreEqual(std::string("914"), obj.data);
					}
					if (should_clean_in_bg) // wait for cleanup
						std::this_thread::sleep_for(1s);

					// See that the object is clean in the pool, ready for reuse
					Assert::AreEqual(std::wstring(L"init"), p->initializer());
					Assert::AreEqual(std::string(), p->data);
				}
			}
		}
	};
}
