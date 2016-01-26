picol: interp.o picol.o
	$(CC) picol.o interp.o -o picol
interp.o: interp.c
	$(CC) -c -o interp.o interp.c
picol.o: picol.c
	$(CC) -c -o picol.o picol.c
clean:
	rm picol picol.exe *.o || true
test: picol
	./picol test.pcl
.PHONY: clean test
