/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */

#include "challenge_state_store.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

namespace {

using scratchbird::security::plugins::challenge::ExchangeStore;
using scratchbird::security::plugins::challenge::ReplayCache;

struct DummyState {
    uint64_t value = 0;
};

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}

}  // namespace

int main() {
    bool ok = true;

    ExchangeStore<DummyState> store(25);
    store.put(1001, DummyState{7});

    DummyState out{};
    auto take_status = store.take(1001, &out);
    ok = expect(take_status == ExchangeStore<DummyState>::TakeStatus::Found,
                "exchange take should return Found for live entry") &&
         ok;
    ok = expect(out.value == 7, "exchange state payload should round-trip") && ok;

    store.put(1002, DummyState{11});
    std::this_thread::sleep_for(std::chrono::milliseconds(35));
    take_status = store.take(1002, &out);
    ok = expect(take_status == ExchangeStore<DummyState>::TakeStatus::Expired,
                "expired exchange should return Expired") &&
         ok;

    ReplayCache replay(50);
    ok = expect(replay.remember("nonce:abc123"),
                "first replay-cache remember should succeed") &&
         ok;
    ok = expect(!replay.remember("nonce:abc123"),
                "duplicate replay-cache remember should fail during TTL window") &&
         ok;
    ok = expect(replay.contains("nonce:abc123"),
                "replay-cache contains should be true before TTL expiry") &&
         ok;

    std::this_thread::sleep_for(std::chrono::milliseconds(65));
    ok = expect(!replay.contains("nonce:abc123"),
                "replay-cache entry should expire after TTL") &&
         ok;
    ok = expect(replay.remember("nonce:abc123"),
                "replay-cache key should be reusable after expiry") &&
         ok;

    if (!ok) {
        return 1;
    }

    std::cout << "challenge_state_store_selftest: PASS\n";
    return 0;
}
