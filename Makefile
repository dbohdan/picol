picol: interp.o picol.o
	$(CC) picol.o interp.o -o picol
%.o: %.c
	$(CC) -c -o $@ $<
clean:
	rm picol picol.exe *.o || true
test: picol
	./picol test.pcl
.PHONY: clean test
