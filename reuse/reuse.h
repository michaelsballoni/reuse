#pragma once

#include "includes.h"

namespace reuse
{
	template <class T>
	class reuse_manager
	{
	public:
		reuse_manager(const size_t maxInventory)
			: m_maxInventory(maxInventory)
			, m_keepRunning(true)
			, m_doneCleaning(false)
			, m_cleanupThread([]() { cleanup(); })
		{
		}

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
					[] { return m_doneCleaning; }
				);
			}
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

			return new T(initializer);
		}

		void put(T* t)
		{
			if (m_keepRunning)
			{
				std::unique_lock<std::mutex> lock(m_incomingMutex);
				m_incoming.push_front(t);
				m_incomingCondition.notify_one();
			}
			else
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
						[] { return !m_incoming.empty() || !m_keepRunning; }
					);
					if (!m_keepRunning || m_incoming.empty())
						continue;

					t = m_incoming.front();
					m_incoming.pop_front();
				}

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
		const size_t m_maxInventory;
		std::vector<T*> m_inventory;
		std::mutex m_inventoryMutex;

		std::forward_list<T*> m_incoming;
		std::mutex m_incomingMutex;
		std::condition_variable m_incomingCondition;

		bool m_keepRunning;
		std::thread m_cleanupThread;
		bool m_doneCleaning;
	};
}
