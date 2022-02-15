#pragma once

#include "reuse.h"

namespace reuse
{
	template <class T>
	class use
	{
	public:
		use(reuse_manager<T>& mgr, const wchar_t* initializer = nullptr)
			: m_mgr(mgr)
			, m_t(nullptr)
		{
			m_t = m_mgr.get(initializer);
		}

		~use()
		{
			m_mgr.put(m_t);
		}

		T& get() { return *m_t; }

	private:
		reuse_manager<T>& m_mgr;
		T* m_t;
	};
}
