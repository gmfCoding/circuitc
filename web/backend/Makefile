# WebSocket Backend Server Makefile

CC = clang
CFLAGS = -Wall -Wextra -g -ggdb -fsanitize=thread -I../../src
LDFLAGS = -fsanitize=thread -lm -lpthread -lssl -lcrypto

SRC_DIR = ../../src
WEB_DIR = .

# Source files
WEB_SRCS = server.c websocket.c http_utils.c
SRC_SRCS = $(SRC_DIR)/elements.c \
           $(SRC_DIR)/simulation.c \
           $(SRC_DIR)/solver.c \
           $(SRC_DIR)/loader.c

# Object files
WEB_OBJS = server.o websocket.o http_utils.o
SRC_OBJS = elements.o simulation.o solver.o loader.o

ALL_OBJS = $(WEB_OBJS) $(SRC_OBJS)

# Target
TARGET = circuit_server

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(ALL_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# Web backend objects
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Source objects (compile from src directory)
elements.o: $(SRC_DIR)/elements.c
	$(CC) $(CFLAGS) -c $< -o $@

simulation.o: $(SRC_DIR)/simulation.c
	$(CC) $(CFLAGS) -c $< -o $@

solver.o: $(SRC_DIR)/solver.c
	$(CC) $(CFLAGS) -c $< -o $@

loader.o: $(SRC_DIR)/loader.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(ALL_OBJS) $(TARGET)

run: $(TARGET)
	./$(TARGET) 8080
