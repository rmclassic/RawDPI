CC = g++
CFLAGS = -Wall -g
DEPS = -lssl -lcrypto -lpthread

main: dns.o DOH.o Excptions.o RawDPI.o
	$(CC) $(DEPS) $(CFLAGS) -o RawDPI RawDPI.o dns.o DOH.o Excptions.o

RawDPI.o: RawDPI.cpp
	$(CC) $(DEPS) $(CFLAGS) -c RawDPI.cpp

dns.o: dns.cpp
	$(CC) $(DEPS) $(CFLAGS) -c dns.cpp

DOH.o: DOH.cpp
	$(CC) $(DEPS) $(CFLAGS) -c DOH.cpp

Excptions.o: Excptions.cpp
	$(CC) $(DEPS) $(CFLAGS) -c Excptions.cpp