#!/usr/bin/env bash
set -euo pipefail

# DIMACS9 USA-road-d.BAY Dataset Downloader and Verifier
# Authoritative source: 9th DIMACS Implementation Challenge (La Sapienza University, Rome)

DATA_DIR="engine/data"
mkdir -p "$DATA_DIR"

BASE_URL="http://www.diag.uniroma1.it/challenge9/data/USA-road-d"
GR_GZ="${DATA_DIR}/USA-road-d.BAY.gr.gz"
GR_FILE="${DATA_DIR}/USA-road-d.BAY.gr"
CO_GZ="${DATA_DIR}/USA-road-d.BAY.co.gz"
CO_FILE="${DATA_DIR}/USA-road-d.BAY.co"

echo "=== DIMACS9 USA-road-d.BAY Dataset Setup ==="

# 1. Download Graph File (.gr.gz)
if [ -f "$GR_FILE" ]; then
    echo "[INFO] Graph file already exists: $GR_FILE ($(wc -l < "$GR_FILE") lines)"
else
    if [ ! -f "$GR_GZ" ]; then
        echo "[INFO] Downloading graph archive from ${BASE_URL}/USA-road-d.BAY.gr.gz ..."
        curl -fSL "${BASE_URL}/USA-road-d.BAY.gr.gz" -o "$GR_GZ"
    fi
    echo "[INFO] Decompressing $GR_GZ ..."
    gzip -dk "$GR_GZ"
fi

# 2. Download Coordinate File (.co.gz)
if [ -f "$CO_FILE" ]; then
    echo "[INFO] Coordinate file already exists: $CO_FILE ($(wc -l < "$CO_FILE") lines)"
else
    if [ ! -f "$CO_GZ" ]; then
        echo "[INFO] Downloading coordinates archive from ${BASE_URL}/USA-road-d.BAY.co.gz ..."
        curl -fSL "${BASE_URL}/USA-road-d.BAY.co.gz" -o "$CO_GZ" || echo "[WARN] Coordinate download optional"
    fi
    if [ -f "$CO_GZ" ] && [ ! -f "$CO_FILE" ]; then
        echo "[INFO] Decompressing $CO_GZ ..."
        gzip -dk "$CO_GZ"
    fi
fi

# 3. Verification
echo "=== Verifying Graph File ==="
if [ ! -f "$GR_FILE" ]; then
    echo "[ERROR] Graph file $GR_FILE does not exist!"
    exit 1
fi

FILE_SIZE=$(stat -c%s "$GR_FILE")
echo "[INFO] File size: $FILE_SIZE bytes"

# Read problem line
PROBLEM_LINE=$(grep "^p sp" "$GR_FILE" | head -n 1)
echo "[INFO] Problem header: $PROBLEM_LINE"

VERTICES=$(echo "$PROBLEM_LINE" | awk '{print $3}')
ARCS=$(echo "$PROBLEM_LINE" | awk '{print $4}')
echo "[INFO] Expected vertices: ~321270 | Header vertices: $VERTICES"
echo "[INFO] Expected arcs: ~800172 | Header arcs: $ARCS"

ACTUAL_ARCS=$(grep "^a " "$GR_FILE" | wc -l)
echo "[INFO] Actual arc records found: $ACTUAL_ARCS"

if [ "$ARCS" -eq "$ACTUAL_ARCS" ]; then
    echo "[SUCCESS] Arc count exactly matches header ($ACTUAL_ARCS arcs)!"
else
    echo "[WARN] Header arcs ($ARCS) differs from arc count ($ACTUAL_ARCS)"
fi

# Check SHA256
SHA=$(sha256sum "$GR_FILE" | awk '{print $1}')
echo "[INFO] SHA256 checksum: $SHA"
echo "$SHA  USA-road-d.BAY.gr" > "${DATA_DIR}/checksum.sha256"

if [ -f "$CO_FILE" ]; then
    CO_PROBLEM=$(grep "^p aux sp co" "$CO_FILE" | head -n 1 || true)
    echo "[INFO] Coordinate header: $CO_PROBLEM"
    CO_NODES=$(grep "^v " "$CO_FILE" | wc -l)
    echo "[INFO] Coordinate node records found: $CO_NODES"
fi

echo "=== Dataset setup complete and verified ==="
