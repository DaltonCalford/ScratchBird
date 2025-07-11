#!/bin/bash

# ScratchBird Installation Script
echo "🔥 Installing ScratchBird Database v0.5"

# Set installation directories
INSTALL_PREFIX="/opt/scratchbird"
BIN_DIR="$INSTALL_PREFIX/bin"
LIB_DIR="$INSTALL_PREFIX/lib"
DOC_DIR="$INSTALL_PREFIX/doc"

# Create directories
sudo mkdir -p "$BIN_DIR" "$LIB_DIR" "$DOC_DIR"

# Install binaries
echo "Installing ScratchBird tools..."
sudo cp src/isql/sb_isql "$BIN_DIR/"
sudo cp src/burp/sb_gbak "$BIN_DIR/"
sudo cp src/alice/sb_gfix "$BIN_DIR/"
sudo cp src/utilities/gsec/sb_gsec "$BIN_DIR/"
sudo cp src/utilities/gstat/sb_gstat "$BIN_DIR/"

# Set permissions
sudo chmod +x "$BIN_DIR"/*

# Create symlinks in /usr/local/bin
echo "Creating system-wide symlinks..."
sudo ln -sf "$BIN_DIR/sb_isql" /usr/local/bin/
sudo ln -sf "$BIN_DIR/sb_gbak" /usr/local/bin/
sudo ln -sf "$BIN_DIR/sb_gfix" /usr/local/bin/
sudo ln -sf "$BIN_DIR/sb_gsec" /usr/local/bin/
sudo ln -sf "$BIN_DIR/sb_gstat" /usr/local/bin/

echo "✅ ScratchBird installation complete!"
echo "Run 'sb_isql' to start the interactive SQL shell"
