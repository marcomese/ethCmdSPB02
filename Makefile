CC = gcc
DEPS = commands.h registers.h dma.h crc32.h madgwickFilter.h
OBJ = main.o commands.o registers.o dma.o crc32.o madgwickFilter.o
LIBS = -lpthread -lm
DBG = 0

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
