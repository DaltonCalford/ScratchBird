# ScratchBird Database Engine

**Firebird-style MGA database engine** with multi-dialect wire compatibility and advanced distributed cluster capabilities.

**Current Phase:** ✅ **Alpha Complete and Beta Release in Preparation** - final code updates and test structures being implemented
**Project Started:** July 2025  
**Status:** All Alpha workstreams complete; 3,600+ tests passing (100%)

---

### **Note to new visitors**

ScratchBird Alpha is **complete and fully functional**. New beta functionality including new NoSQL support and cluster work being applied to main tree

If you are curious, clone the directories and have your friendly local AI analyze the code base - tell it to find out the capabilities of the project from the implemented source code. This will give you a good understanding of what is done.

The initial preview will be a Docker container with the database engine and an AppImage or standalone executable so that you can test the project without any problems of getting rid of it afterward.

This project has become my answer to the constant "Damn I wish I had the ability to...." issues I have encountered over 35 years of database use.

I have been seeing multiple clones of my project(s) via the tracker but I have not received any feedback yet - don't be afraid, I need feedback and I don't bite.

I am sure there are things others have encountered over the years and wish they had a tool to cover it.

Thanks for your interest in the project.

---

## Quick Overview

ScratchBird is a next-generation database management system that combines:

- **Firebird MGA Architecture** - Multi-Generational Architecture for true MVCC
- **Multi-Dialect Support** - ScratchBird native + Firebird/PostgreSQL/MySQL wire protocol compatibility (full protocol implementation)
- **Advanced Security** - Built-in encryption, masking, RLS/CLS, cryptographic audit chain, SCRAM-SHA-256/512 authentication
- **Distributed Ready** - Beta cluster specifications complete; implementation deferred to Beta
- **Modern C++** - High-performance C++17/20 implementation

---

## Related Projects

ScratchBird has been split into multiple repositories for parallel development:

| Repository                  | Description                                                                          | Link                                                          |
| --------------------------- | ------------------------------------------------------------------------------------ | ------------------------------------------------------------- |
| **ScratchBird** (this repo) | Core database engine - storage, transactions, SBLR runtime, parsers, network layer   | You are here                                                  |
| **ScratchBird-driver**      | Language drivers and CLI tools (ODBC/JDBC/Python/Node.js/Go/Rust, sb_admin, sb_isql) | [GitHub](https://github.com/DaltonCalford/ScratchBird-driver) |
| **ScratchRobin**            | GUI database management and administration tools                                     | [GitHub](https://github.com/DaltonCalford/ScratchRobin)       |

---

## Beta (Planned)

- Drivers and CLI tools have moved to [ScratchBird-driver](https://github.com/DaltonCalford/ScratchBird-driver). GUI tools are in [ScratchRobin](https://github.com/DaltonCalford/ScratchRobin).

---

## Quick Start

### Build from Source

```bash
# Prerequisites: C++17 compiler, CMake 3.15+, OpenSSL

# Clone repository
git clone https://github.com/DaltonCalford/ScratchBird.git
cd ScratchBird

# Build
cmake -S . -B build
cmake --build build -j$(nproc)

# Run tests
ctest --test-dir build --output-on-failure
```

### Run the Server

```bash
# Start ScratchBird server
./build/bin/sb_server

# Native: 3092 | PostgreSQL: 5432 | MySQL: 3306 | Firebird: 3050
```

### Test Server (for Development)

For driver development, GUI testing, and security validation:

```bash
# Setup test server
./scripts/test-server-user.sh setup

# Start test server
./scripts/test-server-user.sh start

# Connect (bootstrap mode - any user/pass)
scratchbird://anyuser:anypass@127.0.0.1:3092/testdb

# View status
./scripts/test-server-user.sh status
```

**Documentation:**

- [Test Server Docs](docs/testing/test_server/README.md)
- [Test Server Quick Reference](docs/testing/test_server/TEST_SERVER_QUICK_REFERENCE.md)
- [Public Test Server Setup](docs/testing/test_server/PUBLIC_TEST_SERVER_SETUP.md)

---

## License

Licensed under the [Initial Developer's Public License Version 1.0 (IPL 1.0)](https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/).

---

- **Core Repository:** https://github.com/DaltonCalford/ScratchBird
- **Driver Repository:** https://github.com/DaltonCalford/ScratchBird-driver
- **GUI Tools:** https://github.com/DaltonCalford/ScratchRobin

**Last Updated:** February 6, 2026  
**Status:** ✅ Alpha Complete - 19,400+ lines, 84+ stubs implemented, 3,600+ tests passing  
**Next Milestone:** Pre-Beta integration testing and benchmarking
