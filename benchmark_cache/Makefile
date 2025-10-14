CC     := gcc
CFLAGS := -O1 -g -Werror -I.. -Wall -Wno-unused-variable -Wno-unused-parameter -Wextra

SRCS   := $(wildcard *.c)
BINS   := $(SRCS:.c=.bin)
DEPS   := $(SRCS:.c=.d)

.PHONY: all
all: $(BINS)

%.bin: %.c
	$(CC) $(CFLAGS) -o $@ $< -MMD -MP

-include $(DEPS)

.PHONY: clean
clean:
	rm -f $(BINS) $(DEPS)