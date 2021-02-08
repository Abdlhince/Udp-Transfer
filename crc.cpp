/*
* FILE : crc.cpp
* PROJECT :  Assignment1
* PROGRAMMER : Abdullah Ince & Daniel Meyer
* FIRST VERSION : Feb 07 2021
* DESCRIPTION : This project implements a program to transfer binary and ASCII files using the reliable UDP protocol using the sample code provided.
*/

/*
 * crc.cpp
 *
 * This code is adapted from the sample provided at this web site:
 *
 * http://www.dogma.net/markn/articles/crcman/crcman.htm
 *
 * with corrections and enhancements to make it compatible with
 * Visual Studio for Windows based development, or g++ for Linux based development.
 * All credit rests with the original author.
 *
 * As an exercise in abstraction, create a C++ wrapper for this support to reuse
 * it in any and all applications that require data error checking!
 */

#include <stdio.h>
#include <stdlib.h>

 /*
  * the following global table is used for CRC calculations.
  * Your application must call BuildCRCTable() to initialize
  * this correctly. This table is a global resource for
  * this sample application.
  */

unsigned long CRCTable[256];

/*
 * void BuildCRCTable (void);
 *
 * Instead of performing a straightforward calculation of the 32 bit
 * CRC using a series of logical operations, this program uses the
 * faster table lookup method.  This routine is called once when the
 * program starts up to build the table which will be used later
 * when calculating the CRC values.
 */

#define CRC32_POLYNOMIAL 0xEDB88320L

void BuildCRCTable(void)
{
    int i;
    int j;
    unsigned long crc;

    for (i = 0; i <= 255; i++)
    {
        crc = i;
        for (j = 8; j > 0; j--)
        {
            if (crc & 0x01)
                crc = (crc >> 0x01) ^ CRC32_POLYNOMIAL;
            else
                crc >>= 1;
        }
        CRCTable[i] = crc;
    } /* end for */
} /* end BuildCRCTable */

/*
 * unsigned long CalculateBufferCRC (unsigned int count, unsigned long crc, void *buffer);
 *
 * This routine calculates the CRC for a block of data using the
 * table lookup method. It accepts an original value for the crc,
 * and returns the updated value. A block of any type of data can
 * be passed in via the void* parameter. The parameter count defines
 * how much data at this address we're allowed to process.
 */

unsigned long CalculateBufferCRC(unsigned int count, unsigned long crc, void* buffer)
{
    unsigned char* p;
    unsigned long temp1;
    unsigned long temp2;

    crc = 0;

    p = (unsigned char*)buffer;
    while (count-- != 0)
    {
        temp1 = (crc >> 8) & 0x00FFFFFFL;
        temp2 = CRCTable[((int)crc ^ *p++) & 0xff];
        crc = temp1 ^ temp2;
    } /* end while */
    return crc;
} /* end CalculateBufferCRC */
