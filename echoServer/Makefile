CC = gcc
CFLAGS = -g -Wall
LIB = -lpthread

echoclient: echoclient.c csapp.o
	$(CC) $(CFLAGS) -o echoclient echoclient.c csapp.o $(LIB)

echoserveri: echoserveri.c echo.c csapp.o
	$(CC) $(CFLAGS) -o echoserveri echoserveri.c echo.c csapp.o $(LIB)

csapp.o: csapp.c csapp.h
	$(CC) $(CFLAGS) -c csapp.c

clean:
	rm -f *.o

