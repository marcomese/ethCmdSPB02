#ifndef REGISTERS_H_
#define REGISTERS_H_

#include <stdint.h>
#include <sys/mman.h>

#define PAGE_SIZE         4096UL

// Registers base addresses
#define CTRL_REG_ADDR     0x43C00000
#define STATUS_REG_ADDR   0x43C10000
#define DMA_REG_ADDR      0x40400000

// Registers addresses
//  CTRL_REG registers
#define CMD_RECV_ADDR     0x43C00000

//  STATUS_REG registers
#define TRG_COUNTER_ADDR  0x43C10004
#define GTU_COUNTER_ADDR  0x43C10008
#define DATA_COUNTER_ADDR 0x43C1000C

uint32_t readReg(uint32_t* devAddr, uint32_t baseAddr, uint32_t regAddr);
void writeReg(uint32_t* devAddr, uint32_t baseAddr, uint32_t regAddr, uint32_t data);

typedef struct axiRegisters{
    uint32_t* ctrlReg;
    uint32_t* statusReg;
    uint32_t* dmaReg;
} axiRegisters_t;

#endif