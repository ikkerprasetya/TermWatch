CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99 -D_DEFAULT_SOURCE -MMD -MP -Isrc
TARGET = termwatch

SRCS = src/main.c src/system_info.c src/cpu_info.c src/mem_info.c src/disk_info.c \
       src/net_info.c src/proc_info.c src/history.c src/util.c src/term.c \
       src/canvas.c src/procview.c src/ui.c
OBJS = $(SRCS:.c=.o)
DEPS = $(SRCS:.c=.d)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(DEPS) $(TARGET)

-include $(DEPS)

.PHONY: all clean
