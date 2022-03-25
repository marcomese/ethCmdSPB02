#ifndef DMA_H_
#define DMA_H_

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <sys/mman.h>
#include <pthread.h>
#include <stdint.h>

#define MM2S_CONTROL_REGISTER 0x00
#define MM2S_STATUS_REGISTER 0x04
#define MM2S_SRC_ADDRESS_REGISTER 0x18
#define MM2S_TRNSFR_LENGTH_REGISTER 0x28

#define S2MM_CONTROL_REGISTER 0x30
#define S2MM_STATUS_REGISTER 0x34
#define S2MM_DST_ADDRESS_REGISTER 0x48
#define S2MM_BUFF_LENGTH_REGISTER 0x58

#define IOC_IRQ_FLAG 1 << 12
#define IDLE_FLAG 1 << 1

#define STATUS_HALTED 0x00000001
#define STATUS_IDLE 0x00000002
#define STATUS_SG_INCLDED 0x00000008
#define STATUS_DMA_INTERNAL_ERR 0x00000010
#define STATUS_DMA_SLAVE_ERR 0x00000020
#define STATUS_DMA_DECODE_ERR 0x00000040
#define STATUS_SG_INTERNAL_ERR 0x00000100
#define STATUS_SG_SLAVE_ERR 0x00000200
#define STATUS_SG_DECODE_ERR 0x00000400
#define STATUS_IOC_IRQ 0x00001000
#define STATUS_DELAY_IRQ 0x00002000
#define STATUS_ERR_IRQ 0x00004000

#define HALT_DMA 0x00000000
#define RUN_DMA 0x00000001
#define RESET_DMA 0x00000004
#define ENABLE_IOC_IRQ 0x00001000
#define ENABLE_DELAY_IRQ 0x00002000
#define ENABLE_ERR_IRQ 0x00004000
#define ENABLE_ALL_IRQ 0x00007000

#define MAX_EXIT_CONDITIONS 5
#define OR 0
#define AND 1

typedef struct exitConditions{
    uint8_t conditionsNum;
    pthread_mutex_t* mtx;
    uint8_t operation;
    uint32_t* variables[MAX_EXIT_CONDITIONS];
    uint32_t values[MAX_EXIT_CONDITIONS];
} exitConditions_t;

unsigned int write_dma(unsigned int *virtual_addr, int offset, unsigned int value);
unsigned int read_dma(unsigned int *virtual_addr, int offset);
int dma_s2mm_sync(unsigned int *virtual_addr, exitConditions_t* extC);
void dma_init_s2mm(unsigned int *virtual_addr);
void dma_set_buffer(unsigned int *virtual_addr, unsigned int dest_addr);
void dma_transfer_s2mm(unsigned int *virtual_addr, unsigned int bytes_num, exitConditions_t* extC);
unsigned int isExit(exitConditions_t* extC);

#endif