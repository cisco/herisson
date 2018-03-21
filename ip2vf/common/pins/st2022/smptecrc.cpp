#include <cstdio>
#include <cstring>

#include "common.h"
#include "log.h"
#include "tools.h"
#include "smptecrc.h"

using namespace std;

#define UINT8  unsigned char
#define UINT32 unsigned int

#define get18lsb(reg)         (_crc & 0x0003FFFF)

static unsigned int* crc18_lookup_table = NULL;

/*!
* \fn precomputeTable
* \brief precompute a SMPTE CRC table
*
*/
static void
precomputeTable()
{
    crc18_lookup_table = (unsigned int*)malloc(1024 * sizeof(int));
    for (int j = 0; j < 1024; j++)
    {
        int _crc = 0;
        int word10bits = j;
        for (int i = 0; i < 10; i++)
        {
            int incomingBit = word10bits & 0x1;
            int c14 = (_crc >> 14) & 0x1;
            int c13 = (_crc >> 13) & 0x1;
            int c0 = _crc & 0x1;
            int newC12 = c13 ^ c0 ^ incomingBit;
            int newC13 = c14 ^ c0 ^ incomingBit;
            int newC17 = c0 ^ incomingBit;
            _crc >>= 1;
            _crc |= newC17 << 17;
            _crc = (_crc & ~(0x1 << 13)) | (newC13 << 13);
            _crc = (_crc & ~(0x1 << 12)) | (newC12 << 12);
            word10bits >>= 1;
        }
        crc18_lookup_table[j] = _crc;
    }
}

CSMPTPCrc::CSMPTPCrc()
{
    reset();
    if (!crc18_lookup_table)
        precomputeTable();
}

CSMPTPCrc::~CSMPTPCrc()
{
}

/*!
* \fn reset
* \brief reset the CRC, as define in st0292-1-2012.pdf, chap 5.4
*
*/
void
CSMPTPCrc::reset()
{
    _crc = 0;
}

/*!
* \fn compute_crc18_word
* \brief add a 10bits word value on the CRC computation, as define in st0292-1-2012.pdf, chap 5.4
*
* \param word10bits 10bits word to use on CRC computation
*/
inline void
CSMPTPCrc::compute_crc18_word(unsigned int word10bits)
{
    _crc = (_crc >> 10) ^ crc18_lookup_table[word10bits ^ (_crc & 0x3FF)];
}

/*!
* \fn compute_crc18_scanline
* \brief compute the CRC for a full SMPTE scanline, as define in st0292-1-2012.pdf, chap 5.4
*
* \param buffer pointer to the first word of 10bits to start the CRC computation
* \param nbWords10bits number of 10 bits words to compute
* \param nStartPos offset, if needed, to start the computation. In number of 10 bits word
* \param nStep step to use when took word of 10 bits
* \return the 18 bit word corresponding to the CRC
*/
unsigned int
CSMPTPCrc::compute_crc18_scanline(unsigned char* buffer,
    unsigned int nbWords10bits, int nStartPos,
    int nStep)
{
    reset();
    for (int i = 0; i < (int)nbWords10bits; i++)
    {
        unsigned int data = tools::get10bitsWord(buffer, nStartPos + i * nStep);
        compute_crc18_word(data);
    }
    return (unsigned int)get18lsb(_crc);
}

/*!
* \fn getCRC0
* \brief calculate the first word of the SMPTE crc as define in st0292-1-2012.pdf, page 6
*
* \return the 10 bit word corresponding to xCR0
*/
unsigned int
CSMPTPCrc::getCRC0()
{
    // ---------------------------------------------------------------------------------------------------
    //  Word  9(MSB)    8        7        6        5        4        3        2        1        0(LSB)
    //  YCR0  !B8      CRC8     CRC7     CRC6     CRC5     CRC4     CRC3     CRC2     CRC1     CRC0
    // ---------------------------------------------------------------------------------------------------
    unsigned int rest = get18lsb(_crc);
    rest = (rest)& 0b0111111111;
    rest = rest | ((!(((rest)& 0b0100000000) >> 8)) << 9);
    return rest;
}

/*!
* \fn getCRC1
* \brief calculate the second word of the SMPTE crc as define in st0292-1-2012.pdf, page 6
*
* \return the 10 bit word corresponding to xCR1
*/
unsigned int
CSMPTPCrc::getCRC1()
{
    // ---------------------------------------------------------------------------------------------------
    //  Word  9(MSB)    8        7        6        5        4        3        2        1        0(LSB)
    //  YCR0  !B8     CRC17    CRC16    CRC15    CRC14    CRC13    CRC12    CRC11    CRC10     CRC9
    // ---------------------------------------------------------------------------------------------------
    unsigned int rest = get18lsb(_crc);
    rest = ((rest)& 0b111111111000000000) >> 9;
    rest = rest | ((!(((rest)& 0b0100000000) >> 8)) << 9);
    return rest;
}

