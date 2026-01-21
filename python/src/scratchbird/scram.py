"""SCRAM-SHA-256 implementation for ScratchBird."""

from __future__ import annotations

import base64
import hashlib
import hmac
import secrets


def generate_nonce(length: int = 24) -> str:
    return secrets.token_urlsafe(length)[:length]


def normalize_username(username: str) -> str:
    return username.replace("=", "=3D").replace(",", "=2C")


def parse_server_first(message: str):
    if not message.startswith("r="):
        raise ValueError("invalid server-first")
    parts = dict(part.split("=", 1) for part in message.split(",") if "=" in part)
    nonce = parts.get("r")
    salt_b64 = parts.get("s")
    iterations = parts.get("i")
    if nonce is None or salt_b64 is None or iterations is None:
        raise ValueError("invalid server-first")
    salt = base64.b64decode(salt_b64.encode("ascii"))
    return nonce, salt, int(iterations)


def parse_server_final(message: str) -> bytes:
    if not message.startswith("v="):
        raise ValueError("invalid server-final")
    return base64.b64decode(message[2:].encode("ascii"))


def _hi(password: str, salt: bytes, iterations: int, digest: str) -> bytes:
    return hashlib.pbkdf2_hmac(digest, password.encode("utf-8"), salt, iterations)


def _hmac(key: bytes, msg: str, digest: str) -> bytes:
    return hmac.new(key, msg.encode("utf-8"), digest).digest()


def _hash(data: bytes, digest: str) -> bytes:
    return hashlib.new(digest, data).digest()


class ScramExchange:
    def __init__(self, username: str, digest: str = "sha256"):
        self.username = username
        self.digest = digest
        self.client_nonce = generate_nonce()
        self.client_first_bare = f"n={normalize_username(username)},r={self.client_nonce}"
        self.server_first = ""
        self.client_final = ""
        self.expected_server_signature = b""

    def client_first_message(self) -> str:
        return f"n,,{self.client_first_bare}"

    def handle_server_first(self, password: str, server_first: str) -> str:
        nonce, salt, iterations = parse_server_first(server_first)
        if not nonce.startswith(self.client_nonce):
            raise ValueError("scram nonce mismatch")

        salted_password = _hi(password, salt, iterations, self.digest)
        client_key = _hmac(salted_password, "Client Key", self.digest)
        stored_key = _hash(client_key, self.digest)
        server_key = _hmac(salted_password, "Server Key", self.digest)

        client_final_without_proof = f"c=biws,r={nonce}"
        auth_message = f"{self.client_first_bare},{server_first},{client_final_without_proof}"

        client_signature = _hmac(stored_key, auth_message, self.digest)
        client_proof = bytes(a ^ b for a, b in zip(client_key, client_signature))
        proof_b64 = base64.b64encode(client_proof).decode("ascii")

        self.server_first = server_first
        self.client_final = f"{client_final_without_proof},p={proof_b64}"
        self.expected_server_signature = _hmac(server_key, auth_message, self.digest)
        return self.client_final

    def verify_server_final(self, server_final: str) -> None:
        signature = parse_server_final(server_final)
        if signature != self.expected_server_signature:
            raise ValueError("scram server signature mismatch")
