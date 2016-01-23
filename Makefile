picol: interp.o picol.o
	cc picol.o interp.o -o picol
%.o: %.c
	cc -c -o $@ $<
clean:
	rm picol *.o
.PHONY: clean
