.PHONY: all clean rebuild

CC=gcc
CFLAGS=-O3 -march=native 

LIBTCC_SEARCH := /usr/include /usr/local/include /opt/homebrew/include
LIBTCC_H := $(firstword $(wildcard $(addsuffix /libtcc.h, $(LIBTCC_SEARCH))))

ifeq ($(LIBTCC),)
    ifeq ($(LIBTCC_H),)
        $(error libtcc not found. Install with: apt install libtcc-dev / brew install tcc / source: https://github.com/TinyCC/tinycc)
    endif
    LIBTCC          := -ltcc
    LIBTCC_CFLAGS   := -I$(dir $(LIBTCC_H))
endif

all: mkb-jit mkb-print mkb-interpret

mkb_parser.o: mkb_parser.c
	$(CC) $(CFLAGS) -c mkb_parser.c -o mkb_parser.o

mkb-print: mkb_print.c mkb_parser.o
	$(CC) $(CFLAGS) mkb_print.c mkb_parser.o -o mkb-print

mkb-jit: mkb_jit.c mkb_parser.o
	$(CC) $(CFLAGS) $(LIBTCC_CFLAGS) mkb_jit.c mkb_parser.o -o mkb-jit $(LIBTCC)

mkb-interpret: mkb_interpreter.c mkb_parser.o
	$(CC) $(CFLAGS) mkb_interpreter.c mkb_parser.o -o mkb-interpret

clean:
	$(RM) mkb_parser.o
	$(RM) mkb-jit
	$(RM) mkb-print
	$(RM) mkb-interpret

rebuild: clean all
