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

# Log files should be created by the binary.

install:
	install -d /var/log/exportd /srv/exportd/www
	install -m 755 exportd /usr/local/bin
	install -m 644 /etc/exportd.conf
	install -m 644 /srv/exportd/www/index.html 
	install -m 644 /srv/exportd/www/metrics
	sudo chown exportd:exportd /var/log/exportd    # Let exportd user write to files inside here.
	sudo chown exportd:exportd /srv/exportd

uninstall:
	rm -f /usr/local/bin/exportd
	rm -rf /var/log/exportd /srv/exportd

