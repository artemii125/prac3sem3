# Практика 3: Биржа криптовалюты

## Описание

Рабочий прототип биржи криптовалют с HTTP API. Использует СУБД из практики 1 и сетевое подключение из практики 2.

## Структура проекта

- `DBrun` - сервер базы данных (порт 7432)
- `ExchangeServer` - HTTP сервер биржи (порт 8080)
- `config.json` - конфигурация биржи (лоты, адрес БД)
- `schema.json` - схема базы данных

## Быстрый старт

### 1. Сборка
```bash
make
```

### 2. Запуск (в двух терминалах)

Терминал 1 - База данных:
```bash
./start_db.sh
```

Терминал 2 - Биржа:
```bash
./start_exchange.sh
```

### 3. Тестирование
```bash
./test_api.sh
```

## API Endpoints

### POST /user - Создание пользователя
```bash
curl -X POST http://localhost:8080/user \
  -H "Content-Type: application/json" \
  -d '{"username": "alice"}'
```
Ответ: `{"key": "a8f5f167f44f4964e6c998dee827110c"}`

При создании пользователь получает 1000 единиц каждого актива.

### POST /order - Создание ордера
```bash
curl -X POST http://localhost:8080/order \
  -H "Content-Type: application/json" \
  -H "X-USER-KEY: <key>" \
  -d '{"pair_id": 1, "quantity": 100, "price": 0.5, "type": "buy"}'
```
Ответ: `{"order_id": 1}`

### GET /order - Список ордеров
```bash
curl http://localhost:8080/order
```

### DELETE /order - Удаление ордера
```bash
curl -X DELETE http://localhost:8080/order \
  -H "Content-Type: application/json" \
  -H "X-USER-KEY: <key>" \
  -d '{"order_id": 1}'
```

### GET /lot - Список лотов
```bash
curl http://localhost:8080/lot
```

### GET /pair - Список пар
```bash
curl http://localhost:8080/pair
```

### GET /balance - Баланс пользователя
```bash
curl http://localhost:8080/balance \
  -H "X-USER-KEY: <key>"
```

## Бизнес-логика

### Валютные пары
Лоты из config.json формируют пары "каждый с каждым":
- RUB/BTC, RUB/ETH, RUB/USDT, RUB/USDC
- BTC/RUB, BTC/ETH, BTC/USDT, BTC/USDC
- и т.д.

### Создание ордера
- **Покупка (buy)**: списывается сумма в валюте покупки (quantity × price)
- **Продажа (sell)**: списывается количество продаваемого актива

### Сопоставление ордеров
При создании нового ордера система автоматически:
1. Ищет встречные ордера по той же паре
2. Проверяет совпадение цен
3. Выполняет сделки (частично или полностью)
4. Обновляет балансы пользователей
5. Закрывает выполненные ордера

### Пример
Пользователь 1 создает ордер: купить 300 RUB за USD по цене 0.015 USD за 1 RUB.
- С баланса списывается: 300 × 0.015 = 4.5 USD

Пользователь 2 создает ордер: продать 200 RUB за USD по цене 0.01 USD за 1 RUB.
- Сделка выполняется по цене 0.01 (цена продавца)
- Пользователь 1: +200 RUB, -2 USD
- Пользователь 2: -200 RUB, +2 USD
- Ордер пользователя 1 разбивается: 200 RUB закрыто, 100 RUB остается открытым

## Конфигурация

`config.json`:
```json
{
  "lots": ["RUB", "BTC", "ETH", "USDT", "USDC"],
  "database_ip": "localhost",
  "database_port": 7432
}
```

## База данных

Схема включает 5 таблиц:
- `user` - пользователи
- `user_lot` - балансы пользователей
- `order` - ордера на покупку/продажу
- `lot` - торгуемые активы
- `pair` - валютные пары
