#pragma once

#include <string>

namespace reuse
{
	/// <summary>
	/// Implement reusable for the types you want to pool
	/// This may appear polymorphic, but it is more of a contract than a base class
	/// The pool machinery relies on the presence of the member functions of this class
	/// All objects in a pool must have the same concrete type
	/// </summary>
	class reusable
	{
	protected: // this class cannot be instantiated
		/// <summary>
		/// The initializer string, like a database connection string, is used to pool objects
		/// of the same type but different initializers, allowing for different connection strings
		/// Use of initializer strings is optional
		/// The object pool guarantees that you only pay for what you use
		/// </summary>
		/// <param name="initializer">string passed to constructor of T by pool.get()</param>
		reusable(const std::wstring& initializer = L"")
			: m_initializer(initializer)
		{}
	private: // no really...
		reusable(const reusable&) = delete;
		reusable(reusable &&) = delete;
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
		const std::wstring& initializer() const { return m_initializer; }

	private:
		std::wstring m_initializer;
	};
}
