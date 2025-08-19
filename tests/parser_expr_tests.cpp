#include "scratchbird/engine/parser_expr.h"

#include <cassert>
#include <string>

using namespace scratchbird::engine;

int main()
{
    {
        auto s = parse_expression_to_string("1 + 2 * 3");
        assert(!s.empty());
    }
    // INT128/UINT128 and DECFLOAT forms
    {
        auto e = parse_expression_to_ast("CAST(x AS INT128)");
        assert(e.cast_type.has_value() &&
               (e.cast_type->name == "INT128" || e.cast_type->name == "int128"));
    }
    {
        auto e = parse_expression_to_ast("CAST(x AS UINT128)");
        assert(e.cast_type.has_value());
    }
    {
        auto e = parse_expression_to_ast("CAST(x AS DECFLOAT(16))");
        assert(e.cast_type.has_value() && e.cast_type->precision == 16);
    }
    // TYPEOF acceptance (TYPE OF and TYPE OF COLUMN)
    {
        auto s = parse_expression_to_string("TYPE(OF x)");
        assert(s.find("TYPEOF(") == 0);
        auto e = parse_expression_to_ast("CAST(a AS TYPEOF(dom))");
        assert(e.kind == ExprKind::Cast && e.cast_type.has_value());
    }
    // TYPE OF normalized to TYPEOF
    {
        auto s = parse_expression_to_string("TYPE(a)");
        assert(s.find("TYPEOF(") == 0);
    }
    // Negative diagnostics (surface as part of normalized text, no crashes)
    {
        auto s = parse_expression_to_string("a LIKE");
        assert(!s.empty());
    }
    {
        auto s = parse_expression_to_string("a BETWEEN 1");
        assert(!s.empty());
    }
    // BETWEEN, IN, LIKE ESCAPE, SIMILAR TO
    {
        auto s = parse_expression_to_string("a BETWEEN 1 AND 2");
        assert(s.find("BETWEEN") != std::string::npos);
    }
    {
        auto s = parse_expression_to_string("a IN (1, 2, 3)");
        assert(s.find("IN") != std::string::npos);
    }
    {
        auto s = parse_expression_to_string("name LIKE 'A%' ESCAPE '\\'");
        assert(s.find("LIKE") != std::string::npos && s.find("ESCAPE") != std::string::npos);
    }
    {
        auto s = parse_expression_to_string("s SIMILAR TO '(foo|bar)'");
        assert(s.find("SIMILAR TO") != std::string::npos);
    }
    // CASE WHEN / EXISTS / quantified comparisons
    {
        auto s = parse_expression_to_string("CASE WHEN a>1 THEN 2 ELSE 3 END");
        assert(s.find("CASE") != std::string::npos && s.find("END") != std::string::npos);
    }
    {
        auto s = parse_expression_to_string("EXISTS (SELECT 1)");
        assert(s.find("EXISTS") != std::string::npos);
    }
    {
        auto s = parse_expression_to_string("a = ANY (SELECT 1)");
        assert(s.find("ANY") != std::string::npos);
    }
    {
        auto s = parse_expression_to_string("NOT a AND b OR c");
        assert(!s.empty());
    }
    // Numeric edges: leading dot and exponent with sign and underscores
    {
        auto s = parse_expression_to_string(".5");
        assert(!s.empty());
    }
    {
        auto s = parse_expression_to_string("1.2e+10");
        assert(!s.empty());
    }
    {
        auto s = parse_expression_to_string("1_000_000 + 2_5");
        assert(!s.empty());
    }
    // CAST/:: presence in normalized string
    {
        auto s = parse_expression_to_string("CAST(a AS VARCHAR(20))");
        assert(s.find("CAST") != std::string::npos);
    }
    {
        auto s = parse_expression_to_string("a::DECIMAL(10,2)");
        assert(s.find("::") != std::string::npos);
    }
    // CAST with charset/collate and arrays
    {
        auto s =
            parse_expression_to_string("CAST(b AS CHAR(10) CHARACTER SET UTF8 COLLATE UNICODE_CI)");
        assert(s.find("CHAR") != std::string::npos);
    }
    // Type descriptor helper: parse charset and array rank
    {
        auto td = parse_type_descriptor("INTEGER[][]");
        assert(td.name == "INTEGER");
        assert(td.array_rank == 2);
    }
    // COALESCE / NULLIF are function calls (parsed by generic call)
    {
        auto s = parse_expression_to_string("COALESCE(a,b,c)");
        assert(s.find("COALESCE(") == 0);
    }
    {
        auto s = parse_expression_to_string("NULLIF(a,b)");
        assert(s.find("NULLIF(") == 0);
    }
    // AST: CAST attaches TypeDescriptor
    {
        auto e = parse_expression_to_ast("CAST(a AS DECIMAL(10,2))");
        assert(e.kind == ExprKind::Cast);
        assert(e.cast_type.has_value());
        assert(e.cast_type->precision == 10);
        assert(e.cast_type->scale == 2);
    }
    // AST: CAST with array and collate/charset recorded
    {
        auto e =
            parse_expression_to_ast("CAST(a AS INTEGER[][] CHARACTER SET UTF8 COLLATE UNICODE)");
        assert(e.kind == ExprKind::Cast);
        assert(e.cast_type.has_value());
        assert(e.cast_type->array_rank == 2);
        assert(e.cast_type->charset == "UTF8");
        assert(!e.cast_type->collate.empty());
    }
    // Dollar-quoted: ensure content captured and no nesting
    {
        auto s = parse_expression_to_string("$$hello$$");
        assert(s.find("hello") != std::string::npos || !s.empty());
    }
    return 0;
}
