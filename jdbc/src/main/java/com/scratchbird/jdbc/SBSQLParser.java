/*
 * ScratchBird JDBC Driver
 * Copyright (c) 2025 ScratchBird Project
 */
package com.scratchbird.jdbc;

/**
 * SQL parser utilities for JDBC escape sequences.
 */
public class SBSQLParser {

    /**
     * Converts JDBC escape sequences to native SQL.
     *
     * Supported escape sequences:
     * - {fn function_name(args)} - Scalar functions
     * - {d 'yyyy-mm-dd'} - Date literal
     * - {t 'hh:mm:ss'} - Time literal
     * - {ts 'yyyy-mm-dd hh:mm:ss'} - Timestamp literal
     * - {escape 'char'} - LIKE escape character
     * - {oj outer-join} - Outer join
     * - {call proc(args)} - Stored procedure call
     */
    public static String convertToNativeSQL(String sql) {
        if (sql == null || !sql.contains("{")) {
            return sql;
        }

        StringBuilder result = new StringBuilder();
        int i = 0;
        int len = sql.length();

        while (i < len) {
            char c = sql.charAt(i);

            if (c == '{') {
                // Find matching close brace
                int braceCount = 1;
                int start = i;
                i++;

                while (i < len && braceCount > 0) {
                    c = sql.charAt(i);
                    if (c == '{') braceCount++;
                    else if (c == '}') braceCount--;
                    i++;
                }

                if (braceCount == 0) {
                    String escape = sql.substring(start + 1, i - 1).trim();
                    result.append(processEscape(escape));
                } else {
                    // Unmatched brace, keep as-is
                    result.append(sql.substring(start));
                }
            } else {
                result.append(c);
                i++;
            }
        }

        return result.toString();
    }

    private static String processEscape(String escape) {
        if (escape.isEmpty()) {
            return "{}";
        }

        String lower = escape.toLowerCase();

        // Date literal: {d 'yyyy-mm-dd'}
        if (lower.startsWith("d ")) {
            String dateStr = escape.substring(2).trim();
            return "DATE " + dateStr;
        }

        // Time literal: {t 'hh:mm:ss'}
        if (lower.startsWith("t ")) {
            String timeStr = escape.substring(2).trim();
            return "TIME " + timeStr;
        }

        // Timestamp literal: {ts 'yyyy-mm-dd hh:mm:ss'}
        if (lower.startsWith("ts ")) {
            String tsStr = escape.substring(3).trim();
            return "TIMESTAMP " + tsStr;
        }

        // Escape character: {escape 'char'}
        if (lower.startsWith("escape ")) {
            String escapeChar = escape.substring(7).trim();
            return "ESCAPE " + escapeChar;
        }

        // Outer join: {oj table1 LEFT OUTER JOIN table2 ON ...}
        if (lower.startsWith("oj ")) {
            return escape.substring(3).trim();
        }

        // Scalar function: {fn func(args)}
        if (lower.startsWith("fn ")) {
            String func = escape.substring(3).trim();
            return convertFunction(func);
        }

        // Stored procedure call: {call proc(args)}
        if (lower.startsWith("call ")) {
            String proc = escape.substring(5).trim();
            return "CALL " + proc;
        }

        // Unknown escape, return as-is
        return "{" + escape + "}";
    }

    private static String convertFunction(String func) {
        String lower = func.toLowerCase();

        // String functions
        if (lower.startsWith("concat(")) {
            return func;  // Native support
        }
        if (lower.startsWith("length(")) {
            return "LENGTH" + func.substring(6);
        }
        if (lower.startsWith("lcase(")) {
            return "LOWER" + func.substring(5);
        }
        if (lower.startsWith("ucase(")) {
            return "UPPER" + func.substring(5);
        }
        if (lower.startsWith("substring(")) {
            return "SUBSTRING" + func.substring(9);
        }
        if (lower.startsWith("ltrim(")) {
            return "LTRIM" + func.substring(5);
        }
        if (lower.startsWith("rtrim(")) {
            return "RTRIM" + func.substring(5);
        }

        // Numeric functions
        if (lower.startsWith("abs(")) {
            return "ABS" + func.substring(3);
        }
        if (lower.startsWith("mod(")) {
            return "MOD" + func.substring(3);
        }
        if (lower.startsWith("sqrt(")) {
            return "SQRT" + func.substring(4);
        }
        if (lower.startsWith("power(")) {
            return "POWER" + func.substring(5);
        }
        if (lower.startsWith("ceiling(")) {
            return "CEILING" + func.substring(7);
        }
        if (lower.startsWith("floor(")) {
            return "FLOOR" + func.substring(5);
        }
        if (lower.startsWith("round(")) {
            return "ROUND" + func.substring(5);
        }
        if (lower.startsWith("truncate(")) {
            return "TRUNC" + func.substring(8);
        }

        // Date/time functions
        if (lower.equals("curdate()")) {
            return "CURRENT_DATE";
        }
        if (lower.equals("curtime()")) {
            return "CURRENT_TIME";
        }
        if (lower.equals("now()")) {
            return "CURRENT_TIMESTAMP";
        }
        if (lower.startsWith("dayofmonth(")) {
            return "EXTRACT(DAY FROM " + func.substring(11, func.length() - 1) + ")";
        }
        if (lower.startsWith("month(")) {
            return "EXTRACT(MONTH FROM " + func.substring(6, func.length() - 1) + ")";
        }
        if (lower.startsWith("year(")) {
            return "EXTRACT(YEAR FROM " + func.substring(5, func.length() - 1) + ")";
        }
        if (lower.startsWith("hour(")) {
            return "EXTRACT(HOUR FROM " + func.substring(5, func.length() - 1) + ")";
        }
        if (lower.startsWith("minute(")) {
            return "EXTRACT(MINUTE FROM " + func.substring(7, func.length() - 1) + ")";
        }
        if (lower.startsWith("second(")) {
            return "EXTRACT(SECOND FROM " + func.substring(7, func.length() - 1) + ")";
        }

        // System functions
        if (lower.equals("database()")) {
            return "CURRENT_DATABASE()";
        }
        if (lower.equals("user()")) {
            return "CURRENT_USER";
        }

        // Default: return as-is
        return func;
    }
}
