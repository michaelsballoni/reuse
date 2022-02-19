#pragma once

#include "includes.h"
#include "reusable.h"

namespace reuse
{
	using namespace std::chrono_literals;

	/// <summary>
	/// A pool stores and hands out objects so that objects are not reallocated over and over
	/// The class is templated so that it can new objects of the type with a given initializer
	/// </summary>
	/// <typeparam name="T">The concrete object type to pool</typeparam>
	template <class T>
	class pool
	{
	public:
		/// <summary>
		/// Declare a pool object for a concrete type with given object count limits
		/// </summary>
		/// <param name="maxInventory">How many objects can the pool hold before objects put for recycling are dropped (deleted)</param>
		/// <param name="maxToClean">How many objects can be in queue for cleaned before objects are dropped (deleted) </param>
		pool(const size_t maxInventory, const size_t maxToClean)
			: m_maxInventory(maxInventory)
			, m_maxToClean(maxToClean)
			, m_size(0)
			, m_keepRunning(true)
			, m_doneCleaning(false)
			, m_cleanupThread([&]() { cleanup(); }) // start the background cleanup thread
		{}

		~pool()
		{
			// Raise the flag that the shop is shutting down
			m_keepRunning = false;

			// Wait for the background thread to exit
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

			clear();
		}

		/// <summary>
		/// What is the total count of objects in the pool?
		/// This is the sum of the size of the null iniitalizer bucket, 
		/// and the sizes of the initializer specific buckets
		/// </summary>
		size_t size() const
		{
			return size_t(m_size.load());
		}

		/// <summary>
		/// Get an object for a given initializer string
		/// Objects are created using initializer strings, used, then put() and clean()'d
		/// and handed back out
		/// </summary>
		/// <param name="initializer"></param>
		/// <returns></returns>
		T* get(const std::wstring& initializer = L"")
		{
			if (m_keepRunning)
			{
				std::unique_lock<std::mutex> lock(m_bucketMutex);
				if (initializer.empty()) // consider using the "un" bucket
				{						 // the one reserved for use cases with no initializer strings
					if (!m_unBucket.empty()) // get an object from the un bucket
					{
						T* ret_val = m_unBucket.back();
						m_unBucket.pop_back();
						m_size.fetch_add(-1);
						return ret_val;
					}
				}
				else // we're using initializer strings
				{
					// Find the bucket for the initializer string
					const auto& it = m_initBuckets.find(initializer);
					if (it != m_initBuckets.end() && !it->second.empty()) // a matching bucket exists
					{													 // and has objects to hand out
						T* ret_val = it->second.back();
						it->second.pop_back();
						m_size.fetch_add(-1);
						return ret_val;
					}
				}
			}

			// Failing all of that, including whether we should keep running,
			// new up a new T object with the initializer string
			return new T(initializer);
		}

		/// <summary>
		/// Hand an object back to the pool for reuse
		/// </summary>
		void put(T* t)
		{
			if (t == nullptr) // it can happen
				return;

			if (m_keepRunning)
			{
				if (t->cleanInBackground()) // queue up the object for background cleaning
				{
					std::unique_lock<std::mutex> lock(m_incomingMutex);
					if (m_incoming.size() < m_maxToClean)
					{
						m_incoming.push_back(t);
						m_incomingCondition.notify_one();
						return;
					}
				}
				else // clean up and add to the right bucket
				{
					t->clean();

					std::unique_lock<std::mutex> lock(m_bucketMutex);
					if (size() < m_maxInventory)
					{
						if (t->initializer().empty())
						{
							m_unBucket.push_back(t);
							m_size.fetch_add(1);
						}
						else
						{
							m_initBuckets[t->initializer()].push_back(t);
							m_size.fetch_add(1);
						}
						return;
					}
				}
			}

			// Failing all of that, including whether we should keep running, drop the object (delete)
			delete t;
		}

		/// <summary>
		/// Empty all buckets freeing all memory
		/// </summary>
		void clear()
		{
			{
				std::unique_lock<std::mutex> lock(m_bucketMutex);
				
				for (T* t : m_unBucket)
					delete t;
				m_unBucket.clear();

				for (const auto& initIt : m_initBuckets)
				{
					for (T* t : initIt.second)
						delete t;
				}
				m_initBuckets.clear();
			}

			{
				std::unique_lock<std::mutex> lock(m_incomingMutex);
				for (T* t : m_incoming)
					delete t;
				m_incoming.clear();
			}

			m_size.store(0);
		}

	private:
		/// <summary>
		/// Thread routine for cleaning up objects in the background
		/// </summary>
		void cleanup()
		{
			while (m_keepRunning)
			{
				// Get something to clean
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

				t->clean();

				// Add the object to the right pool...or plan on deleting it if too much inventory
				// NOTE: Don't delete inside the mutex
				bool should_delete = false;
				{
					std::unique_lock<std::mutex> lock(m_bucketMutex);
					if (size() >= m_maxInventory)
						should_delete = true;
					else if (t->initializer().empty())
						m_unBucket.push_back(t);
					else
						m_initBuckets[t->initializer()].push_back(t);
				}

				if (should_delete)
					delete t;
				else
					m_size.fetch_add(1);
			}
			m_doneCleaning = true;
		}

	private:
		std::atomic<int> m_size;

		const size_t m_maxInventory;
		std::vector<T*> m_unBucket;
		std::unordered_map<std::wstring, std::vector<T*>> m_initBuckets;
		std::mutex m_bucketMutex;

		const size_t m_maxToClean;
		std::vector<T*> m_incoming;
		std::mutex m_incomingMutex;
		std::condition_variable m_incomingCondition;

		bool m_keepRunning;
		std::thread m_cleanupThread;
		bool m_doneCleaning;
	};
}
