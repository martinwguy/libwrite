SRCS=chp.c font.c init.c pap.c prop.c read.c save.c section.c text.c
OBJS=chp.o font.o init.o pap.o prop.o read.o save.o section.o text.o

# Where to install it under
PREFIX=/usr/local

CFLAGS=-O

libwrite.a: $(OBJS)
	-rm -f $@
	ar q $@ $(OBJS)
	ranlib $@

install: libwrite.a
	install -m 644 libwrite.a $(PREFIX)/lib/
	install -m 644 libwrite.h $(PREFIX)/include/
	mkdir -p $(PREFIX)/share/doc/libwrite
	install -m 644 libwrite.md example.c $(PREFIX)/share/doc/libwrite/

# A tiny test program creates a sample file "example.wri"
example: example.o libwrite.a
	cc -o example example.o libwrite.a

all: libwrite.a example

clean:
	rm -f *.o libwrite.a example example.wri
