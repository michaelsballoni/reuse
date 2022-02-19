#pragma once

#include "../../sqlite/sqlite3.h"

#include <stdexcept>
#include <string>

namespace fourdb
{
    class fourdberr : public std::runtime_error
    {
    public:
        fourdberr(const std::string& msg) : std::runtime_error(msg) {}
        fourdberr(int rc, sqlite3* db) : std::runtime_error(getExceptionMsg(rc, db)) {}

        static std::string getExceptionMsg(int rc, sqlite3* db)
        {
            std::string dbErrMsg;
            if (db != nullptr)
                dbErrMsg = sqlite3_errmsg(db);

            std::string retVal;
            if (dbErrMsg.empty())
                retVal = "SQLite error: " + std::to_string(rc);
            else
                retVal = "SQLite error: " + dbErrMsg + " (" + std::to_string(rc) + ")";
            return retVal;
        }
    };
}
