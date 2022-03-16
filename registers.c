#include <stdint.h>
#include "registers.h"

static uint32_t getOffset(uint32_t baseAddr, uint32_t regAddr){
    return ((regAddr - baseAddr) >> 2);
}

uint32_t readReg(uint32_t* devAddr, uint32_t baseAddr, uint32_t regAddr){
    uint32_t offset = getOffset(baseAddr, regAddr);
    return *(devAddr + offset);
}

void writeReg(uint32_t* devAddr, uint32_t baseAddr, uint32_t regAddr, uint32_t data){
    uint32_t offset = getOffset(baseAddr, regAddr);
    *(devAddr + offset) = data;
}
