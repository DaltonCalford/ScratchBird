/*
 * ScratchBird JDBC Driver
 * Copyright (c) 2025 ScratchBird Project
 */
package com.scratchbird.jdbc;

import java.io.*;
import java.math.*;
import java.net.*;
import java.sql.*;
import java.util.*;

/**
 * JDBC CallableStatement implementation for ScratchBird.
 */
public class SBCallableStatement extends SBPreparedStatement implements CallableStatement {

    private final Map<Integer, Object> outParameters = new HashMap<>();
    private final Map<String, Integer> namedParameters = new HashMap<>();

    public SBCallableStatement(SBConnection connection, String sql, int resultSetType,
                               int resultSetConcurrency, int resultSetHoldability)
            throws SQLException {
        super(connection, sql, resultSetType, resultSetConcurrency, resultSetHoldability);
    }

    @Override
    public void registerOutParameter(int parameterIndex, int sqlType) throws SQLException {
        outParameters.put(parameterIndex, null);
    }

    @Override
    public void registerOutParameter(int parameterIndex, int sqlType, int scale) throws SQLException {
        registerOutParameter(parameterIndex, sqlType);
    }

    @Override
    public void registerOutParameter(int parameterIndex, int sqlType, String typeName) throws SQLException {
        registerOutParameter(parameterIndex, sqlType);
    }

    @Override
    public void registerOutParameter(String parameterName, int sqlType) throws SQLException {
        int index = getParameterIndex(parameterName);
        registerOutParameter(index, sqlType);
    }

    @Override
    public void registerOutParameter(String parameterName, int sqlType, int scale) throws SQLException {
        registerOutParameter(parameterName, sqlType);
    }

    @Override
    public void registerOutParameter(String parameterName, int sqlType, String typeName) throws SQLException {
        registerOutParameter(parameterName, sqlType);
    }

    @Override
    public boolean wasNull() throws SQLException {
        checkClosed();
        return false;
    }

    // Getters by index
    @Override
    public String getString(int parameterIndex) throws SQLException {
        Object value = outParameters.get(parameterIndex);
        return value != null ? value.toString() : null;
    }

    @Override
    public boolean getBoolean(int parameterIndex) throws SQLException {
        Object value = outParameters.get(parameterIndex);
        if (value == null) return false;
        if (value instanceof Boolean) return (Boolean) value;
        return Boolean.parseBoolean(value.toString());
    }

    @Override
    public byte getByte(int parameterIndex) throws SQLException {
        Object value = outParameters.get(parameterIndex);
        if (value == null) return 0;
        if (value instanceof Number) return ((Number) value).byteValue();
        return Byte.parseByte(value.toString());
    }

    @Override
    public short getShort(int parameterIndex) throws SQLException {
        Object value = outParameters.get(parameterIndex);
        if (value == null) return 0;
        if (value instanceof Number) return ((Number) value).shortValue();
        return Short.parseShort(value.toString());
    }

    @Override
    public int getInt(int parameterIndex) throws SQLException {
        Object value = outParameters.get(parameterIndex);
        if (value == null) return 0;
        if (value instanceof Number) return ((Number) value).intValue();
        return Integer.parseInt(value.toString());
    }

    @Override
    public long getLong(int parameterIndex) throws SQLException {
        Object value = outParameters.get(parameterIndex);
        if (value == null) return 0;
        if (value instanceof Number) return ((Number) value).longValue();
        return Long.parseLong(value.toString());
    }

    @Override
    public float getFloat(int parameterIndex) throws SQLException {
        Object value = outParameters.get(parameterIndex);
        if (value == null) return 0;
        if (value instanceof Number) return ((Number) value).floatValue();
        return Float.parseFloat(value.toString());
    }

    @Override
    public double getDouble(int parameterIndex) throws SQLException {
        Object value = outParameters.get(parameterIndex);
        if (value == null) return 0;
        if (value instanceof Number) return ((Number) value).doubleValue();
        return Double.parseDouble(value.toString());
    }

    @Override
    @Deprecated
    public BigDecimal getBigDecimal(int parameterIndex, int scale) throws SQLException {
        BigDecimal bd = getBigDecimal(parameterIndex);
        return bd != null ? bd.setScale(scale, RoundingMode.HALF_UP) : null;
    }

    @Override
    public byte[] getBytes(int parameterIndex) throws SQLException {
        Object value = outParameters.get(parameterIndex);
        if (value == null) return null;
        if (value instanceof byte[]) return (byte[]) value;
        return value.toString().getBytes();
    }

    @Override
    public java.sql.Date getDate(int parameterIndex) throws SQLException {
        Object value = outParameters.get(parameterIndex);
        if (value == null) return null;
        if (value instanceof java.sql.Date) return (java.sql.Date) value;
        return java.sql.Date.valueOf(value.toString());
    }

    @Override
    public Time getTime(int parameterIndex) throws SQLException {
        Object value = outParameters.get(parameterIndex);
        if (value == null) return null;
        if (value instanceof Time) return (Time) value;
        return Time.valueOf(value.toString());
    }

    @Override
    public Timestamp getTimestamp(int parameterIndex) throws SQLException {
        Object value = outParameters.get(parameterIndex);
        if (value == null) return null;
        if (value instanceof Timestamp) return (Timestamp) value;
        return Timestamp.valueOf(value.toString());
    }

    @Override
    public Object getObject(int parameterIndex) throws SQLException {
        return outParameters.get(parameterIndex);
    }

    @Override
    public BigDecimal getBigDecimal(int parameterIndex) throws SQLException {
        Object value = outParameters.get(parameterIndex);
        if (value == null) return null;
        if (value instanceof BigDecimal) return (BigDecimal) value;
        return new BigDecimal(value.toString());
    }

    @Override
    public Object getObject(int parameterIndex, Map<String, Class<?>> map) throws SQLException {
        return getObject(parameterIndex);
    }

    @Override
    public Ref getRef(int parameterIndex) throws SQLException {
        throw new SQLFeatureNotSupportedException("Ref not supported");
    }

    @Override
    public Blob getBlob(int parameterIndex) throws SQLException {
        byte[] bytes = getBytes(parameterIndex);
        return bytes != null ? new SBBlob(bytes) : null;
    }

    @Override
    public Clob getClob(int parameterIndex) throws SQLException {
        String s = getString(parameterIndex);
        return s != null ? new SBClob(s) : null;
    }

    @Override
    public Array getArray(int parameterIndex) throws SQLException {
        Object value = outParameters.get(parameterIndex);
        if (value instanceof Array) return (Array) value;
        return null;
    }

    @Override
    public java.sql.Date getDate(int parameterIndex, Calendar cal) throws SQLException {
        return getDate(parameterIndex);
    }

    @Override
    public Time getTime(int parameterIndex, Calendar cal) throws SQLException {
        return getTime(parameterIndex);
    }

    @Override
    public Timestamp getTimestamp(int parameterIndex, Calendar cal) throws SQLException {
        return getTimestamp(parameterIndex);
    }

    @Override
    public URL getURL(int parameterIndex) throws SQLException {
        String s = getString(parameterIndex);
        if (s == null) return null;
        try {
            return new URL(s);
        } catch (MalformedURLException e) {
            throw new SQLException("Invalid URL: " + s, "HY000", e);
        }
    }

    // Getters by name
    @Override
    public String getString(String parameterName) throws SQLException {
        return getString(getParameterIndex(parameterName));
    }

    @Override
    public boolean getBoolean(String parameterName) throws SQLException {
        return getBoolean(getParameterIndex(parameterName));
    }

    @Override
    public byte getByte(String parameterName) throws SQLException {
        return getByte(getParameterIndex(parameterName));
    }

    @Override
    public short getShort(String parameterName) throws SQLException {
        return getShort(getParameterIndex(parameterName));
    }

    @Override
    public int getInt(String parameterName) throws SQLException {
        return getInt(getParameterIndex(parameterName));
    }

    @Override
    public long getLong(String parameterName) throws SQLException {
        return getLong(getParameterIndex(parameterName));
    }

    @Override
    public float getFloat(String parameterName) throws SQLException {
        return getFloat(getParameterIndex(parameterName));
    }

    @Override
    public double getDouble(String parameterName) throws SQLException {
        return getDouble(getParameterIndex(parameterName));
    }

    @Override
    public byte[] getBytes(String parameterName) throws SQLException {
        return getBytes(getParameterIndex(parameterName));
    }

    @Override
    public java.sql.Date getDate(String parameterName) throws SQLException {
        return getDate(getParameterIndex(parameterName));
    }

    @Override
    public Time getTime(String parameterName) throws SQLException {
        return getTime(getParameterIndex(parameterName));
    }

    @Override
    public Timestamp getTimestamp(String parameterName) throws SQLException {
        return getTimestamp(getParameterIndex(parameterName));
    }

    @Override
    public Object getObject(String parameterName) throws SQLException {
        return getObject(getParameterIndex(parameterName));
    }

    @Override
    public BigDecimal getBigDecimal(String parameterName) throws SQLException {
        return getBigDecimal(getParameterIndex(parameterName));
    }

    @Override
    public Object getObject(String parameterName, Map<String, Class<?>> map) throws SQLException {
        return getObject(getParameterIndex(parameterName), map);
    }

    @Override
    public Ref getRef(String parameterName) throws SQLException {
        return getRef(getParameterIndex(parameterName));
    }

    @Override
    public Blob getBlob(String parameterName) throws SQLException {
        return getBlob(getParameterIndex(parameterName));
    }

    @Override
    public Clob getClob(String parameterName) throws SQLException {
        return getClob(getParameterIndex(parameterName));
    }

    @Override
    public Array getArray(String parameterName) throws SQLException {
        return getArray(getParameterIndex(parameterName));
    }

    @Override
    public java.sql.Date getDate(String parameterName, Calendar cal) throws SQLException {
        return getDate(getParameterIndex(parameterName), cal);
    }

    @Override
    public Time getTime(String parameterName, Calendar cal) throws SQLException {
        return getTime(getParameterIndex(parameterName), cal);
    }

    @Override
    public Timestamp getTimestamp(String parameterName, Calendar cal) throws SQLException {
        return getTimestamp(getParameterIndex(parameterName), cal);
    }

    @Override
    public URL getURL(String parameterName) throws SQLException {
        return getURL(getParameterIndex(parameterName));
    }

    // Setters by name
    @Override
    public void setNull(String parameterName, int sqlType) throws SQLException {
        setNull(getParameterIndex(parameterName), sqlType);
    }

    @Override
    public void setNull(String parameterName, int sqlType, String typeName) throws SQLException {
        setNull(getParameterIndex(parameterName), sqlType, typeName);
    }

    @Override
    public void setBoolean(String parameterName, boolean x) throws SQLException {
        setBoolean(getParameterIndex(parameterName), x);
    }

    @Override
    public void setByte(String parameterName, byte x) throws SQLException {
        setByte(getParameterIndex(parameterName), x);
    }

    @Override
    public void setShort(String parameterName, short x) throws SQLException {
        setShort(getParameterIndex(parameterName), x);
    }

    @Override
    public void setInt(String parameterName, int x) throws SQLException {
        setInt(getParameterIndex(parameterName), x);
    }

    @Override
    public void setLong(String parameterName, long x) throws SQLException {
        setLong(getParameterIndex(parameterName), x);
    }

    @Override
    public void setFloat(String parameterName, float x) throws SQLException {
        setFloat(getParameterIndex(parameterName), x);
    }

    @Override
    public void setDouble(String parameterName, double x) throws SQLException {
        setDouble(getParameterIndex(parameterName), x);
    }

    @Override
    public void setBigDecimal(String parameterName, BigDecimal x) throws SQLException {
        setBigDecimal(getParameterIndex(parameterName), x);
    }

    @Override
    public void setString(String parameterName, String x) throws SQLException {
        setString(getParameterIndex(parameterName), x);
    }

    @Override
    public void setBytes(String parameterName, byte[] x) throws SQLException {
        setBytes(getParameterIndex(parameterName), x);
    }

    @Override
    public void setDate(String parameterName, java.sql.Date x) throws SQLException {
        setDate(getParameterIndex(parameterName), x);
    }

    @Override
    public void setTime(String parameterName, Time x) throws SQLException {
        setTime(getParameterIndex(parameterName), x);
    }

    @Override
    public void setTimestamp(String parameterName, Timestamp x) throws SQLException {
        setTimestamp(getParameterIndex(parameterName), x);
    }

    @Override
    public void setAsciiStream(String parameterName, InputStream x, int length) throws SQLException {
        setAsciiStream(getParameterIndex(parameterName), x, length);
    }

    @Override
    public void setBinaryStream(String parameterName, InputStream x, int length) throws SQLException {
        setBinaryStream(getParameterIndex(parameterName), x, length);
    }

    @Override
    public void setObject(String parameterName, Object x, int targetSqlType, int scale) throws SQLException {
        setObject(getParameterIndex(parameterName), x, targetSqlType, scale);
    }

    @Override
    public void setObject(String parameterName, Object x, int targetSqlType) throws SQLException {
        setObject(getParameterIndex(parameterName), x, targetSqlType);
    }

    @Override
    public void setObject(String parameterName, Object x) throws SQLException {
        setObject(getParameterIndex(parameterName), x);
    }

    @Override
    public void setCharacterStream(String parameterName, Reader reader, int length) throws SQLException {
        setCharacterStream(getParameterIndex(parameterName), reader, length);
    }

    @Override
    public void setDate(String parameterName, java.sql.Date x, Calendar cal) throws SQLException {
        setDate(getParameterIndex(parameterName), x, cal);
    }

    @Override
    public void setTime(String parameterName, Time x, Calendar cal) throws SQLException {
        setTime(getParameterIndex(parameterName), x, cal);
    }

    @Override
    public void setTimestamp(String parameterName, Timestamp x, Calendar cal) throws SQLException {
        setTimestamp(getParameterIndex(parameterName), x, cal);
    }

    @Override
    public void setURL(String parameterName, URL val) throws SQLException {
        setURL(getParameterIndex(parameterName), val);
    }

    @Override
    public RowId getRowId(int parameterIndex) throws SQLException {
        throw new SQLFeatureNotSupportedException("RowId not supported");
    }

    @Override
    public RowId getRowId(String parameterName) throws SQLException {
        throw new SQLFeatureNotSupportedException("RowId not supported");
    }

    @Override
    public void setRowId(String parameterName, RowId x) throws SQLException {
        throw new SQLFeatureNotSupportedException("RowId not supported");
    }

    @Override
    public void setNString(String parameterName, String value) throws SQLException {
        setNString(getParameterIndex(parameterName), value);
    }

    @Override
    public void setNCharacterStream(String parameterName, Reader value, long length) throws SQLException {
        setNCharacterStream(getParameterIndex(parameterName), value, length);
    }

    @Override
    public void setNClob(String parameterName, NClob value) throws SQLException {
        setNClob(getParameterIndex(parameterName), value);
    }

    @Override
    public void setClob(String parameterName, Reader reader, long length) throws SQLException {
        setClob(getParameterIndex(parameterName), reader, length);
    }

    @Override
    public void setBlob(String parameterName, InputStream inputStream, long length) throws SQLException {
        setBlob(getParameterIndex(parameterName), inputStream, length);
    }

    @Override
    public void setNClob(String parameterName, Reader reader, long length) throws SQLException {
        setNClob(getParameterIndex(parameterName), reader, length);
    }

    @Override
    public NClob getNClob(int parameterIndex) throws SQLException {
        String s = getString(parameterIndex);
        return s != null ? new SBNClob(s) : null;
    }

    @Override
    public NClob getNClob(String parameterName) throws SQLException {
        return getNClob(getParameterIndex(parameterName));
    }

    @Override
    public void setSQLXML(String parameterName, SQLXML xmlObject) throws SQLException {
        setSQLXML(getParameterIndex(parameterName), xmlObject);
    }

    @Override
    public SQLXML getSQLXML(int parameterIndex) throws SQLException {
        String s = getString(parameterIndex);
        return s != null ? new SBSQLXML(s) : null;
    }

    @Override
    public SQLXML getSQLXML(String parameterName) throws SQLException {
        return getSQLXML(getParameterIndex(parameterName));
    }

    @Override
    public String getNString(int parameterIndex) throws SQLException {
        return getString(parameterIndex);
    }

    @Override
    public String getNString(String parameterName) throws SQLException {
        return getString(parameterName);
    }

    @Override
    public Reader getNCharacterStream(int parameterIndex) throws SQLException {
        String s = getString(parameterIndex);
        return s != null ? new StringReader(s) : null;
    }

    @Override
    public Reader getNCharacterStream(String parameterName) throws SQLException {
        return getNCharacterStream(getParameterIndex(parameterName));
    }

    @Override
    public Reader getCharacterStream(int parameterIndex) throws SQLException {
        String s = getString(parameterIndex);
        return s != null ? new StringReader(s) : null;
    }

    @Override
    public Reader getCharacterStream(String parameterName) throws SQLException {
        return getCharacterStream(getParameterIndex(parameterName));
    }

    @Override
    public void setBlob(String parameterName, Blob x) throws SQLException {
        setBlob(getParameterIndex(parameterName), x);
    }

    @Override
    public void setClob(String parameterName, Clob x) throws SQLException {
        setClob(getParameterIndex(parameterName), x);
    }

    @Override
    public void setAsciiStream(String parameterName, InputStream x, long length) throws SQLException {
        setAsciiStream(getParameterIndex(parameterName), x, length);
    }

    @Override
    public void setBinaryStream(String parameterName, InputStream x, long length) throws SQLException {
        setBinaryStream(getParameterIndex(parameterName), x, length);
    }

    @Override
    public void setCharacterStream(String parameterName, Reader reader, long length) throws SQLException {
        setCharacterStream(getParameterIndex(parameterName), reader, length);
    }

    @Override
    public void setAsciiStream(String parameterName, InputStream x) throws SQLException {
        setAsciiStream(getParameterIndex(parameterName), x);
    }

    @Override
    public void setBinaryStream(String parameterName, InputStream x) throws SQLException {
        setBinaryStream(getParameterIndex(parameterName), x);
    }

    @Override
    public void setCharacterStream(String parameterName, Reader reader) throws SQLException {
        setCharacterStream(getParameterIndex(parameterName), reader);
    }

    @Override
    public void setNCharacterStream(String parameterName, Reader value) throws SQLException {
        setNCharacterStream(getParameterIndex(parameterName), value);
    }

    @Override
    public void setClob(String parameterName, Reader reader) throws SQLException {
        setClob(getParameterIndex(parameterName), reader);
    }

    @Override
    public void setBlob(String parameterName, InputStream inputStream) throws SQLException {
        setBlob(getParameterIndex(parameterName), inputStream);
    }

    @Override
    public void setNClob(String parameterName, Reader reader) throws SQLException {
        setNClob(getParameterIndex(parameterName), reader);
    }

    @Override
    public <T> T getObject(int parameterIndex, Class<T> type) throws SQLException {
        Object value = getObject(parameterIndex);
        if (value == null) return null;
        if (type.isInstance(value)) return type.cast(value);
        throw new SQLException("Cannot convert to " + type.getName(), "HY000");
    }

    @Override
    public <T> T getObject(String parameterName, Class<T> type) throws SQLException {
        return getObject(getParameterIndex(parameterName), type);
    }

    private int getParameterIndex(String parameterName) throws SQLException {
        Integer index = namedParameters.get(parameterName);
        if (index == null) {
            throw new SQLException("Parameter not found: " + parameterName, "07009");
        }
        return index;
    }
}
