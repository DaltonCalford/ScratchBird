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
import java.util.Calendar;

/**
 * JDBC PreparedStatement implementation for ScratchBird.
 */
public class SBPreparedStatement extends SBStatement implements PreparedStatement {

    // Original SQL
    protected final String originalSQL;

    // Parsed SQL with ? replaced by $1, $2, etc.
    protected String parsedSQL;

    // Parameters
    protected final List<Object> parameters = new ArrayList<>();
    protected final List<Integer> parameterTypes = new ArrayList<>();
    protected int parameterCount = 0;

    // Generated keys handling
    protected boolean returnGeneratedKeys = false;
    protected int[] generatedKeyColumnIndexes;
    protected String[] generatedKeyColumnNames;

    // Parameter metadata
    protected SBParameterMetaData parameterMetaData;

    // Batch parameters
    protected final List<List<Object>> batchParams = new ArrayList<>();

    /**
     * Creates a new prepared statement.
     */
    public SBPreparedStatement(SBConnection connection, String sql, int resultSetType,
                               int resultSetConcurrency, int resultSetHoldability)
            throws SQLException {
        super(connection, resultSetType, resultSetConcurrency, resultSetHoldability);
        this.originalSQL = sql;
        parseSQL();
    }

    /**
     * Parses SQL and counts parameters.
     */
    private void parseSQL() throws SQLException {
        StringBuilder sb = new StringBuilder();
        int paramIndex = 0;
        boolean inQuote = false;
        boolean inDoubleQuote = false;

        for (int i = 0; i < originalSQL.length(); i++) {
            char c = originalSQL.charAt(i);

            if (c == '\'' && !inDoubleQuote) {
                inQuote = !inQuote;
                sb.append(c);
            } else if (c == '"' && !inQuote) {
                inDoubleQuote = !inDoubleQuote;
                sb.append(c);
            } else if (c == '?' && !inQuote && !inDoubleQuote) {
                paramIndex++;
                sb.append('$').append(paramIndex);
                parameters.add(null);
                parameterTypes.add(Types.NULL);
            } else {
                sb.append(c);
            }
        }

        this.parsedSQL = sb.toString();
        this.parameterCount = paramIndex;
    }

    /**
     * Gets SQL with RETURNING clause if needed.
     */
    protected String getFinalSQL() {
        String sql = parsedSQL;

        // Add RETURNING clause for generated keys
        if (returnGeneratedKeys) {
            if (!sql.toUpperCase().contains("RETURNING")) {
                if (generatedKeyColumnNames != null && generatedKeyColumnNames.length > 0) {
                    StringBuilder returning = new StringBuilder(" RETURNING ");
                    for (int i = 0; i < generatedKeyColumnNames.length; i++) {
                        if (i > 0) returning.append(", ");
                        returning.append(generatedKeyColumnNames[i]);
                    }
                    sql = sql + returning.toString();
                } else {
                    sql = sql + " RETURNING *";
                }
            }
        }

        return sql;
    }

    /**
     * Builds final SQL with parameter values.
     */
    protected String buildFinalSQL() throws SQLException {
        StringBuilder sb = new StringBuilder();
        String sql = getFinalSQL();
        int paramIndex = 0;

        int i = 0;
        while (i < sql.length()) {
            char c = sql.charAt(i);

            if (c == '$' && i + 1 < sql.length() && Character.isDigit(sql.charAt(i + 1))) {
                // Find parameter number
                int j = i + 1;
                while (j < sql.length() && Character.isDigit(sql.charAt(j))) {
                    j++;
                }
                int paramNum = Integer.parseInt(sql.substring(i + 1, j));
                sb.append(formatParameter(paramNum - 1));
                i = j;
            } else {
                sb.append(c);
                i++;
            }
        }

        return sb.toString();
    }

    /**
     * Formats a parameter value for SQL.
     */
    protected String formatParameter(int index) throws SQLException {
        if (index < 0 || index >= parameters.size()) {
            throw new SQLException("Parameter index out of range: " + (index + 1), "07001");
        }

        Object value = parameters.get(index);
        if (value == null) {
            return "NULL";
        }

        int type = parameterTypes.get(index);

        if (value instanceof Boolean) {
            return (Boolean) value ? "TRUE" : "FALSE";
        } else if (value instanceof Number) {
            return value.toString();
        } else if (value instanceof String) {
            return "'" + ((String) value).replace("'", "''") + "'";
        } else if (value instanceof byte[]) {
            byte[] bytes = (byte[]) value;
            StringBuilder sb = new StringBuilder("E'\\\\x");
            for (byte b : bytes) {
                sb.append(String.format("%02x", b & 0xff));
            }
            sb.append("'");
            return sb.toString();
        } else if (value instanceof java.sql.Date) {
            return "DATE '" + value.toString() + "'";
        } else if (value instanceof java.sql.Time) {
            return "TIME '" + value.toString() + "'";
        } else if (value instanceof java.sql.Timestamp) {
            return "TIMESTAMP '" + value.toString() + "'";
        } else if (value instanceof java.util.UUID) {
            return "'" + value.toString() + "'::UUID";
        } else if (value instanceof Array) {
            return formatArray((Array) value);
        } else {
            return "'" + value.toString().replace("'", "''") + "'";
        }
    }

    /**
     * Formats an array parameter.
     */
    protected String formatArray(Array array) throws SQLException {
        Object[] elements = (Object[]) array.getArray();
        StringBuilder sb = new StringBuilder("ARRAY[");
        for (int i = 0; i < elements.length; i++) {
            if (i > 0) sb.append(", ");
            if (elements[i] == null) {
                sb.append("NULL");
            } else if (elements[i] instanceof String) {
                sb.append("'").append(((String) elements[i]).replace("'", "''")).append("'");
            } else {
                sb.append(elements[i].toString());
            }
        }
        sb.append("]");
        return sb.toString();
    }

    @Override
    public ResultSet executeQuery() throws SQLException {
        checkClosed();
        clearResults();

        String sql = buildFinalSQL();
        SBQueryResult result = connection.getProtocol().execute(sql);

        if (result.getColumns() == null || result.getColumns().isEmpty()) {
            throw new SQLException("Query did not return a result set", "02000");
        }

        currentResultSet = new SBResultSet(this, result.getColumns(), result.getRows());
        return currentResultSet;
    }

    @Override
    public int executeUpdate() throws SQLException {
        return (int) executeLargeUpdate();
    }

    @Override
    public long executeLargeUpdate() throws SQLException {
        checkClosed();
        clearResults();

        String sql = buildFinalSQL();
        SBQueryResult result = connection.getProtocol().execute(sql);

        if (returnGeneratedKeys && result.getColumns() != null && !result.getColumns().isEmpty()) {
            generatedKeys = new SBResultSet(this, result.getColumns(), result.getRows());
        }

        updateCount = result.getUpdateCount();
        return updateCount;
    }

    @Override
    public boolean execute() throws SQLException {
        checkClosed();
        clearResults();

        String sql = buildFinalSQL();
        SBQueryResult result = connection.getProtocol().execute(sql);

        if (result.getColumns() != null && !result.getColumns().isEmpty()) {
            if (returnGeneratedKeys) {
                generatedKeys = new SBResultSet(this, result.getColumns(), result.getRows());
                updateCount = result.getUpdateCount();
                return false;
            } else {
                currentResultSet = new SBResultSet(this, result.getColumns(), result.getRows());
                updateCount = -1;
                return true;
            }
        } else {
            updateCount = result.getUpdateCount();
            return false;
        }
    }

    @Override
    public void clearParameters() throws SQLException {
        checkClosed();
        for (int i = 0; i < parameters.size(); i++) {
            parameters.set(i, null);
            parameterTypes.set(i, Types.NULL);
        }
    }

    // ==================== Set Methods ====================

    private void setParameter(int parameterIndex, Object value, int sqlType) throws SQLException {
        checkClosed();
        if (parameterIndex < 1 || parameterIndex > parameterCount) {
            throw new SQLException("Parameter index out of range: " + parameterIndex +
                " (expected 1-" + parameterCount + ")", "07001");
        }
        parameters.set(parameterIndex - 1, value);
        parameterTypes.set(parameterIndex - 1, sqlType);
    }

    @Override
    public void setNull(int parameterIndex, int sqlType) throws SQLException {
        setParameter(parameterIndex, null, sqlType);
    }

    @Override
    public void setNull(int parameterIndex, int sqlType, String typeName) throws SQLException {
        setParameter(parameterIndex, null, sqlType);
    }

    @Override
    public void setBoolean(int parameterIndex, boolean x) throws SQLException {
        setParameter(parameterIndex, x, Types.BOOLEAN);
    }

    @Override
    public void setByte(int parameterIndex, byte x) throws SQLException {
        setParameter(parameterIndex, (int) x, Types.TINYINT);
    }

    @Override
    public void setShort(int parameterIndex, short x) throws SQLException {
        setParameter(parameterIndex, (int) x, Types.SMALLINT);
    }

    @Override
    public void setInt(int parameterIndex, int x) throws SQLException {
        setParameter(parameterIndex, x, Types.INTEGER);
    }

    @Override
    public void setLong(int parameterIndex, long x) throws SQLException {
        setParameter(parameterIndex, x, Types.BIGINT);
    }

    @Override
    public void setFloat(int parameterIndex, float x) throws SQLException {
        setParameter(parameterIndex, x, Types.REAL);
    }

    @Override
    public void setDouble(int parameterIndex, double x) throws SQLException {
        setParameter(parameterIndex, x, Types.DOUBLE);
    }

    @Override
    public void setBigDecimal(int parameterIndex, BigDecimal x) throws SQLException {
        setParameter(parameterIndex, x, Types.NUMERIC);
    }

    @Override
    public void setString(int parameterIndex, String x) throws SQLException {
        setParameter(parameterIndex, x, Types.VARCHAR);
    }

    @Override
    public void setBytes(int parameterIndex, byte[] x) throws SQLException {
        setParameter(parameterIndex, x, Types.VARBINARY);
    }

    @Override
    public void setDate(int parameterIndex, java.sql.Date x) throws SQLException {
        setParameter(parameterIndex, x, Types.DATE);
    }

    @Override
    public void setDate(int parameterIndex, java.sql.Date x, Calendar cal) throws SQLException {
        // TODO: Apply calendar timezone
        setParameter(parameterIndex, x, Types.DATE);
    }

    @Override
    public void setTime(int parameterIndex, java.sql.Time x) throws SQLException {
        setParameter(parameterIndex, x, Types.TIME);
    }

    @Override
    public void setTime(int parameterIndex, java.sql.Time x, Calendar cal) throws SQLException {
        // TODO: Apply calendar timezone
        setParameter(parameterIndex, x, Types.TIME);
    }

    @Override
    public void setTimestamp(int parameterIndex, java.sql.Timestamp x) throws SQLException {
        setParameter(parameterIndex, x, Types.TIMESTAMP);
    }

    @Override
    public void setTimestamp(int parameterIndex, java.sql.Timestamp x, Calendar cal) throws SQLException {
        // TODO: Apply calendar timezone
        setParameter(parameterIndex, x, Types.TIMESTAMP);
    }

    @Override
    public void setAsciiStream(int parameterIndex, InputStream x, int length) throws SQLException {
        try {
            byte[] bytes = new byte[length];
            x.read(bytes);
            setString(parameterIndex, new String(bytes, "US-ASCII"));
        } catch (IOException e) {
            throw new SQLException("Failed to read ASCII stream", "HY000", e);
        }
    }

    @Override
    public void setAsciiStream(int parameterIndex, InputStream x, long length) throws SQLException {
        setAsciiStream(parameterIndex, x, (int) length);
    }

    @Override
    public void setAsciiStream(int parameterIndex, InputStream x) throws SQLException {
        try {
            ByteArrayOutputStream baos = new ByteArrayOutputStream();
            byte[] buffer = new byte[8192];
            int len;
            while ((len = x.read(buffer)) != -1) {
                baos.write(buffer, 0, len);
            }
            setString(parameterIndex, new String(baos.toByteArray(), "US-ASCII"));
        } catch (IOException e) {
            throw new SQLException("Failed to read ASCII stream", "HY000", e);
        }
    }

    @Override
    @Deprecated
    public void setUnicodeStream(int parameterIndex, InputStream x, int length) throws SQLException {
        try {
            byte[] bytes = new byte[length];
            x.read(bytes);
            setString(parameterIndex, new String(bytes, "UTF-8"));
        } catch (IOException e) {
            throw new SQLException("Failed to read Unicode stream", "HY000", e);
        }
    }

    @Override
    public void setBinaryStream(int parameterIndex, InputStream x, int length) throws SQLException {
        try {
            byte[] bytes = new byte[length];
            x.read(bytes);
            setBytes(parameterIndex, bytes);
        } catch (IOException e) {
            throw new SQLException("Failed to read binary stream", "HY000", e);
        }
    }

    @Override
    public void setBinaryStream(int parameterIndex, InputStream x, long length) throws SQLException {
        setBinaryStream(parameterIndex, x, (int) length);
    }

    @Override
    public void setBinaryStream(int parameterIndex, InputStream x) throws SQLException {
        try {
            ByteArrayOutputStream baos = new ByteArrayOutputStream();
            byte[] buffer = new byte[8192];
            int len;
            while ((len = x.read(buffer)) != -1) {
                baos.write(buffer, 0, len);
            }
            setBytes(parameterIndex, baos.toByteArray());
        } catch (IOException e) {
            throw new SQLException("Failed to read binary stream", "HY000", e);
        }
    }

    @Override
    public void setObject(int parameterIndex, Object x, int targetSqlType) throws SQLException {
        setObject(parameterIndex, x, targetSqlType, 0);
    }

    @Override
    public void setObject(int parameterIndex, Object x, int targetSqlType, int scaleOrLength)
            throws SQLException {
        if (x == null) {
            setNull(parameterIndex, targetSqlType);
            return;
        }

        // Convert based on target type
        switch (targetSqlType) {
            case Types.BOOLEAN:
            case Types.BIT:
                if (x instanceof Boolean) {
                    setBoolean(parameterIndex, (Boolean) x);
                } else if (x instanceof Number) {
                    setBoolean(parameterIndex, ((Number) x).intValue() != 0);
                } else {
                    setBoolean(parameterIndex, Boolean.parseBoolean(x.toString()));
                }
                break;
            case Types.TINYINT:
            case Types.SMALLINT:
                if (x instanceof Number) {
                    setShort(parameterIndex, ((Number) x).shortValue());
                } else {
                    setShort(parameterIndex, Short.parseShort(x.toString()));
                }
                break;
            case Types.INTEGER:
                if (x instanceof Number) {
                    setInt(parameterIndex, ((Number) x).intValue());
                } else {
                    setInt(parameterIndex, Integer.parseInt(x.toString()));
                }
                break;
            case Types.BIGINT:
                if (x instanceof Number) {
                    setLong(parameterIndex, ((Number) x).longValue());
                } else {
                    setLong(parameterIndex, Long.parseLong(x.toString()));
                }
                break;
            case Types.REAL:
            case Types.FLOAT:
                if (x instanceof Number) {
                    setFloat(parameterIndex, ((Number) x).floatValue());
                } else {
                    setFloat(parameterIndex, Float.parseFloat(x.toString()));
                }
                break;
            case Types.DOUBLE:
                if (x instanceof Number) {
                    setDouble(parameterIndex, ((Number) x).doubleValue());
                } else {
                    setDouble(parameterIndex, Double.parseDouble(x.toString()));
                }
                break;
            case Types.NUMERIC:
            case Types.DECIMAL:
                if (x instanceof BigDecimal) {
                    setBigDecimal(parameterIndex, (BigDecimal) x);
                } else if (x instanceof Number) {
                    setBigDecimal(parameterIndex, BigDecimal.valueOf(((Number) x).doubleValue()));
                } else {
                    setBigDecimal(parameterIndex, new BigDecimal(x.toString()));
                }
                break;
            case Types.CHAR:
            case Types.VARCHAR:
            case Types.LONGVARCHAR:
                setString(parameterIndex, x.toString());
                break;
            case Types.BINARY:
            case Types.VARBINARY:
            case Types.LONGVARBINARY:
                if (x instanceof byte[]) {
                    setBytes(parameterIndex, (byte[]) x);
                } else {
                    throw new SQLException("Cannot convert to binary", "HY000");
                }
                break;
            case Types.DATE:
                if (x instanceof java.sql.Date) {
                    setDate(parameterIndex, (java.sql.Date) x);
                } else if (x instanceof java.util.Date) {
                    setDate(parameterIndex, new java.sql.Date(((java.util.Date) x).getTime()));
                } else {
                    setDate(parameterIndex, java.sql.Date.valueOf(x.toString()));
                }
                break;
            case Types.TIME:
                if (x instanceof java.sql.Time) {
                    setTime(parameterIndex, (java.sql.Time) x);
                } else if (x instanceof java.util.Date) {
                    setTime(parameterIndex, new java.sql.Time(((java.util.Date) x).getTime()));
                } else {
                    setTime(parameterIndex, java.sql.Time.valueOf(x.toString()));
                }
                break;
            case Types.TIMESTAMP:
                if (x instanceof java.sql.Timestamp) {
                    setTimestamp(parameterIndex, (java.sql.Timestamp) x);
                } else if (x instanceof java.util.Date) {
                    setTimestamp(parameterIndex, new java.sql.Timestamp(((java.util.Date) x).getTime()));
                } else {
                    setTimestamp(parameterIndex, java.sql.Timestamp.valueOf(x.toString()));
                }
                break;
            default:
                setParameter(parameterIndex, x, targetSqlType);
                break;
        }
    }

    @Override
    public void setObject(int parameterIndex, Object x) throws SQLException {
        if (x == null) {
            setNull(parameterIndex, Types.NULL);
        } else if (x instanceof Boolean) {
            setBoolean(parameterIndex, (Boolean) x);
        } else if (x instanceof Byte) {
            setByte(parameterIndex, (Byte) x);
        } else if (x instanceof Short) {
            setShort(parameterIndex, (Short) x);
        } else if (x instanceof Integer) {
            setInt(parameterIndex, (Integer) x);
        } else if (x instanceof Long) {
            setLong(parameterIndex, (Long) x);
        } else if (x instanceof Float) {
            setFloat(parameterIndex, (Float) x);
        } else if (x instanceof Double) {
            setDouble(parameterIndex, (Double) x);
        } else if (x instanceof BigDecimal) {
            setBigDecimal(parameterIndex, (BigDecimal) x);
        } else if (x instanceof String) {
            setString(parameterIndex, (String) x);
        } else if (x instanceof byte[]) {
            setBytes(parameterIndex, (byte[]) x);
        } else if (x instanceof java.sql.Date) {
            setDate(parameterIndex, (java.sql.Date) x);
        } else if (x instanceof java.sql.Time) {
            setTime(parameterIndex, (java.sql.Time) x);
        } else if (x instanceof java.sql.Timestamp) {
            setTimestamp(parameterIndex, (java.sql.Timestamp) x);
        } else if (x instanceof Array) {
            setArray(parameterIndex, (Array) x);
        } else if (x instanceof Blob) {
            setBlob(parameterIndex, (Blob) x);
        } else if (x instanceof Clob) {
            setClob(parameterIndex, (Clob) x);
        } else if (x instanceof java.util.UUID) {
            setParameter(parameterIndex, x, Types.OTHER);
        } else if (x instanceof java.net.URL) {
            setString(parameterIndex, x.toString());
        } else {
            setParameter(parameterIndex, x, Types.OTHER);
        }
    }

    @Override
    public void setCharacterStream(int parameterIndex, Reader reader, int length) throws SQLException {
        try {
            char[] chars = new char[length];
            reader.read(chars);
            setString(parameterIndex, new String(chars));
        } catch (IOException e) {
            throw new SQLException("Failed to read character stream", "HY000", e);
        }
    }

    @Override
    public void setCharacterStream(int parameterIndex, Reader reader, long length) throws SQLException {
        setCharacterStream(parameterIndex, reader, (int) length);
    }

    @Override
    public void setCharacterStream(int parameterIndex, Reader reader) throws SQLException {
        try {
            StringBuilder sb = new StringBuilder();
            char[] buffer = new char[8192];
            int len;
            while ((len = reader.read(buffer)) != -1) {
                sb.append(buffer, 0, len);
            }
            setString(parameterIndex, sb.toString());
        } catch (IOException e) {
            throw new SQLException("Failed to read character stream", "HY000", e);
        }
    }

    @Override
    public void setRef(int parameterIndex, Ref x) throws SQLException {
        throw new SQLFeatureNotSupportedException("Ref not supported");
    }

    @Override
    public void setBlob(int parameterIndex, Blob x) throws SQLException {
        if (x == null) {
            setNull(parameterIndex, Types.BLOB);
        } else {
            setBytes(parameterIndex, x.getBytes(1, (int) x.length()));
        }
    }

    @Override
    public void setBlob(int parameterIndex, InputStream inputStream) throws SQLException {
        setBinaryStream(parameterIndex, inputStream);
    }

    @Override
    public void setBlob(int parameterIndex, InputStream inputStream, long length) throws SQLException {
        setBinaryStream(parameterIndex, inputStream, length);
    }

    @Override
    public void setClob(int parameterIndex, Clob x) throws SQLException {
        if (x == null) {
            setNull(parameterIndex, Types.CLOB);
        } else {
            setString(parameterIndex, x.getSubString(1, (int) x.length()));
        }
    }

    @Override
    public void setClob(int parameterIndex, Reader reader) throws SQLException {
        setCharacterStream(parameterIndex, reader);
    }

    @Override
    public void setClob(int parameterIndex, Reader reader, long length) throws SQLException {
        setCharacterStream(parameterIndex, reader, length);
    }

    @Override
    public void setArray(int parameterIndex, Array x) throws SQLException {
        setParameter(parameterIndex, x, Types.ARRAY);
    }

    @Override
    public void setURL(int parameterIndex, URL x) throws SQLException {
        setString(parameterIndex, x != null ? x.toString() : null);
    }

    @Override
    public void setRowId(int parameterIndex, RowId x) throws SQLException {
        throw new SQLFeatureNotSupportedException("RowId not supported");
    }

    @Override
    public void setNString(int parameterIndex, String value) throws SQLException {
        setString(parameterIndex, value);
    }

    @Override
    public void setNCharacterStream(int parameterIndex, Reader value, long length) throws SQLException {
        setCharacterStream(parameterIndex, value, length);
    }

    @Override
    public void setNCharacterStream(int parameterIndex, Reader value) throws SQLException {
        setCharacterStream(parameterIndex, value);
    }

    @Override
    public void setNClob(int parameterIndex, NClob value) throws SQLException {
        setClob(parameterIndex, value);
    }

    @Override
    public void setNClob(int parameterIndex, Reader reader, long length) throws SQLException {
        setClob(parameterIndex, reader, length);
    }

    @Override
    public void setNClob(int parameterIndex, Reader reader) throws SQLException {
        setClob(parameterIndex, reader);
    }

    @Override
    public void setSQLXML(int parameterIndex, SQLXML xmlObject) throws SQLException {
        if (xmlObject == null) {
            setNull(parameterIndex, Types.SQLXML);
        } else {
            setString(parameterIndex, xmlObject.getString());
        }
    }

    // ==================== Metadata ====================

    @Override
    public ResultSetMetaData getMetaData() throws SQLException {
        checkClosed();
        // Return null if not executed yet
        if (currentResultSet != null) {
            return currentResultSet.getMetaData();
        }
        return null;
    }

    @Override
    public ParameterMetaData getParameterMetaData() throws SQLException {
        checkClosed();
        if (parameterMetaData == null) {
            parameterMetaData = new SBParameterMetaData(parameterCount, parameterTypes);
        }
        return parameterMetaData;
    }

    // ==================== Batch Operations ====================

    @Override
    public void addBatch() throws SQLException {
        checkClosed();
        batchParams.add(new ArrayList<>(parameters));
    }

    @Override
    public void clearBatch() throws SQLException {
        checkClosed();
        batchParams.clear();
    }

    @Override
    public int[] executeBatch() throws SQLException {
        checkClosed();

        int[] results = new int[batchParams.size()];
        SQLException firstException = null;

        for (int i = 0; i < batchParams.size(); i++) {
            try {
                // Restore parameters for this batch
                List<Object> params = batchParams.get(i);
                for (int j = 0; j < params.size(); j++) {
                    parameters.set(j, params.get(j));
                }
                int count = executeUpdate();
                results[i] = count;
            } catch (SQLException e) {
                results[i] = Statement.EXECUTE_FAILED;
                if (firstException == null) {
                    firstException = e;
                }
            }
        }

        batchParams.clear();

        if (firstException != null) {
            throw new BatchUpdateException(firstException.getMessage(),
                firstException.getSQLState(), firstException.getErrorCode(),
                results, firstException);
        }

        return results;
    }

    // ==================== Generated Keys ====================

    public void setReturnGeneratedKeys(boolean returnGeneratedKeys) {
        this.returnGeneratedKeys = returnGeneratedKeys;
    }

    public void setGeneratedKeyColumnIndexes(int[] columnIndexes) {
        this.generatedKeyColumnIndexes = columnIndexes;
        this.returnGeneratedKeys = columnIndexes != null && columnIndexes.length > 0;
    }

    public void setGeneratedKeyColumnNames(String[] columnNames) {
        this.generatedKeyColumnNames = columnNames;
        this.returnGeneratedKeys = columnNames != null && columnNames.length > 0;
    }

    // ==================== Not Supported (Statement) ====================

    @Override
    public ResultSet executeQuery(String sql) throws SQLException {
        throw new SQLException("Cannot call executeQuery(String) on PreparedStatement", "HY000");
    }

    @Override
    public int executeUpdate(String sql) throws SQLException {
        throw new SQLException("Cannot call executeUpdate(String) on PreparedStatement", "HY000");
    }

    @Override
    public boolean execute(String sql) throws SQLException {
        throw new SQLException("Cannot call execute(String) on PreparedStatement", "HY000");
    }

    @Override
    public void addBatch(String sql) throws SQLException {
        throw new SQLException("Cannot call addBatch(String) on PreparedStatement", "HY000");
    }
}
