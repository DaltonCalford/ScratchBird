#include <iostream>
#include <string>

namespace sbcli { int createdb(const std::string& path); }

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "usage: scratchbird-createdb <database_path>" << std::endl;
        return 2;
    }
    return sbcli::createdb(argv[1]);
}

