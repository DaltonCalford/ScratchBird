#include "scratchbird/capi.h"

#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

namespace sbcli
{
    int createdb(const std::string& path)
    {
        SB_CreateDbOptions opts{};
        SB_Database* db = nullptr;
        SB_Status st = sb_create_database(path.c_str(), &opts, &db);
        if (st.code != SB_STATUS_OK) {
            std::cerr << "{\"ok\":false,\"error\":\"" << (st.message ? st.message : "unknown") << "\"}" << std::endl;
            return 1;
        }
        sb_close_database(db);
        std::cout << "{\"ok\":true,\"database\":\"" << path << "\"}" << std::endl;
        return 0;
    }

    int dropdb(const std::string& path)
    {
        // For embedded/simple provider, dropping is unlink; simulate for now
        if (std::remove(path.c_str()) != 0) {
            std::cerr << "{\"ok\":false,\"error\":\"drop failed\"}" << std::endl;
            return 1;
        }
        std::cout << "{\"ok\":true,\"database\":\"" << path << "\"}" << std::endl;
        return 0;
    }
}

