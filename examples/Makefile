all: command hello
command: command.o ../picol.o
	$(CC) -o $@ ../picol.o $<
hello: hello.o ../picol.o
	$(CC) -o $@ ../picol.o $<
%.o: %.c
	$(CC) -I.. -c -o $@ $<
clean:
	rm command command.exe hello hello.exe *.o || true
.PHONY: clean
