#include "scratchbird/engine/lexer.h"

#include <cassert>
#include <string>
#include <vector>

using namespace scratchbird::engine;

int main()
{
    {
        Lexer lx("DATE '2025-08-15'");
        auto toks = lx.lex();
        bool saw_date = false;
        for (size_t i = 0; i < toks.size(); ++i) {
            if (toks[i].kind == TokenKind::Date) {
                saw_date = true;
                break;
            }
        }
        assert(saw_date);
    }
    {
        // Alternate separators and orders (YYYY/MM/DD, DD-MM-YYYY, MM/DD/YYYY)
        Lexer lx("DATE '2025/08/15'; DATE '15-08-2025'; DATE '08/15/2025'");
        auto toks = lx.lex();
        int count = 0;
        for (auto& t : toks)
            if (t.kind == TokenKind::Date)
                count++;
        assert(count == 3);
    }
    {
        Lexer lx("TIME '12:34:56'");
        auto toks = lx.lex();
        bool saw_time = false;
        for (auto& t : toks)
            if (t.kind == TokenKind::Time) {
                saw_time = true;
                break;
            }
        assert(saw_time);
    }
    {
        // Time with timezone, including compact offset
        Lexer lx("TIME '12:34:56Z'; TIME '12:34:56+02'; TIME '12:34:56-0530'");
        auto toks = lx.lex();
        int count = 0;
        for (auto& t : toks)
            if (t.kind == TokenKind::Time)
                count++;
        assert(count == 3);
    }
    {
        Lexer lx("TIMESTAMP '2025-08-15 12:34:56'");
        auto toks = lx.lex();
        bool saw_ts = false;
        for (auto& t : toks)
            if (t.kind == TokenKind::Timestamp) {
                saw_ts = true;
                break;
            }
        assert(saw_ts);
    }
    {
        // Timestamp with timezone and ISO T separator
        Lexer lx("TIMESTAMP '2025-08-15T12:34:56Z'; TIMESTAMP '15/08/2025 12:34:56+0100'");
        auto toks = lx.lex();
        int count = 0;
        for (auto& t : toks)
            if (t.kind == TokenKind::Timestamp)
                count++;
        assert(count == 2);
    }
    {
        Lexer lx("X'0A0B0C0D'");
        auto toks = lx.lex();
        bool saw_uuid = false;
        for (auto& t : toks)
            if (t.kind == TokenKind::Uuid) {
                saw_uuid = true;
                break;
            }
        assert(saw_uuid);
    }
    {
        Lexer lx("UUID '123e4567-e89b-12d3-a456-426614174000'");
        auto toks = lx.lex();
        bool saw_uuid = false;
        for (auto& t : toks)
            if (t.kind == TokenKind::Uuid) {
                saw_uuid = true;
                break;
            }
        assert(saw_uuid);
    }
    {
        // National/charset-prefixed string should be a single String token and capture charset tag
        Lexer lx("_UTF8 'hello'");
        auto toks = lx.lex();
        int string_count = 0;
        std::string charset;
        for (auto& t : toks)
            if (t.kind == TokenKind::String) {
                string_count++;
                charset = t.charset;
            }
        assert(string_count == 1);
        assert(charset == "UTF8");
    }
    {
        // N'...' shorthand
        Lexer lx("N'Olá'");
        auto toks = lx.lex();
        int string_count = 0;
        std::string cs;
        for (auto& t : toks)
            if (t.kind == TokenKind::String) {
                string_count++;
                cs = t.charset;
            }
        assert(string_count == 1);
        assert(cs == "N");
    }
    {
        Lexer lx("DATE 'bad'");
        auto toks = lx.lex();
        bool stayed_string = false;
        for (auto& t : toks)
            if (t.kind == TokenKind::String && t.text == "bad") {
                stayed_string = true;
                break;
            }
        assert(stayed_string);
    }
    return 0;
}
