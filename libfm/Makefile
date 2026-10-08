#
# Copyright (c) 2025-2026 Tony Givargis
# University of California, Irvine
# Makefile
#

CC      = gcc
CFLAGS  = -std=c99 -pedantic -Wall -Wextra -Werror -Wfatal-errors -fPIC -O3
CFLAGS += -DNDEBUG
DEST    = libfm

ifeq ($(shell uname -s),Darwin)
	SOEXT   = dylib
	LDFLAGS = -lm -dynamiclib
else
	SOEXT   = so
	LDFLAGS = -lm -shared
endif

SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)

.PHONY: all clean

all: $(DEST).a $(DEST).$(SOEXT)

$(DEST).a: $(OBJS)
	@echo "[SL] $(DEST).a"
	@$(AR) rcs $@ $(OBJS)

$(DEST).$(SOEXT): $(OBJS)
	@echo "[DL] $(DEST).$(SOEXT)"
	@$(CC) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	@echo "[CC] $<"
	@$(CC) $(CFLAGS) -c $<
	@$(CC) $(CFLAGS) -MM $< > $*.d

clean:
	@rm -f *.dylib *.so *.a *.o *.d *~

-include $(OBJS:.o=.d)
