CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -g -Iinclude
LDFLAGS = -lpthread -lm

SRCS = src/main.c src/init.c src/log.c src/scheduler.c \
       src/pitch.c src/threads.c src/deadlock.c src/analysis.c

OBJS = $(SRCS:src/%.c=obj/%.o)
TARGET = cricket_sim

.PHONY: all clean run chart

all: obj $(TARGET)

obj:
	mkdir -p obj logs charts

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Build OK → ./$(TARGET)"

obj/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

run: all
	./$(TARGET)

chart:
	python3 charts/make_charts.py

clean:
	rm -rf obj $(TARGET) logs charts
