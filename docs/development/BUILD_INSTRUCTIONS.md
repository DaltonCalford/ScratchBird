# ScratchBird Build Instructions

## 1. Prerequisites

- **C++ Compiler:** A C++17 compliant compiler (e.g., GCC, Clang, MSVC).
- **CMake:** Version 3.15 or later.
- **Google Test:** The Google Test library is required for building the test suite. It is included as a submodule in the `third_party` directory.
- **lz4:** The lz4 library is an optional dependency for compression support.

## 2. Building on Linux

1.  **Clone the repository:**

    ```bash
    git clone https://github.com/DaltonCalford/ScratchBird.git
    cd ScratchBird
    ```

2.  **Initialize submodules:**

    ```bash
    git submodule update --init --recursive
    ```

3.  **Create a build directory:**

    ```bash
    mkdir build
    cd build
    ```

4.  **Run CMake:**

    ```bash
    cmake ..
    ```

5.  **Build the project:**

    ```bash
    make
    ```

## 3. Building on Windows

(Instructions to be added)

## 4. Building on macOS

(Instructions to be added)

## 5. CMake Options

- **`BUILD_TESTING`:** Build the test suite. (Default: `ON`)
- **`WITH_LZ4`:** Enable lz4 compression support. (Default: `ON` if lz4 is found)
