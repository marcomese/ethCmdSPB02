#include "dma.h"

unsigned int write_dma(unsigned int *virtual_addr, int offset, unsigned int value)
{
    virtual_addr[offset >> 2] = value;

    return 0;
}

unsigned int read_dma(unsigned int *virtual_addr, int offset)
{
    return virtual_addr[offset >> 2];
}

void dma_s2mm_status(unsigned int *virtual_addr)
{
    unsigned int status = read_dma(virtual_addr, S2MM_STATUS_REGISTER);

    printf("Stream to memory-mapped status (0x%08x@0x%02x):", status, S2MM_STATUS_REGISTER);

    if (status & STATUS_HALTED)
    {
        printf(" Halted.\n");
    }
    else
    {
        printf(" Running.\n");
    }

    if (status & STATUS_IDLE)
    {
        printf(" Idle.\n");
    }

    if (status & STATUS_SG_INCLDED)
    {
        printf(" SG is included.\n");
    }

    if (status & STATUS_DMA_INTERNAL_ERR)
    {
        printf(" DMA internal error.\n");
    }

    if (status & STATUS_DMA_SLAVE_ERR)
    {
        printf(" DMA slave error.\n");
    }

    if (status & STATUS_DMA_DECODE_ERR)
    {
        printf(" DMA decode error.\n");
    }

    if (status & STATUS_SG_INTERNAL_ERR)
    {
        printf(" SG internal error.\n");
    }

    if (status & STATUS_SG_SLAVE_ERR)
    {
        printf(" SG slave error.\n");
    }

    if (status & STATUS_SG_DECODE_ERR)
    {
        printf(" SG decode error.\n");
    }

    if (status & STATUS_IOC_IRQ)
    {
        printf(" IOC interrupt occurred.\n");
    }

    if (status & STATUS_DELAY_IRQ)
    {
        printf(" Interrupt on delay occurred.\n");
    }

    if (status & STATUS_ERR_IRQ)
    {
        printf(" Error interrupt occurred.\n");
    }
}

void dma_mm2s_status(unsigned int *virtual_addr)
{
    unsigned int status = read_dma(virtual_addr, MM2S_STATUS_REGISTER);

    printf("Memory-mapped to stream status (0x%08x@0x%02x):", status, MM2S_STATUS_REGISTER);

    if (status & STATUS_HALTED)
    {
        printf(" Halted.\n");
    }
    else
    {
        printf(" Running.\n");
    }

    if (status & STATUS_IDLE)
    {
        printf(" Idle.\n");
    }

    if (status & STATUS_SG_INCLDED)
    {
        printf(" SG is included.\n");
    }

    if (status & STATUS_DMA_INTERNAL_ERR)
    {
        printf(" DMA internal error.\n");
    }

    if (status & STATUS_DMA_SLAVE_ERR)
    {
        printf(" DMA slave error.\n");
    }

    if (status & STATUS_DMA_DECODE_ERR)
    {
        printf(" DMA decode error.\n");
    }

    if (status & STATUS_SG_INTERNAL_ERR)
    {
        printf(" SG internal error.\n");
    }

    if (status & STATUS_SG_SLAVE_ERR)
    {
        printf(" SG slave error.\n");
    }

    if (status & STATUS_SG_DECODE_ERR)
    {
        printf(" SG decode error.\n");
    }

    if (status & STATUS_IOC_IRQ)
    {
        printf(" IOC interrupt occurred.\n");
    }

    if (status & STATUS_DELAY_IRQ)
    {
        printf(" Interrupt on delay occurred.\n");
    }

    if (status & STATUS_ERR_IRQ)
    {
        printf(" Error interrupt occurred.\n");
    }
}

int dma_mm2s_sync(unsigned int *virtual_addr)
{
    unsigned int mm2s_status = read_dma(virtual_addr, MM2S_STATUS_REGISTER);

    // sit in this while loop as long as the status does not read back 0x00001002 (4098)
    // 0x00001002 = IOC interrupt has occured and DMA is idle
    while (!(mm2s_status & IOC_IRQ_FLAG) || !(mm2s_status & IDLE_FLAG))
    {
        dma_s2mm_status(virtual_addr);
        dma_mm2s_status(virtual_addr);

        mm2s_status = read_dma(virtual_addr, MM2S_STATUS_REGISTER);
    }

    return 0;
}

int dma_s2mm_sync(unsigned int *virtual_addr)
{
    unsigned int s2mm_status = read_dma(virtual_addr, S2MM_STATUS_REGISTER);

    // sit in this while loop as long as the status does not read back 0x00001002 (4098)
    // 0x00001002 = IOC interrupt has occured and DMA is idle
    while (!(s2mm_status & IOC_IRQ_FLAG) || !(s2mm_status & IDLE_FLAG))
    {
        dma_s2mm_status(virtual_addr);
        dma_mm2s_status(virtual_addr);

        s2mm_status = read_dma(virtual_addr, S2MM_STATUS_REGISTER);
    }

    return 0;
}

void dma_init_s2mm(unsigned int *virtual_addr){
    printf("Reset the DMA.\n");
    write_dma(virtual_addr, S2MM_CONTROL_REGISTER, RESET_DMA);
    dma_s2mm_status(virtual_addr);

    printf("Halt the DMA.\n");
    write_dma(virtual_addr, S2MM_CONTROL_REGISTER, HALT_DMA);
    dma_s2mm_status(virtual_addr);

    printf("Enable all interrupts.\n");
    write_dma(virtual_addr, S2MM_CONTROL_REGISTER, ENABLE_ALL_IRQ);
    dma_s2mm_status(virtual_addr);

    return;
}

void dma_set_buffer(unsigned int *virtual_addr, unsigned int dest_addr){
    printf("Writing the destination address for the data from S2MM in DDR...\n");
    write_dma(virtual_addr, S2MM_DST_ADDRESS_REGISTER, dest_addr);
    dma_s2mm_status(virtual_addr);

    return;
}

void dma_transfer_s2mm(unsigned int *virtual_addr, unsigned int bytes_num)
{
    printf("Run the S2MM channel.\n");
    write_dma(virtual_addr, S2MM_CONTROL_REGISTER, RUN_DMA);
    dma_s2mm_status(virtual_addr);

    printf("Writing S2MM transfer length of 32 bytes...\n");
    write_dma(virtual_addr, S2MM_BUFF_LENGTH_REGISTER, bytes_num);
    dma_s2mm_status(virtual_addr);

    printf("Waiting for S2MM sychronization...\n");
    dma_s2mm_sync(virtual_addr);

    dma_s2mm_status(virtual_addr);

    return;
}

/*
int main()
{
    printf("FIFO DMA TEST v2.0\n");

    printf("Opening /dev/mem...\n");
    int ddr_memory = open("/dev/mem", O_RDWR | O_SYNC);

    printf("Memory map the address of the DMA AXI IP via its AXI lite control interface register block.\n");
    unsigned int *dma_virtual_addr = mmap(NULL, 65535, PROT_READ | PROT_WRITE, MAP_SHARED, ddr_memory, 0x40400000);

    printf("Memory map the S2MM destination address register block.\n");
    unsigned int *virtual_dst_addr = mmap(NULL, 65535, PROT_READ | PROT_WRITE, MAP_SHARED, ddr_memory, 0x0e000000);

    printf("Clearing the destination register block...\n");
    memset(virtual_dst_addr, 0, 32);

    printf("Destination memory block data: ");
    print_mem(virtual_dst_addr, 32);

    printf("Reset the DMA.\n");
    write_dma(dma_virtual_addr, S2MM_CONTROL_REGISTER, RESET_DMA);
    write_dma(dma_virtual_addr, MM2S_CONTROL_REGISTER, RESET_DMA);
    dma_s2mm_status(dma_virtual_addr);
    dma_mm2s_status(dma_virtual_addr);

    printf("Halt the DMA.\n");
    write_dma(dma_virtual_addr, S2MM_CONTROL_REGISTER, HALT_DMA);
    write_dma(dma_virtual_addr, MM2S_CONTROL_REGISTER, HALT_DMA);
    dma_s2mm_status(dma_virtual_addr);
    dma_mm2s_status(dma_virtual_addr);

    printf("Enable all interrupts.\n");
    write_dma(dma_virtual_addr, S2MM_CONTROL_REGISTER, ENABLE_ALL_IRQ);
    write_dma(dma_virtual_addr, MM2S_CONTROL_REGISTER, ENABLE_ALL_IRQ);
    dma_s2mm_status(dma_virtual_addr);
    dma_mm2s_status(dma_virtual_addr);

    printf("Writing the destination address for the data from S2MM in DDR...\n");
    write_dma(dma_virtual_addr, S2MM_DST_ADDRESS_REGISTER, 0x0e000000);
    dma_s2mm_status(dma_virtual_addr);

    printf("Run the S2MM channel.\n");
    write_dma(dma_virtual_addr, S2MM_CONTROL_REGISTER, RUN_DMA);
    dma_s2mm_status(dma_virtual_addr);

    printf("Writing S2MM transfer length of 32 bytes...\n");
    write_dma(dma_virtual_addr, S2MM_BUFF_LENGTH_REGISTER, 128);
    dma_s2mm_status(dma_virtual_addr);

    printf("Waiting for S2MM sychronization...\n");
    dma_s2mm_sync(dma_virtual_addr);

    dma_s2mm_status(dma_virtual_addr);
    dma_mm2s_status(dma_virtual_addr);

    printf("Destination memory block: ");
    print_mem(virtual_dst_addr, 128);

    printf("\n");

    return 0;
}
*/