#include "dma.h"

unsigned int isExit(exitConditions_t* extC){
    unsigned int tmpExtCnd = 0;

    for(int i = 0; i < extC->conditionsNum; i++)
        tmpExtCnd += (*extC->variables[i] == extC->values[i]);

    return (tmpExtCnd == extC->conditionsNum);
}

unsigned int write_dma(unsigned int *virtual_addr, int offset, unsigned int value)
{
    virtual_addr[offset >> 2] = value;

    return 0;
}

unsigned int read_dma(unsigned int *virtual_addr, int offset)
{
    return virtual_addr[offset >> 2];
}

int dma_s2mm_sync(unsigned int *virtual_addr, exitConditions_t* exitCond)
{
    unsigned int s2mm_status = read_dma(virtual_addr, S2MM_STATUS_REGISTER);
    unsigned int exitCondition = 0;

    // sit in this while loop as long as the status does not read back 0x00001002 (4098)
    // 0x00001002 = IOC interrupt has occured and DMA is idle
    while ((!(s2mm_status & IOC_IRQ_FLAG) || !(s2mm_status & IDLE_FLAG)) && (!exitCondition)){
        s2mm_status = read_dma(virtual_addr, S2MM_STATUS_REGISTER);

        pthread_mutex_lock(exitCond->mtx);
        exitCondition = isExit(exitCond);
        pthread_mutex_unlock(exitCond->mtx);
    }

    return 0;
}

void dma_init_s2mm(unsigned int *virtual_addr){
    write_dma(virtual_addr, S2MM_CONTROL_REGISTER, RESET_DMA);
    write_dma(virtual_addr, S2MM_CONTROL_REGISTER, HALT_DMA);
    write_dma(virtual_addr, S2MM_CONTROL_REGISTER, ENABLE_ALL_IRQ);

    return;
}

void dma_set_buffer(unsigned int *virtual_addr, unsigned int dest_addr){
    write_dma(virtual_addr, S2MM_DST_ADDRESS_REGISTER, dest_addr);

    return;
}

void dma_transfer_s2mm(unsigned int *virtual_addr, unsigned int bytes_num, exitConditions_t* exitCond)
{
    write_dma(virtual_addr, S2MM_CONTROL_REGISTER, RUN_DMA);
    write_dma(virtual_addr, S2MM_BUFF_LENGTH_REGISTER, bytes_num);

    dma_s2mm_sync(virtual_addr,exitCond);

    return;
}
