# Variables

CC=gcc
STD=c99
CFLAGS= -std=$(STD) -g -O2 -static -msse -mfpmath=sse #-c 
LDFLAGS= -lm
SRC= $(wildcard src/*.c)
HDR= $(wildcard src/*.h)

# Rules

all: $(SRC)
	$(CC) src/main.c -o main.exe $(LDFLAGS)