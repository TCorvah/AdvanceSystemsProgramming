CC = gcc
CFLAGS = -g -Wall -pthread
LDFLAGS = -lpthread

SRC = greptile.c  error.c myQueue.c
OBJ = $(SRC:.c=.o)
HDR = greptile.h apue.h myQueue.h
TARGET = greptile

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c $(HDR)
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJ) $(TARGET)