CFLAGS += -std=c99 -Wall -Wextra -pedantic -Wold-style-declaration
CFLAGS += -Wmissing-prototypes -Wno-unused-parameter
CLIBS  += -lX11 -lXext -lXinerama -lm -lXft $(shell pkg-config --cflags --libs xft)
PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin
CC     ?= gcc

all: sowm

config.h:
	cp config.def.h config.h

sowm: sowm.c sowm.h config.h Makefile
	$(CC) -O3 $(CFLAGS) -o sowm sowm.c $(CLIBS) $(LDFLAGS) 

install: all
	install -Dm755 sowm $(DESTDIR)$(BINDIR)/sowm

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/sowm

clean:
	rm -f sowm *.o

.PHONY: all install uninstall clean
