#!/bin/bash

set -e

echo "=== Настройка базы данных ==="

read -p "PostgreSQL host [localhost]: " DB_HOST
DB_HOST=${DB_HOST:-localhost}

read -p "PostgreSQL port [5432]: " DB_PORT
DB_PORT=${DB_PORT:-5432}

read -p "PostgreSQL superuser [postgres]: " PG_USER
PG_USER=${PG_USER:-postgres}

read -sp "PostgreSQL superuser password: " PG_PASSWORD
echo

read -p "Database name [warehouse_db]: " DB_NAME
DB_NAME=${DB_NAME:-warehouse_db}

read -p "Database user [warehouse_user]: " DB_USER
DB_USER=${DB_USER:-warehouse_user}

read -sp "Database user password: " DB_PASSWORD
echo

echo "1. Подключение к PostgreSQL..."
export PGPASSWORD="$PG_PASSWORD"

echo "2. Создание базы данных..."
psql -h "$DB_HOST" -p "$DB_PORT" -U "$PG_USER" -c "CREATE DATABASE $DB_NAME;" 2>/dev/null || echo "База данных уже существует"

echo "3. Создание пользователя..."
psql -h "$DB_HOST" -p "$DB_PORT" -U "$PG_USER" -c "CREATE USER $DB_USER WITH PASSWORD '$DB_PASSWORD';" 2>/dev/null || echo "Пользователь уже существует"

echo "4. Назначение прав..."
psql -h "$DB_HOST" -p "$DB_PORT" -U "$PG_USER" -c "GRANT ALL PRIVILEGES ON DATABASE $DB_NAME TO $DB_USER;"
psql -h "$DB_HOST" -p "$DB_PORT" -U "$PG_USER" -d "$DB_NAME" -c "GRANT CREATE ON SCHEMA public TO $DB_USER;"

echo "5. Создание таблиц..."
echo "   Подключение к базе данных $DB_NAME..."
export PGPASSWORD="$DB_PASSWORD"

SQL_FILE="database/schema.sql"
if [ -f "$SQL_FILE" ]; then
    psql -h "$DB_HOST" -p "$DB_PORT" -U "$DB_USER" -d "$DB_NAME" -f "$SQL_FILE"
else
    echo "   SQL файл не найден: $SQL_FILE"
    echo "   Создайте schema.sql с определением таблиц"
fi

echo "6. Создание тестовых данных..."
read -p "Создать тестовые данные? (y/n): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    TEST_DATA_FILE="database/test_data.sql"
    if [ -f "$TEST_DATA_FILE" ]; then
        psql -h "$DB_HOST" -p "$DB_PORT" -U "$DB_USER" -d "$DB_NAME" -f "$TEST_DATA_FILE"
        echo "   Тестовые данные созданы"
    else
        echo "   Файл тестовых данных не найден"
    fi
fi

echo "7. Обновление конфигурации..."
CONFIG_FILE="config/config.json"
if [ -f "$CONFIG_FILE" ]; then
    echo "   Обновите $CONFIG_FILE вручную с следующими значениями:"
    echo "   host: $DB_HOST"
    echo "   port: $DB_PORT"
    echo "   database: $DB_NAME"
    echo "   user: $DB_USER"
    echo "   password: $DB_PASSWORD"
else
    echo "   Конфигурационный файл не найден"
fi

echo "=== Настройка базы данных завершена! ==="

unset PGPASSWORD
