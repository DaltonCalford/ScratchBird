/*
 * ScratchBird JDBC Driver
 * Copyright (c) 2025 ScratchBird Project
 */
package com.scratchbird.jdbc;

import java.sql.*;
import java.util.*;
import java.util.concurrent.Executor;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.logging.Level;
import java.util.logging.Logger;

/**
 * JDBC Connection implementation for ScratchBird.
 *
 * <p>This class manages a connection to a ScratchBird database server,
 * providing methods for creating statements, managing transactions,
 * and querying database metadata.</p>
 */
public class SBConnection implements Connection {

    private static final Logger LOGGER = Logger.getLogger(SBConnection.class.getName());

    /** Snapshot isolation level (ScratchBird extension) */
    public static final int TRANSACTION_SNAPSHOT = 5;

    // Connection properties
    private final SBConnectionProperties properties;
    private final String connectionId;
    private static final AtomicInteger connectionCounter = new AtomicInteger(0);

    // Connection state
    private final AtomicBoolean closed = new AtomicBoolean(false);
    private boolean autoCommit = true;
    private boolean readOnly = false;
    private int transactionIsolation = TRANSACTION_READ_COMMITTED;
    private String catalog;
    private String schema;
    private int holdability = ResultSet.HOLD_CURSORS_OVER_COMMIT;
    private Map<String, Class<?>> typeMap = new HashMap<>();

    // Network protocol handler (stub for now)
    private SBProtocolHandler protocol;

    // Warnings
    private SQLWarning warnings;

    // Client info
    private Properties clientInfo = new Properties();

    // Savepoint counter
    private int savepointCounter = 0;

    /**
     * Creates a new connection with the given properties.
     *
     * @param props connection properties
     * @throws SQLException if connection fails
     */
    public SBConnection(SBConnectionProperties props) throws SQLException {
        this.properties = props;
        this.connectionId = "conn-" + connectionCounter.incrementAndGet();

        // Initialize from properties
        this.autoCommit = props.isAutoCommit();
        this.readOnly = props.isReadOnly();
        this.schema = props.getCurrentSchema();

        if (props.getApplicationName() != null) {
            clientInfo.setProperty("ApplicationName", props.getApplicationName());
        }

        // Connect to server
        connect();

        LOGGER.log(Level.FINE, "Connection {0} established to {1}:{2}/{3}",
            new Object[]{connectionId, props.getHost(), props.getPort(), props.getDatabase()});
    }

    /**
     * Establishes connection to the server.
     */
    private void connect() throws SQLException {
        try {
            protocol = new SBProtocolHandler(properties);
            protocol.connect();

            // Set initial connection parameters
            if (schema != null && !schema.equals("public")) {
                protocol.execute("SET SCHEMA '" + schema.replace("'", "''") + "'");
            }

            catalog = properties.getDatabase();

        } catch (Exception e) {
            throw new SQLException("Failed to connect to " + properties.getHost() +
                ":" + properties.getPort() + "/" + properties.getDatabase() +
                ": " + e.getMessage(), "08001", e);
        }
    }

    @Override
    public Statement createStatement() throws SQLException {
        checkClosed();
        return new SBStatement(this, ResultSet.TYPE_FORWARD_ONLY,
            ResultSet.CONCUR_READ_ONLY, holdability);
    }

    @Override
    public Statement createStatement(int resultSetType, int resultSetConcurrency) throws SQLException {
        checkClosed();
        return new SBStatement(this, resultSetType, resultSetConcurrency, holdability);
    }

    @Override
    public Statement createStatement(int resultSetType, int resultSetConcurrency,
                                     int resultSetHoldability) throws SQLException {
        checkClosed();
        return new SBStatement(this, resultSetType, resultSetConcurrency, resultSetHoldability);
    }

    @Override
    public PreparedStatement prepareStatement(String sql) throws SQLException {
        checkClosed();
        return new SBPreparedStatement(this, sql, ResultSet.TYPE_FORWARD_ONLY,
            ResultSet.CONCUR_READ_ONLY, holdability);
    }

    @Override
    public PreparedStatement prepareStatement(String sql, int resultSetType,
                                              int resultSetConcurrency) throws SQLException {
        checkClosed();
        return new SBPreparedStatement(this, sql, resultSetType, resultSetConcurrency, holdability);
    }

    @Override
    public PreparedStatement prepareStatement(String sql, int resultSetType,
                                              int resultSetConcurrency, int resultSetHoldability)
            throws SQLException {
        checkClosed();
        return new SBPreparedStatement(this, sql, resultSetType, resultSetConcurrency,
            resultSetHoldability);
    }

    @Override
    public PreparedStatement prepareStatement(String sql, int autoGeneratedKeys) throws SQLException {
        checkClosed();
        SBPreparedStatement stmt = new SBPreparedStatement(this, sql,
            ResultSet.TYPE_FORWARD_ONLY, ResultSet.CONCUR_READ_ONLY, holdability);
        stmt.setReturnGeneratedKeys(autoGeneratedKeys == Statement.RETURN_GENERATED_KEYS);
        return stmt;
    }

    @Override
    public PreparedStatement prepareStatement(String sql, int[] columnIndexes) throws SQLException {
        checkClosed();
        SBPreparedStatement stmt = new SBPreparedStatement(this, sql,
            ResultSet.TYPE_FORWARD_ONLY, ResultSet.CONCUR_READ_ONLY, holdability);
        stmt.setGeneratedKeyColumnIndexes(columnIndexes);
        return stmt;
    }

    @Override
    public PreparedStatement prepareStatement(String sql, String[] columnNames) throws SQLException {
        checkClosed();
        SBPreparedStatement stmt = new SBPreparedStatement(this, sql,
            ResultSet.TYPE_FORWARD_ONLY, ResultSet.CONCUR_READ_ONLY, holdability);
        stmt.setGeneratedKeyColumnNames(columnNames);
        return stmt;
    }

    @Override
    public CallableStatement prepareCall(String sql) throws SQLException {
        checkClosed();
        return new SBCallableStatement(this, sql, ResultSet.TYPE_FORWARD_ONLY,
            ResultSet.CONCUR_READ_ONLY, holdability);
    }

    @Override
    public CallableStatement prepareCall(String sql, int resultSetType, int resultSetConcurrency)
            throws SQLException {
        checkClosed();
        return new SBCallableStatement(this, sql, resultSetType, resultSetConcurrency, holdability);
    }

    @Override
    public CallableStatement prepareCall(String sql, int resultSetType, int resultSetConcurrency,
                                         int resultSetHoldability) throws SQLException {
        checkClosed();
        return new SBCallableStatement(this, sql, resultSetType, resultSetConcurrency,
            resultSetHoldability);
    }

    @Override
    public String nativeSQL(String sql) throws SQLException {
        checkClosed();
        // Convert JDBC escape sequences to native SQL
        return SBSQLParser.convertToNativeSQL(sql);
    }

    @Override
    public void setAutoCommit(boolean autoCommit) throws SQLException {
        checkClosed();
        if (this.autoCommit != autoCommit) {
            if (!this.autoCommit) {
                // Commit any pending transaction before changing mode
                commit();
            }
            this.autoCommit = autoCommit;
            protocol.execute("SET AUTOCOMMIT " + (autoCommit ? "ON" : "OFF"));
        }
    }

    @Override
    public boolean getAutoCommit() throws SQLException {
        checkClosed();
        return autoCommit;
    }

    @Override
    public void commit() throws SQLException {
        checkClosed();
        if (autoCommit) {
            throw new SQLException("Cannot commit when autoCommit is enabled", "25000");
        }
        protocol.execute("COMMIT");
    }

    @Override
    public void rollback() throws SQLException {
        checkClosed();
        if (autoCommit) {
            throw new SQLException("Cannot rollback when autoCommit is enabled", "25000");
        }
        protocol.execute("ROLLBACK");
    }

    @Override
    public void rollback(Savepoint savepoint) throws SQLException {
        checkClosed();
        if (autoCommit) {
            throw new SQLException("Cannot rollback when autoCommit is enabled", "25000");
        }
        if (savepoint == null) {
            throw new SQLException("Savepoint cannot be null", "HY000");
        }
        String name = ((SBSavepoint) savepoint).getSavepointName();
        protocol.execute("ROLLBACK TO SAVEPOINT " + name);
    }

    @Override
    public void close() throws SQLException {
        if (closed.compareAndSet(false, true)) {
            try {
                if (!autoCommit) {
                    try {
                        rollback();
                    } catch (SQLException e) {
                        // Ignore rollback errors during close
                    }
                }
                if (protocol != null) {
                    protocol.close();
                }
            } finally {
                LOGGER.log(Level.FINE, "Connection {0} closed", connectionId);
            }
        }
    }

    @Override
    public boolean isClosed() throws SQLException {
        return closed.get();
    }

    @Override
    public DatabaseMetaData getMetaData() throws SQLException {
        checkClosed();
        return new SBDatabaseMetaData(this);
    }

    @Override
    public void setReadOnly(boolean readOnly) throws SQLException {
        checkClosed();
        this.readOnly = readOnly;
        protocol.execute("SET TRANSACTION READ " + (readOnly ? "ONLY" : "WRITE"));
    }

    @Override
    public boolean isReadOnly() throws SQLException {
        checkClosed();
        return readOnly;
    }

    @Override
    public void setCatalog(String catalog) throws SQLException {
        checkClosed();
        // ScratchBird doesn't support changing database after connection
        // Just store the value, actual database is set at connection time
        this.catalog = catalog;
    }

    @Override
    public String getCatalog() throws SQLException {
        checkClosed();
        return catalog;
    }

    @Override
    public void setTransactionIsolation(int level) throws SQLException {
        checkClosed();
        String isolationLevel;
        switch (level) {
            case TRANSACTION_READ_UNCOMMITTED:
                isolationLevel = "READ UNCOMMITTED";
                break;
            case TRANSACTION_READ_COMMITTED:
                isolationLevel = "READ COMMITTED";
                break;
            case TRANSACTION_REPEATABLE_READ:
                isolationLevel = "REPEATABLE READ";
                break;
            case TRANSACTION_SERIALIZABLE:
                isolationLevel = "SERIALIZABLE";
                break;
            case TRANSACTION_SNAPSHOT:
                isolationLevel = "SNAPSHOT";
                break;
            default:
                throw new SQLException("Invalid transaction isolation level: " + level, "HY024");
        }
        protocol.execute("SET TRANSACTION ISOLATION LEVEL " + isolationLevel);
        this.transactionIsolation = level;
    }

    @Override
    public int getTransactionIsolation() throws SQLException {
        checkClosed();
        return transactionIsolation;
    }

    @Override
    public SQLWarning getWarnings() throws SQLException {
        checkClosed();
        return warnings;
    }

    @Override
    public void clearWarnings() throws SQLException {
        checkClosed();
        warnings = null;
    }

    @Override
    public Map<String, Class<?>> getTypeMap() throws SQLException {
        checkClosed();
        return new HashMap<>(typeMap);
    }

    @Override
    public void setTypeMap(Map<String, Class<?>> map) throws SQLException {
        checkClosed();
        this.typeMap = new HashMap<>(map);
    }

    @Override
    public void setHoldability(int holdability) throws SQLException {
        checkClosed();
        if (holdability != ResultSet.HOLD_CURSORS_OVER_COMMIT &&
            holdability != ResultSet.CLOSE_CURSORS_AT_COMMIT) {
            throw new SQLException("Invalid holdability: " + holdability, "HY024");
        }
        this.holdability = holdability;
    }

    @Override
    public int getHoldability() throws SQLException {
        checkClosed();
        return holdability;
    }

    @Override
    public Savepoint setSavepoint() throws SQLException {
        checkClosed();
        if (autoCommit) {
            throw new SQLException("Cannot set savepoint when autoCommit is enabled", "25000");
        }
        String name = "sp_" + (++savepointCounter);
        protocol.execute("SAVEPOINT " + name);
        return new SBSavepoint(savepointCounter, name);
    }

    @Override
    public Savepoint setSavepoint(String name) throws SQLException {
        checkClosed();
        if (autoCommit) {
            throw new SQLException("Cannot set savepoint when autoCommit is enabled", "25000");
        }
        if (name == null || name.isEmpty()) {
            throw new SQLException("Savepoint name cannot be null or empty", "HY000");
        }
        protocol.execute("SAVEPOINT " + name);
        return new SBSavepoint(0, name);
    }

    @Override
    public void releaseSavepoint(Savepoint savepoint) throws SQLException {
        checkClosed();
        if (savepoint == null) {
            throw new SQLException("Savepoint cannot be null", "HY000");
        }
        String name = ((SBSavepoint) savepoint).getSavepointName();
        protocol.execute("RELEASE SAVEPOINT " + name);
    }

    @Override
    public Clob createClob() throws SQLException {
        checkClosed();
        return new SBClob();
    }

    @Override
    public Blob createBlob() throws SQLException {
        checkClosed();
        return new SBBlob();
    }

    @Override
    public NClob createNClob() throws SQLException {
        checkClosed();
        return new SBNClob();
    }

    @Override
    public SQLXML createSQLXML() throws SQLException {
        checkClosed();
        return new SBSQLXML();
    }

    @Override
    public boolean isValid(int timeout) throws SQLException {
        if (timeout < 0) {
            throw new SQLException("Timeout must be >= 0", "HY024");
        }
        if (closed.get()) {
            return false;
        }
        try {
            return protocol.isAlive(timeout);
        } catch (Exception e) {
            return false;
        }
    }

    @Override
    public void setClientInfo(String name, String value) throws SQLClientInfoException {
        if (closed.get()) {
            Map<String, ClientInfoStatus> failures = new HashMap<>();
            failures.put(name, ClientInfoStatus.REASON_UNKNOWN);
            throw new SQLClientInfoException("Connection is closed", failures);
        }
        if (value != null) {
            clientInfo.setProperty(name, value);
        } else {
            clientInfo.remove(name);
        }
        try {
            if ("ApplicationName".equals(name)) {
                protocol.execute("SET APPLICATION_NAME = '" + (value != null ? value.replace("'", "''") : "") + "'");
            }
        } catch (SQLException e) {
            Map<String, ClientInfoStatus> failures = new HashMap<>();
            failures.put(name, ClientInfoStatus.REASON_UNKNOWN);
            throw new SQLClientInfoException("Failed to set client info", failures, e);
        }
    }

    @Override
    public void setClientInfo(Properties properties) throws SQLClientInfoException {
        if (closed.get()) {
            Map<String, ClientInfoStatus> failures = new HashMap<>();
            for (String key : properties.stringPropertyNames()) {
                failures.put(key, ClientInfoStatus.REASON_UNKNOWN);
            }
            throw new SQLClientInfoException("Connection is closed", failures);
        }
        Map<String, ClientInfoStatus> failures = new HashMap<>();
        for (String key : properties.stringPropertyNames()) {
            try {
                setClientInfo(key, properties.getProperty(key));
            } catch (SQLClientInfoException e) {
                failures.putAll(e.getFailedProperties());
            }
        }
        if (!failures.isEmpty()) {
            throw new SQLClientInfoException("Failed to set some client info properties", failures);
        }
    }

    @Override
    public String getClientInfo(String name) throws SQLException {
        checkClosed();
        return clientInfo.getProperty(name);
    }

    @Override
    public Properties getClientInfo() throws SQLException {
        checkClosed();
        return new Properties(clientInfo);
    }

    @Override
    public Array createArrayOf(String typeName, Object[] elements) throws SQLException {
        checkClosed();
        return new SBArray(typeName, elements);
    }

    @Override
    public Struct createStruct(String typeName, Object[] attributes) throws SQLException {
        checkClosed();
        return new SBStruct(typeName, attributes);
    }

    @Override
    public void setSchema(String schema) throws SQLException {
        checkClosed();
        if (schema != null && !schema.isEmpty()) {
            protocol.execute("SET SCHEMA '" + schema.replace("'", "''") + "'");
            this.schema = schema;
        }
    }

    @Override
    public String getSchema() throws SQLException {
        checkClosed();
        return schema;
    }

    @Override
    public void abort(Executor executor) throws SQLException {
        if (executor == null) {
            throw new SQLException("Executor cannot be null", "HY000");
        }
        executor.execute(() -> {
            try {
                if (!closed.get()) {
                    protocol.abort();
                    closed.set(true);
                }
            } catch (Exception e) {
                LOGGER.log(Level.WARNING, "Error during connection abort", e);
            }
        });
    }

    @Override
    public void setNetworkTimeout(Executor executor, int milliseconds) throws SQLException {
        checkClosed();
        if (milliseconds < 0) {
            throw new SQLException("Network timeout must be >= 0", "HY024");
        }
        protocol.setNetworkTimeout(milliseconds);
    }

    @Override
    public int getNetworkTimeout() throws SQLException {
        checkClosed();
        return protocol.getNetworkTimeout();
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

    // ==================== ScratchBird Extensions ====================

    /**
     * Cancels the currently executing query.
     *
     * @throws SQLException if cancellation fails
     */
    public void cancelQuery() throws SQLException {
        checkClosed();
        protocol.cancelCurrentQuery();
    }

    /**
     * Gets connection properties.
     *
     * @return connection properties
     */
    public SBConnectionProperties getConnectionProperties() {
        return properties;
    }

    /**
     * Gets the protocol handler.
     *
     * @return protocol handler
     */
    SBProtocolHandler getProtocol() {
        return protocol;
    }

    /**
     * Gets the connection ID.
     *
     * @return connection ID
     */
    public String getConnectionId() {
        return connectionId;
    }

    /**
     * Adds a warning to the warning chain.
     *
     * @param warning warning to add
     */
    void addWarning(SQLWarning warning) {
        if (warnings == null) {
            warnings = warning;
        } else {
            warnings.setNextWarning(warning);
        }
    }

    // ==================== Private Methods ====================

    /**
     * Checks if connection is closed and throws exception if so.
     */
    private void checkClosed() throws SQLException {
        if (closed.get()) {
            throw new SQLException("Connection is closed", "08003");
        }
    }
}
