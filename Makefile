<<<<<<< HEAD
CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99 -D_DEFAULT_SOURCE -MMD -MP -Isrc
TARGET = termwatch

SRCS = src/main.c src/system_info.c src/cpu_info.c src/mem_info.c src/term.c src/ui.c
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
=======
CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99 -Isrc
TARGET = sysinfo_tool

SRCS = src/main.c src/system_info.c src/cpu_info.c src/mem_info.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
>>>>>>> 6ec40683499a86f91f1e5cd6917dca80c8d1fee0
