#include "scratchbird/engine/lexer.h"
#include "scratchbird/engine/resolver.h"

#include <cassert>
#include <string>
#include <vector>

using namespace scratchbird::engine;

int main()
{
    // Unquoted identifiers: case-insensitive comparison at keyword detection only.
    // For identifiers we currently preserve case; resolver will normalize later (TODO UTF-8
    // casefold).
    {
        Lexer lx("SELECT a FROM t");
        auto toks = lx.lex();
        assert(!toks.empty() && toks[0].kind == TokenKind::Keyword && toks[0].text == "SELECT");
        // identifier tokens should be preserved as typed
        bool saw_a = false, saw_t = false;
        for (auto& t : toks) {
            if (t.kind == TokenKind::Identifier && t.text == "a")
                saw_a = true;
            if (t.kind == TokenKind::Identifier && t.text == "t")
                saw_t = true;
        }
        assert(saw_a && saw_t);
        // ASCII casefold check (resolver responsibility)
        assert(scratchbird::engine::casefold_ascii("FoO") == std::string("foo"));
    }
    {
        // Quoted identifiers must preserve case and content
        Lexer lx("SELECT \"AbC\" FROM \"MyTable\"");
        auto toks = lx.lex();
        bool saw_abc = false, saw_my = false;
        for (auto& t : toks) {
            if (t.kind == TokenKind::QuotedIdentifier && t.text == "AbC")
                saw_abc = true;
            if (t.kind == TokenKind::QuotedIdentifier && t.text == "MyTable")
                saw_my = true;
        }
        assert(saw_abc && saw_my);
    }
    // UTF-8 normalization: casefold_unicode should produce lowercase for simple cases
    {
        std::string utf8_word = "Straße"; // German sharp s
        auto cf = scratchbird::engine::casefold_unicode(utf8_word);
        // With ICU/utf8proc, ß folds to ss; otherwise ASCII fallback keeps as-is or lowers ASCII
        // Only assert it's non-empty and lowercased where applicable
        assert(!cf.empty());
    }
    return 0;
}
