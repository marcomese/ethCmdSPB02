#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <time.h>
#include "commands.h"
#include "registers.h"
#include "dma.h"

#define PAGE_SIZE      4096UL

#define CONN_PORT      5000
#define CONN_MAX_QUEUE 10

#define DATA_ADDR      0x0E000000
#define FIFO_DATA_LEN  4

pthread_mutex_t mtx; // portare dentro cmdDecodeArgs_t e chkFifoArg_t e dichiararla in main

typedef struct cmdDecodeArgs{
    axiRegisters_t* regs;
    uint32_t* cmdID;
    int connfd;
    int* socketStatus;
} cmdDecodeArgs_t;

typedef struct chkFifoArgs{
    axiRegisters_t* regs;
    uint32_t* cmdID;
    int* socketStatus;
    uint32_t* fifoData;
} chkFifoArgs_t;


//    ridefinirla passando per riferimento l'output senza usare static

const char* genFileName(uint32_t eventCounter){
    time_t rawtime = time(NULL);
    struct tm *ptm = localtime(&rawtime);
    static char fileName[40] = "";

    snprintf(fileName, 40, "/srv/ftp/clkb_event_%04d%02d%02d%02d%02d%02d-%04d.dat",
             ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday,
             ptm->tm_hour, ptm->tm_min, ptm->tm_sec,
             eventCounter);

    return fileName;
}

void* cmdDecodeThread(void *arg){
    cmdDecodeArgs_t* cmdArg = (cmdDecodeArgs_t*)arg;
    const char *welcomeStr = "CLK BOARD\n";
    char ethStr[CMD_MAX_LEN] = "";
    int localSocketStatus;

    write(cmdArg->connfd, welcomeStr, strlen(welcomeStr));

    while (*cmdArg->cmdID != EXIT){
        localSocketStatus = read(cmdArg->connfd, ethStr, CMD_MAX_LEN);

        pthread_mutex_lock(&mtx);
        *cmdArg->socketStatus = localSocketStatus;
        pthread_mutex_unlock(&mtx);

        if(localSocketStatus > 0)
            *cmdArg->cmdID = decodeCmdStr(cmdArg->regs, cmdArg->connfd, ethStr);
        else
            pthread_exit(NULL);
    }

    pthread_exit((void *)cmdArg->cmdID);
}

void *checkFifoThread(void *arg){
    chkFifoArgs_t* chkArg = (chkFifoArgs_t*)arg;
    uint16_t fifoDataCounter = 0;
    uint32_t* fifoData;
    uint32_t eventCounter = 0;
    int localSocketStatus;

    while (*chkArg->cmdID != EXIT){
        //fifoDataCounter = fifoGetCounter(chkArg->regs);
        fifoDataCounter = readReg(chkArg->regs->statusReg, STATUS_REG_ADDR, DATA_COUNTER_ADDR);

        pthread_mutex_lock(&mtx);
        localSocketStatus = *chkArg->socketStatus;
        pthread_mutex_unlock(&mtx);

        if(localSocketStatus <= 0)
            pthread_exit(NULL);

        if(fifoDataCounter > 0){
            //fifoData = fifoGetData(chkArg->regs);
            dma_transfer_s2mm(chkArg->regs->dmaReg, 128);

            FILE *outFile = fopen(genFileName(++eventCounter), "a");

            for(int i = 0; i < FIFO_DATA_LEN; i++){
                fprintf(outFile, "%d", *(fifoData+i));
                if(i != FIFO_DATA_LEN-1)
                    fprintf(outFile, ",");
            }
            fprintf(outFile,"\n");

            fclose(outFile);
        }
    }

    pthread_exit((void *)&fifoData);
}

int main(int argc, char *argv[]){
    pthread_t cmdDecID;
    pthread_t chkSttID;
    int threadErr;
    int listenfd = 0;
    int connfd = 0;
    struct sockaddr_in serv_addr;
    cmdDecodeArgs_t cmdDecodeArg;
    chkFifoArgs_t chkFifoArg;
    uint32_t cmdDecRetVal;
    uint32_t chkSttRetVal;
    uint32_t cmdID = NONE;
    int socketStatus = 1;
    axiRegisters_t axiRegs;
    uint32_t* fifoData;

    int devmem = open("/dev/mem", O_RDWR | O_SYNC);
    if (devmem < 0)
        printf("Error in opening /dev/mem\n");

    axiRegs.ctrlReg = (uint32_t*)mmap(0, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, devmem, CTRL_REG_ADDR);
    axiRegs.statusReg = (uint32_t*)mmap(0, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, devmem, STATUS_REG_ADDR);
    axiRegs.dmaReg = (uint32_t*)mmap(0, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, devmem, DMA_REG_ADDR);

    fifoData = (uint32_t*)mmap(0, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, devmem, DATA_ADDR);

    printf("Initializing DMA...\n");
    dma_init_s2mm(axiRegs.dmaReg);
    dma_set_buffer(axiRegs.dmaReg, DATA_ADDR);
    printf("DMA Initialized!\n");

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(CONN_PORT);

    bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

    listen(listenfd, CONN_MAX_QUEUE);

    cmdDecodeArg.regs = &axiRegs;
    cmdDecodeArg.cmdID = &cmdID;
    cmdDecodeArg.socketStatus = &socketStatus;

    chkFifoArg.regs = &axiRegs;
    chkFifoArg.cmdID = &cmdID;
    chkFifoArg.socketStatus = &socketStatus;
    chkFifoArg.fifoData = fifoData;

    while (1)
    {
        connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);

        cmdID = NONE;

        cmdDecodeArg.connfd = connfd;

        if(pthread_mutex_init(&mtx, NULL) != 0)
            printf("\nERR: Cannot init mutex\n");

        threadErr = pthread_create(&cmdDecID, NULL, &cmdDecodeThread, (void*)&cmdDecodeArg);
        if (threadErr != 0)
            printf("\tERR: Cannot create cmdDecode thread: [%s]\n", strerror(threadErr));

        threadErr = pthread_create(&chkSttID, NULL, &checkFifoThread, (void*)&chkFifoArg);
        if (threadErr != 0)
            printf("\tERR: Cannot create checkStatus thread: [%s]\n", strerror(threadErr));

        pthread_join(cmdDecID, (void**)&cmdDecRetVal);
        pthread_join(chkSttID, (void**)&chkSttRetVal);
        pthread_mutex_destroy(&mtx);

        close(connfd);
    }
}
