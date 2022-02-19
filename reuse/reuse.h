#pragma once

#include "reuse_pool.h"

namespace reuse
{
	/// <summary>
	/// use is a RAII class for managing the lifetime of the use of a pooled object
	/// and its recycling for later reuse
	/// </summary>
	template <class T>
	class use
	{
	public:
		/// <summary>
		/// Declaring a use object allocates a T from the pool
		/// When the use object goes out of scape, the object is returned to the pool
		/// </summary>
		/// <param name="pool">pool to get objects from and put objects back</param>
		/// <param name="initializer">string to initialize the object</param>
		use(pool<T>& pool, const std::wstring& initializer = L"")
			: m_pool(pool)
			, m_t(nullptr)
		{
			m_t = m_pool.get(initializer);
		}

		// Free the object back to the pool
		~use()
		{
			m_pool.put(m_t);
			m_t = nullptr;
		}

		/// <summary>
		/// Access the pooled object
		/// </summary>
		T& get() { return *m_t; }

	private:
		pool<T>& m_pool;
		T* m_t;
	};
}
