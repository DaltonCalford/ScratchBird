#!/bin/bash
set -euo pipefail
cd /home/dcalford/CliWork/ScratchBird

echo "Finding beta requirements directories..."
BETA_DIRS=$(find docs/specifications/beta_requirements/ -mindepth 1 -type d 2>/dev/null | wc -l || echo 0)
echo "BETA_DIRS=$BETA_DIRS"

echo "Counting P0 items..."
P0_RAW=$(grep -r "Priority.*P0" docs/specifications/beta_requirements/ 2>/dev/null | wc -l)
echo "P0_RAW=$P0_RAW"

echo "Counting P1 items..."
P1_RAW=$(grep -r "Priority.*P1" docs/specifications/beta_requirements/ 2>/dev/null | wc -l)
echo "P1_RAW=$P1_RAW"

echo "Counting P2 items..."
P2_RAW=$(grep -r "Priority.*P2" docs/specifications/beta_requirements/ 2>/dev/null | wc -l)
echo "P2_RAW=$P2_RAW"

echo "Converting to integers..."
P0_COUNT=$((P0_RAW + 0))
P1_COUNT=$((P1_RAW + 0))
P2_COUNT=$((P2_RAW + 0))

echo "P0_COUNT=$P0_COUNT"
echo "P1_COUNT=$P1_COUNT"
echo "P2_COUNT=$P2_COUNT"

echo "Done!"
