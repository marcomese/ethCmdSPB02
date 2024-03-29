#ifndef COMMANDS_H_
#define COMMANDS_H_

#include <stdlib.h>
#include <sys/mman.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include "registers.h"

#define NONE            0x00

#define START_RUN       0x02
#define STOP_RUN        0x03
#define RELEASE_BUSY    0x04
#define SET_BUSY        0x05
#define TRIGGER         0x06
#define RESET_GPS       0x07
#define CONFIGURE_GPS   0x08
#define GPS1_ON         0x09
#define GPS2_ON         0x0A
#define RESET_GTU_COUNT 0x0B
#define RESET_PACKET_NR 0x0C
#define RESET_TRG_COUNT 0x0D
#define RESET_ALL_COUNT 0x0E
#define PPS_TRG_ON      0x0F
#define PPS_TRG_OFF     0x10
#define MASK_EXT_TRG    0x11
#define UNMASK_EXT_TRG  0x12
#define SELF_TRG        0x13
#define SELF_TRG_OFF    0x14
#define NO_ZYNQ1        0x15
#define NO_ZYNQ2        0x16
#define NO_ZYNQ3        0x17
#define ZYNQ1_ON        0x18
#define ZYNQ2_ON        0x19
#define ZYNQ3_ON        0x1A
#define READ_STATUS     0x1B
#define READ_GTUCOUNTER 0x1C
#define READ_TRGCOUNTER 0x1D
#define READ_L11COUNTER 0x1E
#define READ_L12COUNTER 0x1F
#define READ_L13COUNTER 0x20
#define NO_GPS1         0x21
#define NO_GPS2         0x22
#define GPSAUTO_ON      0x23
#define GPSAUTO_NO      0x24

#define EXIT            0xFF

#define CMD_MAX_LEN     15

#define STATUS_ID_MAX_LEN 128

#define TCP_SND_BUF     2048

struct cmd;
typedef void (*funcPtr_t)(axiRegisters_t* regDev, int connfd, struct cmd* cmd);

typedef struct cmd{
    const char *cmdStr;
    uint8_t cmdVal;
    const char *feedbackStr;
    funcPtr_t funcPtr;
    uint32_t baseAddr;
    uint32_t regAddr;
} cmd_t;

uint32_t decodeCmdStr(axiRegisters_t* regDev, int connfd, char* ethStr);

#endif
