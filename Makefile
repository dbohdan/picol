picol: interp.o picol.o
	$(CC) picol.o interp.o -o picol
interp.o: interp.c picol.h
	$(CC) -c -o $@ $<
picol.o: picol.c picol.h
	$(CC) -c -o $@ $<

test: picol
	./picol test.pcl

examples: examples/command examples/hello
examples/command: examples/command.o picol.o
	$(CC) -o examples/command picol.o examples/command.o
examples/hello: examples/hello.o picol.o
	$(CC) -o examples/hello picol.o examples/hello.o
examples/command.o: examples/command.c picol.h
	$(CC) -c -I. -o $@ $<
examples/hello.o: examples/hello.c picol.h
	$(CC) -c -I. -o $@ $<
	
clean:
	rm picol picol.exe *.o || true
	rm examples/command examples/command.exe examples/command.o || true
	rm examples/hello examples/hello.exe examples/hello.o || true

.PHONY: clean examples test
