CC = gcc

ifdef CROSS_COMPILE
	CC = $(CROSS_COMPILE)gcc
endif

all:
	$(CC) writer.c -o writer

clean:
	rm writer
