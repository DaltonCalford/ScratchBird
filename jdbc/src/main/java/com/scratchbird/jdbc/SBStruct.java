/*
 * ScratchBird JDBC Driver
 * Copyright (c) 2025 ScratchBird Project
 */
package com.scratchbird.jdbc;

import java.sql.*;
import java.util.*;

/**
 * JDBC Struct implementation for ScratchBird.
 */
public class SBStruct implements Struct {
    private final String typeName;
    private final Object[] attributes;

    public SBStruct(String typeName, Object[] attributes) {
        this.typeName = typeName;
        this.attributes = attributes != null ? attributes.clone() : new Object[0];
    }

    @Override
    public String getSQLTypeName() throws SQLException {
        return typeName;
    }

    @Override
    public Object[] getAttributes() throws SQLException {
        return attributes.clone();
    }

    @Override
    public Object[] getAttributes(Map<String, Class<?>> map) throws SQLException {
        return getAttributes();
    }
}
