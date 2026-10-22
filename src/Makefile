#
# Copyright (c) 2025-2026 Tony Givargis
# University of California, Irvine
# Makefile
#

CC      = gcc
CFLAGS  = -ansi -pedantic -Wall -Wextra -Werror -Wfatal-errors -O3
LDFLAGS =
LDLIBS  =
DEST    = fm

SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)

.PHONY: all clean

all: $(DEST)

$(DEST): $(OBJS)
	@echo "[LN] $<"
	@$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

%.o: %.c
	@echo "[CC] $<"
	@$(CC) $(CFLAGS) -MMD -MP -c $<

clean:
	rm -f $(DEST) *.so *.a *.o *.d *~

-include $(OBJS:.o=.d)
