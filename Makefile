CC=gcc
CFLAGS=-I -Wall -g
#DEPS =
OBJ = main.o

#%.o: %.c $(DEPS)
#	$(CC) -c -o $@ $< $(CFLAGS)

main: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f main *.o *~ core $(INCDIR)/*~