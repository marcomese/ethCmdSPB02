/**
 * @file   crc32.h
 * @author pesoli <pesoli@pc-eusodp.roma2.infn.it>
 * @date   Thu Jan  3 09:33:41 2013
 * 
 * @brief  
 * 
 * 
 */

/*
 * When using "crc32" these initial CRC values must be given to
 * the respective function the first time it is called. The function can
 * then be called with the return value from the last call of the function
 * to generate a running CRC over multiple data blocks.
 */

#define startCRC32  (0xFFFFFFFF)    /* CRC initialised to all 1s */

/**
 * Provides a table driven implementation of the IEEE-802.3 32-bit CRC
 * algorithm for byte data.
 * 
 * @param data Pointer to the byte data 
 * @param dataLen Number of bytes of data to be processed
 * @param crc Initial CRC value to be used (can be the output from a previous call to this function)
 * 
 * @return  32-bit CRC value for the specified data
 */
unsigned int crc_32(unsigned char *data, unsigned int dataLen,
		   unsigned int crc);


