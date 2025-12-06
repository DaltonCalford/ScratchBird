/**
 * expression_to_string.cpp - Convert parser Expression AST to SQL string
 *
 * OPT-L2: Implementation of expression-to-string conversion for EXPLAIN output.
 */

#include "scratchbird/parser/expression_to_string.h"
#include <sstream>

namespace scratchbird::parser
{

const char* binaryOpToString(BinaryOp op)
{
    switch (op)
    {
        case BinaryOp::ADD: return "+";
        case BinaryOp::SUBTRACT: return "-";
        case BinaryOp::MULTIPLY: return "*";
        case BinaryOp::DIVIDE: return "/";
        case BinaryOp::MODULO: return "%";
        case BinaryOp::EQ: return "=";
        case BinaryOp::NE: return "<>";
        case BinaryOp::LT: return "<";
        case BinaryOp::GT: return ">";
        case BinaryOp::LE: return "<=";
        case BinaryOp::GE: return ">=";
        case BinaryOp::AND: return "AND";
        case BinaryOp::OR: return "OR";
        case BinaryOp::LIKE: return "LIKE";
        case BinaryOp::ILIKE: return "ILIKE";
        case BinaryOp::IN: return "IN";
        case BinaryOp::NOT_IN: return "NOT IN";
        case BinaryOp::ARRAY_OVERLAP: return "&&";
        case BinaryOp::ARRAY_CONTAINS: return "@>";
        case BinaryOp::ARRAY_CONTAINED_BY: return "<@";
        case BinaryOp::REGEX_MATCH: return "~";
        case BinaryOp::REGEX_MATCH_CI: return "~*";
        case BinaryOp::REGEX_NOT_MATCH: return "!~";
        case BinaryOp::REGEX_NOT_MATCH_CI: return "!~*";
        case BinaryOp::RANGE_STRICTLY_LEFT: return "<<";
        case BinaryOp::RANGE_STRICTLY_RIGHT: return ">>";
        case BinaryOp::RANGE_ADJACENT: return "-|-";
        default: return "?";
    }
}

const char* aggregateFuncToString(AggregateFunc func)
{
    switch (func)
    {
        case AggregateFunc::COUNT: return "COUNT";
        case AggregateFunc::SUM: return "SUM";
        case AggregateFunc::AVG: return "AVG";
        case AggregateFunc::MIN: return "MIN";
        case AggregateFunc::MAX: return "MAX";
        case AggregateFunc::ARRAY_AGG: return "ARRAY_AGG";
        default: return "UNKNOWN";
    }
}

std::string dataTypeToSqlString(core::DataType type, uint32_t precision, uint32_t scale)
{
    switch (type)
    {
        case core::DataType::INT8: return "TINYINT";
        case core::DataType::INT16: return "SMALLINT";
        case core::DataType::INT32: return "INTEGER";
        case core::DataType::INT64: return "BIGINT";
        case core::DataType::UINT8: return "TINYINT UNSIGNED";
        case core::DataType::UINT16: return "SMALLINT UNSIGNED";
        case core::DataType::UINT32: return "INTEGER UNSIGNED";
        case core::DataType::UINT64: return "BIGINT UNSIGNED";
        case core::DataType::FLOAT32: return "REAL";
        case core::DataType::FLOAT64: return "DOUBLE PRECISION";
        case core::DataType::DECIMAL:
            if (precision > 0 && scale > 0)
                return "DECIMAL(" + std::to_string(precision) + "," + std::to_string(scale) + ")";
            else if (precision > 0)
                return "DECIMAL(" + std::to_string(precision) + ")";
            return "DECIMAL";
        case core::DataType::MONEY: return "MONEY";
        case core::DataType::BOOLEAN: return "BOOLEAN";
        case core::DataType::VARCHAR:
            if (precision > 0)
                return "VARCHAR(" + std::to_string(precision) + ")";
            return "VARCHAR";
        case core::DataType::CHAR:
            if (precision > 0)
                return "CHAR(" + std::to_string(precision) + ")";
            return "CHAR";
        case core::DataType::TEXT: return "TEXT";
        case core::DataType::BYTEA: return "BYTEA";
        case core::DataType::BLOB: return "BLOB";
        case core::DataType::DATE: return "DATE";
        case core::DataType::TIME: return "TIME";
        case core::DataType::TIMESTAMP: return "TIMESTAMP";
        // Note: TIMESTAMP with timezone handled by TypeInfo.with_timezone flag
        case core::DataType::INTERVAL: return "INTERVAL";
        case core::DataType::UUID: return "UUID";
        case core::DataType::JSON: return "JSON";
        case core::DataType::JSONB: return "JSONB";
        case core::DataType::XML: return "XML";
        // Spatial types
        case core::DataType::POINT: return "POINT";
        case core::DataType::LINESTRING: return "LINESTRING";
        case core::DataType::POLYGON: return "POLYGON";
        case core::DataType::MULTIPOINT: return "MULTIPOINT";
        case core::DataType::MULTILINESTRING: return "MULTILINESTRING";
        case core::DataType::MULTIPOLYGON: return "MULTIPOLYGON";
        case core::DataType::GEOMETRYCOLLECTION: return "GEOMETRYCOLLECTION";
        // Network types
        case core::DataType::INET: return "INET";
        case core::DataType::CIDR: return "CIDR";
        case core::DataType::MACADDR: return "MACADDR";
        case core::DataType::MACADDR8: return "MACADDR8";
        // Text search types
        case core::DataType::TSVECTOR: return "TSVECTOR";
        case core::DataType::TSQUERY: return "TSQUERY";
        // Range types
        case core::DataType::INT4RANGE: return "INT4RANGE";
        case core::DataType::INT8RANGE: return "INT8RANGE";
        case core::DataType::NUMRANGE: return "NUMRANGE";
        case core::DataType::TSRANGE: return "TSRANGE";
        case core::DataType::TSTZRANGE: return "TSTZRANGE";
        case core::DataType::DATERANGE: return "DATERANGE";
        // Collection types
        case core::DataType::ARRAY: return "ARRAY";
        case core::DataType::COMPOSITE: return "COMPOSITE";
        // Other types
        case core::DataType::VECTOR: return "VECTOR";
        case core::DataType::VARIANT: return "VARIANT";
        case core::DataType::NULL_TYPE: return "NULL";
        case core::DataType::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}

std::string expressionToString(const Expression* expr, const StringPool& string_pool)
{
    if (!expr)
    {
        return "";
    }

    std::ostringstream ss;

    switch (expr->kind())
    {
        case ASTKind::LITERAL:
        {
            auto literal = static_cast<const LiteralExpr*>(expr);
            switch (literal->literalType())
            {
                case LiteralExpr::INTEGER:
                    ss << literal->intValue();
                    break;
                case LiteralExpr::FLOAT:
                    ss << literal->floatValue();
                    break;
                case LiteralExpr::STRING:
                    ss << "'" << string_pool.get(literal->stringValue()) << "'";
                    break;
                case LiteralExpr::NULL_LITERAL:
                    ss << "NULL";
                    break;
                case LiteralExpr::RANGE:
                    ss << string_pool.get(literal->rangeValue());
                    break;
            }
            break;
        }

        case ASTKind::IDENTIFIER:
        {
            auto ident = static_cast<const IdentifierExpr*>(expr);
            if (ident->isQualified())
            {
                ss << string_pool.get(ident->qualifier()) << ".";
            }
            ss << string_pool.get(ident->name());
            break;
        }

        case ASTKind::BINARY_OP:
        {
            auto binop = static_cast<const BinaryOpExpr*>(expr);
            bool needs_parens = (binop->op() == BinaryOp::AND || binop->op() == BinaryOp::OR);

            // Special handling for IN/NOT IN (right side is typically a subquery or list)
            if (binop->op() == BinaryOp::IN || binop->op() == BinaryOp::NOT_IN)
            {
                ss << "(" << expressionToString(binop->left(), string_pool) << " "
                   << binaryOpToString(binop->op()) << " "
                   << expressionToString(binop->right(), string_pool) << ")";
            }
            else
            {
                ss << "(";
                if (needs_parens && binop->left()->kind() == ASTKind::BINARY_OP)
                {
                    ss << expressionToString(binop->left(), string_pool);
                }
                else
                {
                    ss << expressionToString(binop->left(), string_pool);
                }

                ss << " " << binaryOpToString(binop->op()) << " ";

                if (needs_parens && binop->right()->kind() == ASTKind::BINARY_OP)
                {
                    ss << expressionToString(binop->right(), string_pool);
                }
                else
                {
                    ss << expressionToString(binop->right(), string_pool);
                }
                ss << ")";
            }
            break;
        }

        case ASTKind::CAST:
        {
            auto cast = static_cast<const CastExpr*>(expr);
            if (cast->isTryCast())
            {
                ss << "TRY_CAST(";
            }
            else
            {
                ss << "CAST(";
            }
            ss << expressionToString(cast->expr(), string_pool) << " AS "
               << dataTypeToSqlString(cast->targetType().type,
                                      cast->targetType().precision,
                                      cast->targetType().scale) << ")";
            break;
        }

        case ASTKind::FUNCTION_CALL:
        {
            auto func = static_cast<const FunctionCallExpr*>(expr);
            ss << string_pool.get(func->name()) << "(";
            const auto& args = func->args();
            for (size_t i = 0; i < args.size(); ++i)
            {
                if (i > 0) ss << ", ";
                ss << expressionToString(args[i], string_pool);
            }
            ss << ")";
            break;
        }

        case ASTKind::SEQUENCE_FUNCTION:
        {
            auto seq = static_cast<const SequenceFunctionExpr*>(expr);
            switch (seq->functionType())
            {
                case SequenceFunctionType::NEXTVAL:
                    ss << "NEXTVAL(" << expressionToString(seq->sequenceName(), string_pool) << ")";
                    break;
                case SequenceFunctionType::CURRVAL:
                    ss << "CURRVAL(" << expressionToString(seq->sequenceName(), string_pool) << ")";
                    break;
                case SequenceFunctionType::SETVAL:
                    ss << "SETVAL(" << expressionToString(seq->sequenceName(), string_pool) << ", "
                       << expressionToString(seq->value(), string_pool);
                    if (seq->isCalled())
                    {
                        ss << ", " << expressionToString(seq->isCalled(), string_pool);
                    }
                    ss << ")";
                    break;
            }
            break;
        }

        case ASTKind::EXTRACT:
        {
            auto extract = static_cast<const ExtractExpr*>(expr);
            ss << "EXTRACT(" << extract->fieldName() << " FROM "
               << expressionToString(extract->source(), string_pool) << ")";
            break;
        }

        case ASTKind::AGGREGATE_FUNC:
        {
            auto agg = static_cast<const AggregateExpr*>(expr);
            ss << aggregateFuncToString(agg->func()) << "(";
            if (agg->distinct())
            {
                ss << "DISTINCT ";
            }
            if (agg->arg())
            {
                ss << expressionToString(agg->arg(), string_pool);
            }
            else
            {
                ss << "*";  // COUNT(*)
            }
            ss << ")";
            break;
        }

        case ASTKind::COALESCE:
        {
            auto coalesce = static_cast<const CoalesceExpr*>(expr);
            ss << "COALESCE(";
            const auto& args = coalesce->args();
            for (size_t i = 0; i < args.size(); ++i)
            {
                if (i > 0) ss << ", ";
                ss << expressionToString(args[i], string_pool);
            }
            ss << ")";
            break;
        }

        case ASTKind::NULLIF:
        {
            auto nullif = static_cast<const NullIfExpr*>(expr);
            ss << "NULLIF(" << expressionToString(nullif->expr1(), string_pool)
               << ", " << expressionToString(nullif->expr2(), string_pool) << ")";
            break;
        }

        case ASTKind::CASE:
        {
            auto case_expr = static_cast<const CaseExpr*>(expr);
            ss << "CASE";
            if (case_expr->isSimpleCase())
            {
                ss << " " << expressionToString(case_expr->caseOperand(), string_pool);
            }
            for (const auto& when_clause : case_expr->whenClauses())
            {
                ss << " WHEN " << expressionToString(when_clause.condition, string_pool)
                   << " THEN " << expressionToString(when_clause.result, string_pool);
            }
            if (case_expr->elseResult())
            {
                ss << " ELSE " << expressionToString(case_expr->elseResult(), string_pool);
            }
            ss << " END";
            break;
        }

        case ASTKind::SUBQUERY:
        {
            auto subq = static_cast<const SubqueryExpr*>(expr);
            switch (subq->type())
            {
                case SubqueryType::EXISTS:
                    ss << "EXISTS (subquery)";
                    break;
                case SubqueryType::IN:
                    ss << "(subquery)";
                    break;
                case SubqueryType::NOT_IN:
                    ss << "(subquery)";
                    break;
                case SubqueryType::ARRAY:
                    ss << "ARRAY(subquery)";
                    break;
                default:
                    ss << "(subquery)";
                    break;
            }
            break;
        }

        case ASTKind::GROUPING:
        {
            auto grouping = static_cast<const GroupingExpr*>(expr);
            ss << "GROUPING(" << expressionToString(grouping->arg(), string_pool) << ")";
            break;
        }

        default:
            // Fallback for unhandled expression types
            ss << "(expr)";
            break;
    }

    return ss.str();
}

} // namespace scratchbird::parser
