CFLAGS+=-std=c99 -g -Wall
LDLIBS=-I/usr/include/taglib -ltag_c
PREFIX=/usr/local

BIN=albumdetails

all: $(BIN)

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(BIN) $(DESTDIR)$(PREFIX)/bin/

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(BIN)

clean:
	rm -f $(OBJ) $(BIN)
