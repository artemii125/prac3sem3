#!/bin/bash

echo "=== Тест API биржи ==="
echo

echo "1. Создание пользователя 1"
USER1=$(curl -s -X POST http://localhost:8080/user -H "Content-Type: application/json" -d '{"username": "alice"}')
echo $USER1
KEY1=$(echo $USER1 | grep -o '"key":"[^"]*"' | cut -d'"' -f4)
echo "Ключ пользователя 1: $KEY1"
echo

echo "2. Создание пользователя 2"
USER2=$(curl -s -X POST http://localhost:8080/user -H "Content-Type: application/json" -d '{"username": "bob"}')
echo $USER2
KEY2=$(echo $USER2 | grep -o '"key":"[^"]*"' | cut -d'"' -f4)
echo "Ключ пользователя 2: $KEY2"
echo

echo "3. Получение списка лотов"
curl -s http://localhost:8080/lot | jq .
echo

echo "4. Получение списка пар"
curl -s http://localhost:8080/pair | jq .
echo

echo "5. Баланс пользователя 1"
curl -s http://localhost:8080/balance -H "X-USER-KEY: $KEY1" | jq .
echo

echo "6. Создание ордера на покупку (пользователь 1 покупает BTC за RUB)"
ORDER1=$(curl -s -X POST http://localhost:8080/order \
  -H "Content-Type: application/json" \
  -H "X-USER-KEY: $KEY1" \
  -d '{"pair_id": 2, "quantity": 0.5, "price": 5000000, "type": "buy"}')
echo $ORDER1
echo

echo "7. Создание ордера на продажу (пользователь 2 продает BTC за RUB)"
ORDER2=$(curl -s -X POST http://localhost:8080/order \
  -H "Content-Type: application/json" \
  -H "X-USER-KEY: $KEY2" \
  -d '{"pair_id": 2, "quantity": 0.3, "price": 4900000, "type": "sell"}')
echo $ORDER2
echo

echo "8. Список всех ордеров"
curl -s http://localhost:8080/order | jq .
echo

echo "9. Баланс пользователя 1 после сделки"
curl -s http://localhost:8080/balance -H "X-USER-KEY: $KEY1" | jq .
echo

echo "10. Баланс пользователя 2 после сделки"
curl -s http://localhost:8080/balance -H "X-USER-KEY: $KEY2" | jq .
