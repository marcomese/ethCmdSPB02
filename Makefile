CC = gcc
DEPS = commands.h registers.h dma.h
OBJ = main.o commands.o registers.o dma.o
LIBS = -lpthread
DBG = main.c commands.c registers.c dma.c

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $<

ethCmd: $(OBJ)
	$(CC) -o $@ $^ $(LIBS)

debug: $(DBG)
	$(CC) -O0 -ggdb -o $@ $^ $(LIBS)

clean:
	rm ./*.o
