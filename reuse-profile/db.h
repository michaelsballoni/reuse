#pragma once

#include "dbreader.h"

#include "../../sqlite/sqlite3.h"

#include <memory>
#include <string>

namespace fourdb
{
    class db
	{
	public:
        db(const std::wstring& filePath);
        ~db();

        std::shared_ptr<dbreader> exec(const std::string& sql);

    private:
        sqlite3* m_db;
    };
}
