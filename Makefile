CXX = g++
CXXFLAGS = -std=c++17 -Iinclude -Wall

DB_SOURCES = src/db/Collection.cpp src/db/Schema.cpp src/db/SQLProcessor.cpp src/db/Server.cpp src/db/dataBase.cpp
ADT_SOURCES = src/massive.cpp src/singlyLinkedList.cpp src/doubleLinkedList.cpp src/stack.cpp src/Utils.cpp
EXCHANGE_SOURCES = src/db/Exchange.cpp src/db/HttpServer.cpp

DB_OBJECTS = $(DB_SOURCES:.cpp=.o)
ADT_OBJECTS = $(ADT_SOURCES:.cpp=.o)
EXCHANGE_OBJECTS = $(EXCHANGE_SOURCES:.cpp=.o)

all: DBrun ExchangeServer

DBrun: $(DB_OBJECTS) $(ADT_OBJECTS) src/main.o
	$(CXX) $(CXXFLAGS) -o DBrun $(DB_OBJECTS) $(ADT_OBJECTS) src/main.o

ExchangeServer: $(DB_OBJECTS) $(ADT_OBJECTS) $(EXCHANGE_OBJECTS) src/exchange_main.o
	$(CXX) $(CXXFLAGS) -o ExchangeServer $(EXCHANGE_OBJECTS) src/exchange_main.o

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(DB_OBJECTS) $(ADT_OBJECTS) $(EXCHANGE_OBJECTS) src/main.o src/exchange_main.o DBrun ExchangeServer
