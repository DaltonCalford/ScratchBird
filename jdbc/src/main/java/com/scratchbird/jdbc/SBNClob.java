/*
 * ScratchBird JDBC Driver
 * Copyright (c) 2025 ScratchBird Project
 */
package com.scratchbird.jdbc;

import java.sql.*;

/**
 * JDBC NClob implementation for ScratchBird.
 */
public class SBNClob extends SBClob implements NClob {
    public SBNClob() {
        super();
    }

    public SBNClob(String s) {
        super(s);
    }
}
