#!/bin/bash

echo "=== Очистка проекта ==="

rm -rf build_*
rm -rf bin
rm -rf lib

find . -name "*.o" -delete
find . -name "*.so" -delete
find . -name "*.a" -delete
find . -name "CMakeCache.txt" -delete
find . -name "cmake_install.cmake" -delete
find . -name "Makefile" -delete
find . -name "compile_commands.json" -delete
find . -type d -name "CMakeFiles" -exec rm -rf {} + 2>/dev/null || true

echo "Очистка завершена"
