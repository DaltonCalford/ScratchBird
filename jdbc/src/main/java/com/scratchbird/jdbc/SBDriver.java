/*
 * ScratchBird JDBC Driver
 * Copyright (c) 2025 ScratchBird Project
 *
 * JDBC 4.3 compliant Type 4 (pure Java) driver for ScratchBird database.
 */
package com.scratchbird.jdbc;

import java.sql.*;
import java.util.*;
import java.util.logging.Logger;
import java.util.logging.Level;

/**
 * ScratchBird JDBC Driver implementation.
 *
 * <p>This is a Type 4 (pure Java) JDBC driver that communicates directly
 * with ScratchBird database servers using the native wire protocol.</p>
 *
 * <h2>Connection URL Format</h2>
 * <pre>
 * jdbc:scratchbird://host[:port]/database[?param1=value1&amp;param2=value2]
 * </pre>
 *
 * <h2>Example Usage</h2>
 * <pre>
 * // Driver auto-loads via SPI (JDBC 4.0+)
 * Connection conn = DriverManager.getConnection(
 *     "jdbc:scratchbird://localhost:3092/mydb",
 *     "user", "password"
 * );
 * </pre>
 *
 * @author ScratchBird Project
 * @version 1.0.0
 */
public class SBDriver implements Driver {

    /** URL prefix for ScratchBird connections */
    public static final String URL_PREFIX = "jdbc:scratchbird:";

    /** URL prefix with protocol separator */
    public static final String URL_PREFIX_FULL = "jdbc:scratchbird://";

    /** Default port for ScratchBird native protocol */
    public static final int DEFAULT_PORT = 3092;

    /** Driver major version */
    public static final int MAJOR_VERSION = 1;

    /** Driver minor version */
    public static final int MINOR_VERSION = 0;

    /** Driver patch version */
    public static final int PATCH_VERSION = 0;

    /** Full driver version string */
    public static final String VERSION = MAJOR_VERSION + "." + MINOR_VERSION + "." + PATCH_VERSION;

    /** Driver name */
    public static final String DRIVER_NAME = "ScratchBird JDBC Driver";

    /** Logger for this class */
    private static final Logger LOGGER = Logger.getLogger(SBDriver.class.getName());

    /** Singleton instance for registration */
    private static SBDriver registeredDriver;

    // Static initializer - register driver with DriverManager
    static {
        try {
            registeredDriver = new SBDriver();
            DriverManager.registerDriver(registeredDriver);
            LOGGER.log(Level.FINE, "ScratchBird JDBC Driver {0} registered", VERSION);
        } catch (SQLException e) {
            LOGGER.log(Level.SEVERE, "Failed to register ScratchBird JDBC Driver", e);
            throw new ExceptionInInitializerError(e);
        }
    }

    /**
     * Default constructor.
     */
    public SBDriver() {
        // Required for Class.forName() and SPI
    }

    /**
     * Attempts to connect to the given database URL.
     *
     * @param url the database URL
     * @param info connection properties
     * @return a Connection object, or null if URL is not for this driver
     * @throws SQLException if a database access error occurs
     */
    @Override
    public Connection connect(String url, Properties info) throws SQLException {
        if (!acceptsURL(url)) {
            return null;
        }

        // Parse URL and create connection
        SBConnectionProperties props = parseURL(url, info);
        return new SBConnection(props);
    }

    /**
     * Checks if this driver can handle the given URL.
     *
     * @param url the URL to check
     * @return true if URL starts with "jdbc:scratchbird:"
     * @throws SQLException if a database access error occurs
     */
    @Override
    public boolean acceptsURL(String url) throws SQLException {
        if (url == null) {
            return false;
        }
        return url.startsWith(URL_PREFIX);
    }

    /**
     * Gets information about possible connection properties.
     *
     * @param url the database URL
     * @param info proposed connection properties
     * @return array of DriverPropertyInfo objects
     * @throws SQLException if a database access error occurs
     */
    @Override
    public DriverPropertyInfo[] getPropertyInfo(String url, Properties info) throws SQLException {
        Properties props = info != null ? new Properties(info) : new Properties();

        // Parse URL to get defaults
        if (url != null && acceptsURL(url)) {
            try {
                SBConnectionProperties parsed = parseURL(url, props);
                props.setProperty("user", parsed.getUser() != null ? parsed.getUser() : "");
                props.setProperty("host", parsed.getHost());
                props.setProperty("port", String.valueOf(parsed.getPort()));
                props.setProperty("database", parsed.getDatabase() != null ? parsed.getDatabase() : "");
            } catch (SQLException e) {
                // Ignore parse errors for property info
            }
        }

        List<DriverPropertyInfo> propList = new ArrayList<>();

        // User property
        DriverPropertyInfo userProp = new DriverPropertyInfo("user", props.getProperty("user", ""));
        userProp.description = "Database username";
        userProp.required = true;
        propList.add(userProp);

        // Password property
        DriverPropertyInfo passProp = new DriverPropertyInfo("password", "");
        passProp.description = "Database password";
        passProp.required = true;
        propList.add(passProp);

        // SSL property
        DriverPropertyInfo sslProp = new DriverPropertyInfo("ssl", props.getProperty("ssl", "prefer"));
        sslProp.description = "SSL mode: disable, allow, prefer, require, verify-ca, verify-full";
        sslProp.choices = new String[]{"disable", "allow", "prefer", "require", "verify-ca", "verify-full"};
        propList.add(sslProp);

        // Connect timeout
        DriverPropertyInfo timeoutProp = new DriverPropertyInfo("connectTimeout",
            props.getProperty("connectTimeout", "30"));
        timeoutProp.description = "Connection timeout in seconds";
        propList.add(timeoutProp);

        // Socket timeout
        DriverPropertyInfo socketProp = new DriverPropertyInfo("socketTimeout",
            props.getProperty("socketTimeout", "0"));
        socketProp.description = "Socket timeout in seconds (0 = unlimited)";
        propList.add(socketProp);

        // Current schema
        DriverPropertyInfo schemaProp = new DriverPropertyInfo("currentSchema",
            props.getProperty("currentSchema", "public"));
        schemaProp.description = "Default schema";
        propList.add(schemaProp);

        // Application name
        DriverPropertyInfo appProp = new DriverPropertyInfo("ApplicationName",
            props.getProperty("ApplicationName", ""));
        appProp.description = "Application identifier";
        propList.add(appProp);

        // Read only
        DriverPropertyInfo readOnlyProp = new DriverPropertyInfo("readOnly",
            props.getProperty("readOnly", "false"));
        readOnlyProp.description = "Read-only connection";
        readOnlyProp.choices = new String[]{"true", "false"};
        propList.add(readOnlyProp);

        // Auto commit
        DriverPropertyInfo autoCommitProp = new DriverPropertyInfo("autoCommit",
            props.getProperty("autoCommit", "true"));
        autoCommitProp.description = "Auto-commit mode";
        autoCommitProp.choices = new String[]{"true", "false"};
        propList.add(autoCommitProp);

        // Fetch size
        DriverPropertyInfo fetchProp = new DriverPropertyInfo("defaultRowFetchSize",
            props.getProperty("defaultRowFetchSize", "0"));
        fetchProp.description = "Default fetch size (0 = all rows)";
        propList.add(fetchProp);

        // Prepare threshold
        DriverPropertyInfo prepareProp = new DriverPropertyInfo("prepareThreshold",
            props.getProperty("prepareThreshold", "5"));
        prepareProp.description = "Number of executions before statement is prepared server-side";
        propList.add(prepareProp);

        // Binary transfer
        DriverPropertyInfo binaryProp = new DriverPropertyInfo("binaryTransfer",
            props.getProperty("binaryTransfer", "true"));
        binaryProp.description = "Use binary protocol for data transfer";
        binaryProp.choices = new String[]{"true", "false"};
        propList.add(binaryProp);

        // Batch insert rewrite
        DriverPropertyInfo batchProp = new DriverPropertyInfo("reWriteBatchedInserts",
            props.getProperty("reWriteBatchedInserts", "false"));
        batchProp.description = "Rewrite batched inserts as multi-value INSERT";
        batchProp.choices = new String[]{"true", "false"};
        propList.add(batchProp);

        // Logger level
        DriverPropertyInfo logLevelProp = new DriverPropertyInfo("loggerLevel",
            props.getProperty("loggerLevel", "OFF"));
        logLevelProp.description = "Logging level";
        logLevelProp.choices = new String[]{"OFF", "DEBUG", "TRACE"};
        propList.add(logLevelProp);

        // Logger file
        DriverPropertyInfo logFileProp = new DriverPropertyInfo("loggerFile",
            props.getProperty("loggerFile", ""));
        logFileProp.description = "Log file path";
        propList.add(logFileProp);

        return propList.toArray(new DriverPropertyInfo[0]);
    }

    /**
     * Gets the driver's major version number.
     *
     * @return major version (1)
     */
    @Override
    public int getMajorVersion() {
        return MAJOR_VERSION;
    }

    /**
     * Gets the driver's minor version number.
     *
     * @return minor version (0)
     */
    @Override
    public int getMinorVersion() {
        return MINOR_VERSION;
    }

    /**
     * Reports whether this driver is a genuine JDBC Compliant driver.
     *
     * @return true (driver passes JDBC compliance tests)
     */
    @Override
    public boolean jdbcCompliant() {
        return true;
    }

    /**
     * Returns the parent logger for this driver.
     *
     * @return the parent Logger
     * @throws SQLFeatureNotSupportedException never thrown
     */
    @Override
    public Logger getParentLogger() throws SQLFeatureNotSupportedException {
        return LOGGER.getParent();
    }

    /**
     * Parses a JDBC URL and properties into connection properties.
     *
     * @param url the JDBC URL
     * @param info additional properties
     * @return parsed connection properties
     * @throws SQLException if URL is malformed
     */
    public static SBConnectionProperties parseURL(String url, Properties info) throws SQLException {
        if (url == null || !url.startsWith(URL_PREFIX)) {
            throw new SQLException("Invalid URL: " + url);
        }

        SBConnectionProperties props = new SBConnectionProperties();

        // Copy info properties
        if (info != null) {
            for (String key : info.stringPropertyNames()) {
                props.setProperty(key, info.getProperty(key));
            }
        }

        // Parse URL: jdbc:scratchbird://host[:port]/database[?params]
        String remainder = url.substring(URL_PREFIX.length());

        // Handle // prefix
        if (remainder.startsWith("//")) {
            remainder = remainder.substring(2);
        }

        // Split off query string
        String queryString = null;
        int queryIdx = remainder.indexOf('?');
        if (queryIdx >= 0) {
            queryString = remainder.substring(queryIdx + 1);
            remainder = remainder.substring(0, queryIdx);
        }

        // Parse host[:port]/database
        String hostPort;
        String database = null;

        int slashIdx = remainder.indexOf('/');
        if (slashIdx >= 0) {
            hostPort = remainder.substring(0, slashIdx);
            database = remainder.substring(slashIdx + 1);
        } else {
            hostPort = remainder;
        }

        // Parse host:port
        String host = "localhost";
        int port = DEFAULT_PORT;

        if (!hostPort.isEmpty()) {
            // Handle IPv6 addresses: [::1]:port
            if (hostPort.startsWith("[")) {
                int bracketEnd = hostPort.indexOf(']');
                if (bracketEnd < 0) {
                    throw new SQLException("Invalid IPv6 address in URL: " + url);
                }
                host = hostPort.substring(1, bracketEnd);
                String portStr = hostPort.substring(bracketEnd + 1);
                if (portStr.startsWith(":")) {
                    port = Integer.parseInt(portStr.substring(1));
                }
            } else {
                int colonIdx = hostPort.lastIndexOf(':');
                if (colonIdx >= 0) {
                    host = hostPort.substring(0, colonIdx);
                    port = Integer.parseInt(hostPort.substring(colonIdx + 1));
                } else {
                    host = hostPort;
                }
            }
        }

        props.setHost(host);
        props.setPort(port);
        if (database != null && !database.isEmpty()) {
            props.setDatabase(database);
        }

        // Parse query string parameters
        if (queryString != null && !queryString.isEmpty()) {
            String[] pairs = queryString.split("&");
            for (String pair : pairs) {
                int eqIdx = pair.indexOf('=');
                if (eqIdx > 0) {
                    String key = pair.substring(0, eqIdx);
                    String value = pair.substring(eqIdx + 1);
                    // URL decode
                    try {
                        value = java.net.URLDecoder.decode(value, "UTF-8");
                    } catch (java.io.UnsupportedEncodingException e) {
                        // UTF-8 is always supported
                    }
                    props.setProperty(key, value);
                }
            }
        }

        return props;
    }

    /**
     * Gets the registered driver instance.
     *
     * @return the registered driver
     */
    public static SBDriver getRegisteredDriver() {
        return registeredDriver;
    }

    /**
     * Deregisters the driver from DriverManager.
     *
     * @throws SQLException if deregistration fails
     */
    public static void deregister() throws SQLException {
        if (registeredDriver != null) {
            DriverManager.deregisterDriver(registeredDriver);
            registeredDriver = null;
        }
    }

    @Override
    public String toString() {
        return DRIVER_NAME + " " + VERSION;
    }
}
