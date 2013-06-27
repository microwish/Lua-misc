NAME = lua-misc
VERSION = 0.1
DIST := $(NAME)-$(VERSION)

CC = gcc
RM = rm -rf

CFLAGS = -Wall -g -fPIC -I/home/microwish/lua/include
#CFLAGS = -Wall -g -O2 -fPIC -I/home/microwish/lua/include
LFLAGS = -shared -Wl,-Bstatic -L/home/microwish/lua/lib -llua -Wl,-Bdynamic
INSTALL_PATH = /home/microwish/lua-misc/lib

all: misc.so

misc.so: misc.o
	$(CC) -o $@ $< $(LFLAGS)

misc.o: lmisclib.c
	$(CC) -o $@ $(CFLAGS) -c $<

install: misc.so
	install -D -s $< $(INSTALL_PATH)/$<

clean:
	$(RM) *.so *.o

dist:
	if [ -d $(DIST) ]; then $(RM) $(DIST); fi
	mkdir -p $(DIST)
	cp *.c Makefile $(DIST)/
	tar czvf $(DIST).tar.gz $(DIST)
	$(RM) $(DIST)

.PHONY: all clean dist
