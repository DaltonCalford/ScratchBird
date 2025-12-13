/*
 * ScratchBird JDBC Driver
 * Copyright (c) 2025 ScratchBird Project
 */
package com.scratchbird.jdbc;

import java.sql.*;
import java.util.*;

/**
 * JDBC Array implementation for ScratchBird.
 */
public class SBArray implements Array {
    private final String typeName;
    private Object[] elements;

    public SBArray(String typeName, Object[] elements) {
        this.typeName = typeName;
        this.elements = elements != null ? elements.clone() : new Object[0];
    }

    @Override
    public String getBaseTypeName() throws SQLException {
        return typeName;
    }

    @Override
    public int getBaseType() throws SQLException {
        switch (typeName.toLowerCase()) {
            case "boolean": return Types.BOOLEAN;
            case "smallint": return Types.SMALLINT;
            case "integer": return Types.INTEGER;
            case "bigint": return Types.BIGINT;
            case "real": return Types.REAL;
            case "double precision":
            case "float8": return Types.DOUBLE;
            case "numeric":
            case "decimal": return Types.NUMERIC;
            case "varchar":
            case "text":
            case "char": return Types.VARCHAR;
            case "bytea": return Types.VARBINARY;
            case "date": return Types.DATE;
            case "time": return Types.TIME;
            case "timestamp": return Types.TIMESTAMP;
            default: return Types.OTHER;
        }
    }

    @Override
    public Object getArray() throws SQLException {
        return elements.clone();
    }

    @Override
    public Object getArray(Map<String, Class<?>> map) throws SQLException {
        return getArray();
    }

    @Override
    public Object getArray(long index, int count) throws SQLException {
        if (index < 1 || index > elements.length) {
            throw new SQLException("Array index out of range", "HY090");
        }
        int start = (int) (index - 1);
        int len = Math.min(count, elements.length - start);
        Object[] result = new Object[len];
        System.arraycopy(elements, start, result, 0, len);
        return result;
    }

    @Override
    public Object getArray(long index, int count, Map<String, Class<?>> map) throws SQLException {
        return getArray(index, count);
    }

    @Override
    public ResultSet getResultSet() throws SQLException {
        return getResultSet(1, elements.length);
    }

    @Override
    public ResultSet getResultSet(Map<String, Class<?>> map) throws SQLException {
        return getResultSet();
    }

    @Override
    public ResultSet getResultSet(long index, int count) throws SQLException {
        List<SBColumnInfo> cols = new ArrayList<>();
        SBColumnInfo indexCol = new SBColumnInfo();
        indexCol.setName("INDEX");
        indexCol.setTypeOid(23);  // int4
        cols.add(indexCol);

        SBColumnInfo valueCol = new SBColumnInfo();
        valueCol.setName("VALUE");
        valueCol.setTypeOid(25);  // text
        cols.add(valueCol);

        List<Object[]> rows = new ArrayList<>();
        int start = (int) (index - 1);
        int end = Math.min(start + count, elements.length);
        for (int i = start; i < end; i++) {
            rows.add(new Object[]{i + 1, elements[i]});
        }

        return new SBResultSet(null, cols, rows);
    }

    @Override
    public ResultSet getResultSet(long index, int count, Map<String, Class<?>> map) throws SQLException {
        return getResultSet(index, count);
    }

    @Override
    public void free() throws SQLException {
        elements = new Object[0];
    }
}
