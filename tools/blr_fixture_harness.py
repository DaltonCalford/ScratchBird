#!/usr/bin/env python3
import argparse
import datetime
import os
import re
from dataclasses import dataclass


BLR_VERSION5 = 0x05
BLR_EOC = 0x4C
BLR_BEGIN = 0x02
BLR_END = 0xFF
BLR_ASSIGNMENT = 0x01
BLR_ADD = 0x22
BLR_LITERAL = 0x15
BLR_LONG = 0x08
BLR_TEXT2 = 0x0F
BLR_PARAMETER = 0x19
BLR_MISSING = 0x3D
BLR_NOT = 0x3B
BLR_START_SAVEPOINT = 0x86
BLR_END_SAVEPOINT = 0x87
BLR_DCL_CURSOR = 0xA6
BLR_CURSOR_STMT = 0xA7
BLR_CURSOR_OPEN = 0x00
BLR_CURSOR_CLOSE = 0x01
BLR_CURSOR_FETCH = 0x02
BLR_CURSOR_FETCH_SCROLL = 0x03
BLR_RSE = 0x43
BLR_RELATION = 0x4A
BLR_RELATION2 = 0x92
BLR_RELATION3 = 0x94
BLR_SCROLLABLE = 0x6D
BLR_EXEC_SQL = 0xB0
BLR_EXEC_STMT = 0xBD
BLR_EXEC_STMT_INPUTS = 0x01
BLR_EXEC_STMT_OUTPUTS = 0x02
BLR_EXEC_STMT_SQL = 0x03
BLR_EXEC_STMT_PROC_BLOCK = 0x04
BLR_EXEC_STMT_DATA_SRC = 0x05
BLR_EXEC_STMT_USER = 0x06
BLR_EXEC_STMT_PWD = 0x07
BLR_EXEC_STMT_TRAN = 0x08
BLR_EXEC_STMT_TRAN_CLONE = 0x09
BLR_EXEC_STMT_PRIVS = 0x0A
BLR_EXEC_STMT_IN_PARAMS = 0x0B
BLR_EXEC_STMT_IN_PARAMS2 = 0x0C
BLR_EXEC_STMT_OUT_PARAMS = 0x0D
BLR_EXEC_STMT_ROLE = 0x0E
BLR_EXEC_STMT_IN_EXCESS = 0x0F


@dataclass
class Fixture:
    name: str
    blr_hex: str
    expected: str
    status: str


class BlrParseError(RuntimeError):
    pass


class BlrReader:
    def __init__(self, data):
        self.data = data
        self.pos = 0

    def remaining(self):
        return len(self.data) - self.pos

    def peek(self):
        if self.pos >= len(self.data):
            return None
        return self.data[self.pos]

    def read_byte(self):
        if self.pos >= len(self.data):
            raise BlrParseError("Unexpected end of BLR stream")
        b = self.data[self.pos]
        self.pos += 1
        return b

    def read_bytes(self, count):
        if self.pos + count > len(self.data):
            raise BlrParseError("Unexpected end of BLR stream")
        chunk = self.data[self.pos:self.pos + count]
        self.pos += count
        return chunk

    def read_u16(self):
        b0, b1 = self.read_bytes(2)
        return b0 | (b1 << 8)

    def read_u32(self):
        b = self.read_bytes(4)
        return b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24)

    def read_i8(self):
        b = self.read_byte()
        return b - 256 if b > 127 else b


class BlrTranslator:
    def __init__(self, data):
        self.reader = BlrReader(data)
        self.tokens = []

    def translate(self):
        if self.reader.read_byte() != BLR_VERSION5:
            raise BlrParseError("Unsupported BLR version")
        self.tokens.append("VERSION(2)")
        if self.reader.peek() == BLR_BEGIN:
            self.parse_block()
        else:
            self.parse_expression()
        if self.reader.read_byte() != BLR_EOC:
            raise BlrParseError("Missing BLR end-of-command")
        if self.reader.remaining() != 0:
            raise BlrParseError("Trailing bytes after BLR end-of-command")
        return self.tokens

    def parse_block(self):
        if self.reader.read_byte() != BLR_BEGIN:
            raise BlrParseError("Expected blr_begin")
        self.tokens.append("EXT_BLOCK")
        while True:
            op = self.reader.peek()
            if op is None:
                raise BlrParseError("Unterminated blr_begin block")
            if op == BLR_END:
                self.reader.read_byte()
                self.tokens.append("END")
                return
            self.parse_statement()

    def parse_statement(self):
        op = self.reader.peek()
        if op == BLR_BEGIN:
            self.parse_block()
            return
        if op == BLR_START_SAVEPOINT:
            self.reader.read_byte()
            self.tokens.append("EXT_SAVEPOINT_BEGIN")
            while True:
                next_op = self.reader.peek()
                if next_op is None:
                    raise BlrParseError("Unterminated blr_start_savepoint")
                if next_op == BLR_END_SAVEPOINT:
                    self.reader.read_byte()
                    self.tokens.append("EXT_SAVEPOINT_END")
                    return
                self.parse_statement()
            return
        if op == BLR_DCL_CURSOR:
            self.reader.read_byte()
            self.parse_dcl_cursor()
            return
        if op == BLR_EXEC_STMT:
            self.reader.read_byte()
            self.parse_exec_stmt()
            return
        if op == BLR_EXEC_SQL:
            self.reader.read_byte()
            self.parse_exec_sql()
            return
        if op == BLR_CURSOR_STMT:
            self.reader.read_byte()
            self.parse_cursor_stmt()
            return
        raise BlrParseError(f"Unsupported statement opcode 0x{op:02X}")

    def parse_cursor_stmt(self):
        subcode = self.reader.read_byte()
        cursor_id = self.reader.read_u16()
        if subcode == BLR_CURSOR_OPEN:
            self.tokens.append("CURSOR_OPEN")
            self.tokens.append(f"id={cursor_id}")
            return
        if subcode == BLR_CURSOR_CLOSE:
            self.tokens.append("CURSOR_CLOSE")
            self.tokens.append(f"id={cursor_id}")
            return
        if subcode == BLR_CURSOR_FETCH:
            self.tokens.append("CURSOR_FETCH")
            self.tokens.append(f"id={cursor_id}")
            self.parse_cursor_fetch_body()
            return
        if subcode == BLR_CURSOR_FETCH_SCROLL:
            raise BlrParseError("Cursor fetch scroll not supported in fixtures")
        raise BlrParseError(f"Unsupported cursor subcode 0x{subcode:02X}")

    def parse_exec_stmt(self):
        self.tokens.append("EXEC_STMT")
        inputs = 0
        outputs = 0
        while True:
            code = self.reader.read_byte()
            if code == BLR_END:
                break
            if code == BLR_EXEC_STMT_INPUTS:
                inputs = self.reader.read_u16()
                self.tokens.append(f"EXEC_STMT_INPUTS({inputs})")
            elif code == BLR_EXEC_STMT_OUTPUTS:
                outputs = self.reader.read_u16()
                self.tokens.append(f"EXEC_STMT_OUTPUTS({outputs})")
            elif code == BLR_EXEC_STMT_SQL:
                expr_tokens = self.parse_expression_tokens()
                self.tokens.append(f"EXEC_STMT_SQL({format_expr_tokens(expr_tokens)})")
            elif code == BLR_EXEC_STMT_PROC_BLOCK:
                raise BlrParseError("EXEC_STMT proc block not supported in fixtures")
            elif code == BLR_EXEC_STMT_DATA_SRC:
                expr_tokens = self.parse_expression_tokens()
                self.tokens.append(f"EXEC_STMT_DATA_SRC({format_expr_tokens(expr_tokens)})")
            elif code == BLR_EXEC_STMT_USER:
                expr_tokens = self.parse_expression_tokens()
                self.tokens.append(f"EXEC_STMT_USER({format_expr_tokens(expr_tokens)})")
            elif code == BLR_EXEC_STMT_PWD:
                expr_tokens = self.parse_expression_tokens()
                self.tokens.append(f"EXEC_STMT_PWD({format_expr_tokens(expr_tokens)})")
            elif code == BLR_EXEC_STMT_ROLE:
                expr_tokens = self.parse_expression_tokens()
                self.tokens.append(f"EXEC_STMT_ROLE({format_expr_tokens(expr_tokens)})")
            elif code == BLR_EXEC_STMT_TRAN:
                raise BlrParseError("EXEC_STMT external transaction not supported in fixtures")
            elif code == BLR_EXEC_STMT_TRAN_CLONE:
                scope = self.reader.read_byte()
                self.tokens.append(f"EXEC_STMT_TRAN_CLONE({scope})")
            elif code == BLR_EXEC_STMT_PRIVS:
                self.tokens.append("EXEC_STMT_PRIVS")
            elif code in (BLR_EXEC_STMT_IN_PARAMS, BLR_EXEC_STMT_IN_PARAMS2):
                if inputs == 0:
                    raise BlrParseError("EXEC_STMT inputs count not set before in_params")
                exprs = []
                for _ in range(inputs):
                    if code == BLR_EXEC_STMT_IN_PARAMS2:
                        _ = self.read_meta_name()
                    expr_tokens = self.parse_expression_tokens()
                    exprs.append(format_expr_tokens(expr_tokens))
                self.tokens.append(f"EXEC_STMT_IN_PARAMS({','.join(exprs)})")
            elif code == BLR_EXEC_STMT_OUT_PARAMS:
                if outputs == 0:
                    raise BlrParseError("EXEC_STMT outputs count not set before out_params")
                exprs = []
                for _ in range(outputs):
                    expr_tokens = self.parse_expression_tokens()
                    exprs.append(format_expr_tokens(expr_tokens))
                self.tokens.append(f"EXEC_STMT_OUT_PARAMS({','.join(exprs)})")
            elif code == BLR_EXEC_STMT_IN_EXCESS:
                count = self.reader.read_u16()
                numbers = []
                for _ in range(count):
                    numbers.append(str(self.reader.read_u16()))
                self.tokens.append(f"EXEC_STMT_IN_EXCESS({','.join(numbers)})")
            else:
                raise BlrParseError(f"Unsupported exec_stmt subcode 0x{code:02X}")

    def parse_exec_sql(self):
        expr_tokens = self.parse_expression_tokens()
        self.tokens.append("EXEC_SQL")
        self.tokens.append(format_expr_tokens(expr_tokens))

    def parse_dcl_cursor(self):
        cursor_id = self.reader.read_u16()
        scrollable = False
        if self.reader.peek() == BLR_SCROLLABLE:
            self.reader.read_byte()
            scrollable = True
        relations = self.parse_rse()
        select_count = self.reader.read_u16()
        select_exprs = []
        for _ in range(select_count):
            expr_tokens = self.parse_expression_tokens()
            select_exprs.append(format_expr_tokens(expr_tokens))
        self.tokens.append("CURSOR_DECLARE")
        self.tokens.append(f"id={cursor_id}")
        if relations:
            self.tokens.append(f"RSE({','.join(relations)})")
        if select_exprs:
            self.tokens.append(f"SELECT({','.join(select_exprs)})")
        if scrollable:
            self.tokens.append("SCROLLABLE")

    def parse_cursor_fetch_body(self):
        if self.reader.read_byte() != BLR_BEGIN:
            raise BlrParseError("Expected blr_begin for cursor fetch body")
        if self.reader.peek() == BLR_END:
            self.reader.read_byte()
            return
        while True:
            op = self.reader.peek()
            if op is None:
                raise BlrParseError("Unterminated cursor fetch body")
            if op == BLR_END:
                self.reader.read_byte()
                return
            raise BlrParseError("Unsupported cursor fetch body content")

    def parse_rse(self):
        op = self.reader.read_byte()
        if op != BLR_RSE:
            raise BlrParseError(f"Unsupported RSE opcode 0x{op:02X}")
        stream_count = self.reader.read_byte()
        relations = []
        for _ in range(stream_count):
            rel_op = self.reader.read_byte()
            if rel_op == BLR_RELATION:
                relations.append(self.read_meta_name())
            elif rel_op == BLR_RELATION2:
                relations.append(self.read_meta_name())
                _ = self.read_meta_name()
            elif rel_op == BLR_RELATION3:
                schema = self.read_meta_name()
                name = self.read_meta_name()
                relations.append(f"{schema}.{name}")
                _ = self.read_meta_name()
            else:
                raise BlrParseError(f"Unsupported relation opcode 0x{rel_op:02X}")
        while True:
            clause = self.reader.read_byte()
            if clause == BLR_END:
                break
            raise BlrParseError(f"Unsupported RSE clause opcode 0x{clause:02X}")
        return relations

    def read_meta_name(self):
        length = self.reader.read_byte()
        raw = bytes(self.reader.read_bytes(length))
        try:
            return raw.decode("ascii")
        except UnicodeDecodeError:
            return raw.decode("latin-1")

    def parse_expression(self):
        self.tokens.extend(self.parse_expression_tokens())

    def parse_expression_tokens(self):
        op = self.reader.read_byte()
        if op == BLR_ADD:
            tokens = ["EXPR_ADD"]
            tokens.extend(self.parse_expression_tokens())
            tokens.extend(self.parse_expression_tokens())
            return tokens
        if op == BLR_LITERAL:
            return [self.parse_literal()]
        if op == BLR_PARAMETER:
            msg_num = self.reader.read_byte()
            param_num = self.reader.read_u16()
            if msg_num == 0:
                return [f"PARAM({param_num})"]
            return [f"PARAM({msg_num},{param_num})"]
        if op == BLR_MISSING:
            tokens = ["EXT_EXPR_IS_NULL"]
            tokens.extend(self.parse_expression_tokens())
            return tokens
        if op == BLR_NOT:
            tokens = ["EXT_EXPR_NOT"]
            tokens.extend(self.parse_expression_tokens())
            return tokens
        raise BlrParseError(f"Unsupported expression opcode 0x{op:02X}")

    def parse_literal(self):
        type_code = self.reader.read_byte()
        if type_code == BLR_LONG:
            scale = self.reader.read_i8()
            value = self.reader.read_u32()
            if scale != 0:
                raise BlrParseError("Scaled integers not supported in fixtures")
            return f"LITERAL_INT32({value})"
        if type_code == BLR_TEXT2:
            text = self.read_text2()
            return f"LITERAL_STRING(\"{text}\")"
        raise BlrParseError(f"Unsupported literal type 0x{type_code:02X}")

    def parse_text_literal(self):
        if self.reader.read_byte() != BLR_LITERAL:
            raise BlrParseError("Expected blr_literal for exec_stmt SQL")
        type_code = self.reader.read_byte()
        if type_code != BLR_TEXT2:
            raise BlrParseError("Expected blr_text2 for exec_stmt SQL")
        return self.read_text2()

    def read_text2(self):
        _ttype = self.reader.read_u16()
        length = self.reader.read_u16()
        raw = bytes(self.reader.read_bytes(length))
        try:
            return raw.decode("ascii")
        except UnicodeDecodeError:
            return raw.decode("latin-1")


def format_expr_tokens(tokens):
    if not tokens:
        return ""
    if len(tokens) == 1:
        return tokens[0]
    return ";".join(tokens)


def parse_fixtures(path):
    with open(path, "r", encoding="utf-8") as handle:
        lines = handle.readlines()

    fixtures = []
    current = None
    capture = None
    buffer = []

    def flush_current():
        if current:
            fixtures.append(current)

    for line in lines:
        stripped = line.strip()
        if stripped.startswith("### "):
            flush_current()
            current = {
                "name": stripped[4:],
                "blr_hex": "",
                "expected": "",
                "status": "",
            }
            capture = None
            buffer = []
            continue
        if current is None:
            continue
        if stripped == "BLR (hex):":
            capture = "blr_wait"
            continue
        if stripped == "Expected SBLR (symbolic)":
            capture = "expected_wait"
            continue
        if stripped == "Expected SBLR (symbolic):":
            capture = "expected_wait"
            continue
        if stripped.startswith("Status:"):
            current["status"] = stripped.split("Status:", 1)[1].strip()
            continue
        if stripped.startswith("```"):
            if capture == "blr_wait":
                capture = "blr"
                buffer = []
                continue
            if capture == "expected_wait":
                capture = "expected"
                buffer = []
                continue
            if capture == "blr":
                current["blr_hex"] = "\n".join(buffer).strip()
                capture = None
                buffer = []
                continue
            if capture == "expected":
                current["expected"] = "\n".join(buffer).strip()
                capture = None
                buffer = []
                continue
        if capture in ("blr", "expected"):
            buffer.append(line.rstrip("\n"))

    flush_current()

    results = []
    for item in fixtures:
        results.append(Fixture(
            name=item["name"],
            blr_hex=item["blr_hex"],
            expected=item["expected"],
            status=item["status"],
        ))
    return results


def parse_hex_bytes(text):
    parts = re.findall(r"[0-9A-Fa-f]{2}", text)
    return [int(p, 16) for p in parts]


def split_tokens(text):
    tokens = []
    buf = []
    in_quote = False
    quote_char = ""
    for ch in text.strip():
        if ch in ("\"", "'"):
            if not in_quote:
                in_quote = True
                quote_char = ch
                buf.append(ch)
                continue
            if ch == quote_char:
                in_quote = False
                buf.append(ch)
                quote_char = ""
                continue
        if ch.isspace() and not in_quote:
            if buf:
                tokens.append("".join(buf))
                buf = []
            continue
        buf.append(ch)
    if buf:
        tokens.append("".join(buf))
    return tokens


def normalize_expected(text):
    return " ".join(line.strip() for line in text.splitlines() if line.strip())


def truncate(text, limit=160):
    if len(text) <= limit:
        return text
    return text[:limit - 3] + "..."


def run(fixtures):
    rows = []
    passed = 0
    failed = 0

    for fixture in fixtures:
        try:
            blr_bytes = parse_hex_bytes(fixture.blr_hex)
            translator = BlrTranslator(blr_bytes)
            tokens = translator.translate()
            output = " ".join(tokens)
            expected = normalize_expected(fixture.expected)
            actual_tokens = split_tokens(output)
            expected_tokens = split_tokens(expected)
            if actual_tokens == expected_tokens:
                result = "PASS"
                detail = "matched"
                passed += 1
            else:
                result = "FAIL"
                detail = truncate(f"expected='{expected}' actual='{output}'")
                failed += 1
        except Exception as exc:  # noqa: BLE001
            result = "FAIL"
            detail = truncate(f"error: {exc}")
            failed += 1
        rows.append((fixture.name, fixture.status, result, detail))

    return rows, passed, failed


def write_report(path, rows, passed, failed):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    total = passed + failed
    timestamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d %H:%M:%SZ")

    lines = [
        "# BLR Fixture Harness Report",
        "",
        f"Generated: {timestamp}",
        f"Fixtures: {total}",
        f"Pass: {passed}",
        f"Fail: {failed}",
        "",
        "| Fixture | Status | Result | Details |",
        "| --- | --- | --- | --- |",
    ]
    for name, status, result, detail in rows:
        lines.append(f"| {name} | {status or '-'} | {result} | {detail} |")

    with open(path, "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")


def main():
    parser = argparse.ArgumentParser(description="Translate BLR fixtures to SBLR tokens.")
    parser.add_argument(
        "--fixtures",
        default="ScratchBird/docs/specifications/FIREBIRD_BLR_FIXTURES.md",
        help="Path to the BLR fixtures markdown file.",
    )
    parser.add_argument(
        "--report",
        default="ScratchBird-Analysis/reports/blr_fixture_report.md",
        help="Path to write the report markdown file.",
    )
    args = parser.parse_args()

    fixtures = parse_fixtures(args.fixtures)
    rows, passed, failed = run(fixtures)
    write_report(args.report, rows, passed, failed)

    total = passed + failed
    print(f"Fixtures: {total}  Pass: {passed}  Fail: {failed}")
    print(f"Report: {args.report}")


if __name__ == "__main__":
    main()
