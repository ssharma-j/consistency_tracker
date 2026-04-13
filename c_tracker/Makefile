# Makefile for Habit Tracker

CC      = gcc
CFLAGS  = -Wall -Wextra -std=c99 -g -O2
TARGET  = habit_tracker
SRCS    = habit_tracker.c
OBJS    = $(SRCS:.c=.o)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c habit_tracker.h
	$(CC) $(CFLAGS) -c -o $@ $<

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET) habit_data.txt habits_export.csv habits_export.pdf
