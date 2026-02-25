Last updated: 2026-02-24

# MY-EMU-011 Handshake Parity

- Capture mode: `live`
- Native endpoint: `127.0.0.1:3306`
- Emulated endpoint: `127.0.0.1:13306`
- Native capture: `ok`
- Emulated capture: `ok`
- Native server_version: `9.6.0`
- Emulated server_version: `8.4.8`
- Baseline series target: `8.4`
- Native version series: `9.6`
- Emulated version series: `8.4`

## Comparison
- Byte-equivalent: `false`
- Native length: `77`
- Emulated length: `77`
- First mismatch offset: `5`
- Semantic parity (normalized): `true`
- Native endpoint series is outside baseline target; server-version mismatch treated as expected track drift.
- Missing capabilities (emulated): `0x40000800`
- Missing capability names: `SSL, SSL_VERIFY_SERVER_CERT`
- Extra capabilities (emulated): `0x00000000`
- Capability drift is currently optional-only.

## Capture Artifacts
- Native capture: `artifacts/emulation/mysql/p5s1w2/my-emu-010-wire-captures/mysql-native-live-handshake.hex`
- Emulated capture: `artifacts/emulation/mysql/p5s1w2/my-emu-010-wire-captures/mysql-emulated-live-handshake.hex`
