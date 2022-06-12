CC = gcc
DEPS = commands.h registers.h dma.h
OBJ = main.o commands.o registers.o dma.o
LIBS = -lpthread

ifeq ($(DBG), 1)
	%.o: %.c $(DEPS)
		$(CC) -O0 -ggdb -c -o $@ $<

	ethCmd: $(OBJ)
		$(CC) -O0 -ggdb -o $@ $^ $(LIBS)
else
	%.o: %.c $(DEPS)
		$(CC) -c -o $@ $<

	ethCmd: $(OBJ)
		$(CC) -o $@ $^ $(LIBS)
endif

clean:
	rm ./*.o
