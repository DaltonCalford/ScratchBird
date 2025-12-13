/*
 * ScratchBird JDBC Driver
 * Copyright (c) 2025 ScratchBird Project
 */
package com.scratchbird.jdbc;

import java.io.*;
import java.sql.*;

/**
 * JDBC Blob implementation for ScratchBird.
 */
public class SBBlob implements Blob {
    private byte[] data;

    public SBBlob() {
        this.data = new byte[0];
    }

    public SBBlob(byte[] data) {
        this.data = data != null ? data.clone() : new byte[0];
    }

    @Override
    public long length() throws SQLException {
        return data.length;
    }

    @Override
    public byte[] getBytes(long pos, int length) throws SQLException {
        if (pos < 1 || pos > data.length) {
            throw new SQLException("Invalid position: " + pos, "HY090");
        }
        int start = (int) (pos - 1);
        int len = Math.min(length, data.length - start);
        byte[] result = new byte[len];
        System.arraycopy(data, start, result, 0, len);
        return result;
    }

    @Override
    public InputStream getBinaryStream() throws SQLException {
        return new ByteArrayInputStream(data);
    }

    @Override
    public InputStream getBinaryStream(long pos, long length) throws SQLException {
        return new ByteArrayInputStream(getBytes(pos, (int) length));
    }

    @Override
    public long position(byte[] pattern, long start) throws SQLException {
        if (start < 1) return -1;
        for (int i = (int) (start - 1); i <= data.length - pattern.length; i++) {
            boolean match = true;
            for (int j = 0; j < pattern.length; j++) {
                if (data[i + j] != pattern[j]) {
                    match = false;
                    break;
                }
            }
            if (match) return i + 1;
        }
        return -1;
    }

    @Override
    public long position(Blob pattern, long start) throws SQLException {
        return position(pattern.getBytes(1, (int) pattern.length()), start);
    }

    @Override
    public int setBytes(long pos, byte[] bytes) throws SQLException {
        return setBytes(pos, bytes, 0, bytes.length);
    }

    @Override
    public int setBytes(long pos, byte[] bytes, int offset, int len) throws SQLException {
        if (pos < 1) throw new SQLException("Invalid position: " + pos, "HY090");
        int start = (int) (pos - 1);
        int required = start + len;
        if (required > data.length) {
            byte[] newData = new byte[required];
            System.arraycopy(data, 0, newData, 0, data.length);
            data = newData;
        }
        System.arraycopy(bytes, offset, data, start, len);
        return len;
    }

    @Override
    public OutputStream setBinaryStream(long pos) throws SQLException {
        final long startPos = pos;
        return new ByteArrayOutputStream() {
            @Override
            public void close() throws IOException {
                try {
                    setBytes(startPos, toByteArray());
                } catch (SQLException e) {
                    throw new IOException(e);
                }
            }
        };
    }

    @Override
    public void truncate(long len) throws SQLException {
        if (len < data.length) {
            byte[] newData = new byte[(int) len];
            System.arraycopy(data, 0, newData, 0, (int) len);
            data = newData;
        }
    }

    @Override
    public void free() throws SQLException {
        data = new byte[0];
    }
}
