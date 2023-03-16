CC = gcc
DEPS = commands.h registers.h dma.h crc32.h imu_algebra.h imu_constants.h imu_math.h imu_types.h imu_utils.h imu.h
OBJ = main.o commands.o registers.o dma.o crc32.o imu_algebra.o imu_math.o imu_utils.o imu.o
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
