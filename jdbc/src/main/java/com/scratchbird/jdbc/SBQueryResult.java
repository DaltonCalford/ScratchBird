/*
 * ScratchBird JDBC Driver
 * Copyright (c) 2025 ScratchBird Project
 */
package com.scratchbird.jdbc;

import java.util.*;

/**
 * Result from a query execution.
 */
public class SBQueryResult {
    private List<SBColumnInfo> columns;
    private List<Object[]> rows;
    private String commandTag;
    private long updateCount;

    public List<SBColumnInfo> getColumns() { return columns; }
    public void setColumns(List<SBColumnInfo> columns) { this.columns = columns; }

    public List<Object[]> getRows() { return rows; }
    public void setRows(List<Object[]> rows) { this.rows = rows; }

    public String getCommandTag() { return commandTag; }
    public void setCommandTag(String commandTag) { this.commandTag = commandTag; }

    public long getUpdateCount() { return updateCount; }
    public void setUpdateCount(long updateCount) { this.updateCount = updateCount; }
}
