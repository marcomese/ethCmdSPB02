CC = gcc
DEPS = commands.h registers.h dma.h
OBJ = main.o commands.o registers.o dma.o
LIBS = -lpthread
DBG = main.c commands.c registers.c dma.c

%.o: %.c $(DEPS)
	ifeq ($(DBG),1)
		$(CC) -O0 -ggdb -c -o $@ $<
	else
		$(CC) -c -o $@ $<
	endif

ethCmd: $(OBJ)
	ifeq ($(DBG),1)
		$(CC) -O0 -ggdb -o $@ $^ $(LIBS)
	else
		$(CC) -o $@ $^ $(LIBS)
	endif

clean:
	rm ./*.o
