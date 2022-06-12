CC = gcc
DEPS = commands.h registers.h dma.h
OBJ = main.o commands.o registers.o dma.o
LIBS = -lpthread

%.o: %.c $(DEPS)
	$(CC) -O0 -ggdb -c -o $@ $<

ethCmd: $(OBJ)
	$(CC) -o $@ $^ $(LIBS)

debug: $(OBJ)
	$(CC) -O0 -ggdb -o $@ $^ $(LIBS)

clean:
	rm ./*.o
