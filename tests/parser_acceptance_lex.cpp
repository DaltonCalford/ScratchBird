#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

// Minimal declarations from lexer.cpp
namespace scratchbird::engine
{
    enum class TokenKind {
        Identifier,
        QuotedIdentifier,
        Integer,
        Decimal,
        String,
        Symbol,
        Keyword,
        End
    };
    struct Token {
        TokenKind kind;
        std::string text;
    };
    class Lexer
    {
      public:
        explicit Lexer(std::string_view input);
        std::vector<Token> lex();
    };
} // namespace scratchbird::engine

using namespace scratchbird::engine;

static bool contains_acceptance_line(const std::string& line)
{
    // Very simple: look for a YAML list dash, then a quoted SQL starting with
    // SELECT/INSERT/UPDATE/DELETE/MERGE/WITH
    auto pos = line.find("- ");
    if (pos == std::string::npos)
        return false;
    auto q = line.find_first_of("'\"", pos + 2);
    if (q == std::string::npos)
        return false;
    std::string sql = line.substr(q + 1);
    // Trim trailing quote
    if (!sql.empty() && (sql.back() == '\'' || sql.back() == '"'))
        sql.pop_back();
    // quick uppercase copy
    std::string up(sql);
    for (char& c : up)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return up.rfind("SELECT", 0) == 0 || up.rfind("INSERT", 0) == 0 || up.rfind("UPDATE", 0) == 0 ||
           up.rfind("DELETE", 0) == 0 || up.rfind("MERGE", 0) == 0 || up.rfind("WITH", 0) == 0;
}

static std::string extract_sql(const std::string& line)
{
    auto q1 = line.find_first_of("'\"");
    if (q1 == std::string::npos)
        return {};
    auto q2 = line.find_last_of("'\"");
    if (q2 == std::string::npos || q2 <= q1)
        return {};
    return line.substr(q1 + 1, q2 - q1 - 1);
}

int main()
{
    namespace fs = std::filesystem;
    const fs::path specs_root = fs::path("specs");
    size_t tested = 0;
    for (auto& dir : {fs::path("sql"), fs::path("ddl")}) {
        fs::path p = specs_root / dir;
        if (!fs::exists(p))
            continue;
        for (auto& entry : fs::directory_iterator(p)) {
            if (!entry.is_regular_file())
                continue;
            if (entry.path().extension() != ".yaml")
                continue;
            std::ifstream in(entry.path());
            if (!in)
                continue;
            std::string line;
            while (std::getline(in, line)) {
                if (!contains_acceptance_line(line))
                    continue;
                std::string sql = extract_sql(line);
                if (sql.empty())
                    continue;
                Lexer lx(sql);
                auto tokens = lx.lex();
                // Minimal assertion: at least one non-End token produced
                bool has_real = false;
                for (const auto& t : tokens) {
                    if (t.kind != TokenKind::End) {
                        has_real = true;
                        break;
                    }
                }
                if (!has_real) {
                    std::cerr << "Tokenization failed for: " << sql << " from " << entry.path()
                              << "\n";
                    return 1;
                }
                // If it starts with SELECT, assert first token is SELECT keyword
                std::string up(sql);
                for (char& c : up)
                    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                if (up.rfind("SELECT", 0) == 0) {
                    if (tokens.empty() || tokens[0].kind != TokenKind::Keyword ||
                        tokens[0].text != "SELECT") {
                        std::cerr << "Expected SELECT keyword as first token for: " << sql << "\n";
                        return 1;
                    }
                }
                if (up.rfind("INSERT", 0) == 0) {
                    if (tokens.empty() || tokens[0].kind != TokenKind::Keyword ||
                        tokens[0].text != "INSERT") {
                        std::cerr << "Expected INSERT keyword as first token for: " << sql << "\n";
                        return 1;
                    }
                }
                if (up.rfind("UPDATE", 0) == 0) {
                    if (tokens.empty() || tokens[0].kind != TokenKind::Keyword ||
                        tokens[0].text != "UPDATE") {
                        std::cerr << "Expected UPDATE keyword as first token for: " << sql << "\n";
                        return 1;
                    }
                }
                if (up.rfind("DELETE", 0) == 0) {
                    if (tokens.empty() || tokens[0].kind != TokenKind::Keyword ||
                        tokens[0].text != "DELETE") {
                        std::cerr << "Expected DELETE keyword as first token for: " << sql << "\n";
                        return 1;
                    }
                }
                if (up.rfind("WITH", 0) == 0) {
                    if (tokens.empty() || tokens[0].kind != TokenKind::Keyword ||
                        tokens[0].text != "WITH") {
                        std::cerr << "Expected WITH keyword as first token for: " << sql << "\n";
                        return 1;
                    }
                }

                // Simple sanity: if SQL contains SKIP/FIRST/ROWS, ensure they appear as keyword
                // tokens
                auto need_check = [&](const char* word) {
                    std::string W(word);
                    return up.find(W) != std::string::npos;
                };
                if (need_check(" SKIP") || need_check("FIRST") || need_check(" ROWS")) {
                    auto has_kw = [&](const char* w) {
                        for (auto& t : tokens) {
                            if (t.kind == TokenKind::Keyword && t.text == w)
                                return true;
                        }
                        return false;
                    };
                    if (need_check(" SKIP") && !has_kw("SKIP")) {
                        std::cerr << "Expected SKIP token in: " << sql << "\n";
                        return 1;
                    }
                    if (need_check("FIRST") && !has_kw("FIRST")) {
                        std::cerr << "Expected FIRST token in: " << sql << "\n";
                        return 1;
                    }
                    if (need_check(" ROWS") && !has_kw("ROWS")) {
                        std::cerr << "Expected ROWS token in: " << sql << "\n";
                        return 1;
                    }
                }
                ++tested;
            }
        }
    }
    std::cout << "Acceptance tokenization OK: " << tested << " cases\n";
    return 0;
}
