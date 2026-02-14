# Test Server Build Fix Guide

**Problem:** CMakeLists.txt references test files that don't exist  
**Solution:** Pre-build binaries or use minimal setup script

---

## Quick Fix (Recommended)

### Step 1: Pre-build Binaries

```bash
cd /opt/ScratchBird

# Use the pre-build script (builds only essential binaries)
./scripts/prebuild-binaries.sh
```

This builds:
- `sb_server` - The main server binary
- `sb_isql` - Interactive SQL client
- `sb_admin` - Admin utility

**Time:** ~5-10 minutes (vs 30+ minutes for full build)

### Step 2: Run Setup

```bash
# Now run the setup script
sudo ./scripts/setup-test-server.sh
```

---

## Alternative: Fix CMakeLists.txt Manually

If you want to build everything:

```bash
# Check for missing test files
grep -n "add_executable" tests/CMakeLists.txt | grep -v "#" | \
  while read line; do 
    file=$(echo $line | grep -oP 'test_\w+\.cpp' || echo ""); 
    if [ -n "$file" ] && [ ! -f "tests/unit/$file" ] && [ ! -f "tests/integration/$file" ]; then
      echo "Missing: $file"; 
    fi
  done

# Comment out any missing tests in CMakeLists.txt
# Then rebuild:
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

---

## Alternative: Minimal Setup (No Build)

If you have pre-built binaries elsewhere:

```bash
# Use the minimal setup script
sudo ./scripts/setup-test-server-minimal.sh

# It will search for binaries in:
# - /opt/ScratchBird/build/bin
# - ./build/bin
# - /usr/local/bin
```

---

## Verification

After setup, verify everything works:

```bash
# Check service status
sudo systemctl status scratchbird-test

# Test connection
/opt/ScratchBird/build/bin/sb_isql \
    --host=127.0.0.1 \
    --port=13092 \
    --database=testdb \
    --user=admin \
    --query="SELECT 'Hello World';"
```

---

## Common Issues

### Issue 1: "Cannot find source file: test_join_ordering.cpp"

**Fix:** Already applied in commit `c371234c`. Pull latest changes:
```bash
git pull origin main
```

### Issue 2: "sb_server: command not found"

**Fix:** Build binaries first:
```bash
./scripts/prebuild-binaries.sh
```

### Issue 3: "Port 13092 already in use"

**Fix:** Check what's using it:
```bash
sudo lsof -i :13092
sudo systemctl stop scratchbird-test  # If running
```

### Issue 4: "Permission denied on /var/scratchbird"

**Fix:** Fix permissions:
```bash
sudo chown -R scratchbird:scratchbird /var/scratchbird
sudo chmod 750 /var/scratchbird
```

---

## Full Setup Sequence

```bash
# 1. Clone repository
git clone https://github.com/DaltonCalford/ScratchBird.git
cd ScratchBird

# 2. Pre-build essential binaries
./scripts/prebuild-binaries.sh

# 3. Run full setup
sudo ./scripts/setup-test-server.sh

# 4. Verify
sudo systemctl status scratchbird-test
/opt/ScratchBird/build/bin/sb_isql --host=127.0.0.1 --port=13092 \
    --user=admin --query="SELECT 'Success';"
```

---

## Files Modified

| File | Change |
|------|--------|
| `tests/CMakeLists.txt` | Commented out `test_join_ordering` and `test_mv_rewriter` |
| `scripts/prebuild-binaries.sh` | New - builds only essential binaries |
| `scripts/setup-test-server-minimal.sh` | New - works with pre-built binaries |

---

## Connection Info (After Setup)

```
Host:     127.0.0.1
Port:     13092
Database: testdb
Users:    SYSARCH / SysArch2026!
          TESTUSER / TestUser2026!
```

**Test Server is Ready!**
