#!/bin/bash

set -e

BUILD_TYPE="${1:-Release}"
BUILD_DIR="build_${BUILD_TYPE,,}"

echo "=== Сборка проекта ($BUILD_TYPE) ==="

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "Конфигурация CMake..."
cmake .. -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

echo "Сборка проекта..."
make -j$(nproc)

echo "=== Сборка завершена ==="
echo "Исполняемый файл: $BUILD_DIR/warehouse_backend"
