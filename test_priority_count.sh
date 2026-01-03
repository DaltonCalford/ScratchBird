#!/bin/bash
cd /home/dcalford/CliWork/ScratchBird

P0_RAW=$(grep -r "Priority.*P0" docs/specifications/beta_requirements/ 2>/dev/null | wc -l)
P1_RAW=$(grep -r "Priority.*P1" docs/specifications/beta_requirements/ 2>/dev/null | wc -l)
P2_RAW=$(grep -r "Priority.*P2" docs/specifications/beta_requirements/ 2>/dev/null | wc -l)

P0_COUNT=$((P0_RAW + 0))
P1_COUNT=$((P1_RAW + 0))
P2_COUNT=$((P2_RAW + 0))

echo "P0: RAW='$P0_RAW' COUNT='$P0_COUNT'"
echo "P1: RAW='$P1_RAW' COUNT='$P1_COUNT'"
echo "P2: RAW='$P2_RAW' COUNT='$P2_COUNT'"
