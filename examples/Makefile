all: command hello
command: command.o ../picol.o
	cc -o $@ ../picol.o $<
hello: hello.o ../picol.o
	cc -o $@ ../picol.o $<
%.o: %.c
	cc -I.. -c -o $@ $<
clean:
	rm hello *.o
.PHONY: clean
