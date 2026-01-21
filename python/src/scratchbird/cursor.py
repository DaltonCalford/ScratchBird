"""Cursor implementation for ScratchBird Python driver."""

from __future__ import annotations

from typing import Iterable, List, Optional, Sequence

from . import errors
from .sql import substitute_parameters


class Cursor:
    def __init__(self, connection):
        self._connection = connection
        self._closed = False
        self._results: List = []
        self._pos = 0
        self.description = None
        self.rowcount = -1
        self.arraysize = 1
        self.lastrowid = None

    def execute(self, sql: str, params=None) -> None:
        self._ensure_open()
        if sql is None:
            raise errors.ProgrammingError("sql is required")
        self._results = []
        self._pos = 0
        self.description = None
        self.rowcount = -1
        self.lastrowid = None
        rendered = substitute_parameters(sql, params)
        columns, rows, rowcount = self._connection._execute_query(rendered)
        self._results = rows
        self._pos = 0
        if columns:
            self.description = [
                (
                    col.name,
                    col.wire_type,
                    None,
                    col.type_modifier or None,
                    None,
                    None,
                    None,
                )
                for col in columns
            ]
        if rowcount is None:
            rowcount = -1
        if rowcount < 0 and rows:
            rowcount = len(rows)
        self.rowcount = rowcount

    def executemany(self, sql: str, seq_of_params: Iterable) -> None:
        self._ensure_open()
        if sql is None:
            raise errors.ProgrammingError("sql is required")
        self._results = []
        self._pos = 0
        self.description = None
        self.rowcount = -1
        self.lastrowid = None
        total = 0
        rowcount_known = True
        for params in seq_of_params:
            rendered = substitute_parameters(sql, params)
            columns, rows, rowcount = self._connection._execute_query(rendered)
            self._results = rows
            self._pos = 0
            if columns:
                self.description = [
                    (
                        col.name,
                        col.wire_type,
                        None,
                        col.type_modifier or None,
                        None,
                        None,
                        None,
                    )
                    for col in columns
                ]
            if rowcount is None or rowcount < 0:
                rowcount_known = False
            else:
                total += rowcount
        self.rowcount = total if rowcount_known else -1

    def __iter__(self):
        return self

    def __next__(self):
        row = self.fetchone()
        if row is None:
            raise StopIteration
        return row

    def fetchone(self):
        self._ensure_open()
        if self._pos >= len(self._results):
            return None
        row = self._results[self._pos]
        self._pos += 1
        return row

    def fetchmany(self, size: Optional[int] = None) -> List:
        self._ensure_open()
        if size is None:
            size = self.arraysize
        if size <= 0:
            return []
        start = self._pos
        end = min(self._pos + size, len(self._results))
        self._pos = end
        return self._results[start:end]

    def fetchall(self) -> List:
        self._ensure_open()
        if self._pos >= len(self._results):
            return []
        rows = self._results[self._pos :]
        self._pos = len(self._results)
        return rows

    def close(self) -> None:
        self._closed = True

    def setinputsizes(self, sizes) -> None:
        self._ensure_open()

    def setoutputsize(self, size, column=None) -> None:
        self._ensure_open()

    def _ensure_open(self) -> None:
        if self._closed:
            raise errors.InterfaceError("cursor is closed")

    @property
    def connection(self):
        return self._connection
