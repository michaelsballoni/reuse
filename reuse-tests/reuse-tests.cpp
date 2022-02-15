#include "pch.h"
#include "CppUnitTest.h"

#include "../reuse/use.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace reuse
{
	std::atomic<int> test_class_count = 0;

	class test_class
	{
	public:
		test_class(const wchar_t* initializer)
			: init(initializer)
		{
			test_class_count.fetch_add(1);
		}

		~test_class()
		{
			clean();
			test_class_count.fetch_add(-1);
		}

		void clean()
		{
			data = "";
		}

		void process()
		{
			data = "914";
		}

		std::wstring init;
		std::string data;
	};

	TEST_CLASS(reusetests)
	{
	public:
		TEST_METHOD(TestReuse)
		{
			reuse_manager<test_class> mgr(1);
			test_class* obj = nullptr;
			{
				use<test_class> use(mgr, L"init");
				obj = &use.get();

				Assert::AreEqual(std::string(), obj->data);
				Assert::AreEqual(std::wstring(L"init"), obj->init);
				
				obj->process();
				Assert::AreEqual(std::string("914"), obj->data);
			}
			Assert::AreEqual(std::wstring(L"init"), obj->init);
			Assert::AreEqual(std::string(), obj->data);
		}
	};
}
