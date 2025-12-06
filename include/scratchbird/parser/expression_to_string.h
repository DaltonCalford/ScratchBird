/**
 * expression_to_string.h - Convert parser Expression AST to SQL string
 *
 * OPT-L2: Infrastructure for converting Expression AST nodes back to SQL
 * string representation. Used by EXPLAIN to show actual filter expressions
 * instead of placeholders.
 *
 * Supported expression types:
 * - LiteralExpr (INTEGER, FLOAT, STRING, NULL, RANGE)
 * - IdentifierExpr (qualified and unqualified)
 * - BinaryOpExpr (arithmetic, comparison, logical, array, regex operators)
 * - CastExpr (CAST and TRY_CAST)
 * - FunctionCallExpr
 * - SequenceFunctionExpr (NEXTVAL, CURRVAL, SETVAL)
 * - ExtractExpr
 * - AggregateExpr
 * - CoalesceExpr
 * - NullIfExpr
 * - CaseExpr (simple and searched)
 * - SubqueryExpr (limited support - shows placeholder)
 * - ArrayLiteral
 */

#ifndef SCRATCHBIRD_PARSER_EXPRESSION_TO_STRING_H
#define SCRATCHBIRD_PARSER_EXPRESSION_TO_STRING_H

#include "scratchbird/parser/ast.h"
#include <string>

namespace scratchbird::parser
{

/**
 * Convert an Expression AST node to SQL string representation
 *
 * @param expr Expression to convert
 * @param string_pool String pool for resolving StringIds
 * @return SQL string representation of the expression
 */
std::string expressionToString(const Expression* expr, const StringPool& string_pool);

/**
 * Convert a BinaryOp enum to SQL operator string
 *
 * @param op Binary operator
 * @return SQL operator string (e.g., "=", "<>", "AND", etc.)
 */
const char* binaryOpToString(BinaryOp op);

/**
 * Convert an AggregateFunc enum to SQL function name
 *
 * @param func Aggregate function
 * @return SQL function name (e.g., "COUNT", "SUM", etc.)
 */
const char* aggregateFuncToString(AggregateFunc func);

/**
 * Convert a DataType to SQL type name for CAST expressions
 *
 * @param type Data type
 * @param precision Type precision (for VARCHAR, DECIMAL, etc.)
 * @param scale Type scale (for DECIMAL)
 * @return SQL type name
 */
std::string dataTypeToSqlString(core::DataType type, uint32_t precision = 0, uint32_t scale = 0);

} // namespace scratchbird::parser

#endif // SCRATCHBIRD_PARSER_EXPRESSION_TO_STRING_H
