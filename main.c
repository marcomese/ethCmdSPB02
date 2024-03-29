#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
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
#include <endian.h>
#include <math.h>
#include "commands.h"
#include "registers.h"
#include "dma.h"
#include "crc32.h"
#include "imu.h"

#define CONN_PORT        5000
#define IMU_PORT         5001
#define CONN_MAX_QUEUE   10

#define BIND_MAX_TRIES   10
#define LISTEN_MAX_TRIES 10

#define DATA_HEADER      0x424B4C43
#define DATA_ADDR        0x00000000
#define DATA_BYTES       512
#define DATA_NUMERICS    6
#define DATA_WORDS       (DATA_BYTES/4)
#define DATA_GPS_BYTES   (DATA_BYTES-(DATA_NUMERICS*4))

#define UNIXTIME_LEN     15
#define FILENAME_LEN     55
#define TRG_NUM_PER_FILE 25

#define TRGCNT_IDX 0
#define GTUCNT_IDX 1
#define TRGFLG_IDX 2
#define ALIVET_IDX 3
#define DEADT_IDX  4
#define STATUS_IDX 5

#define RUN_STATUS_MASK 0x01

#define CAN_TIMESTAMP_ID 19
#define CAN_AX_ID        20
#define CAN_AY_ID        21
#define CAN_AZ_ID        22
#define CAN_GX_ID        23
#define CAN_GY_ID        24
#define CAN_GZ_ID        25
#define CAN_Q0_ID        32
#define CAN_Q1_ID        33
#define CAN_Q2_ID        34
#define CAN_Q3_ID        35
#define CAN_ROLL_ID      36
#define CAN_PITCH_ID     37
#define CAN_YAW_ID       38

#define ACCEL_SCALE 2.0/32767.0
#define GYRO_SCALE  250.0/32767.0

#define GYRO_X_OFFSET 61.98
#define GYRO_Y_OFFSET 27.80
#define GYRO_Z_OFFSET 54.97

#define IMUSTR_MAX_LEN 1024

pthread_mutex_t mtx;

typedef struct cmdDecodeArgs{
    axiRegisters_t* regs;
    uint32_t*       cmdID;
    int             connfd;
    int*            socketStatus;
} cmdDecodeArgs_t;

typedef struct chkFifoArgs{
    axiRegisters_t* regs;
    uint32_t*       cmdID;
    int*            socketStatus;
    uint32_t*       fifoData;
    uint32_t*       imuTimestamp;
} chkFifoArgs_t;

typedef struct canReaderArgs{
    uint32_t* cmdID;
    int       canSocket;
    uint32_t* imuTimestamp;
    imu_t*    imu;
    float*    quat;
    float*    eulers;
} canReaderArgs_t;

typedef struct imuDataOutArgs{
    uint32_t* cmdID;
    uint32_t* imuTimestamp;
    imu_t*    imu;
    float*    quat;
    float*    eulers;
} imuDataOutArgs_t;

typedef struct spb2Data{
    uint32_t     header;
    uint32_t     unixTime;
    uint32_t     trgCount;
    uint32_t     gtuCount;
    uint32_t     trgFlag;
    uint32_t     aliveTime;
    uint32_t     deadTime;
    uint32_t     status;
    char         gpsStr[DATA_GPS_BYTES];
    unsigned int crc;
} spb2Data_t;

void genFileName(uint32_t fileCounter, char* fileName, uint32_t fileNameLen){
    time_t rawtime = time(NULL);
    struct tm *ptm = localtime(&rawtime);

    snprintf(fileName, fileNameLen, "/srv/ftp/clkb_event_%04d%02d%02d%02d%02d%02d-%04d.dat.lock",
             ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday,
             ptm->tm_hour, ptm->tm_min, ptm->tm_sec,
             fileCounter);

    return;
}

void unlockFile(char* fileName){
    char unlockedFileName[FILENAME_LEN] = "";

    if(strncmp(fileName,"",FILENAME_LEN) != 0){
        strncpy(unlockedFileName,fileName,strlen(fileName)-5);
        rename(fileName,unlockedFileName);
    }

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

        if(localSocketStatus > 0)
            *cmdArg->cmdID = decodeCmdStr(cmdArg->regs, cmdArg->connfd, ethStr);
        else{
            pthread_mutex_unlock(&mtx);
            pthread_exit(NULL);
        }
        pthread_mutex_unlock(&mtx);

        strncpy(ethStr,"",CMD_MAX_LEN);
    }

    pthread_exit((void *)cmdArg->cmdID);
}   

void* checkFifoThread(void *arg){
    FILE *outFile;
    chkFifoArgs_t* chkArg = (chkFifoArgs_t*)arg;
    unsigned int exitCondition = 0;
    uint32_t statusReg = 0;
    uint32_t running = 0;
    int socketStatusLocal = 0;
    uint32_t cmdIDLocal = NONE;
    uint32_t eventCounter = 0;
    uint32_t fileCounter = 0;
    uint32_t imuTimestamp = 0;
    char fileName[FILENAME_LEN] = "";
    spb2Data_t data = {0, 0, 0, 0, 0, 0, 0, 0, "", 0};

    while(!exitCondition){
        dma_transfer_s2mm(chkArg->regs->dmaReg, DATA_BYTES, chkArg->socketStatus, chkArg->cmdID, chkArg->regs->statusReg, &mtx);

        pthread_mutex_lock(&mtx);
        socketStatusLocal = *chkArg->socketStatus;
        cmdIDLocal = *chkArg->cmdID;
        imuTimestamp = *chkArg->imuTimestamp;
        pthread_mutex_unlock(&mtx);

        exitCondition = (socketStatusLocal <= 0) || (cmdIDLocal == EXIT);

        running = statusReg & RUN_STATUS_MASK;

        memset(data.gpsStr, '\0', DATA_GPS_BYTES);

        if(!exitCondition && running){
            if(!(eventCounter++ % TRG_NUM_PER_FILE)){
                unlockFile(fileName);
                genFileName(fileCounter++,fileName,FILENAME_LEN);
            }

            outFile = fopen(fileName, "ab");

            data.header    = DATA_HEADER;
            pthread_mutex_lock(&mtx);
            data.unixTime  = (uint32_t)time(NULL);
            data.trgCount  = *(chkArg->fifoData+TRGCNT_IDX);
            data.gtuCount  = *(chkArg->fifoData+GTUCNT_IDX);
            data.trgFlag   = *(chkArg->fifoData+TRGFLG_IDX);
            data.aliveTime = *(chkArg->fifoData+ALIVET_IDX);
            data.deadTime  = *(chkArg->fifoData+DEADT_IDX);
            data.status    = statusReg;

            for(int i = DATA_NUMERICS; i < DATA_WORDS; i++){
                data.gpsStr[((i-DATA_NUMERICS)*4)]     = (char)(*(chkArg->fifoData+i)  & 0x000000FF);
                data.gpsStr[(((i-DATA_NUMERICS)*4)+1)] = (char)((*(chkArg->fifoData+i) & 0x0000FF00) >> 8);
                data.gpsStr[(((i-DATA_NUMERICS)*4)+2)] = (char)((*(chkArg->fifoData+i) & 0x00FF0000) >> 16);
                data.gpsStr[(((i-DATA_NUMERICS)*4)+3)] = (char)((*(chkArg->fifoData+i) & 0xFF000000) >> 24);
            }

            data.gpsStr[DATA_GPS_BYTES-3] = (char)((imuTimestamp & 0x0000FF));
            data.gpsStr[DATA_GPS_BYTES-2] = (char)((imuTimestamp & 0x00FF00) >> 8);
            data.gpsStr[DATA_GPS_BYTES-1] = (char)((imuTimestamp & 0xFF0000) >> 16);

            pthread_mutex_unlock(&mtx);

            data.crc = crc_32((unsigned char *)&data, sizeof(data)-sizeof(data.crc), startCRC32);

            fwrite(&data, sizeof(data), 1, outFile);

            fclose(outFile);
        }else{
            eventCounter = 0;
            fileCounter = 0;
            unlockFile(fileName);
        }
    }

    pthread_exit((void *)chkArg->fifoData);
}

void* canReaderThread(void *arg){
    canReaderArgs_t* canArg = (canReaderArgs_t*)arg;
    struct can_frame frame;
    unsigned int exitCondition = 0;
    uint32_t cmdIDLocal = 0;
    int      nBytes     = 0;
    uint8_t  dataIdx    = 0;
    uint32_t timestamp  = 0;
    int16_t  accel[3]   = {0,0,0};
    int16_t  gyro[3]    = {0,0,0};
    float    quat[4]    = {0.0,0.0,0.0,0.0};
    float    eulers[3]  = {0.0,0.0,0.0};

    while(1){
        nBytes = read(canArg->canSocket, &frame, sizeof(struct can_frame));

        pthread_mutex_lock(&mtx);
        cmdIDLocal = *canArg->cmdID;
        pthread_mutex_unlock(&mtx);

        exitCondition = (nBytes <= 0) || (cmdIDLocal == EXIT);

        if(exitCondition != 0)
            break;

        dataIdx = frame.data[0];

        switch(dataIdx){
            case CAN_TIMESTAMP_ID:
                timestamp = frame.data[1]      |
                            frame.data[2] << 8 |
                            frame.data[3] << 16|
                            frame.data[4] << 24;
                break;
            case CAN_AX_ID:
            case CAN_AY_ID:
            case CAN_AZ_ID:
                accel[dataIdx-CAN_AX_ID] = frame.data[1] | frame.data[2] << 8;
                break;
            case CAN_GX_ID:
            case CAN_GY_ID:
            case CAN_GZ_ID:
                gyro[dataIdx-CAN_GX_ID] = frame.data[1] | frame.data[2] << 8;
                break;
            case CAN_Q0_ID:
            case CAN_Q1_ID:
            case CAN_Q2_ID:
            case CAN_Q3_ID:
                quat[dataIdx-CAN_Q0_ID] = (float)(frame.data[1]      |
                                                  frame.data[2] << 8 |
                                                  frame.data[3] << 16|
                                                  frame.data[4] << 24)/1000.0;
                break;
            case CAN_ROLL_ID:
            case CAN_PITCH_ID:
            case CAN_YAW_ID:
                eulers[dataIdx-CAN_ROLL_ID] = (float)(frame.data[1]      |
                                                      frame.data[2] << 8 |
                                                      frame.data[3] << 16|
                                                      frame.data[4] << 24)/1000.0;
                break;
        }

        if(dataIdx == CAN_YAW_ID){
            pthread_mutex_lock(&mtx);

            memcpy(canArg->quat, quat, sizeof(quat));
            memcpy(canArg->eulers, eulers, sizeof(eulers));

            imu_set_accelerometer_raw(canArg->imu, accel[0], accel[1], accel[2]);
            imu_set_gyro_raw(canArg->imu, gyro[0], gyro[1], gyro[2]);
            imu_main_loop(canArg->imu);

            *canArg->imuTimestamp = timestamp;
            pthread_mutex_unlock(&mtx);
        }
    }

    fprintf(stderr,"ERR: error reading from CAN...\n");
    pthread_exit((void *)nBytes);
}

void* imuDataOutThread(void* arg){
    imuDataOutArgs_t* imuArg = (imuDataOutArgs_t*)arg;
    uint32_t cmdIDLocal = 0;
    int socketStatus = 0;
    unsigned int exitCondition = 0;
    int err = -1;
    int imuSockFd = 0;
    int imuConnFd = 0;
    struct sockaddr_in imu_addr;
    char imuStr[IMUSTR_MAX_LEN] = "";
    char oldImuStr[IMUSTR_MAX_LEN] = "";
    float    quat[4]    = {0.0,0.0,0.0,0.0};
    float    eulers[3]  = {0.0,0.0,0.0};

    imuSockFd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&imu_addr, '0', sizeof(imu_addr));

    imu_addr.sin_family = AF_INET;
    imu_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    imu_addr.sin_port = htons(IMU_PORT);

    err = bind(imuSockFd, (struct sockaddr*)&imu_addr, sizeof(imu_addr));
    if(err < 0){
        fprintf(stderr,"\tERR: Error in bind function: [%d]\nRetry %d...\n", err);
        pthread_exit((void *)err);
    }

    err = listen(imuSockFd, CONN_MAX_QUEUE);
    if(err < 0){
        fprintf(stderr,"\tERR: Error in listen function: [%d]\nRetry %d...\n", err);
        pthread_exit((void *)err);
    }

    while(1){
        imuConnFd = accept(imuSockFd, (struct sockaddr*)NULL, NULL);

        if(imuConnFd < 0){
            fprintf(stderr,"\tERR: Error in accept: [%s]\n", strerror(err));
            pthread_exit((void *)imuConnFd);
        }

        while(1){
            pthread_mutex_lock(&mtx);
             snprintf(imuStr,IMUSTR_MAX_LEN,
                    "$%c\tT = %08x\n"
                    "\t\taxR = %.0f, ayR = %.0f, azR = %.0f\n"
                    "\t\tgxR = %.0f, gyR = %.0f, gzR = %.0f\n"
                    "\t\tax = %.4f, ay = %.4f, az = %.4f\n"
                    "\t\tgx = %.4f, gy = %.4f, gz = %.4f\n"
                    "\t\troll = %.4f, pitch = %.4f, yaw = %.4f\n"
                    "Q%f,%f,%f,%f\n",
                    2,
                    *imuArg->imuTimestamp,
                    imuArg->imu->accelerometer_raw.x, imuArg->imu->accelerometer_raw.y, imuArg->imu->accelerometer_raw.z,
                    imuArg->imu->gyro_raw.x, imuArg->imu->gyro_raw.y, imuArg->imu->gyro_raw.z,
                    imuArg->imu->accelerometer.x, imuArg->imu->accelerometer.y, imuArg->imu->accelerometer.z,
                    imuArg->imu->gyro.x, imuArg->imu->gyro.y, imuArg->imu->gyro.z,
                    imuArg->eulers[0]*180.0/PI, imuArg->eulers[1]*180.0/PI, imuArg->eulers[2]*180.0/PI,
                    imuArg->quat[0], imuArg->quat[1], imuArg->quat[2], imuArg->quat[3]);
/*                     imuArg->imu->orientation.roll*180.0/PI, imuArg->imu->orientation.pitch*180.0/PI, imuArg->imu->orientation.yaw*180.0/PI,
                    imuArg->imu->orientation_quat.w, imuArg->imu->orientation_quat.x, imuArg->imu->orientation_quat.y, imuArg->imu->orientation_quat.z); */

            cmdIDLocal = *imuArg->cmdID;
            pthread_mutex_unlock(&mtx);

            getpeername(imuConnFd,(struct sockaddr*)NULL, NULL);

            exitCondition = (errno == ENOTCONN) || (cmdIDLocal == EXIT);

            if(exitCondition != 0)
                break;

            if(strncmp(imuStr,oldImuStr,IMUSTR_MAX_LEN) != 0){
                write(imuConnFd,imuStr,strlen(imuStr));
                strncpy(oldImuStr,imuStr,IMUSTR_MAX_LEN);
            }

            strncpy(imuStr,"",IMUSTR_MAX_LEN);
        }

        close(imuConnFd);
    }

    pthread_exit((void *)imuConnFd);
}

int main(int argc, char *argv[]){
    axiRegisters_t axiRegs;
    cmdDecodeArgs_t cmdDecodeArg;
    chkFifoArgs_t chkFifoArg;
    canReaderArgs_t canReaderArgs;
    imuDataOutArgs_t imuDataOutArgs;
    imu_t imu;
    pthread_t cmdDecID;
    pthread_t chkSttID;
    pthread_t canRdrID;
    pthread_t imuDatID;
    int listenfd = 0;
    int connfd = 0;
    struct sockaddr_in serv_addr;
    uint32_t* fifoData;
    uint32_t cmdDecRetVal = 0;
    uint32_t chkSttRetVal = 0;
    uint32_t canRdrRetVal = 0;
    uint32_t imuDatRetVal = 0;
    uint32_t canData = 0;
    uint32_t cmdID = NONE;
    int socketStatus = 1;
    int err = -1;
    int tries = 0;
    void* mmapRet = NULL;
    int canSocket = 0;
    struct ifreq ifr;
    struct sockaddr_can canAddr;
    struct can_filter rfilter;
    uint32_t imuTimestamp = 0;
    float quat[4] = {0.0,0.0,0.0,0.0};
    float eulers[3] = {0.0,0.0,0.0};

    int devmem = open("/dev/mem", O_RDWR | O_SYNC);
    if (devmem < 0)
        fprintf(stderr,"Error in opening /dev/mem\n");

    mmapRet = mmap(0, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, devmem, CTRL_REG_ADDR);
    if(mmapRet == MAP_FAILED)
        fprintf(stderr,"Error in mapping CTRL_REG_ADDR\n");
    
    axiRegs.ctrlReg = (uint32_t*)mmapRet;

    mmapRet = mmap(0, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, devmem, STATUS_REG_ADDR);
    if(mmapRet == MAP_FAILED)
        fprintf(stderr,"Error in mapping STATUS_REG_ADDR\n");

    axiRegs.statusReg = (uint32_t*)mmapRet;

    mmapRet = mmap(0, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, devmem, L1CNT_REG_ADDR);
    if(mmapRet == MAP_FAILED)
        fprintf(stderr,"Error in mapping L1CNT_REG_ADDR\n");

    axiRegs.l1CntReg = (uint32_t*)mmapRet;

    mmapRet = mmap(0, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, devmem, DMA_REG_ADDR);
    if(mmapRet == MAP_FAILED)
        fprintf(stderr,"Error in mapping DMA_REG_ADDR\n");

    axiRegs.dmaReg = (uint32_t*)mmapRet;

    mmapRet = mmap(0, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, devmem, DATA_ADDR);
    if(mmapRet == MAP_FAILED)
        fprintf(stderr,"Error in mapping DATA_ADDR\n");

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
            fprintf(stderr,"\tERR: Error in bind function: [%d]\nRetry %d...\n", err, tries);
            tries++;
        }else{
            printf("Bind OK\n");
            break;
        }
    }

    if(tries >= BIND_MAX_TRIES){
        fprintf(stderr,"Cannot bind to socket, program must be restarted\n");
        return -1;
    }

    tries = 0;

    while(tries < LISTEN_MAX_TRIES){
        err = listen(listenfd, CONN_MAX_QUEUE);
        if(err < 0){
            fprintf(stderr,"\tERR: Error in listen function: [%d]\nRetry %d...\n", err, tries);
            tries++;
        }else{
            printf("Listen OK\n");
            break;
        }
    }

    if(tries >= LISTEN_MAX_TRIES){
        fprintf(stderr,"Cannot listen to socket, program must be restarted\n");
        return -1;
    }

    cmdDecodeArg.regs         = &axiRegs;
    cmdDecodeArg.cmdID        = &cmdID;
    cmdDecodeArg.socketStatus = &socketStatus;

    chkFifoArg.regs         = &axiRegs;
    chkFifoArg.cmdID        = &cmdID;
    chkFifoArg.socketStatus = &socketStatus;
    chkFifoArg.fifoData     = fifoData;
    chkFifoArg.imuTimestamp = &imuTimestamp;

    canSocket = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if(canSocket < 0)
        fprintf(stderr,"\tERR: Cannot initialize CAN socket...\n");

    strcpy(ifr.ifr_name, "can0" );
    ioctl(canSocket, SIOCGIFINDEX, &ifr);

    memset(&canAddr, 0, sizeof(canAddr));
    canAddr.can_family = AF_CAN;
    canAddr.can_ifindex = ifr.ifr_ifindex;

    err = bind(canSocket, (struct sockaddr *)&canAddr, sizeof(canAddr));
    if (err < 0)
        fprintf(stderr,"\tERR: Cannot bind CAN socket...\n");

    rfilter.can_id   = 0x0B2;
    rfilter.can_mask = 0x0FF;

    setsockopt(canSocket, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter));

	imu = imu_init();
	imu_set_calibration_mode(&imu, IMU_CALIBMODE_NEVER);
	imu_set_gyro_scale_factor(&imu, GYRO_SCALE);
	imu_set_accelerometer_scale_factor(&imu, ACCEL_SCALE);
    imu.gyro_offset.x = GYRO_X_OFFSET;
    imu.gyro_offset.y = GYRO_Y_OFFSET;
    imu.gyro_offset.z = GYRO_Z_OFFSET;

    canReaderArgs.cmdID        = &cmdID;
    canReaderArgs.canSocket    = canSocket;
    canReaderArgs.imuTimestamp = &imuTimestamp;
    canReaderArgs.imu          = &imu;
    canReaderArgs.quat         = quat;
    canReaderArgs.eulers       = eulers;

    imuDataOutArgs.cmdID        = &cmdID;
    imuDataOutArgs.imuTimestamp = &imuTimestamp;
    imuDataOutArgs.imu          = &imu;
    imuDataOutArgs.quat         = quat;
    imuDataOutArgs.eulers       = eulers;

    if(canSocket >= 0){
        err = pthread_create(&canRdrID, NULL, &canReaderThread, (void*)&canReaderArgs);
        if(err != 0){
            fprintf(stderr,"\tERR: Cannot create canReader thread...: [%s]\n", strerror(err));
            close(canSocket);
        }
    }

    err = pthread_create(&imuDatID, NULL, &imuDataOutThread, (void*)&imuDataOutArgs);
    if(err != 0)
        fprintf(stderr,"\tERR: Cannot create imuDataOut thread...: [%s]\n", strerror(err));

    while (1)
    {
        cmdID = NONE;
        socketStatus = 1;

        connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);
        if(connfd < 0){
            fprintf(stderr,"\tERR: Error in accept: [%s]\n", strerror(err));
            continue;
        }

        cmdDecodeArg.connfd = connfd;

        err = pthread_mutex_init(&mtx, NULL);
        if(err != 0){
            fprintf(stderr,"\nERR: Cannot init mutex, disconnecting...: [%s]\n", strerror(err));
            close(connfd);
            continue;
        }

        err = pthread_create(&cmdDecID, NULL, &cmdDecodeThread, (void*)&cmdDecodeArg);
        if(err != 0){
            fprintf(stderr,"\tERR: Cannot create cmdDecode thread, disconnecting...: [%s]\n", strerror(err));
            close(connfd);
            continue;
        }

        err = pthread_create(&chkSttID, NULL, &checkFifoThread, (void*)&chkFifoArg);
        if(err != 0){
            fprintf(stderr,"\tERR: Cannot create checkFifo thread, disconnecting...: [%s]\n", strerror(err));
            close(connfd);
            continue;
        }

        pthread_join(cmdDecID, (void**)&cmdDecRetVal);
        pthread_join(chkSttID, (void**)&chkSttRetVal);
        pthread_mutex_destroy(&mtx);

        close(connfd);
    }

    pthread_join(canRdrID, (void**)&canRdrRetVal);
    pthread_join(imuDatID, (void**)&imuDatRetVal);
}
