#include "db.h"

#include <stdexcept>

namespace fourdb
{
    db::db(const std::wstring& filePath)
        : m_db(nullptr)
    {
        // Hack
        std::string filePathA;
        for (auto c : filePath)
            filePathA += (char)c;

        int rc = sqlite3_open(filePathA.c_str(), &m_db);
        if (rc != SQLITE_OK)
            throw fourdberr(rc, m_db);
    }

    db::~db()
    {
        if (m_db != nullptr)
        {
            sqlite3_close(m_db);
        }
    }

    std::shared_ptr<dbreader> db::exec(const std::string& sql)
    {
        auto reader = std::make_shared<dbreader>(m_db, sql);
        return reader;
    }
}
