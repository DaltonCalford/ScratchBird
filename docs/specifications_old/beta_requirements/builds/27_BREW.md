# Homebrew Formula Requirements

**Package Manager:** Homebrew
**Target Platform:** macOS (also Linux via Linuxbrew)
**Document Version:** 1.0
**Last Updated:** 2026-01-03

---

## 1. Overview

This document specifies all requirements for creating a Homebrew formula for ScratchBird. Homebrew is the de facto package manager for macOS and is also available on Linux (Homebrew on Linux, formerly Linuxbrew).

**Homebrew Advantages**:
- Standard macOS package manager (~80% of Mac developers use it)
- Automatic dependency resolution
- Simple installation (`brew install scratchbird`)
- Easy updates (`brew upgrade`)
- Supports multiple architectures (Intel x86_64 and Apple Silicon ARM64)
- Available on Linux as well

---

## 2. System Requirements

### 2.1 For Formula Development

| Component | Requirement |
|-----------|-------------|
| **OS** | macOS 11+ or Linux |
| **Homebrew** | Latest version |
| **Xcode** | 13+ (macOS) or Xcode Command Line Tools |
| **Architecture** | x86_64 or ARM64 |

### 2.2 Homebrew Installation

**macOS**:
```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

**Linux**:
```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

---

## 3. Homebrew Basics

### 3.1 Terminology

| Term | Meaning |
|------|---------|
| **Formula** | Package definition (Ruby DSL) |
| **Bottle** | Pre-compiled binary package |
| **Cellar** | Installation location (`/usr/local/Cellar` or `/opt/homebrew/Cellar`) |
| **Keg** | Specific version installation |
| **Tap** | Third-party repository |
| **Cask** | macOS application package (.app) |

### 3.2 Directory Structure

**macOS (Apple Silicon)**:
```
/opt/homebrew/           # Homebrew prefix
├── Cellar/              # Installed packages
│   └── scratchbird/
│       └── 0.1.0/       # Version-specific installation
├── bin/                 # Symlinks to executables
├── lib/                 # Symlinks to libraries
└── opt/                 # Symlinks to latest versions
```

**macOS (Intel) / Linux**:
```
/usr/local/              # Homebrew prefix
├── Cellar/
├── bin/
├── lib/
└── opt/
```

---

## 4. Creating a Formula

### 4.1 Basic Formula Template

**scratchbird.rb**:
```ruby
class Scratchbird < Formula
  desc "High-performance SQL database engine"
  homepage "https://github.com/yourusername/scratchbird"
  url "https://github.com/yourusername/scratchbird/archive/refs/tags/v0.1.0.tar.gz"
  sha256 "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890"
  license "MIT"
  head "https://github.com/yourusername/scratchbird.git", branch: "main"

  depends_on "cmake" => :build
  depends_on "ninja" => :build
  depends_on "spdlog"
  depends_on "googletest" => :build
  depends_on "openssl@3"
  depends_on "lz4"

  def install
    system "cmake", "-S", ".", "-B", "build",
           "-G", "Ninja",
           "-DCMAKE_BUILD_TYPE=Release",
           "-DENABLE_TESTING=OFF",
           *std_cmake_args
    system "cmake", "--build", "build"
    system "cmake", "--install", "build"
  end

  test do
    assert_match version.to_s, shell_output("#{bin}/scratchbird --version")
  end
end
```

### 4.2 Formula Components

**Metadata**:
```ruby
desc "Short description (80 chars max)"
homepage "https://project-website.com"
url "https://download-url/package-0.1.0.tar.gz"
sha256 "hash-of-tarball"
license "MIT"  # SPDX identifier
```

**Dependencies**:
```ruby
depends_on "cmake" => :build        # Build-time only
depends_on "openssl@3"              # Runtime dependency
depends_on "python@3.11" => :optional  # Optional dependency
depends_on :macos => :big_sur       # Minimum macOS version
```

**Build Instructions**:
```ruby
def install
  # Build and install commands
  system "cmake", "-B", "build", *std_cmake_args
  system "cmake", "--build", "build"
  system "cmake", "--install", "build"
end
```

**Test Block**:
```ruby
test do
  # Simple smoke test
  assert_match version.to_s, shell_output("#{bin}/scratchbird --version")
end
```

---

## 5. Advanced Formula Features

### 5.1 Multi-Architecture Support

**Universal Binary (Intel + Apple Silicon)**:
```ruby
def install
  # Build for both architectures
  ENV.universal_binary

  system "cmake", "-S", ".", "-B", "build",
         "-DCMAKE_OSX_ARCHITECTURES=x86_64;arm64",
         *std_cmake_args
  system "cmake", "--build", "build"
  system "cmake", "--install", "build"
end
```

**Architecture-Specific Builds**:
```ruby
def install
  if Hardware::CPU.arm?
    # Apple Silicon specific options
    args = %W[
      -DCMAKE_OSX_ARCHITECTURES=arm64
      -DENABLE_NEON=ON
    ]
  else
    # Intel specific options
    args = %W[
      -DCMAKE_OSX_ARCHITECTURES=x86_64
      -DENABLE_AVX2=ON
    ]
  end

  system "cmake", "-B", "build", *args, *std_cmake_args
  system "cmake", "--build", "build"
  system "cmake", "--install", "build"
end
```

### 5.2 Version-Specific Dependencies

```ruby
class Scratchbird < Formula
  desc "High-performance SQL database engine"
  # ...

  on_macos do
    depends_on "openssl@3"
  end

  on_linux do
    depends_on "openssl@1.1"
  end

  # macOS version specific
  on_monterey :or_newer do
    depends_on "llvm" => :build
  end
end
```

### 5.3 Service Management (launchd)

```ruby
def install
  # ... build commands ...

  # Install service plist
  (prefix/"homebrew.mxcl.scratchbird.plist").write plist
end

def plist
  <<~EOS
    <?xml version="1.0" encoding="UTF-8"?>
    <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
    <plist version="1.0">
    <dict>
      <key>Label</key>
      <string>#{plist_name}</string>
      <key>ProgramArguments</key>
      <array>
        <string>#{opt_bin}/scratchbird</string>
        <string>server</string>
      </array>
      <key>RunAtLoad</key>
      <true/>
      <key>KeepAlive</key>
      <true/>
      <key>WorkingDirectory</key>
      <string>#{var}/scratchbird</string>
      <key>StandardErrorPath</key>
      <string>#{var}/log/scratchbird.log</string>
      <key>StandardOutPath</key>
      <string>#{var}/log/scratchbird.log</string>
    </dict>
    </plist>
  EOS
end

service do
  run [opt_bin/"scratchbird", "server"]
  working_dir var/"scratchbird"
  log_path var/"log/scratchbird.log"
  error_log_path var/"log/scratchbird.log"
end
```

### 5.4 Post-Install Messages

```ruby
def caveats
  <<~EOS
    To start ScratchBird:
      brew services start scratchbird

    Or run manually:
      scratchbird server

    Configuration file:
      #{etc}/scratchbird/scratchbird.conf

    Data directory:
      #{var}/scratchbird
  EOS
end
```

---

## 6. Creating the Source Archive

### 6.1 GitHub Release Tarball

**Automatic (GitHub)**:
```bash
# GitHub automatically creates tarballs for tags
# URL format:
https://github.com/yourusername/scratchbird/archive/refs/tags/v0.1.0.tar.gz
```

**Calculate SHA256**:
```bash
# Download tarball
curl -L -o scratchbird-0.1.0.tar.gz \
    https://github.com/yourusername/scratchbird/archive/refs/tags/v0.1.0.tar.gz

# Calculate hash
shasum -a 256 scratchbird-0.1.0.tar.gz
```

### 6.2 Custom Tarball

```bash
# Create tarball
git archive --format=tar.gz --prefix=scratchbird-0.1.0/ \
    -o scratchbird-0.1.0.tar.gz v0.1.0

# Upload to GitHub releases or your server
```

---

## 7. Testing the Formula

### 7.1 Local Testing

**Install from local formula**:
```bash
# Test installation
brew install --build-from-source ./scratchbird.rb

# Verify installation
scratchbird --version

# Run formula tests
brew test scratchbird

# Uninstall
brew uninstall scratchbird
```

### 7.2 Formula Audit

```bash
# Run all checks
brew audit --strict --online scratchbird.rb

# Fix style issues
brew style --fix scratchbird.rb

# Check formula validity
brew formula scratchbird.rb
```

### 7.3 Build Bottle Locally

```bash
# Build bottle for current architecture
brew install --build-bottle scratchbird

# Create bottle
brew bottle scratchbird

# Output: scratchbird--0.1.0.arm64_monterey.bottle.tar.gz
```

---

## 8. Creating a Tap

### 8.1 What is a Tap?

A **tap** is a third-party Homebrew repository. For official distribution, you'll create your own tap.

**Naming convention**: `homebrew-<tapname>`

### 8.2 Create Tap Repository

```bash
# Create repository
mkdir homebrew-scratchbird
cd homebrew-scratchbird

# Create Formula directory
mkdir Formula

# Add formula
cp scratchbird.rb Formula/

# Initialize git
git init
git add Formula/scratchbird.rb
git commit -m "Add scratchbird formula"

# Push to GitHub (repository name: homebrew-scratchbird)
git remote add origin https://github.com/yourusername/homebrew-scratchbird.git
git push -u origin main
```

### 8.3 Tap Directory Structure

```
homebrew-scratchbird/
├── Formula/
│   └── scratchbird.rb
├── Casks/              # Optional: for .app bundles
├── README.md
└── .github/
    └── workflows/
        └── tests.yml   # CI/CD
```

---

## 9. Using Your Tap

### 9.1 Users Install from Tap

```bash
# Add tap
brew tap yourusername/scratchbird

# Install formula
brew install scratchbird

# Or in one command
brew install yourusername/scratchbird/scratchbird
```

### 9.2 Tap Updates

```bash
# Update tap
brew update

# Upgrade package
brew upgrade scratchbird

# Uninstall
brew uninstall scratchbird

# Remove tap
brew untap yourusername/scratchbird
```

---

## 10. Building Bottles

### 10.1 What are Bottles?

**Bottles** are pre-compiled binaries that install instantly without compiling from source.

**Benefits**:
- Fast installation (no compilation)
- Consistent builds
- Reduced dependencies (no build tools needed)

### 10.2 Build Bottles for Multiple macOS Versions

**Using GitHub Actions** (recommended):

**.github/workflows/bottles.yml**:
```yaml
name: Build Homebrew Bottles

on:
  push:
    tags: ['v*']

jobs:
  bottle:
    strategy:
      matrix:
        os:
          - macos-13      # Ventura (Intel)
          - macos-14      # Sonoma (Apple Silicon)

    runs-on: ${{ matrix.os }}

    steps:
      - uses: actions/checkout@v4

      - name: Setup Homebrew
        run: |
          brew update
          brew tap yourusername/scratchbird

      - name: Build bottle
        run: |
          brew install --build-bottle yourusername/scratchbird/scratchbird
          brew bottle yourusername/scratchbird/scratchbird

      - name: Upload bottle
        uses: actions/upload-artifact@v4
        with:
          name: bottles-${{ matrix.os }}
          path: "*.bottle.*"

      - name: Release
        uses: softprops/action-gh-release@v1
        if: startsWith(github.ref, 'refs/tags/')
        with:
          files: "*.bottle.*"
        env:
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

### 10.3 Add Bottles to Formula

```ruby
class Scratchbird < Formula
  desc "High-performance SQL database engine"
  homepage "https://github.com/yourusername/scratchbird"
  url "https://github.com/yourusername/scratchbird/archive/refs/tags/v0.1.0.tar.gz"
  sha256 "abc123..."
  license "MIT"

  bottle do
    root_url "https://github.com/yourusername/scratchbird/releases/download/v0.1.0"
    sha256 cellar: :any, arm64_sonoma:  "def456..."
    sha256 cellar: :any, arm64_ventura: "ghi789..."
    sha256 cellar: :any, ventura:       "jkl012..."
    sha256 cellar: :any, monterey:      "mno345..."
  end

  # ... rest of formula
end
```

**Generate bottle DSL automatically**:
```bash
# After building bottle
brew bottle --json yourusername/scratchbird/scratchbird

# This creates .bottle.json with the bottle do...end block
# Copy the block into your formula
```

---

## 11. Publishing to Homebrew Core

### 11.1 Requirements

To be accepted into Homebrew/homebrew-core:
- **Stable release**: No alpha/beta versions
- **Notable project**: Must be reasonably well-known or useful
- **Open source**: Must have an OSI-approved license
- **Building from source**: Must build successfully from source
- **No vendored dependencies**: Use Homebrew dependencies
- **Maintained**: Active development and maintenance
- **Documentation**: Good README and homepage

### 11.2 Submission Process

1. **Create formula** in your tap first
2. **Test thoroughly** on multiple macOS versions
3. **Fork homebrew-core**:
   ```bash
   brew tap homebrew/core
   cd $(brew --repository homebrew/core)
   git remote add yourusername https://github.com/yourusername/homebrew-core
   ```
4. **Create branch**:
   ```bash
   git checkout -b scratchbird
   ```
5. **Add formula**:
   ```bash
   cp ~/homebrew-scratchbird/Formula/scratchbird.rb Formula/
   ```
6. **Test**:
   ```bash
   brew install --build-from-source scratchbird
   brew test scratchbird
   brew audit --new-formula scratchbird
   ```
7. **Submit pull request** to https://github.com/Homebrew/homebrew-core

### 11.3 Formula Style Guide

Follow Homebrew's Ruby style:
```ruby
class Scratchbird < Formula
  desc "High-performance SQL database engine"
  homepage "https://github.com/yourusername/scratchbird"
  url "https://github.com/yourusername/scratchbird/archive/v0.1.0.tar.gz"
  sha256 "abc123..."
  license "MIT"

  # Use this format for dependencies
  depends_on "cmake" => :build
  depends_on "ninja" => :build
  depends_on "openssl@3"

  # Use std_cmake_args helper
  def install
    system "cmake", "-S", ".", "-B", "build", *std_cmake_args
    system "cmake", "--build", "build"
    system "cmake", "--install", "build"
  end

  # Minimal but meaningful test
  test do
    assert_match version.to_s, shell_output("#{bin}/scratchbird --version")
    # Test actual functionality if possible
    system "#{bin}/scratchbird", "test-command"
  end
end
```

---

## 12. Versioning and Updates

### 12.1 Update Formula Version

```ruby
class Scratchbird < Formula
  desc "High-performance SQL database engine"
  homepage "https://github.com/yourusername/scratchbird"
  url "https://github.com/yourusername/scratchbird/archive/v0.2.0.tar.gz"  # ← Updated
  sha256 "new-hash-here"  # ← Updated
  license "MIT"

  # Update bottle hashes if providing bottles
  bottle do
    # ... new bottle hashes
  end

  # ... rest unchanged
end
```

### 12.2 Versioned Formulas

For major version upgrades:
```ruby
# Formula/scratchbird.rb      - Latest version (0.2.0)
# Formula/scratchbird@0.1.rb  - Old version (0.1.0)
```

**Old version formula**:
```ruby
class ScratchbirdAT01 < Formula
  desc "High-performance SQL database engine (legacy version)"
  homepage "https://github.com/yourusername/scratchbird"
  url "https://github.com/yourusername/scratchbird/archive/v0.1.0.tar.gz"
  sha256 "abc123..."
  license "MIT"

  keg_only :versioned_formula

  # ... rest of formula
end
```

---

## 13. Service Management

### 13.1 Start/Stop Service

```bash
# Start service (launchd)
brew services start scratchbird

# Stop service
brew services stop scratchbird

# Restart service
brew services restart scratchbird

# List all services
brew services list

# Run at startup
# (brew services start automatically adds to login items)
```

### 13.2 Manual Service Control

```bash
# Load service manually
launchctl load ~/Library/LaunchAgents/homebrew.mxcl.scratchbird.plist

# Unload service
launchctl unload ~/Library/LaunchAgents/homebrew.mxcl.scratchbird.plist
```

---

## 14. CI/CD Integration

### 14.1 Test Formula in CI

**.github/workflows/homebrew.yml**:
```yaml
name: Homebrew Test

on:
  push:
    branches: [main]
  pull_request:

jobs:
  test:
    strategy:
      matrix:
        os: [macos-13, macos-14]

    runs-on: ${{ matrix.os }}

    steps:
      - uses: actions/checkout@v4

      - name: Install Homebrew dependencies
        run: brew install cmake ninja spdlog googletest openssl@3 lz4

      - name: Test formula
        run: |
          brew install --build-from-source ./Formula/scratchbird.rb
          brew test scratchbird
          brew audit --strict scratchbird

      - name: Test service
        run: |
          brew services start scratchbird
          sleep 5
          brew services stop scratchbird
```

---

## 15. Troubleshooting

### 15.1 Common Issues

**Formula not found**:
```bash
# Update Homebrew
brew update

# Check tap is added
brew tap

# Reinstall tap
brew untap yourusername/scratchbird
brew tap yourusername/scratchbird
```

**Build failures**:
```bash
# Verbose build
brew install --verbose --debug scratchbird

# Check build log
cat $(brew --cache)/Logs/scratchbird
```

**Bottle issues**:
```bash
# Force build from source
brew install --build-from-source scratchbird

# Clear cache
rm -rf $(brew --cache)/scratchbird
```

---

## 16. Best Practices

### 16.1 Formula Quality

- **Use official sources**: GitHub releases, official websites
- **Verify checksums**: Always include sha256
- **Minimal dependencies**: Only include necessary deps
- **Test thoroughly**: Write meaningful tests
- **Follow conventions**: Use Homebrew helpers (`std_cmake_args`, etc.)

### 16.2 Maintenance

- **Keep updated**: Release new versions promptly
- **Monitor issues**: Respond to user reports
- **Test updates**: Before releasing new formulas
- **Document changes**: Update caveats and README

---

## 17. Additional Resources

- **Homebrew Documentation:** https://docs.brew.sh/
- **Formula Cookbook:** https://docs.brew.sh/Formula-Cookbook
- **Acceptable Formulae:** https://docs.brew.sh/Acceptable-Formulae
- **Homebrew on Linux:** https://docs.brew.sh/Homebrew-on-Linux

---

**Document Version:** 1.0
**Last Updated:** 2026-01-03
**Maintainer:** Build Infrastructure Team
**Status:** Beta Preparation
