CFLAGS ?= -Wall

all: picolsh picolsh-linenoise

picolsh: shell.c picol.h
	$(CC) shell.c -o $@ $(CFLAGS) -DPICOL_SHELL_LINENOISE=0
picolsh-linenoise: shell.c picol.h vendor/linenoise.o
	$(CC) vendor/linenoise.o shell.c -o $@ $(CFLAGS)

test: picolsh
	./picolsh test.pcl

vendor/linenoise.o: vendor/linenoise.h vendor/linenoise.c
	$(CC) -c vendor/linenoise.c -o $@ $(CFLAGS)

examples: examples/command examples/hello examples/regexp-ext
examples/command: examples/command.c picol.h
	$(CC) -I. examples/command.c -o $@ $(CFLAGS)
examples/hello: examples/hello.c picol.h
	$(CC) -I. examples/hello.c -o $@ $(CFLAGS)
examples/regexp-ext: examples/regexp-ext.c picol.h extensions/regexp-wrapper.h
	$(CC) -I. examples/regexp-ext.c -o $@ $(CFLAGS)

examples-test: examples
	./examples/command
	./examples/hello
	./examples/regexp-ext

clean:
	-rm -f picolsh picol.exe shell.obj
	-rm -f examples/command examples/command.exe command.obj
	-rm -f examples/hello examples/hello.exe hello.obj
	-rm -f examples/regexp-ext examples/regexp-ext.exe regexp-ext.obj
	-rm -f vendor/linenoise.o

.PHONY: all clean examples examples-test test
