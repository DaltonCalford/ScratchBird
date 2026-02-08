# Windows Native Build Requirements

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Platform:** Windows
**Target:** Windows (x86_64)
**Document Version:** 1.0
**Last Updated:** 2026-01-02

---

## 1. Overview

This document specifies all requirements for building ScratchBird natively on Windows systems using Visual Studio and the MSVC compiler.

---

## 2. System Requirements

### 2.1 Minimum System Requirements

| Component | Requirement |
|-----------|-------------|
| **OS** | Windows 10 version 1909 or later / Windows 11 |
| **Architecture** | x64 (64-bit) |
| **RAM** | 16 GB (32 GB recommended for parallel builds) |
| **Disk Space** | 20 GB free (Visual Studio + source + build artifacts + dependencies) |
| **CPU** | 4 cores minimum (8+ recommended for faster builds) |

### 2.2 Supported Windows Versions

**Tier 1 (Fully Supported)**:
- Windows 11 (22H2 or later)
- Windows 10 (version 21H2 or later)

**Tier 2 (Community Supported)**:
- Windows Server 2019
- Windows Server 2022

---

## 3. Build Tools

### 3.1 Visual Studio

**Visual Studio 2022** (Recommended):
- **Edition:** Community (free), Professional, or Enterprise
- **Version:** 17.4 or later
- **Download:** https://visualstudio.microsoft.com/downloads/

**Visual Studio 2019** (Supported):
- **Edition:** Community (free), Professional, or Enterprise
- **Version:** 16.11 or later

**Required Workloads:**
- Desktop development with C++

**Required Components:**
- MSVC v143 - VS 2022 C++ x64/x86 build tools (or v142 for VS 2019)
- Windows 10 SDK (10.0.19041.0 or later)
- C++ CMake tools for Windows
- C++ Clang tools for Windows (optional, for Clang/LLVM)

### 3.2 Build System

**CMake**:
- **Minimum Version:** 3.20
- **Recommended Version:** 3.25 or later
- **Download:** https://cmake.org/download/ OR install via Visual Studio

**Visual Studio Generator**:
- Visual Studio 17 2022 (default for VS 2022)
- Visual Studio 16 2019 (for VS 2019)

**Alternative: Ninja** (Optional, for faster builds):
- **Version:** 1.10+
- **Install:** Via chocolatey or manual download

### 3.3 Git for Windows

**Git**:
- **Version:** 2.30 or later
- **Download:** https://git-scm.com/download/win
- **Purpose:** Source code management

---

## 4. Dependency Management

### 4.1 vcpkg (Recommended)

**vcpkg** is the recommended package manager for C++ dependencies on Windows.

**Installation:**
```powershell
# Clone vcpkg
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg

# Bootstrap vcpkg
.\bootstrap-vcpkg.bat

# Integrate with Visual Studio
.\vcpkg integrate install
```

**Add vcpkg to PATH:**
```powershell
# Add to system environment variables
setx PATH "%PATH%;C:\vcpkg"
```

### 4.2 Alternative: Manual Installation

Dependencies can also be installed manually by downloading pre-built binaries or building from source. See section 7 for details.

---

## 5. Core Dependencies

### 5.1 Required Libraries

| Library | Min Version | Purpose |
|---------|-------------|---------|
| **spdlog** | 1.10.0+ | Logging framework |
| **GoogleTest** | 1.14.0+ | Unit testing framework |
| **OpenSSL** | 1.1.1+ or 3.0+ | TLS, encryption, hashing |
| **LZ4** | 1.9.3+ | Fast compression |
| **zlib** | 1.2.11+ | General compression |

### 5.2 Windows-Specific Notes

- **Threading:** Windows threading primitives are used instead of pthread
- **DLL vs Static:** vcpkg can build libraries as static or dynamic (DLL)
- **Runtime Library:** Must match (MT, MD, MTd, MDd)

---

## 6. Optional Dependencies

### 6.1 Spatial Support (Optional)

| Library | Min Version | Purpose |
|---------|-------------|---------|
| **GEOS** | 3.10+ | Geometry operations |
| **PROJ** | 9.0+ | Geographic projections |

### 6.2 XML Support (Optional)

| Library | Min Version | Purpose |
|---------|-------------|---------|
| **libxml2** | 2.10+ | XML parsing and manipulation |

---

## 7. Installation Instructions

### 7.1 Install Visual Studio 2022

1. Download Visual Studio 2022 Installer
2. Run installer
3. Select **"Desktop development with C++"** workload
4. Under "Individual components", ensure these are selected:
   - MSVC v143 - VS 2022 C++ x64/x86 build tools
   - Windows 10 SDK (latest version)
   - C++ CMake tools for Windows
   - Git for Windows
5. Click "Install"

**Installation Size:** ~7-10 GB

### 7.2 Install CMake (if not included with VS)

```powershell
# Option 1: Download installer from cmake.org
# https://cmake.org/download/

# Option 2: Install via chocolatey
choco install cmake --install-arguments=ADD_CMAKE_TO_PATH=System

# Verify installation
cmake --version  # Should show 3.20 or later
```

### 7.3 Install Dependencies via vcpkg

```powershell
# Navigate to vcpkg directory
cd C:\vcpkg

# Install core dependencies (x64 architecture)
.\vcpkg install spdlog:x64-windows
.\vcpkg install gtest:x64-windows
.\vcpkg install openssl:x64-windows
.\vcpkg install lz4:x64-windows
.\vcpkg install zlib:x64-windows

# Install optional dependencies
.\vcpkg install geos:x64-windows
.\vcpkg install proj:x64-windows
.\vcpkg install libxml2:x64-windows

# Static linking (for static libraries)
.\vcpkg install spdlog:x64-windows-static
.\vcpkg install gtest:x64-windows-static
.\vcpkg install openssl:x64-windows-static
.\vcpkg install lz4:x64-windows-static
.\vcpkg install zlib:x64-windows-static
```

**Note:** `:x64-windows` builds DLLs, `:x64-windows-static` builds static libraries

---

## 8. Verification Steps

### 8.1 Verify Visual Studio Installation

```powershell
# Check MSVC compiler
"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\<version>\bin\Hostx64\x64\cl.exe"

# Should display Microsoft C/C++ Optimizing Compiler version
```

### 8.2 Verify CMake

```powershell
cmake --version  # Should show 3.20 or later
```

### 8.3 Verify Git

```powershell
git --version  # Should show 2.30 or later
```

### 8.4 Verify vcpkg Integration

```powershell
# Check vcpkg is integrated
.\vcpkg integrate install

# List installed packages
.\vcpkg list
```

### 8.5 Verify Dependencies

```powershell
# Check spdlog
.\vcpkg list spdlog

# Check OpenSSL
.\vcpkg list openssl

# Should show installed versions
```

---

## 9. CMake Configuration

### 9.1 Basic Configuration (Visual Studio Generator)

```powershell
# Clone repository
git clone https://github.com/yourusername/ScratchBird.git
cd ScratchBird

# Create build directory
mkdir build
cd build

# Configure with CMake (Visual Studio 2022, x64)
cmake -G "Visual Studio 17 2022" -A x64 `
      -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake `
      -DCMAKE_BUILD_TYPE=Release `
      ..
```

### 9.2 Configuration with Ninja (Faster Builds)

```powershell
# Open "x64 Native Tools Command Prompt for VS 2022"
# Or use vcvarsall.bat to set up environment

cmake -G Ninja `
      -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake `
      -DCMAKE_BUILD_TYPE=Release `
      -DCMAKE_C_COMPILER=cl `
      -DCMAKE_CXX_COMPILER=cl `
      ..
```

### 9.3 Debug Configuration

```powershell
cmake -G "Visual Studio 17 2022" -A x64 `
      -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake `
      -DCMAKE_BUILD_TYPE=Debug `
      ..
```

### 9.4 Configuration with Optional Features

```powershell
cmake -G "Visual Studio 17 2022" -A x64 `
      -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake `
      -DCMAKE_BUILD_TYPE=Release `
      -DENABLE_SPATIAL_SUPPORT=ON `
      -DENABLE_XML_SUPPORT=ON `
      -DENABLE_TESTING=ON `
      ..
```

### 9.5 Static Linking Configuration

```powershell
cmake -G "Visual Studio 17 2022" -A x64 `
      -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake `
      -DVCPKG_TARGET_TRIPLET=x64-windows-static `
      -DCMAKE_BUILD_TYPE=Release `
      ..
```

---

## 10. Build Commands

### 10.1 Build with Visual Studio (MSBuild)

```powershell
# From build directory

# Build Release configuration
cmake --build . --config Release

# Build Debug configuration
cmake --build . --config Debug

# Parallel build (use all CPU cores)
cmake --build . --config Release -- /m
```

### 10.2 Build with Ninja

```powershell
# From build directory (must use x64 Native Tools Command Prompt)
ninja
```

### 10.3 Build Specific Target

```powershell
# Build only the core library
cmake --build . --config Release --target scratchbird_core

# Build only tests
cmake --build . --config Release --target tests
```

### 10.4 Build from Visual Studio IDE

1. Open `ScratchBird.sln` in Visual Studio
2. Select configuration: **Release** or **Debug**
3. Select platform: **x64**
4. Build → Build Solution (Ctrl+Shift+B)

---

## 11. Testing

### 11.1 Run All Tests

```powershell
# From build directory
ctest -C Release --output-on-failure
```

### 11.2 Run Tests in Parallel

```powershell
ctest -C Release -j8 --output-on-failure  # 8 parallel jobs
```

### 11.3 Run Specific Test Suite

```powershell
ctest -C Release -R unit_tests --output-on-failure
```

### 11.4 Run Tests from Visual Studio

1. Test → Run All Tests
2. Or right-click test in Test Explorer → Run

---

## 12. Installation

### 12.1 System-Wide Installation

```powershell
# From build directory (run PowerShell as Administrator)
cmake --build . --config Release --target install
```

### 12.2 Custom Installation Prefix

```powershell
# Configure with custom prefix
cmake -G "Visual Studio 17 2022" -A x64 `
      -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake `
      -DCMAKE_INSTALL_PREFIX=C:\ScratchBird `
      ..

# Build and install
cmake --build . --config Release
cmake --build . --config Release --target install
```

---

## 13. Troubleshooting

### 13.1 Common Issues

**Issue: CMake cannot find vcpkg libraries**
```powershell
# Solution: Ensure CMAKE_TOOLCHAIN_FILE is set
cmake -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake ..
```

**Issue: Visual Studio version not found**
```powershell
# Solution: Specify correct generator
cmake -G "Visual Studio 17 2022" ..  # For VS 2022
cmake -G "Visual Studio 16 2019" ..  # For VS 2019
```

**Issue: Runtime library mismatch**
```powershell
# Ensure all libraries use the same runtime
# Use static triplet for static linking
.\vcpkg install spdlog:x64-windows-static
```

**Issue: Missing Windows SDK**
```powershell
# Install Windows SDK via Visual Studio Installer
# Modify installation → Individual components → Windows 10 SDK
```

### 13.2 DLL Not Found at Runtime

```powershell
# Option 1: Copy DLLs to executable directory
copy C:\vcpkg\installed\x64-windows\bin\*.dll .\build\Release\

# Option 2: Add vcpkg bin to PATH
setx PATH "%PATH%;C:\vcpkg\installed\x64-windows\bin"

# Option 3: Use static linking (recommended)
.\vcpkg install <package>:x64-windows-static
```

### 13.3 Clean Build

```powershell
# Delete build directory
rmdir /s /q build
mkdir build
cd build
cmake ..
```

---

## 14. Package Creation

### 14.1 Create MSI Installer

Requires **WiX Toolset** v3.11+:

```powershell
# Install WiX
# Download from https://wixtoolset.org/

# Configure CMake with CPack
cmake -DCPACK_GENERATOR=WIX ..

# Build and create installer
cmake --build . --config Release
cpack -C Release
```

### 14.2 Create ZIP Package

```powershell
# Configure CMake
cmake -DCPACK_GENERATOR=ZIP ..

# Build and create package
cmake --build . --config Release
cpack -C Release
```

---

## 15. Development Setup

### 15.1 Install Development Tools

**Visual Studio Extensions:**
- Visual Assist (optional, code navigation)
- ReSharper C++ (optional, refactoring)
- ClangFormat (code formatting)

**External Tools:**
```powershell
# Install via chocolatey
choco install doxygen.install
choco install graphviz
choco install cppcheck
```

### 15.2 Enable Sanitizers (Clang on Windows)

```powershell
# Requires Clang/LLVM for Windows
# Install via Visual Studio Installer: "C++ Clang tools for Windows"

cmake -G Ninja `
      -DCMAKE_C_COMPILER=clang-cl `
      -DCMAKE_CXX_COMPILER=clang-cl `
      -DENABLE_SANITIZERS=ON `
      ..
```

---

## 16. Performance Optimization

### 16.1 Link-Time Code Generation (LTCG)

```powershell
cmake -G "Visual Studio 17 2022" `
      -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON `
      ..
```

### 16.2 Profile-Guided Optimization (PGO)

```powershell
# 1. Build with instrumentation
cmake --build . --config Release -- /p:WholeProgramOptimization=PGInstrument

# 2. Run representative workload
.\Release\scratchbird_tests.exe

# 3. Rebuild with profile data
cmake --build . --config Release -- /p:WholeProgramOptimization=PGOptimize
```

---

## 17. Continuous Integration Setup

### 17.1 GitHub Actions Example

```yaml
# .github/workflows/windows-build.yml
jobs:
  build:
    runs-on: windows-2022
    steps:
      - uses: actions/checkout@v4

      - name: Setup vcpkg
        run: |
          git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
          cd C:\vcpkg
          .\bootstrap-vcpkg.bat
          .\vcpkg integrate install

      - name: Install dependencies
        run: |
          C:\vcpkg\vcpkg install spdlog:x64-windows gtest:x64-windows openssl:x64-windows lz4:x64-windows

      - name: Configure
        run: |
          cmake -G "Visual Studio 17 2022" -A x64 `
                -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake `
                -B build

      - name: Build
        run: cmake --build build --config Release

      - name: Test
        run: ctest --test-dir build -C Release --output-on-failure
```

---

## 18. Version Matrix

Tested and supported combinations:

| Windows Version | Visual Studio | CMake | Status |
|-----------------|---------------|-------|--------|
| Windows 11 22H2 | VS 2022 17.8 | 3.27 | ✅ Tested |
| Windows 10 21H2 | VS 2022 17.4 | 3.25 | ✅ Tested |
| Windows 10 21H2 | VS 2019 16.11 | 3.22 | ✅ Tested |
| Windows Server 2022 | VS 2022 17.4 | 3.25 | ○ Community |

---

## 19. Additional Resources

- **Visual Studio Documentation:** https://docs.microsoft.com/en-us/visualstudio/
- **vcpkg Documentation:** https://vcpkg.io/en/docs/README.html
- **CMake Windows Guide:** https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html#visual-studio-generators
- **MSVC Compiler Options:** https://docs.microsoft.com/en-us/cpp/build/reference/compiler-options

---

**Document Version:** 1.0
**Last Updated:** 2026-01-02
**Maintainer:** Build Infrastructure Team
**Status:** Beta Preparation
