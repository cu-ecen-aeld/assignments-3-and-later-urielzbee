CC = gcc

ifdef CROSS_COMPILE
	CC = $(CROSS_COMPILE)gcc
endif

all:
	$(CC) aesdsocket.c -o aesdsocket

clean:
	rm aesdsocket
