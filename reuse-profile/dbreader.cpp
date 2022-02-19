#include "dbreader.h"

namespace fourdb
{
    dbreader::dbreader(sqlite3* db, const std::string& sql)
        : m_db(db)
        , m_stmt(nullptr)
        , m_doneReading(false)
    {
        int rc = sqlite3_prepare_v3(m_db, sql.c_str(), -1, 0, &m_stmt, nullptr);
        if (rc != SQLITE_OK)
            throw fourdberr(rc, db);
    }

    dbreader::~dbreader()
    {
        sqlite3_finalize(m_stmt);
    }

    bool dbreader::read()
    {
        if (m_doneReading)
            return false;

        int rc = sqlite3_step(m_stmt);
        if (rc == SQLITE_ROW)
        {
            return true;
        }
        else if (rc == SQLITE_DONE)
        {
            m_doneReading = true;
            return false;
        }
        else
            throw fourdberr(rc, m_db);
    }

    unsigned dbreader::getColCount()
    {
        return static_cast<unsigned>(sqlite3_column_count(m_stmt));
    }

    std::string dbreader::getColName(unsigned idx)
    {
        return sqlite3_column_name(m_stmt, idx);
    }

    std::string dbreader::getString(unsigned idx)
    {
        auto str = sqlite3_column_text(m_stmt, idx);
        if (str != nullptr)
            return reinterpret_cast<const char*>(str);
        else
            return std::string();
    }

    bool dbreader::isNull(unsigned idx)
    {
        return sqlite3_column_type(m_stmt, idx) == SQLITE_NULL;
    }
}
