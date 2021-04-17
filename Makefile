CFLAGS ?= -Wall -Wextra
PREFIX ?= /usr/local

all: picolsh picolsh-big-stack picolsh-linenoise

picolsh: shell.c picol.h
	$(CC) shell.c -o $@ $(CFLAGS) -DPICOL_SHELL_LINENOISE=0
picolsh-big-stack: shell.c picol.h
	$(CC) shell.c -o $@ $(CFLAGS) -DPICOL_SHELL_LINENOISE=0 -DPICOL_SMALL_STACK=0
picolsh-linenoise: shell.c picol.h vendor/linenoise.o
	$(CC) vendor/linenoise.o shell.c -o $@ $(CFLAGS)

test: picolsh picolsh-big-stack
	./picolsh test.pcl
	./picolsh-big-stack test.pcl

vendor/linenoise.o: vendor/linenoise.h vendor/linenoise.c
	$(CC) -c vendor/linenoise.c -o $@ $(CFLAGS)

examples: examples/command examples/hello examples/privdata examples/regexp-ext
examples/command: examples/command.c picol.h
	$(CC) -I. examples/command.c -o $@ $(CFLAGS)
examples/hello: examples/hello.c picol.h
	$(CC) -I. examples/hello.c -o $@ $(CFLAGS)
examples/privdata: examples/privdata.c picol.h
	$(CC) -I. examples/privdata.c -o $@ $(CFLAGS)
examples/regexp-ext: examples/regexp-ext.c picol.h extensions/regexp-wrapper.h
	$(CC) -I. examples/regexp-ext.c -o $@ $(CFLAGS)

examples-test: examples
	./examples/command
	./examples/hello
	./examples/privdata
	./examples/regexp-ext

clean:
	-rm -f picolsh picolsh-big-stack picolsh-linenoise picolsh.exe shell.obj
	-rm -f examples/command examples/command.exe command.obj
	-rm -f examples/hello examples/hello.exe hello.obj
	-rm -f examples/privdata examples/privdata.exe privdata.obj
	-rm -f examples/regexp-ext examples/regexp-ext.exe regexp-ext.obj
	-rm -f vendor/linenoise.o

install: install-bin install-include
install-bin: picolsh-linenoise
	install $< $(DESTDIR)$(PREFIX)/bin/picolsh
install-include: picol.h
	install -m 0644 $< $(DESTDIR)$(PREFIX)/include

upx: all
	upx -9 picolsh picolsh-big-stack picolsh-linenoise

.PHONY: all clean examples examples-test install install-bin install-include test upx
