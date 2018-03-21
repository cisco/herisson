#ifndef _SMPTECRC_H
#define _SMPTECRC_H


using namespace std;

/*
 Based on CRC define on ST0292-1-2012.pdf, section 5.4

 polynomial generator equation:   CRC(X) = X^18 + X^5 + X^4 + 1

*/

class CSMPTPCrc
{
private:
    unsigned int _crc;

public:
    CSMPTPCrc();
    ~CSMPTPCrc();

public:
    void         compute_crc18_word(unsigned int word10bits);
    unsigned int compute_crc18_scanline(unsigned char* buffer, unsigned int nbWords10bits, int nStartPos, int nStep);
    void         reset();
    unsigned int getCRC0();
    unsigned int getCRC1();

};

#endif //_SMPTECRC_H
