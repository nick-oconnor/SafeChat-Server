CC = g++
CFLAGS = -Wall -O3
LDLIBS = -lpthread

build:
	$(CC) $(CFLAGS) *.cpp -o safechat-server $(LDLIBS)
install:
	sudo cp safechat-server /usr/bin/
remove:
	sudo rm /usr/bin/safechat-server
