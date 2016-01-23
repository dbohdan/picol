picol: interp.o picol.o
	gcc picol.o interp.o -o picol
%.o: %.c
	gcc -c -o $@ $<
clean:
	rm picol *.o
.PHONY: clean
