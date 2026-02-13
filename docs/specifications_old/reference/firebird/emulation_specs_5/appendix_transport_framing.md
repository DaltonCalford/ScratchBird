# Appendix: Transport Framing and XDR Stream (Authoritative)

This appendix defines how XDR data is framed on the wire for the TCP remote protocol.

## 1) TCP Stream Semantics
- The Firebird remote protocol runs over a **byte-stream TCP connection**.
- There is **no explicit message length prefix** at the TCP layer. Packet boundaries are determined by XDR decoding.
- The sender may flush partial buffers at any time; the receiver **must treat the connection as a continuous stream** and read until the requested XDR data is satisfied.

## 2) Partial Flush Behavior
- A sender may split an XDR record into multiple TCP sends (chunks).
- The receiver must continue reading until the XDR decode operation completes for the requested packet.
- There is no out-of-band framing marker that indicates the end of a packet; the XDR codec is authoritative.

## 3) Compression and Encryption
- When wire compression is enabled, the **compressed stream is still a byte stream**; the decompressor feeds the XDR decoder with decompressed bytes until decode completes.
- When wire encryption is enabled, encryption is applied **to the byte stream** before transmission and removed before XDR decoding.

These rules are authoritative. Any implementation that expects explicit length headers at the TCP layer is non-conformant.
