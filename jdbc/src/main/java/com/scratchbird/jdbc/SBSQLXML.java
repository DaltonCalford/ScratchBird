/*
 * ScratchBird JDBC Driver
 * Copyright (c) 2025 ScratchBird Project
 */
package com.scratchbird.jdbc;

import java.io.*;
import java.sql.*;
import javax.xml.transform.*;
import javax.xml.transform.stream.*;

/**
 * JDBC SQLXML implementation for ScratchBird.
 */
public class SBSQLXML implements SQLXML {
    private String data;
    private boolean freed = false;

    public SBSQLXML() {
        this.data = null;
    }

    public SBSQLXML(String xml) {
        this.data = xml;
    }

    @Override
    public void free() throws SQLException {
        data = null;
        freed = true;
    }

    @Override
    public InputStream getBinaryStream() throws SQLException {
        checkFreed();
        if (data == null) return null;
        try {
            return new ByteArrayInputStream(data.getBytes("UTF-8"));
        } catch (UnsupportedEncodingException e) {
            throw new SQLException("UTF-8 not supported", "HY000", e);
        }
    }

    @Override
    public OutputStream setBinaryStream() throws SQLException {
        checkFreed();
        return new ByteArrayOutputStream() {
            @Override
            public void close() throws IOException {
                data = toString("UTF-8");
            }
        };
    }

    @Override
    public Reader getCharacterStream() throws SQLException {
        checkFreed();
        if (data == null) return null;
        return new StringReader(data);
    }

    @Override
    public Writer setCharacterStream() throws SQLException {
        checkFreed();
        return new StringWriter() {
            @Override
            public void close() throws IOException {
                data = toString();
            }
        };
    }

    @Override
    public String getString() throws SQLException {
        checkFreed();
        return data;
    }

    @Override
    public void setString(String value) throws SQLException {
        checkFreed();
        this.data = value;
    }

    @Override
    public <T extends Source> T getSource(Class<T> sourceClass) throws SQLException {
        checkFreed();
        if (sourceClass == null || sourceClass == StreamSource.class) {
            return sourceClass.cast(new StreamSource(getCharacterStream()));
        }
        throw new SQLFeatureNotSupportedException("Source class not supported: " + sourceClass);
    }

    @Override
    public <T extends Result> T setResult(Class<T> resultClass) throws SQLException {
        checkFreed();
        if (resultClass == null || resultClass == StreamResult.class) {
            return resultClass.cast(new StreamResult(setCharacterStream()));
        }
        throw new SQLFeatureNotSupportedException("Result class not supported: " + resultClass);
    }

    private void checkFreed() throws SQLException {
        if (freed) {
            throw new SQLException("SQLXML has been freed", "HY000");
        }
    }
}
