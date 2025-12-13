/*
 * ScratchBird JDBC Driver
 * Copyright (c) 2025 ScratchBird Project
 */
package com.scratchbird.jdbc;

import java.sql.*;
import java.util.List;

/**
 * JDBC ResultSetMetaData implementation for ScratchBird.
 */
public class SBResultSetMetaData implements ResultSetMetaData {

    private final List<SBColumnInfo> columns;

    public SBResultSetMetaData(List<SBColumnInfo> columns) {
        this.columns = columns;
    }

    private SBColumnInfo getColumn(int column) throws SQLException {
        if (column < 1 || column > columns.size()) {
            throw new SQLException("Column index out of range: " + column, "42703");
        }
        return columns.get(column - 1);
    }

    @Override
    public int getColumnCount() throws SQLException {
        return columns.size();
    }

    @Override
    public boolean isAutoIncrement(int column) throws SQLException {
        return false;  // Would need catalog lookup
    }

    @Override
    public boolean isCaseSensitive(int column) throws SQLException {
        int type = getColumnType(column);
        return type == Types.CHAR || type == Types.VARCHAR || type == Types.LONGVARCHAR;
    }

    @Override
    public boolean isSearchable(int column) throws SQLException {
        return true;
    }

    @Override
    public boolean isCurrency(int column) throws SQLException {
        return false;
    }

    @Override
    public int isNullable(int column) throws SQLException {
        return columnNullableUnknown;
    }

    @Override
    public boolean isSigned(int column) throws SQLException {
        int type = getColumnType(column);
        return type == Types.SMALLINT || type == Types.INTEGER || type == Types.BIGINT ||
               type == Types.REAL || type == Types.DOUBLE || type == Types.NUMERIC ||
               type == Types.DECIMAL;
    }

    @Override
    public int getColumnDisplaySize(int column) throws SQLException {
        int type = getColumnType(column);
        switch (type) {
            case Types.BOOLEAN: return 5;
            case Types.SMALLINT: return 6;
            case Types.INTEGER: return 11;
            case Types.BIGINT: return 20;
            case Types.REAL: return 14;
            case Types.DOUBLE: return 24;
            case Types.NUMERIC:
            case Types.DECIMAL: return 40;
            case Types.DATE: return 10;
            case Types.TIME: return 8;
            case Types.TIMESTAMP: return 29;
            default: return 255;
        }
    }

    @Override
    public String getColumnLabel(int column) throws SQLException {
        return getColumn(column).getName();
    }

    @Override
    public String getColumnName(int column) throws SQLException {
        return getColumn(column).getName();
    }

    @Override
    public String getSchemaName(int column) throws SQLException {
        return "";  // Would need catalog lookup
    }

    @Override
    public int getPrecision(int column) throws SQLException {
        SBColumnInfo col = getColumn(column);
        int modifier = col.getTypeModifier();
        if (modifier > 4) {
            return ((modifier - 4) >> 16) & 0xFFFF;
        }
        return 0;
    }

    @Override
    public int getScale(int column) throws SQLException {
        SBColumnInfo col = getColumn(column);
        int modifier = col.getTypeModifier();
        if (modifier > 4) {
            return (modifier - 4) & 0xFFFF;
        }
        return 0;
    }

    @Override
    public String getTableName(int column) throws SQLException {
        return "";  // Would need catalog lookup
    }

    @Override
    public String getCatalogName(int column) throws SQLException {
        return "";
    }

    @Override
    public int getColumnType(int column) throws SQLException {
        SBColumnInfo col = getColumn(column);
        return mapOidToSqlType(col.getTypeOid());
    }

    @Override
    public String getColumnTypeName(int column) throws SQLException {
        SBColumnInfo col = getColumn(column);
        return mapOidToTypeName(col.getTypeOid());
    }

    @Override
    public boolean isReadOnly(int column) throws SQLException {
        return true;  // Default to read-only
    }

    @Override
    public boolean isWritable(int column) throws SQLException {
        return false;
    }

    @Override
    public boolean isDefinitelyWritable(int column) throws SQLException {
        return false;
    }

    @Override
    public String getColumnClassName(int column) throws SQLException {
        int type = getColumnType(column);
        switch (type) {
            case Types.BOOLEAN: return Boolean.class.getName();
            case Types.SMALLINT: return Short.class.getName();
            case Types.INTEGER: return Integer.class.getName();
            case Types.BIGINT: return Long.class.getName();
            case Types.REAL: return Float.class.getName();
            case Types.DOUBLE: return Double.class.getName();
            case Types.NUMERIC:
            case Types.DECIMAL: return java.math.BigDecimal.class.getName();
            case Types.CHAR:
            case Types.VARCHAR:
            case Types.LONGVARCHAR: return String.class.getName();
            case Types.BINARY:
            case Types.VARBINARY:
            case Types.LONGVARBINARY: return byte[].class.getName();
            case Types.DATE: return java.sql.Date.class.getName();
            case Types.TIME: return java.sql.Time.class.getName();
            case Types.TIMESTAMP: return java.sql.Timestamp.class.getName();
            case Types.ARRAY: return java.sql.Array.class.getName();
            default: return Object.class.getName();
        }
    }

    @Override
    public <T> T unwrap(Class<T> iface) throws SQLException {
        if (iface.isAssignableFrom(getClass())) {
            return iface.cast(this);
        }
        throw new SQLException("Cannot unwrap to " + iface.getName(), "0A000");
    }

    @Override
    public boolean isWrapperFor(Class<?> iface) throws SQLException {
        return iface.isAssignableFrom(getClass());
    }

    // ==================== Type Mapping ====================

    private int mapOidToSqlType(int oid) {
        switch (oid) {
            case 16: return Types.BOOLEAN;       // bool
            case 21: return Types.SMALLINT;      // int2
            case 23: return Types.INTEGER;       // int4
            case 20: return Types.BIGINT;        // int8
            case 700: return Types.REAL;         // float4
            case 701: return Types.DOUBLE;       // float8
            case 1700: return Types.NUMERIC;     // numeric
            case 18: return Types.CHAR;          // char
            case 25: return Types.VARCHAR;       // text
            case 1042: return Types.CHAR;        // bpchar
            case 1043: return Types.VARCHAR;     // varchar
            case 17: return Types.VARBINARY;     // bytea
            case 1082: return Types.DATE;        // date
            case 1083: return Types.TIME;        // time
            case 1114: return Types.TIMESTAMP;   // timestamp
            case 1184: return Types.TIMESTAMP_WITH_TIMEZONE; // timestamptz
            case 2950: return Types.OTHER;       // uuid
            case 114: return Types.OTHER;        // json
            case 3802: return Types.OTHER;       // jsonb
            case 1007: return Types.ARRAY;       // int4[]
            case 1009: return Types.ARRAY;       // text[]
            case 869: return Types.OTHER;        // inet
            case 650: return Types.OTHER;        // cidr
            case 829: return Types.OTHER;        // macaddr
            default: return Types.OTHER;
        }
    }

    private String mapOidToTypeName(int oid) {
        switch (oid) {
            case 16: return "boolean";
            case 21: return "smallint";
            case 23: return "integer";
            case 20: return "bigint";
            case 700: return "real";
            case 701: return "double precision";
            case 1700: return "numeric";
            case 18: return "char";
            case 25: return "text";
            case 1042: return "bpchar";
            case 1043: return "varchar";
            case 17: return "bytea";
            case 1082: return "date";
            case 1083: return "time";
            case 1114: return "timestamp";
            case 1184: return "timestamptz";
            case 2950: return "uuid";
            case 114: return "json";
            case 3802: return "jsonb";
            case 1007: return "integer[]";
            case 1009: return "text[]";
            case 869: return "inet";
            case 650: return "cidr";
            case 829: return "macaddr";
            default: return "unknown";
        }
    }
}
