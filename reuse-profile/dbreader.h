#pragma once

#include "dbcore.h"

#include "../../sqlite/sqlite3.h"

#include <string>

namespace fourdb
{
    class dbreader
    {
    public:
        dbreader(sqlite3* db, const std::string& sql);
        ~dbreader();

        bool read();

        unsigned getColCount();
        std::string getColName(unsigned idx);
        std::string getString(unsigned idx);
        bool isNull(unsigned idx);

    private:
        sqlite3* m_db;
        sqlite3_stmt* m_stmt;
        bool m_doneReading;
    };
}
