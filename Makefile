NAME = lua-mcpack
VERSION = 0.1
DIST := $(NAME)-$(VERSION)

CC = gcc
RM = rm -rf

CFLAGS = -Wall -g -fPIC -I/home/microwish/libmcpack/include -I/home/microwish/lua/include
#CFLAGS = -Wall -g -O2 -fPIC -I/home/microwish/libmcpack/include -I/home/microwish/lua/include
LFLAGS = -shared -lstdc++ -lm -L/lib64 -Wl,-Bstatic -L/home/microwish/lua/lib -llua -L/home/microwish/lib/libmcpack/lib -lmcpack -Wl,-Bdynamic
INSTALL_PATH = /home/microwish/lua-mcpack/lib

all: mcpack.so

mcpack.so: mcpack.o
  $(CC) -o $@ $< $(LFLAGS)

mcpack.o: lmcpacklib.c
	$(CC) -o $@ $(CFLAGS) -c $<

install: mcpack.so
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
