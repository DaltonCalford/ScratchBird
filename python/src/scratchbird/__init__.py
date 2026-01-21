"""ScratchBird DB-API 2.0 driver."""

from .connection import Connection, connect
from .errors import (
    Warning,
    Error,
    InterfaceError,
    DatabaseError,
    DataError,
    OperationalError,
    IntegrityError,
    InternalError,
    ProgrammingError,
    NotSupportedError,
)

apilevel = "2.0"
threadsafety = 2
paramstyle = "named"

__all__ = [
    "connect",
    "Connection",
    "Warning",
    "Error",
    "InterfaceError",
    "DatabaseError",
    "DataError",
    "OperationalError",
    "IntegrityError",
    "InternalError",
    "ProgrammingError",
    "NotSupportedError",
]
