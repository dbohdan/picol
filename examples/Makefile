all: hello
hello: hello.o ../picol.o
	gcc -o hello ../picol.o hello.o 
%.o: %.c
	gcc -I.. -c -o $@ $<
clean:
	rm hello *.o
.PHONY: clean
