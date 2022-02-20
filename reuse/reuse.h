#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace reuse
{
	using namespace std::chrono_literals;

	/// <summary>
	/// Implement reusable for the types you want to pool
	/// </summary>
	class reusable
	{
	protected: // this class cannot be instantiated
		/// <summary>
		/// The initializer string, like a database connection string, is used to pool objects
		/// of the same type but different initializers, allowing for different connection strings
		/// Use of initializer strings is optional
		/// </summary>
		/// <param name="initializer">string passed to constructor of T by pool.get()</param>
		reusable(const std::wstring& initializer = L"")
			: m_initializer(initializer)
		{}
		reusable(const reusable&) = delete;
		reusable(reusable&&) = delete;
		reusable& operator=(const reusable&) = delete;

	public:
		virtual ~reusable() {}

		/// <summary>
		/// How do we return this object to a reusable state?
		/// </summary>
		virtual void clean() {}

		/// <summary>
		/// Should this type be cleaned up in the background?
		/// If the clean() routine is lengthy and/or resource intensive this may make sense
		/// Otherwise just return false and clean() will get called as the object is put back in the pool
		/// </summary>
		/// <returns></returns>
		virtual bool cleanInBackground() const { return false; }

		/// <summary>
		/// What is the intializer for this object?
		/// This is used by the pool machinery to put objects into initializer-specific buckets
		/// </summary>
		virtual std::wstring initializer() const { return m_initializer; }

	private:
		std::wstring m_initializer;
	};

	/// <summary>
	/// A pool stores and hands out objects so that objects are not reallocated over and over
	/// The class is templated so that it can new objects of the type with a given initializer
	/// The class takes a constructor function, and T can be a base class, so you can
	/// have the constructor function decide what concrete types to create based on initializers
	/// </summary>
	/// <typeparam name="T">
	/// The object type to pool
	/// Can be a base class of a class library
	/// </typeparam>
	template <class T>
	class pool
	{
	public:
		/// <summary>
		/// Declare a pool object for a type with given object count limits
		/// </summary>
		/// <param name="constructor">What object should be created based on the initializer?</param>
		/// <param name="maxInventory">How many objects can the pool hold before objects put for recycling are dropped (deleted)?</param>
		/// <param name="maxToClean">How many objects can be in queue for cleaned before objects are dropped (deleted)?</param>
		pool
		(
			const std::function<T* (const std::wstring&)> constructor, 
			const size_t maxInventory = 1000U, 
			const size_t maxToClean= 1000U
		)
			: m_constructor(constructor)
			, m_maxInventory(maxInventory)
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

			// Free memory in the object buckets
			{
				std::unique_lock<std::mutex> lock(m_bucketMutex);

				for (T* t : m_unBucket)
					delete t;

				for (const auto& initIt : m_initBuckets)
				{
					for (T* t : initIt.second)
						delete t;
				}
			}

			// Free memory in the incoming list
			{
				std::unique_lock<std::mutex> lock(m_incomingMutex);
				for (T* t : m_incoming)
					delete t;
			}
		}

		/// <summary>
		/// Get a reuse object to get an object from the pool
		/// and automatically return it back to the pool
		/// </summary>
		/// <param name="initializer">Initializer for the object to return</param>
		/// <returns>
		/// An object for gaining access to a pooled object
		/// </returns>
		auto use(const std::wstring& initializer = L"")
		{
			return reuse<T>(*this, initializer);
		}

	private:
		/// <summary>
		/// Get an object for a given initializer string
		/// Objects are created using initializer strings, used, then put() and clean()'d
		/// and handed back out
		/// </summary>
		/// <param name="initializer">Initalizer for the object to return</param>
		/// <returns>Pointer to a new or reused object</returns>
		T* get(const std::wstring& initializer)
		{
			if (m_keepRunning)
			{
				std::unique_lock<std::mutex> lock(m_bucketMutex);

				// Consider using the null bucket used with empty initializer strings
				if (initializer.empty()) 
				{
					if (!m_unBucket.empty())
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
					// See if a matching bucket exists and has objects to hand out
					const auto& it = m_initBuckets.find(initializer);
					if (it != m_initBuckets.end() && !it->second.empty())
					{ 
						T* ret_val = it->second.back();
						it->second.pop_back();
						m_size.fetch_add(-1);
						return ret_val;
					}
				}
			}

			// Failing all of that, including whether we should keep running,
			// construct a new T object with the initializer
			return m_constructor(initializer);
		}

		/// <summary>
		/// Hand an object back to the pool for reuse
		/// </summary>
		void put(T* t)
		{
			if (t == nullptr)
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
				else // clean up and directly add to the right bucket
				{
					t->clean();

					if (m_size.load() < m_maxInventory)
					{
						std::wstring initializer = t->initializer();
						std::unique_lock<std::mutex> lock(m_bucketMutex);
						if (initializer.empty())
							m_unBucket.push_back(t);
						else
							m_initBuckets[initializer].push_back(t);
						m_size.fetch_add(1);
						return;
					}
				}
			}

			// Failing all of that, including whether we should keep running, drop the object (delete)
			delete t;
		}

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

				// Delete the object if our shelves are full
				if (m_size.load() >= m_maxInventory)
				{
					delete t;
					continue;
				}

				// Clean it
				t->clean();

				// Add the object to the right pool
				std::wstring initializer = t->initializer();
				std::unique_lock<std::mutex> lock(m_bucketMutex);
				if (initializer.empty())
					m_unBucket.push_back(t);
				else
					m_initBuckets[initializer].push_back(t);
				m_size.fetch_add(1);
			}

			// All done.
			m_doneCleaning = true;
		}

	public:
		/// <summary>
		/// use is a RAII class for managing the lifetime of access to a pooled object
		/// </summary>
		template <class T>
		class reuse
		{
		public:
			/// <summary>
			/// Declaring an instance of this class gets a T object from the pool
			/// When this reuse object goes out of scape, the T object is returned to the pool
			/// </summary>
			/// <param name="pool">pool to get objects from and put objects back</param>
			/// <param name="initializer">string to initialize the object</param>
			reuse(pool<T>& pool, const std::wstring& initializer = L"")
				: m_pool(pool)
			{
				m_t = m_pool.get(initializer);
			}

			/// <summary>
			/// Move constructor to carry along the object pointer
			/// without an unnecessary get() / put() pair of pool calls 
			/// </summary>
			reuse(reuse&& other)
				: m_pool(other.m_pool)
				, m_t(other.m_t)
			{
				other.m_t = nullptr;
			}

			// Free the object back to the pool
			~reuse()
			{
				m_pool.put(m_t);
			}

			/// <summary>
			/// Access the pooled object
			/// </summary>
			T& get() { return *m_t; }

		private:
			pool<T>& m_pool;
			T* m_t;
		};

	private:
		const std::function<T* (const std::wstring&)> m_constructor;

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
