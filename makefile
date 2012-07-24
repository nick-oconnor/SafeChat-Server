build:
	g++ -c -O3 -Wall -s -Iheaders -MMD -MP -MF main.o.d -o main.o main.cpp
	g++ -o safechat-server -Wl,-S main.o -lpthread
	rm main.o*
install:
	cp safechat-server /usr/bin/
