all: command hello
command: command.o ../picol.o
	gcc -o $@ ../picol.o $<
hello: hello.o ../picol.o
	gcc -o $@ ../picol.o $<
%.o: %.c
	gcc -I.. -c -o $@ $<
clean:
	rm hello *.o
.PHONY: clean
