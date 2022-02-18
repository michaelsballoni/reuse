#pragma once

#include "includes.h"

namespace reuse
{
	using namespace std::chrono_literals;

	class reusable
	{
	public:
		reusable(const wchar_t* initializer = nullptr)
			: m_initializer(initializer == nullptr ? L"" : initializer)
		{}

		virtual ~reusable() {}

		virtual bool cleanable() const { return false; }
		virtual void clean() {}

		const std::wstring& initializer() const { return m_initializer; }

	private:
		std::wstring m_initializer;
	};

	template <class T>
	class reuse_manager
	{
	public:
		reuse_manager(const size_t maxInventory, const size_t maxToClean)
			: m_maxInventory(maxInventory)
			, m_maxToClean(maxToClean)
			, m_keepRunning(true)
			, m_doneCleaning(false)
			, m_cleanupThread([&]() { cleanup(); })
		{}

		~reuse_manager()
		{
			m_keepRunning = false;

			while (!m_doneCleaning)
			{
				std::unique_lock<std::mutex> lock(m_incomingMutex);
				m_incomingCondition.wait_for
				(
					lock,
					10ms,
					[&] { return m_doneCleaning; }
				);
			}
			m_cleanupThread.join();
		}

		T* get(const wchar_t* initializer = nullptr)
		{
			if (m_keepRunning)
			{
				std::unique_lock<std::mutex> lock(m_inventoryMutex);
				if (!m_inventory.empty()) {
					T* ret_val = m_inventory.back();
					m_inventory.pop_back();
					return ret_val;
				}
			}
			else
				return nullptr;

			return new T(initializer);
		}

		void put(T* t)
		{
			if (m_keepRunning)
			{
				if (t->cleanable())
				{
					std::unique_lock<std::mutex> lock(m_incomingMutex);
					if (m_incoming.size() < m_maxToClean)
					{
						m_incoming.push_back(t);
						m_incomingCondition.notify_one();
						return;
					}
				}
				else
				{
					std::unique_lock<std::mutex> lock(m_inventoryMutex);
					if (m_inventory.size() < m_maxInventory)
					{
						m_inventory.push_back(t);
						return;
					}
				}
			}

			delete t;
		}

		void clear()
		{
			{
				std::unique_lock<std::mutex> lock(m_inventoryMutex);
				for (T* t : m_inventory)
					delete t;
				m_inventory.clear();
			}

			{
				std::unique_lock<std::mutex> lock(m_incomingMutex);
				for (T* t : m_incoming)
					delete t;
				m_incoming.clear();
			}
		}

	private:
		void cleanup()
		{
			while (m_keepRunning)
			{
				T* t;
				{
					std::unique_lock<std::mutex> lock(m_incomingMutex);
					m_incomingCondition.wait_for
					(
						lock,
						10ms,
						[&] { return !m_incoming.empty() || !m_keepRunning; }
					);
					if (m_incoming.empty() || !m_keepRunning)
						continue;

					t = m_incoming.back();
					m_incoming.pop_back();
				}

				// we only get here if t is cleanable
				t->clean();

				bool should_delete = false;
				{
					std::unique_lock<std::mutex> lock(m_inventoryMutex);
					if (m_inventory.size() > m_maxInventory)
						should_delete = true;
					else
						m_inventory.push_back(t);
				}

				if (should_delete)
					delete t;
			}
			m_doneCleaning = true;
		}

	private:
		const std::function<reusable* (const std::wstring&)> m_constructor;

		const size_t m_maxInventory;
		std::vector<T*> m_inventory;
		std::mutex m_inventoryMutex;

		const size_t m_maxToClean;
		std::vector<T*> m_incoming;
		std::mutex m_incomingMutex;
		std::condition_variable m_incomingCondition;

		bool m_keepRunning;
		std::thread m_cleanupThread;
		bool m_doneCleaning;
	};
}
