# **Specification: Alpha Phase 4 / Beta Phase 0 Implementation Plan**

**Status:** READY FOR IMPLEMENTATION **Dependencies:** Alpha 3 (Network Layer) ✅ 100% Complete **Goal:** Transition ScratchBird from a development codebase to a distributable, installable product. **Output:** Installable packages (Windows/Linux), updated tools, and end-user documentation.

---

## **1. Cross-Compilation & Packaging Specification**

**Objective:** Enable the Ubuntu host to build native Windows binaries and generate all installer formats via CMake and CPack.

### **1.1 Build Environment (Ubuntu Host)**

Configure the build system for dual-target compilation (Linux Native + Windows Cross-Compile via MinGW-w64).

- **Compiler Standard:** MinGW-w64 (POSIX threading variant).

- **Target Architectures:** `x86_64-linux-gnu` and `x86_64-w64-mingw32`.

- **CMake Toolchain File:** Create `cmake/Toolchain-MinGW-w64.cmake`:
  
  - System Name: `Windows`
  
  - Compilers: `x86_64-w64-mingw32-gcc`, `x86_64-w64-mingw32-g++`
  
  - Linker Flags: `-static-libgcc -static-libstdc++ -static -lpthread` (Ensure standalone `.exe` with no DLL dependencies).

### **1.2 CPack Configuration**

Update root `CMakeLists.txt` to include CPack rules for generating artifacts.

- **Metadata:**
  
  - Name: `scratchbird`
  
  - Version: `0.9.0-beta0`
  
  - Vendor: `[Your Name/Entity]`
  
  - License: `LICENSE` (MIT)

- **Generators & Layouts:**
  
  1. **DEB (Linux):** Install to `/opt/scratchbird/bin`, config to `/opt/scratchbird/conf`.
  
  2. **RPM (Linux):** Same paths as DEB.
  
  3. **TGZ (Linux Portable):** Flat structure.
  
  4. **NSIS (Windows .exe):**
     
     - Install to `C:\Program Files\ScratchBird`.
     
     - Config to `C:\ProgramData\ScratchBird`.
     
     - Includes Start Menu shortcuts and Uninstaller.
     
     - **Installer UI:** Integrate specific "Welcome," "Disclaimer," and "Design Goals" text (see Section 7).
  
  5. **ZIP (Windows Portable):** Flat structure containing static `.exe` files and default `.conf`.
  
  6. **AppImage:** Specific generator for the GUI tool (ScratchRobin).
  
  7. **Docker:** Create official `Dockerfile` and `docker-compose.yml`.

### **1.3 Build Profiles**

Modify `CMakeLists.txt` to support four distinct build configurations (selectable via `-DBUILD_PROFILE=...`):

1. **Linux Release:** `-O3`, stripped symbols.

2. **Linux Debug:** `-g`, assertions enabled, symbols retained.

3. **Windows Release:** Optimized, statically linked.

4. **Windows Debug:** Debug symbols (PDBs where possible/DWARF), statically linked.

---

## **2. Client Library API**

**Objective:** Formalize the C API for network communication.

- **Task:** Create `/docs/specifications/CLIENT_LIBRARY_API_SPECIFICATION.md`.

- **Content Scope:**
  
  - Connection functions: `sb_connect(const char* conn_str, ...)`
  
  - Execution functions: `sb_execute`, `sb_prepare`, `sb_fetch`
  
  - Type Mapping: C types <-> SBLR types.
  
  - Error Handling: SQLSTATE mapping.
  
  - **Connection String Standard:** Define syntax for both network (`sb://user:pass@host:port/db`) and embedded (`embedded:/path/to/db.sb`) modes.

---

## **3. Administration Client (sb_admin)**

**Objective:** Specification for the master admin CLI.

- **Task:** Create `/docs/specifications/SB_ADMIN_CLI_SPECIFICATION.md`.

- **Command Scope:**
  
  - Server Control: `shutdown`, `restart`, `reload_config`.
  
  - Monitoring: `show connections`, `show pool_status`, `show stats`.
  
  - Session Control: `kill <session_id>`.
  
  - User Management: `create user`, `alter user`, `grant`.

---

## **4. Tool Update: Wire Protocol Adoption**

**Objective:** Update existing tools to use `libscratchbird_client` instead of direct engine linking.

- **Targets:** `sb_isql`, `sb_verify`, `sb_backup`, `sb_security`.

- **Requirement:**
  
  - Refactor build linking to use the Client Library (Step 2).
  
  - Implement dual-mode support:
    
    - **Default:** Network mode (connects to `localhost:3092`).
    
    - **Override:** Embedded mode (connects via direct file access if connection string specifies `embedded:`).
  
  - Add CLI flags: `-H <host>`, `-P <port>`, `-u <user>`, `-p <password>`.

---

## **5. ScratchRobin (GUI Tool)**

**Objective:** A visual management tool based on FlameRobin.

- **Task 5a:** Create specification `/docs/specifications/FlameRobin_Specification_for_AI.md`.
  
  - Define Rebranding: Change Name/Logo/About box.
  
  - Define Driver Swap: Replace `fbclient` with `libscratchbird_client`.
  
  - Define UI Updates: Add support for displaying ScratchBird's 11 index types and new security roles.

- **Task 5b:** Packaging.
  
  - Linux: Build as **AppImage**.
  
  - Windows: Include as an optional component in the NSIS installer.

---

## **6. End-User Documentation Suite**

**Objective:** Create the structured user guide.

- **Root Directory:** `/docs/end-user/`

- **Structure:**
  
  - `installation/` (Guides for Win/Linux/Docker)
  
  - `configuration/` (`sb_server.conf` reference)
  
  - `admin/` (Security, Backup, Restore)
  
  - `language-guide/` (The SQL Reference)
    
    - `ddl/` (Index file + `create_table.md`, etc.)
    
    - `dml/` (Index file + `select.md`, etc.)
    
    - `psql/` (Index file + `procedures.md`, etc.)
    
    - `functions/` (Index file + `json_functions.md`, etc.)
  
  - `faq/`

- **Indexing Logic:**
  
  - Create a `LANGUAGE-REFERENCE.md` master index.
  
  - Each sub-directory has a local `index.md`.
  
  - Every file includes navigation: `[Back to Index]`.

---

## **7. Wiki Automation**

**Objective:** Sync documentation to GitHub Wiki.

- **Task:** Create script `scripts/publish_wiki.sh`.

- **Logic:**
  
  - Clone the wiki repo (`scratchbird.wiki.git`).
  
  - Copy contents of `/docs/end-user/*` to wiki root.
  
  - Commit and push changes.

---

## **8. Installer Assets (Text Copy)**

**Objective:** Define text for NSIS installer screens and `README_INSTALL.txt`.

- **Assets to Create:**
  
  - `installer/welcome.txt`: "Beta 0 Warning" (Do not use for production, data formats may change).
  
  - `installer/goals.txt`: "Philosophy" (Universal Polyglot, MGA, Modern C++).
  
  - `installer/license.txt`: MIT License + Data Safety Disclaimer.
  
  - `installer/feedback.txt`: Links to GitHub Issues.

---

## **9. Beta 1 "Early Access" Package Generation**

**Objective:** Generate final artifacts for distribution.

- **Action:** Execute full build and package cycle.

- **Required Artifacts:**
  
  1. `scratchbird_0.9.0-beta0_amd64.deb`
  
  2. `scratchbird-0.9.0-beta0-1.x86_64.rpm`
  
  3. `scratchbird-0.9.0-beta0-linux.tar.gz`
  
  4. `scratchbird-0.9.0-beta0-win64-setup.exe` (NSIS)
  
  5. `scratchbird-0.9.0-beta0-win64.zip`
  
  6. `ScratchRobin-0.9.0-x86_64.AppImage`
  
  7. `scratchbird-docker.tar` (or Dockerfile)

# INSTALLATION SPECIFICATIONS WITH INSTALL NOTIFICATION TEXT

### **1. Review of Your Steps: Gaps and Clarifications**

Your instructions are generally clear, but I have identified a few gaps and technical recommendations to ensure this phase goes smoothly.

- **Refinement on Step 1 (Windows Cross-Compile):**
  
  - **Gap:** You need to specify *which* cross-compiler.
  
  - **Recommendation:** Explicitly state the use of **MinGW-w64** (specifically the POSIX threading variant) and **CMake toolchains**. This is the standard for compiling Windows C++ binaries on Linux. You will likely need to statically link the standard libraries (`-static-libgcc -static-libstdc++`) so users don't need to install MinGW DLLs on Windows.

- **Refinement on Step 4 (Client Tools Update):**
  
  - **Critical Detail:** Currently, `sb_isql` likely links directly to the engine or uses a mock. Ensure this step explicitly requires linking against the new `libscratchbird_client` to validate the wire protocol "dogfooding."

- **Refinement on Step 5 (ScratchRobin GUI):**
  
  - **Scope Warning:** Forking and rebranding FlameRobin (a complex wxWidgets C++ application) is a massive task.
  
  - **Recommendation:** For "Beta 0," considering you already support the Firebird wire protocol, I recommend a "Phase 5a": **Verify existing FlameRobin compatibility.** If the standard FlameRobin connects to ScratchBird (thinking it's Firebird), that is a huge win for Beta 0. You can build the custom "ScratchRobin" branding in parallel without blocking the release.

- **Refinement on Step 9 (Beta 1 Package):**
  
  - **Missing Item:** **CI/CD Pipeline.** You shouldn't be building these packages manually on your laptop. You need a GitHub Action (or similar) that triggers on a tag, runs the build, runs the 1,255 parser tests, and generates the artifacts.

- **Additional Missing Item: Default Configuration:**
  
  - You need a step to create a default `sb_server.conf` that is OS-aware (e.g., sticking data in `/var/lib/scratchbird` on Linux vs `C:\ProgramData\ScratchBird` on Windows).

---

### **2. Installation Tool Recommendations**

Since you are one developer acting as a release engineer, you need tools that automate 90% of the work.

#### **A. The "Silver Bullet": CPack**

Since your project is likely using CMake (standard for modern C++), you should use **CPack**. It is built into CMake and can generate all the following formats from a single configuration file.

- **Linux (Debian/Ubuntu):** CPack can generate **.DEB** files.
  
  - *Why:* It handles dependencies and puts files in standard paths (`/usr/bin`, `/etc`).

- **Linux (RedHat/Fedora):** CPack can generate **.RPM** files.

- **Windows:** CPack can generate **NSIS (Nullsoft Scriptable Install System)** installers.
  
  - *Why:* NSIS is lightweight, scriptable, and produces a single `.exe` installer. It’s perfect for C++ projects.

#### **B. The "Dev Standard": Docker**

- **Recommendation:** Provide an official `Dockerfile` and `docker-compose.yml`.

- *Why:* 80% of your technical beta testers (backend devs) won't want to install an `.exe` or `.deb`. They will want to run `docker-compose up`. This is the fastest way to get adoption.

#### **C. The "Portable" Option**

- **Windows:** A simple `.zip` file containing the static binaries and the `.conf` file. Many devs prefer "unzip and run" over running an installer.

- **Linux:** A `.tar.gz` or an **AppImage** (specifically for the GUI tool, ScratchRobin).

---

### **3. Installer Introduction & Legal Texts**

Here is the copy for your installer screens (NSIS/Windows) or the `README_INSTALL.txt` for Linux.

#### **Screen 1: Welcome / The "Beta 0" Warning**

**Title:** Welcome to the ScratchBird Database Engine (Beta 0)

**Body:**

> Welcome to the bleeding edge.
> 
> You are about to install **ScratchBird (v0.9.0 - Alpha 4 / Beta 0)**.
> 
> **What this is:** This is a "Universal Database Engine" built from scratch using Multi-Generational Architecture (MGA). It speaks PostgreSQL, MySQL, Firebird, and Native SBLR protocols.
> 
> **Current State:** This is a **Developer Preview**. While the core engine passes 100% of its 3,000+ regression tests, the network layer and client tools are in active stabilization.
> 
> **⚠️ WARNING:**
> 
> - **Do not use this for production data.**
> 
> - **Do not use this for critical financial or medical records.**
> 
> - Data formats may change between Beta versions (breaking compatibility).
> 
> By clicking "Next," you acknowledge that you are an intrepid explorer willing to encounter dragons.

---

#### **Screen 2: Design Goals (The "Why")**

**Title:** The Philosophy of ScratchBird

**Body:**

> ScratchBird was built to solve a specific problem: Database fragmentation.
> 
> 1. **Universal Polyglot:** We believe the engine shouldn't care which client you use. Connect with `psql`, `mysql`, or legacy Firebird tools—it all compiles to the same ScratchBird Bytecode (SBLR).
> 
> 2. **Pure MGA:** We believe readers should never block writers, and writers should never block readers. We achieve this through a clean-room implementation of Multi-Generational Architecture.
> 
> 3. **Modern Power:** We are built on modern C++, utilizing AI-assisted specifications to deliver 11 index types, 86 data types, and enterprise-grade security in a lightweight footprint.

---

#### **Screen 3: License & Legal Disclaimer**

*(Note: You need to decide on a license, e.g., MIT, Apache 2.0, or GPL. Assuming MIT for now).*

**Title:** License Agreement

**Body:**

> **Copyright (c) 2025 Dalton Calford**
> 
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
> 
> IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
> 
> **Data Safety Warning:** This software is currently in **BETA**. It includes complex transaction management and file I/O operations. While extensive automated testing has been performed, unforeseen bugs may cause data corruption or loss. You assume all risks associated with the use of this software.

---

#### **Screen 4: Feedback & Contribution**

**Title:** Help Us Build the Future

**Body:**

> You aren't just a user; you are a tester.
> 
> If (and when) you find a bug, or if the engine behaves unexpectedly, please report it. Your logs and reproduction steps are the fuel that will get us to Version 1.0.
> 
> **Where to report bugs:** [Link to GitHub Issues]
> 
> **Where to read the docs:** [Link to Documentation Site]
> 
> Thank you for flying with ScratchBird.
