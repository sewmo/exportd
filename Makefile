CC = gcc
CFLAGS = -g -Wall -Wextra -pedantic -Iinclude -MMD -MP

SRC = $(wildcard src/*.c)
OBJ = $(patsubst src/%.c, build/%.o, $(SRC))

TARGET = exportd

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) 

build/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

-include $(OBJ:.o=.d)

clean:
	rm -rf build $(TARGET)
	mkdir build
