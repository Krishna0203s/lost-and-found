CC ?= gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude -pthread

ifeq ($(OS),Windows_NT)
    LDFLAGS = -lsqlite3 -pthread -lws2_32 -lm
    TARGET = lostfound_server.exe
    TEST_TARGET = test_matching.exe
    RUN_CMD = $(TARGET) -p 8080
    TEST_CMD = $(TEST_TARGET)
    RM_CMD = del /f /q src\*.o tests\*.o $(TARGET) $(TEST_TARGET) lostfound.db 2>nul || rm -f src/*.o tests/*.o $(TARGET) $(TEST_TARGET) lostfound.db
else
    LDFLAGS = -lsqlite3 -pthread -lm
    TARGET = lostfound_server
    TEST_TARGET = test_matching
    RUN_CMD = ./$(TARGET) -p 8080
    TEST_CMD = ./$(TEST_TARGET)
    RM_CMD = rm -f src/*.o tests/*.o $(TARGET) $(TEST_TARGET) lostfound.db
endif

SRCS = src/main.c src/db.c src/matching.c src/validation.c src/handlers.c src/http.c src/cJSON.c
OBJS = $(SRCS:.c=.o)

TEST_SRCS = tests/test_matching.c src/matching.c src/validation.c
TEST_OBJS = $(TEST_SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@
	@echo "Build complete: ./$(TARGET)"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TEST_TARGET)
	@echo "Running matching engine unit tests..."
	$(TEST_CMD)

$(TEST_TARGET): $(TEST_OBJS)
	$(CC) $(TEST_OBJS) $(LDFLAGS) -o $@

clean:
	-$(RM_CMD)

run: $(TARGET)
	$(RUN_CMD)

.PHONY: all clean test run

