/*
 * ScratchBird JDBC Driver
 * Copyright (c) 2025 ScratchBird Project
 */
package com.scratchbird.jdbc;

import java.util.Properties;

/**
 * Connection properties for ScratchBird JDBC connections.
 *
 * <p>This class holds all configuration options for establishing a connection
 * to a ScratchBird database server.</p>
 */
public class SBConnectionProperties {

    // Connection target
    private String host = "localhost";
    private int port = SBDriver.DEFAULT_PORT;
    private String database;

    // Authentication
    private String user;
    private String password;

    // SSL/TLS
    private String ssl = "prefer";
    private String sslMode;
    private String sslCert;
    private String sslKey;
    private String sslRootCert;
    private String sslPassword;

    // Timeouts (seconds)
    private int connectTimeout = 30;
    private int socketTimeout = 0;
    private int loginTimeout = 30;

    // Connection options
    private boolean tcpKeepAlive = true;
    private String currentSchema = "public";
    private String applicationName;
    private boolean readOnly = false;
    private boolean autoCommit = true;

    // Performance options
    private int defaultRowFetchSize = 0;
    private int prepareThreshold = 5;
    private boolean binaryTransfer = true;
    private boolean reWriteBatchedInserts = false;

    // Logging
    private String loggerLevel = "OFF";
    private String loggerFile;

    // Additional properties
    private Properties extraProperties = new Properties();

    /**
     * Default constructor.
     */
    public SBConnectionProperties() {
    }

    /**
     * Constructs properties from a Properties object.
     *
     * @param props source properties
     */
    public SBConnectionProperties(Properties props) {
        if (props != null) {
            for (String key : props.stringPropertyNames()) {
                setProperty(key, props.getProperty(key));
            }
        }
    }

    /**
     * Sets a property by name.
     *
     * @param key property name
     * @param value property value
     */
    public void setProperty(String key, String value) {
        if (key == null || value == null) {
            return;
        }

        switch (key.toLowerCase()) {
            case "host":
            case "servername":
            case "pghost":
                this.host = value;
                break;
            case "port":
            case "portnumber":
            case "pgport":
                this.port = Integer.parseInt(value);
                break;
            case "database":
            case "databasename":
            case "dbname":
            case "pgdatabase":
                this.database = value;
                break;
            case "user":
            case "username":
            case "pguser":
                this.user = value;
                break;
            case "password":
            case "pgpassword":
                this.password = value;
                break;
            case "ssl":
                this.ssl = value;
                break;
            case "sslmode":
                this.sslMode = value;
                if (this.ssl == null || "prefer".equals(this.ssl)) {
                    this.ssl = value;
                }
                break;
            case "sslcert":
                this.sslCert = value;
                break;
            case "sslkey":
                this.sslKey = value;
                break;
            case "sslrootcert":
                this.sslRootCert = value;
                break;
            case "sslpassword":
                this.sslPassword = value;
                break;
            case "connecttimeout":
                this.connectTimeout = Integer.parseInt(value);
                break;
            case "sockettimeout":
                this.socketTimeout = Integer.parseInt(value);
                break;
            case "logintimeout":
                this.loginTimeout = Integer.parseInt(value);
                break;
            case "tcpkeepalive":
                this.tcpKeepAlive = Boolean.parseBoolean(value);
                break;
            case "currentschema":
            case "searchpath":
                this.currentSchema = value;
                break;
            case "applicationname":
            case "application_name":
                this.applicationName = value;
                break;
            case "readonly":
                this.readOnly = Boolean.parseBoolean(value);
                break;
            case "autocommit":
                this.autoCommit = Boolean.parseBoolean(value);
                break;
            case "defaultrowfetchsize":
            case "fetchsize":
                this.defaultRowFetchSize = Integer.parseInt(value);
                break;
            case "preparethreshold":
                this.prepareThreshold = Integer.parseInt(value);
                break;
            case "binarytransfer":
                this.binaryTransfer = Boolean.parseBoolean(value);
                break;
            case "rewritebatchedinserts":
                this.reWriteBatchedInserts = Boolean.parseBoolean(value);
                break;
            case "loggerlevel":
            case "loglevel":
                this.loggerLevel = value.toUpperCase();
                break;
            case "loggerfile":
            case "logfile":
                this.loggerFile = value;
                break;
            default:
                extraProperties.setProperty(key, value);
                break;
        }
    }

    /**
     * Gets a property by name.
     *
     * @param key property name
     * @return property value or null
     */
    public String getProperty(String key) {
        if (key == null) {
            return null;
        }

        switch (key.toLowerCase()) {
            case "host":
            case "servername":
                return host;
            case "port":
            case "portnumber":
                return String.valueOf(port);
            case "database":
            case "databasename":
            case "dbname":
                return database;
            case "user":
            case "username":
                return user;
            case "password":
                return password;
            case "ssl":
                return ssl;
            case "sslmode":
                return sslMode != null ? sslMode : ssl;
            case "sslcert":
                return sslCert;
            case "sslkey":
                return sslKey;
            case "sslrootcert":
                return sslRootCert;
            case "sslpassword":
                return sslPassword;
            case "connecttimeout":
                return String.valueOf(connectTimeout);
            case "sockettimeout":
                return String.valueOf(socketTimeout);
            case "logintimeout":
                return String.valueOf(loginTimeout);
            case "tcpkeepalive":
                return String.valueOf(tcpKeepAlive);
            case "currentschema":
                return currentSchema;
            case "applicationname":
                return applicationName;
            case "readonly":
                return String.valueOf(readOnly);
            case "autocommit":
                return String.valueOf(autoCommit);
            case "defaultrowfetchsize":
                return String.valueOf(defaultRowFetchSize);
            case "preparethreshold":
                return String.valueOf(prepareThreshold);
            case "binarytransfer":
                return String.valueOf(binaryTransfer);
            case "rewritebatchedinserts":
                return String.valueOf(reWriteBatchedInserts);
            case "loggerlevel":
                return loggerLevel;
            case "loggerfile":
                return loggerFile;
            default:
                return extraProperties.getProperty(key);
        }
    }

    // Getters and setters

    public String getHost() {
        return host;
    }

    public void setHost(String host) {
        this.host = host;
    }

    public int getPort() {
        return port;
    }

    public void setPort(int port) {
        this.port = port;
    }

    public String getDatabase() {
        return database;
    }

    public void setDatabase(String database) {
        this.database = database;
    }

    public String getUser() {
        return user;
    }

    public void setUser(String user) {
        this.user = user;
    }

    public String getPassword() {
        return password;
    }

    public void setPassword(String password) {
        this.password = password;
    }

    public String getSsl() {
        return ssl;
    }

    public void setSsl(String ssl) {
        this.ssl = ssl;
    }

    public String getSslMode() {
        return sslMode != null ? sslMode : ssl;
    }

    public void setSslMode(String sslMode) {
        this.sslMode = sslMode;
    }

    public String getSslCert() {
        return sslCert;
    }

    public void setSslCert(String sslCert) {
        this.sslCert = sslCert;
    }

    public String getSslKey() {
        return sslKey;
    }

    public void setSslKey(String sslKey) {
        this.sslKey = sslKey;
    }

    public String getSslRootCert() {
        return sslRootCert;
    }

    public void setSslRootCert(String sslRootCert) {
        this.sslRootCert = sslRootCert;
    }

    public String getSslPassword() {
        return sslPassword;
    }

    public void setSslPassword(String sslPassword) {
        this.sslPassword = sslPassword;
    }

    public int getConnectTimeout() {
        return connectTimeout;
    }

    public void setConnectTimeout(int connectTimeout) {
        this.connectTimeout = connectTimeout;
    }

    public int getSocketTimeout() {
        return socketTimeout;
    }

    public void setSocketTimeout(int socketTimeout) {
        this.socketTimeout = socketTimeout;
    }

    public int getLoginTimeout() {
        return loginTimeout;
    }

    public void setLoginTimeout(int loginTimeout) {
        this.loginTimeout = loginTimeout;
    }

    public boolean isTcpKeepAlive() {
        return tcpKeepAlive;
    }

    public void setTcpKeepAlive(boolean tcpKeepAlive) {
        this.tcpKeepAlive = tcpKeepAlive;
    }

    public String getCurrentSchema() {
        return currentSchema;
    }

    public void setCurrentSchema(String currentSchema) {
        this.currentSchema = currentSchema;
    }

    public String getApplicationName() {
        return applicationName;
    }

    public void setApplicationName(String applicationName) {
        this.applicationName = applicationName;
    }

    public boolean isReadOnly() {
        return readOnly;
    }

    public void setReadOnly(boolean readOnly) {
        this.readOnly = readOnly;
    }

    public boolean isAutoCommit() {
        return autoCommit;
    }

    public void setAutoCommit(boolean autoCommit) {
        this.autoCommit = autoCommit;
    }

    public int getDefaultRowFetchSize() {
        return defaultRowFetchSize;
    }

    public void setDefaultRowFetchSize(int defaultRowFetchSize) {
        this.defaultRowFetchSize = defaultRowFetchSize;
    }

    public int getPrepareThreshold() {
        return prepareThreshold;
    }

    public void setPrepareThreshold(int prepareThreshold) {
        this.prepareThreshold = prepareThreshold;
    }

    public boolean isBinaryTransfer() {
        return binaryTransfer;
    }

    public void setBinaryTransfer(boolean binaryTransfer) {
        this.binaryTransfer = binaryTransfer;
    }

    public boolean isReWriteBatchedInserts() {
        return reWriteBatchedInserts;
    }

    public void setReWriteBatchedInserts(boolean reWriteBatchedInserts) {
        this.reWriteBatchedInserts = reWriteBatchedInserts;
    }

    public String getLoggerLevel() {
        return loggerLevel;
    }

    public void setLoggerLevel(String loggerLevel) {
        this.loggerLevel = loggerLevel;
    }

    public String getLoggerFile() {
        return loggerFile;
    }

    public void setLoggerFile(String loggerFile) {
        this.loggerFile = loggerFile;
    }

    public Properties getExtraProperties() {
        return extraProperties;
    }

    /**
     * Checks if SSL is required.
     *
     * @return true if SSL mode requires encryption
     */
    public boolean isSslRequired() {
        String mode = getSslMode();
        return "require".equalsIgnoreCase(mode) ||
               "verify-ca".equalsIgnoreCase(mode) ||
               "verify-full".equalsIgnoreCase(mode);
    }

    /**
     * Checks if SSL certificate verification is required.
     *
     * @return true if SSL mode requires certificate verification
     */
    public boolean isSslVerify() {
        String mode = getSslMode();
        return "verify-ca".equalsIgnoreCase(mode) ||
               "verify-full".equalsIgnoreCase(mode);
    }

    /**
     * Converts to Properties object.
     *
     * @return Properties containing all settings
     */
    public Properties toProperties() {
        Properties props = new Properties();
        props.setProperty("host", host);
        props.setProperty("port", String.valueOf(port));
        if (database != null) props.setProperty("database", database);
        if (user != null) props.setProperty("user", user);
        if (password != null) props.setProperty("password", password);
        props.setProperty("ssl", ssl);
        if (sslMode != null) props.setProperty("sslMode", sslMode);
        if (sslCert != null) props.setProperty("sslCert", sslCert);
        if (sslKey != null) props.setProperty("sslKey", sslKey);
        if (sslRootCert != null) props.setProperty("sslRootCert", sslRootCert);
        props.setProperty("connectTimeout", String.valueOf(connectTimeout));
        props.setProperty("socketTimeout", String.valueOf(socketTimeout));
        props.setProperty("loginTimeout", String.valueOf(loginTimeout));
        props.setProperty("tcpKeepAlive", String.valueOf(tcpKeepAlive));
        props.setProperty("currentSchema", currentSchema);
        if (applicationName != null) props.setProperty("ApplicationName", applicationName);
        props.setProperty("readOnly", String.valueOf(readOnly));
        props.setProperty("autoCommit", String.valueOf(autoCommit));
        props.setProperty("defaultRowFetchSize", String.valueOf(defaultRowFetchSize));
        props.setProperty("prepareThreshold", String.valueOf(prepareThreshold));
        props.setProperty("binaryTransfer", String.valueOf(binaryTransfer));
        props.setProperty("reWriteBatchedInserts", String.valueOf(reWriteBatchedInserts));
        props.setProperty("loggerLevel", loggerLevel);
        if (loggerFile != null) props.setProperty("loggerFile", loggerFile);
        props.putAll(extraProperties);
        return props;
    }

    @Override
    public String toString() {
        return "SBConnectionProperties{" +
               "host='" + host + '\'' +
               ", port=" + port +
               ", database='" + database + '\'' +
               ", user='" + user + '\'' +
               ", ssl='" + ssl + '\'' +
               ", currentSchema='" + currentSchema + '\'' +
               '}';
    }
}
