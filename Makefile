CC = g++
CFLAGS = -Wall -O3
LDLIBS = -lpthread

build: clean
build:
	mkdir -p build dist
build: main server connection link

main: main.o link
client: server.o link
crypto: connection.o link

main.o:
	$(CC) $(CFLAGS) -c main.cpp -o build/main.o
client.o:
	$(CC) $(CFLAGS) -c server.cpp -o build/server.o
crypto.o:
	$(CC) $(CFLAGS) -c connection.cpp -o build/connection.o
link:
	$(CC) $(CFLAGS) build/* -o dist/safechat-server $(LDLIBS)

clean:
	rm -r build dist
install:
	sudo cp dist/safechat-server /usr/bin/
remove:
	sudo rm /usr/bin/safechat-server
