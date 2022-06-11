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

#define CONN_PORT        5000
#define CONN_MAX_QUEUE   10

#define BIND_MAX_TRIES   10
#define LISTEN_MAX_TRIES 10

#define DATA_ADDR        0x0E000000
#define DATA_BYTES       128
#define DATA_NUMERICS    3
#define DATA_WORDS       (DATA_BYTES/4)
#define DATA_GPS_BYTES   (DATA_BYTES-(DATA_NUMERICS*4))

#define UNIXTIME_LEN     15
#define FILENAME_LEN     50
#define TRG_NUM_PER_FILE 25

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

void getUnixTime(char* unixTime){
    time_t rawtime = time(NULL);
    struct tm *ptm = localtime(&rawtime);

    snprintf(unixTime, UNIXTIME_LEN, "%04d%02d%02d%02d%02d%02d",
             ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday,
             ptm->tm_hour, ptm->tm_min, ptm->tm_sec);

    return;
}

void genFileName(uint32_t fileCounter, char* fileName, uint32_t fileNameLen){
    time_t rawtime = time(NULL);
    struct tm *ptm = localtime(&rawtime);

    snprintf(fileName, fileNameLen, "/srv/ftp/clkb_event_%04d%02d%02d%02d%02d%02d-%04d.dat",
             ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday,
             ptm->tm_hour, ptm->tm_min, ptm->tm_sec,
             fileCounter);

    return;
}

void* cmdDecodeThread(void *arg){
    cmdDecodeArgs_t* cmdArg = (cmdDecodeArgs_t*)arg;
    const char *welcomeStr = "CLK BOARD\n";
    char ethStr[CMD_MAX_LEN] = "";
    int localSocketStatus;

    write(cmdArg->connfd, welcomeStr, strlen(welcomeStr));

    while(*cmdArg->cmdID != EXIT){
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

void* checkFifoThread(void *arg){
    chkFifoArgs_t* chkArg = (chkFifoArgs_t*)arg;
    uint32_t eventCounter = 0;
    uint32_t fileCounter = 0;
    char fileName[FILENAME_LEN] = "";
    char unixTime[UNIXTIME_LEN] = "";
    int socketStatusLocal = 0;
    uint32_t cmdIDLocal = NONE;
    unsigned int exitCondition = 0;
    FILE *outFile;
    char gpsStr[DATA_GPS_BYTES];
    char reversedGpsStr[DATA_GPS_BYTES];
    char *revGpsPtr = reversedGpsStr;

    while(!exitCondition){
        dma_transfer_s2mm(chkArg->regs->dmaReg, 128, chkArg->socketStatus, chkArg->cmdID, &mtx);

        pthread_mutex_lock(&mtx);
        socketStatusLocal = *chkArg->socketStatus;
        cmdIDLocal = *chkArg->cmdID;
        pthread_mutex_unlock(&mtx);

        exitCondition = (socketStatusLocal <= 0) || (cmdIDLocal == EXIT);

        if(!exitCondition){
            if(!(eventCounter % TRG_NUM_PER_FILE)){
                genFileName(fileCounter++,fileName,FILENAME_LEN);
            }

            outFile = fopen(fileName, "a");

            eventCounter++;
            
            getUnixTime(unixTime);

            fprintf(outFile, "%s,", unixTime);

            for(int i = 0; i < DATA_NUMERICS; i++)
                fprintf(outFile,"%u,", (unsigned int)(*(chkArg->fifoData+i)));

            memset(gpsStr, '\0', DATA_GPS_BYTES);
            memset(reversedGpsStr, '\0', DATA_GPS_BYTES);

            for(int i = DATA_NUMERICS; i < DATA_WORDS; i++){
                gpsStr[((i-DATA_NUMERICS)*4)]     = (char)(*(chkArg->fifoData+i) & 0x000000FF);
                gpsStr[(((i-DATA_NUMERICS)*4)+1)] = (char)((*(chkArg->fifoData+i) & 0x0000FF00) >> 8);
                gpsStr[(((i-DATA_NUMERICS)*4)+2)] = (char)((*(chkArg->fifoData+i) & 0x00FF0000) >> 16);
                gpsStr[(((i-DATA_NUMERICS)*4)+3)] = (char)((*(chkArg->fifoData+i) & 0xFF000000) >> 24);

                printf("i0=%d i1=%d i2=%d i3=%d\n",
                       ((i-DATA_NUMERICS)*4),
                       (((i-DATA_NUMERICS)*4)+1),
                       (((i-DATA_NUMERICS)*4)+2),
                       (((i-DATA_NUMERICS)*4)+3))
            }

            for(int i = DATA_GPS_BYTES-1; i >= 0; i--){
                if(gpsStr[i] == '\0')
                    continue;
                
                *revGpsPtr++ = gpsStr[i];
            }

            fprintf(outFile, "%s\n", reversedGpsStr);

            fclose(outFile);

        }
    }

    pthread_exit((void *)chkArg->fifoData);
}

int main(int argc, char *argv[]){
    axiRegisters_t axiRegs;
    cmdDecodeArgs_t cmdDecodeArg;
    chkFifoArgs_t chkFifoArg;
    pthread_t cmdDecID;
    pthread_t chkSttID;
    int listenfd = 0;
    int connfd = 0;
    struct sockaddr_in serv_addr;
    uint32_t* fifoData;
    uint32_t cmdDecRetVal = 0;
    uint32_t chkSttRetVal = 0;
    uint32_t cmdID = NONE;
    int socketStatus = 1;
    int err = -1;
    int tries = 0;
    void* mmapRet = NULL;

    int devmem = open("/dev/mem", O_RDWR | O_SYNC);
    if (devmem < 0)
        printf("Error in opening /dev/mem\n");

    mmapRet = mmap(0, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, devmem, CTRL_REG_ADDR);
    if(mmapRet == MAP_FAILED)
        printf("Error in mapping CTRL_REG_ADDR\n");
    
    axiRegs.ctrlReg = (uint32_t*)mmapRet;

    mmapRet = mmap(0, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, devmem, STATUS_REG_ADDR);
    if(mmapRet == MAP_FAILED)
        printf("Error in mapping STATUS_REG_ADDR\n");

    axiRegs.statusReg = (uint32_t*)mmapRet;

    mmapRet = mmap(0, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, devmem, L1CNT_REG_ADDR);
    if(mmapRet == MAP_FAILED)
        printf("Error in mapping L1CNT_REG_ADDR\n");

    axiRegs.l1CntReg = (uint32_t*)mmapRet;

    mmapRet = mmap(0, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, devmem, DMA_REG_ADDR);
    if(mmapRet == MAP_FAILED)
        printf("Error in mapping DMA_REG_ADDR\n");

    axiRegs.dmaReg = (uint32_t*)mmapRet;

    mmapRet = mmap(0, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, devmem, DATA_ADDR);
    if(mmapRet == MAP_FAILED)
        printf("Error in mapping DATA_ADDR\n");

    fifoData = (uint32_t*)mmapRet;

    printf("Initializing DMA...\n");
    dma_init_s2mm(axiRegs.dmaReg);
    dma_set_buffer(axiRegs.dmaReg, DATA_ADDR);
    printf("DMA Initialized!\n");

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(CONN_PORT);

    while(tries < BIND_MAX_TRIES){
        err = bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
        if(err < 0){
            printf("\tERR: Error in bind function: [%d]\nRetry %d...\n", err, tries);
            tries++;
        }else{
            printf("Bind OK\n");
            break;
        }
    }

    if(tries >= BIND_MAX_TRIES){
        printf("Cannot bind to socket, program must be restarted\n");
        return -1;
    }

    tries = 0;

    while(tries < LISTEN_MAX_TRIES){
        err = listen(listenfd, CONN_MAX_QUEUE);
        if(err < 0){
            printf("\tERR: Error in listen function: [%d]\nRetry %d...\n", err, tries);
            tries++;
        }else{
            printf("Listen OK\n");
            break;
        }
    }

    if(tries >= LISTEN_MAX_TRIES){
        printf("Cannot listen to socket, program must be restarted\n");
        return -1;
    }

    cmdDecodeArg.regs = &axiRegs;
    cmdDecodeArg.cmdID = &cmdID;
    cmdDecodeArg.socketStatus = &socketStatus;

    chkFifoArg.regs = &axiRegs;
    chkFifoArg.cmdID = &cmdID;
    chkFifoArg.socketStatus = &socketStatus;
    chkFifoArg.fifoData = fifoData;

    while (1)
    {
        cmdID = NONE;
        socketStatus = 1;

        connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);
        if(connfd < 0){
            printf("\tERR: Error in accept: [%s]\n", strerror(err));
            continue;
        }

        cmdDecodeArg.connfd = connfd;

        err = pthread_mutex_init(&mtx, NULL);
        if(err != 0){
            printf("\nERR: Cannot init mutex, disconnecting...: [%s]\n", strerror(err));
            close(connfd);
            continue;
        }

        err = pthread_create(&cmdDecID, NULL, &cmdDecodeThread, (void*)&cmdDecodeArg);
        if(err != 0){
            printf("\tERR: Cannot create cmdDecode thread, disconnecting...: [%s]\n", strerror(err));
            close(connfd);
            continue;
        }

        err = pthread_create(&chkSttID, NULL, &checkFifoThread, (void*)&chkFifoArg);
        if(err != 0){
            printf("\tERR: Cannot create checkFifo thread, disconnecting...: [%s]\n", strerror(err));
            close(connfd);
            continue;
        }

        pthread_join(cmdDecID, (void**)&cmdDecRetVal);
        pthread_join(chkSttID, (void**)&chkSttRetVal);
        pthread_mutex_destroy(&mtx);

        close(connfd);
    }
}
