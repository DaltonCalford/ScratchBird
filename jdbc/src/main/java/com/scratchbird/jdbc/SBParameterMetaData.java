/*
 * ScratchBird JDBC Driver
 * Copyright (c) 2025 ScratchBird Project
 */
package com.scratchbird.jdbc;

import java.sql.*;
import java.util.List;

/**
 * JDBC ParameterMetaData implementation for ScratchBird.
 */
public class SBParameterMetaData implements ParameterMetaData {
    private final int parameterCount;
    private final List<Integer> parameterTypes;

    public SBParameterMetaData(int parameterCount, List<Integer> parameterTypes) {
        this.parameterCount = parameterCount;
        this.parameterTypes = parameterTypes;
    }

    @Override
    public int getParameterCount() throws SQLException {
        return parameterCount;
    }

    @Override
    public int isNullable(int param) throws SQLException {
        checkParam(param);
        return parameterNullableUnknown;
    }

    @Override
    public boolean isSigned(int param) throws SQLException {
        checkParam(param);
        int type = getParameterType(param);
        return type == Types.SMALLINT || type == Types.INTEGER || type == Types.BIGINT ||
               type == Types.REAL || type == Types.DOUBLE || type == Types.NUMERIC ||
               type == Types.DECIMAL;
    }

    @Override
    public int getPrecision(int param) throws SQLException {
        checkParam(param);
        return 0;
    }

    @Override
    public int getScale(int param) throws SQLException {
        checkParam(param);
        return 0;
    }

    @Override
    public int getParameterType(int param) throws SQLException {
        checkParam(param);
        if (param <= parameterTypes.size()) {
            return parameterTypes.get(param - 1);
        }
        return Types.VARCHAR;
    }

    @Override
    public String getParameterTypeName(int param) throws SQLException {
        int type = getParameterType(param);
        switch (type) {
            case Types.BOOLEAN: return "boolean";
            case Types.SMALLINT: return "smallint";
            case Types.INTEGER: return "integer";
            case Types.BIGINT: return "bigint";
            case Types.REAL: return "real";
            case Types.DOUBLE: return "double precision";
            case Types.NUMERIC: return "numeric";
            case Types.VARCHAR: return "varchar";
            case Types.DATE: return "date";
            case Types.TIME: return "time";
            case Types.TIMESTAMP: return "timestamp";
            case Types.VARBINARY: return "bytea";
            default: return "unknown";
        }
    }

    @Override
    public String getParameterClassName(int param) throws SQLException {
        int type = getParameterType(param);
        switch (type) {
            case Types.BOOLEAN: return Boolean.class.getName();
            case Types.SMALLINT: return Short.class.getName();
            case Types.INTEGER: return Integer.class.getName();
            case Types.BIGINT: return Long.class.getName();
            case Types.REAL: return Float.class.getName();
            case Types.DOUBLE: return Double.class.getName();
            case Types.NUMERIC: return java.math.BigDecimal.class.getName();
            case Types.VARCHAR: return String.class.getName();
            case Types.DATE: return java.sql.Date.class.getName();
            case Types.TIME: return java.sql.Time.class.getName();
            case Types.TIMESTAMP: return java.sql.Timestamp.class.getName();
            case Types.VARBINARY: return byte[].class.getName();
            default: return Object.class.getName();
        }
    }

    @Override
    public int getParameterMode(int param) throws SQLException {
        checkParam(param);
        return parameterModeIn;
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

    private void checkParam(int param) throws SQLException {
        if (param < 1 || param > parameterCount) {
            throw new SQLException("Parameter index out of range: " + param, "07009");
        }
    }
}
