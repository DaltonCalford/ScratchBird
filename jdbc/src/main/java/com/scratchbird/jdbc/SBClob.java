/*
 * ScratchBird JDBC Driver
 * Copyright (c) 2025 ScratchBird Project
 */
package com.scratchbird.jdbc;

import java.io.*;
import java.sql.*;

/**
 * JDBC Clob implementation for ScratchBird.
 */
public class SBClob implements Clob {
    private StringBuilder data;

    public SBClob() {
        this.data = new StringBuilder();
    }

    public SBClob(String s) {
        this.data = new StringBuilder(s != null ? s : "");
    }

    @Override
    public long length() throws SQLException {
        return data.length();
    }

    @Override
    public String getSubString(long pos, int length) throws SQLException {
        if (pos < 1 || pos > data.length()) {
            throw new SQLException("Invalid position: " + pos, "HY090");
        }
        int start = (int) (pos - 1);
        int end = Math.min(start + length, data.length());
        return data.substring(start, end);
    }

    @Override
    public Reader getCharacterStream() throws SQLException {
        return new StringReader(data.toString());
    }

    @Override
    public Reader getCharacterStream(long pos, long length) throws SQLException {
        return new StringReader(getSubString(pos, (int) length));
    }

    @Override
    public InputStream getAsciiStream() throws SQLException {
        try {
            return new ByteArrayInputStream(data.toString().getBytes("US-ASCII"));
        } catch (UnsupportedEncodingException e) {
            throw new SQLException("ASCII encoding not supported", "HY000", e);
        }
    }

    @Override
    public long position(String searchstr, long start) throws SQLException {
        if (start < 1) return -1;
        int index = data.indexOf(searchstr, (int) (start - 1));
        return index >= 0 ? index + 1 : -1;
    }

    @Override
    public long position(Clob searchstr, long start) throws SQLException {
        return position(searchstr.getSubString(1, (int) searchstr.length()), start);
    }

    @Override
    public int setString(long pos, String str) throws SQLException {
        return setString(pos, str, 0, str.length());
    }

    @Override
    public int setString(long pos, String str, int offset, int len) throws SQLException {
        if (pos < 1) throw new SQLException("Invalid position: " + pos, "HY090");
        int start = (int) (pos - 1);
        String toInsert = str.substring(offset, offset + len);
        if (start >= data.length()) {
            while (data.length() < start) data.append(' ');
            data.append(toInsert);
        } else {
            data.replace(start, Math.min(start + len, data.length()), toInsert);
        }
        return len;
    }

    @Override
    public OutputStream setAsciiStream(long pos) throws SQLException {
        final long startPos = pos;
        return new ByteArrayOutputStream() {
            @Override
            public void close() throws IOException {
                try {
                    setString(startPos, toString("US-ASCII"));
                } catch (SQLException e) {
                    throw new IOException(e);
                }
            }
        };
    }

    @Override
    public Writer setCharacterStream(long pos) throws SQLException {
        final long startPos = pos;
        return new StringWriter() {
            @Override
            public void close() throws IOException {
                try {
                    setString(startPos, toString());
                } catch (SQLException e) {
                    throw new IOException(e);
                }
            }
        };
    }

    @Override
    public void truncate(long len) throws SQLException {
        if (len < data.length()) {
            data.setLength((int) len);
        }
    }

    @Override
    public void free() throws SQLException {
        data = new StringBuilder();
    }
}
