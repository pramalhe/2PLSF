# ==============================================================================
#
# Makefile.real
#
# ==============================================================================


# ==============================================================================
# Variables
# ==============================================================================

CC      := gcc-10
CFLAGS  := -g -Wall -Winline -O2
#CFLAGS  += -m32
#CFLAGS  += -DTL2_OPTIM_HASHLOG
#CFLAGS  += -DTL2_RESIZE_HASHLOG
LD      := gcc-10

LIBTL2 := libtl2.a

SRCS := \
	tl2.c \
	tmalloc.c \
#
OBJS := ${SRCS:.c=.o}

AR      := ar
RANLIB  := ranlib

RM := rm -f


# ==============================================================================
# Rules
# ==============================================================================

.PHONY: default
default: lazy

.PHONY: clean
clean:
	$(RM) $(LIBTL2) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(LIBTL2): $(OBJS)
	$(AR) cru $@ $^
	$(RANLIB) $@

.PHONY: lazy
lazy: $(LIBTL2)

.PHONY: eager
eager: CFLAGS += -DTL2_EAGER
eager: $(LIBTL2)

.PHONY: lazy-nocm
lazy-nocm: CFLAGS += -DTL2_NOCM
lazy-nocm: $(LIBTL2)

.PHONY: eager-nocm
eager-nocm: CFLAGS += -DTL2_EAGER
eager-nocm: CFLAGS += -DTL2_NOCM
eager-nocm: $(LIBTL2)

.PHONY: otm
otm: CFLAGS += -m32
otm: $(LIBTL2)


# ==============================================================================
# Dependencies
# ==============================================================================

%.o: %.h


# ==============================================================================
#
# End of Makefile.real
#
# ==============================================================================
