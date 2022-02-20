#include "../reuse/reuse.h"

#include "db.h"

#include <chrono>
#include <iostream>

// Define our SQLite (well, 4db) reusable wrapper
class sqlite_reuse : public reuse::reusable
{
public:
	sqlite_reuse(const std::wstring& filePath)
		: reuse::reusable(filePath)
		, m_db(filePath)
	{}

	fourdb::db& db() { return m_db; }

	// defaults to no-op clean and foreground (no-op) clean
	// and initializer() for free
private:
	fourdb::db m_db; // our resource-intense cargo
};

typedef std::chrono::high_resolution_clock high_resolution_clock;
typedef std::chrono::milliseconds milliseconds;

int wmain(int argc, wchar_t* argv[])
{
	if (argc < 2)
	{
		std::cout << "Usage: <db file path>" << std::endl;
		return 0;
	}

	std::wstring db_file_path = argv[1];

	size_t loopCount = 1000;

	std::string sql_query = "SELECT tbl_name FROM sqlite_master WHERE type = 'table'";

	for (int run = 1; run <= 10; ++run)
	{
		// Do the standard DB connect / query / close
		{
			std::cout << "Traditional: ";
			auto start = high_resolution_clock::now();
			for (size_t c = 1; c <= loopCount; ++c)
			{
				// This one-liner does it all
				fourdb::db(db_file_path).exec(sql_query);
			}
			auto elapsedMs = std::chrono::duration_cast<milliseconds>(high_resolution_clock::now() - start);
			std::cout << elapsedMs.count() << "ms" << std::endl;
		}

		// Pool the database connection and reuse it
		{
			std::cout << "Pooled: ";
			auto start = high_resolution_clock::now();
			{
				// Create the pool
				reuse::pool<sqlite_reuse>
					pool
					(
						[](const std::wstring& initializer)
						{
							return new sqlite_reuse(initializer);
						}
					);
				for (size_t c = 1; c <= loopCount; ++c)
				{
					// This even longer one-liner does it all
					pool.use(db_file_path).get().db().exec(sql_query);
				}
			}
			auto elapsedMs = std::chrono::duration_cast<milliseconds>(high_resolution_clock::now() - start);
			std::cout << elapsedMs.count() << "ms" << std::endl;
		}
	}

	return 0;
}
