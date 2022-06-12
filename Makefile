CC = gcc
DEPS = commands.h registers.h dma.h
OBJ = main.o commands.o registers.o dma.o
LIBS = -lpthread

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $<

ethCmd: $(OBJ)
	$(CC) -O0 -o $@ $^ $(LIBS)

debug: $(OBJ)
	$(CC) -O0 -ggdb -o $@ $^ $(LIBS)

clean:
	rm ./*.o
