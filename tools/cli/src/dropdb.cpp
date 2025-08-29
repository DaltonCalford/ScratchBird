#include <iostream>
#include <string>

namespace sbcli { int dropdb(const std::string& path); }

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "usage: scratchbird-dropdb <database_path>" << std::endl;
        return 2;
    }
    return sbcli::dropdb(argv[1]);
}

