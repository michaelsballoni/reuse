#include "pch.h"
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
		test_class(const std::wstring& initializer, bool _cleanInBackground)
			: init(initializer)
			, m_cleanInBackground(_cleanInBackground)
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

		std::wstring init;
		std::string data;

	private:
		bool m_cleanInBackground;
	};

	TEST_CLASS(reusetests)
	{
	public:
		TEST_METHOD(TestReuse)
		{
			std::vector<bool> should_clean_in_bgs{ true, false };
			for (bool should_clean_in_bg : should_clean_in_bgs)
			{
				pool<test_class> 
					pool
					(
						[&](const std::wstring& initializer) 
						{
							return new test_class(initializer, should_clean_in_bg); 
						},
						1, 
						1
					);
				test_class* obj;
				{
					auto use = pool.use(L"init");
					obj = &use.get();

					Assert::AreEqual(std::string(), obj->data);
					Assert::AreEqual(std::wstring(L"init"), obj->init);

					obj->process();
					Assert::AreEqual(std::string("914"), obj->data);
				}
				if (should_clean_in_bg) // wait for cleanup
					std::this_thread::sleep_for(1s);
				Assert::AreEqual(std::wstring(L"init"), obj->init);
				Assert::AreEqual(std::string(), obj->data);
			}
		}
	};
}
