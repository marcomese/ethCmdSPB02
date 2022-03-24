#include "commands.h"

#define COUNT(ARRAY) (sizeof(ARRAY) / sizeof(*ARRAY))

static uint8_t sorted = 0;

const char *errStr = "Error invalid command.\n";

static void writeCmd(axiRegisters_t *regDev, int connfd, cmd_t *c){
    writeReg(regDev->ctrlReg, CTRL_REG_ADDR, CMD_RECV_ADDR, c->cmdVal);
    printf(c->feedbackStr);
    write(connfd, c->feedbackStr, strlen(c->feedbackStr));
}

static void readCmd(axiRegisters_t *regDev, int connfd, cmd_t *c){
    uint32_t regVal = 0;
    char resStr[TCP_SND_BUF] = "";

    regVal = readReg(regDev->statusReg, STATUS_REG_ADDR, c->regAddr);
    sprintf(resStr, "%s%u\n", c->feedbackStr, (unsigned int)regVal);
    printf(resStr);
    write(connfd, resStr, strlen(resStr));
}

static void echo(axiRegisters_t *regDev, int connfd, cmd_t *c){
    printf(c->feedbackStr);
    write(connfd, c->feedbackStr, strlen(c->feedbackStr));
}

static cmd_t commands[] = {
    {"start run", START_RUN, "START RUN\n", writeCmd, CTRL_REG_ADDR, CMD_RECV_ADDR},
    {"stop run", STOP_RUN, "STOP RUN\n", writeCmd, CTRL_REG_ADDR, CMD_RECV_ADDR},
    {"rel busy", RELEASE_BUSY, "RELEASE BUSY\n", writeCmd, CTRL_REG_ADDR, CMD_RECV_ADDR},
    {"set busy", SET_BUSY, "SET BUSY\n", writeCmd, CTRL_REG_ADDR, CMD_RECV_ADDR},
    {"trg", TRIGGER, "TRIGGER\n", writeCmd, CTRL_REG_ADDR, CMD_RECV_ADDR},
    {"gps reset", RESET_GPS, "RESET GPS\n", writeCmd, CTRL_REG_ADDR, CMD_RECV_ADDR},
    {"gps configure", CONFIGURE_GPS, "CONFIGURE GPS\n", writeCmd, CTRL_REG_ADDR, CMD_RECV_ADDR},
    {"gps no", NO_GPS, "NO GPS\n", writeCmd, CTRL_REG_ADDR, CMD_RECV_ADDR},
    {"gps on", GPS_ON, "GPS ON\n", writeCmd, CTRL_REG_ADDR, CMD_RECV_ADDR},
    {"gtu reset", RESET_GTU_COUNT, "RESET GTU COUNT\n", writeCmd, CTRL_REG_ADDR, CMD_RECV_ADDR},
    {"pck reset", RESET_PACKET_NR, "RESET PACKET NR\n", writeCmd, CTRL_REG_ADDR, CMD_RECV_ADDR},
    {"trg reset", RESET_TRG_COUNT, "RESET TRG COUNT\n", writeCmd, CTRL_REG_ADDR, CMD_RECV_ADDR},
    {"all reset", RESET_ALL_COUNT, "RESET ALL COUNT\n", writeCmd, CTRL_REG_ADDR, CMD_RECV_ADDR},
    {"pps on", PPS_TRG_ON, "PPS TRG ON\n", writeCmd, CTRL_REG_ADDR, CMD_RECV_ADDR},
    {"pps off", PPS_TRG_OFF, "PPS TRG OFF\n", writeCmd, CTRL_REG_ADDR, CMD_RECV_ADDR},
    {"msk exttrg", MASK_EXT_TRG, "MASK EXT TRG\n", writeCmd, CTRL_REG_ADDR, CMD_RECV_ADDR},
    {"usk exttrg", UNMASK_EXT_TRG, "UNMASK EXT TRG\n", writeCmd, CTRL_REG_ADDR, CMD_RECV_ADDR},
    {"trg self on", SELF_TRG, "SELF TRG ON\n", writeCmd, CTRL_REG_ADDR, CMD_RECV_ADDR},
    {"trg self off", SELF_TRG_OFF, "SELF TRG OFF\n", writeCmd, CTRL_REG_ADDR, CMD_RECV_ADDR},
    {"zq1 no", NO_ZYNQ1, "NO ZYNQ1\n", writeCmd, CTRL_REG_ADDR, CMD_RECV_ADDR},
    {"zq2 no", NO_ZYNQ2, "NO ZYNQ2\n", writeCmd, CTRL_REG_ADDR, CMD_RECV_ADDR},
    {"zq3 no", NO_ZYNQ3, "NO ZYNQ3\n", writeCmd, CTRL_REG_ADDR, CMD_RECV_ADDR},
    {"zq1 on", ZYNQ1_ON, "ZYNQ1 ON\n", writeCmd, CTRL_REG_ADDR, CMD_RECV_ADDR},
    {"zq2 on", ZYNQ2_ON, "ZYNQ2 ON\n", writeCmd, CTRL_REG_ADDR, CMD_RECV_ADDR},
    {"zq3 on", ZYNQ3_ON, "ZYNQ3 ON\n", writeCmd, CTRL_REG_ADDR, CMD_RECV_ADDR},
    {"status", READ_STATUS, "STATUS REGISTER=", readCmd, STATUS_REG_ADDR, STATUS_REG_ADDR},
    {"gtu counter", READ_GTUCOUNTER, "GTU COUNTER=", readCmd, STATUS_REG_ADDR, GTU_COUNTER_ADDR},
    {"trg counter", READ_TRGCOUNTER, "TRG COUNTER=", readCmd, STATUS_REG_ADDR, TRG_COUNTER_ADDR},
    {"l11 counter", READ_L11COUNTER, "L1_1 COUNTER=", readCmd, L1CNT_REG_ADDR, L1_1_COUNTER_ADDR},
    {"l12 counter", READ_L12COUNTER, "L1_2 COUNTER=", readCmd, L1CNT_REG_ADDR, L1_2_COUNTER_ADDR}
    {"l13 counter", READ_L13COUNTER, "L1_3 COUNTER=", readCmd, L1CNT_REG_ADDR, L1_3_COUNTER_ADDR}
    {"exit", EXIT, "EXIT\n", echo, NONE, NONE}};

static int compare(const void *p1, const void *p2){
    return strcmp(*((const char **)p1), *((const char **)p2));
}

static cmd_t *getCmd(const char *name){
    if (!sorted){
        qsort(commands, COUNT(commands), sizeof(*commands), compare);
        sorted = 1;
    }

    cmd_t *item = (cmd_t *)bsearch(&name, commands, COUNT(commands), sizeof(*commands), compare);

    return item;
}

uint32_t decodeCmdStr(axiRegisters_t* regDev, int connfd, char *ethStr){
    char cmdStr[CMD_MAX_LEN] = "";

    for (int i = 0; (ethStr[i] != '\r') && (ethStr[i] != '\n'); i++)
        cmdStr[i] = ethStr[i];

    cmd_t *cmd = getCmd(cmdStr);

    if (cmd != NULL){
        cmd->funcPtr(regDev, connfd, cmd);
        return cmd->cmdVal;
    }else{
        printf(errStr);
        write(connfd, errStr, strlen(errStr));
    }

    return 0;
}
