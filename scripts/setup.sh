#!/bin/bash

set -e

echo "=== Настройка Warehouse Management System Backend ==="

if [[ "$OSTYPE" != "linux-gnu"* ]]; then
    echo "Внимание: Этот скрипт предназначен для Linux систем"
    read -p "Продолжить? (y/n): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

if [ "$EUID" -ne 0 ]; then 
    echo "Скрипт требует прав администратора для установки пакетов"
    echo "Запустите: sudo $0"
    exit 1
fi

echo "1. Обновление списка пакетов..."

echo "2. Установка необходимых пакетов..."
apt-get install -y -qq \
    build-essential \
    cmake \
    g++ \
    gcc \
    git \
    libpoco-dev \
    libpoco-doc \
    libpq-dev \
    postgresql-client \
    postgresql-server-dev-all \
    clang-format \
    clang-tidy \
    valgrind \
    curl \
    wget \
    unzip

echo "3. Проверка установленных версий..."
echo "   CMake: $(cmake --version | head -n1)"
echo "   G++: $(g++ --version | head -n1)"
echo "   POCO: $(poco-config --version 2>/dev/null || echo 'Не найдена команда poco-config')"

echo "4. Настройка PostgreSQL..."
if ! systemctl is-active --quiet postgresql; then
    echo "   Установка PostgreSQL..."
    apt-get install -y -qq postgresql postgresql-contrib
    systemctl start postgresql
    systemctl enable postgresql
fi

echo "5. Создание конфигурационных файлов..."
if [ ! -f "config/config.json" ]; then
    echo "   Копирование примера конфигурации..."
    cp config/config.json.example config/config.json
    echo "   Отредактируйте config/config.json перед запуском"
else
    echo "   Конфигурационный файл уже существует"
fi

echo "6. Настройка прав доступа..."
chmod +x scripts/*.sh
chmod 755 logs/

echo "7. Инициализация Git hooks..."
if [ -d ".git" ]; then
    echo "   Настройка pre-commit hook..."
    cat > .git/hooks/pre-commit << 'PRE_COMMIT_HOOK'
#!/bin/bash
echo "Running pre-commit checks..."

# Проверка форматирования кода
echo "Checking code formatting..."
find src -name "*.cpp" -o -name "*.h" | xargs clang-format --dry-run --Werror

# Проверка сборки
echo "Checking if project builds..."
mkdir -p build_check
cd build_check
cmake .. > /dev/null 2>&1
make -j4 > /dev/null 2>&1
cd ..
rm -rf build_check

echo "Pre-commit checks passed!"
PRE_COMMIT_HOOK
    chmod +x .git/hooks/pre-commit
fi

echo "=== Настройка завершена! ==="
echo ""
echo "Следующие шаги:"
echo "1. Отредактируйте config/config.json"
echo "2. Выполните: ./scripts/setup-database.sh"
echo "3. Соберите проект: mkdir build && cd build && cmake .. && make"
echo "4. Запустите: ./warehouse_backend"
