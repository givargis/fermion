#
# Copyright (c) 2025-2026 Tony Givargis
# University of California, Irvine
# Makefile
#

CC      = gcc
CFLAGS  = -std=c99 -pedantic -Wall -Wextra -Werror -Wfatal-errors -fPIC -O3
CFLAGS += -DNDEBUG
LDLIBS  = -lm
DEST    = fm2json

SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)

.PHONY: all libfm clean

all: libfm $(OBJS)
	@echo "[LN] $(DEST)"
	@$(CC) -o $(DEST) $(OBJS) $(LDLIBS) ../libfm/libfm.a

%.o: %.c
	@echo "[CC] $<"
	@$(CC) $(CFLAGS) -c $<
	@$(CC) $(CFLAGS) -MM $< > $*.d

libfm:
	@$(MAKE) -C ../libfm

clean:
	@rm -f $(DEST) *.o *.d *~
	@$(MAKE) -C ../libfm clean > /dev/null

-include $(OBJS:.o=.d)
