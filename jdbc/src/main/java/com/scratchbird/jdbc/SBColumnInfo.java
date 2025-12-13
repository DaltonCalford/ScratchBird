/*
 * ScratchBird JDBC Driver
 * Copyright (c) 2025 ScratchBird Project
 */
package com.scratchbird.jdbc;

/**
 * Column metadata from server response.
 */
public class SBColumnInfo {
    private String name;
    private int tableOid;
    private short columnNumber;
    private int typeOid;
    private short typeSize;
    private int typeModifier;
    private short formatCode;

    public String getName() { return name; }
    public void setName(String name) { this.name = name; }

    public int getTableOid() { return tableOid; }
    public void setTableOid(int tableOid) { this.tableOid = tableOid; }

    public short getColumnNumber() { return columnNumber; }
    public void setColumnNumber(short columnNumber) { this.columnNumber = columnNumber; }

    public int getTypeOid() { return typeOid; }
    public void setTypeOid(int typeOid) { this.typeOid = typeOid; }

    public short getTypeSize() { return typeSize; }
    public void setTypeSize(short typeSize) { this.typeSize = typeSize; }

    public int getTypeModifier() { return typeModifier; }
    public void setTypeModifier(int typeModifier) { this.typeModifier = typeModifier; }

    public short getFormatCode() { return formatCode; }
    public void setFormatCode(short formatCode) { this.formatCode = formatCode; }
}
