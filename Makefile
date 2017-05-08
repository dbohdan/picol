CFLAGS = -Wall

picol: interp.c picol.h
	$(CC) interp.c -o $@ $(CFLAGS)

test: picol
	./picol test.pcl

examples: examples/command examples/hello examples/regexp-lib
examples/command: examples/command.c picol.h
	$(CC) -I. examples/command.c -o $@ $(CFLAGS)
examples/hello: examples/hello.c picol.h
	$(CC) -I. examples/hello.c -o $@ $(CFLAGS)
vendor/regexp.o: vendor/regexp.h vendor/regexp.c
	$(CC) -c vendor/regexp.c -o $@ $(CFLAGS)
examples/regexp-lib: examples/regexp-lib.c vendor/regexp.o picol.h
	$(CC) -I. vendor/regexp.o examples/regexp-lib.c -o $@ $(CFLAGS)

examples-test: examples
	./examples/command
	./examples/hello
	./examples/regexp-lib

clean:
	-rm picol picol.exe interp.obj
	-rm examples/command examples/command.exe command.obj
	-rm examples/hello examples/hello.exe hello.obj
	-rm examples/regexp-lib examples/regexp-lib.exe vendor/regexp.o regexp.obj regexp-lib.obj

.PHONY: clean examples examples-test test
