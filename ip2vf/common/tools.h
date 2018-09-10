#ifndef _TOOLS_H
#define _TOOLS_H

#include <string>
#include <vector>
#include <fstream>
#ifdef _WIN32
#define _WINSOCKAPI_    // Used to remove winsock duplicate definition when we include <windows.h> before winsock2
#include <windows.h>    // For HANDLE
#endif
#ifdef _WIN32

#ifdef VMILIBRARY_EXPORTS
#pragma once
#define VMILIBRARY_API_TOOLS __declspec(dllexport)
#else
#pragma once
#define VMILIBRARY_API_TOOLS __declspec(dllimport)
#endif

#else
#define VMILIBRARY_API_TOOLS 
#endif
using namespace std;

// Constants used for fast get10bitsWord()
const unsigned char GET_IN1_MASK[4] = { 0b11111111, 0b00111111, 0b00001111, 0b00000011 };
const unsigned char GET_IN2_MASK[4] = { 0b11000000, 0b11110000, 0b11111100, 0b11111111 };
const unsigned char GET_IN1_INDICES[4]  = { 2 ,4, 6, 8 };
const unsigned char GET_IN2_INDICES[4]  = { 6, 4, 2, 0 };

// Constants used for fast set10bitsWord()
const unsigned char SET_IN1_MASK[8] = { 0b00000000, 0b00111111, 0b11000000, 0b00001111, 0b11110000, 0b00000011, 0b11111100, 0b00000000 };
const unsigned char SET_IN2_MASK[8] = { 0b11111111, 0b11000000, 0b00111111, 0b11110000, 0b00001111, 0b11111100, 0b00000011, 0b11111111 };
const unsigned char SET_INDICES[8] = { 2, 6, 4, 4, 6, 2, 8, 0 };

void DUMP_RGBPIXEL_AT(unsigned char* rgb, int w, int h, int x, int y);
void DUMP_YUV8PIXEL_AT(unsigned char* yuv, int w, int h, int x, int y);

namespace tools
{

    VMILIBRARY_API_TOOLS double          getCurrentTimeInS();
    VMILIBRARY_API_TOOLS long long       getCurrentTimeInMicroS();
    VMILIBRARY_API_TOOLS long long       getCurrentTimeInMilliS();
    VMILIBRARY_API_TOOLS long long       getUTCEpochTimeInMs();
    VMILIBRARY_API_TOOLS void            split(const string &s, char delim, vector<string> &elems);
    VMILIBRARY_API_TOOLS vector<string>  split(const string &s, char delim);
    VMILIBRARY_API_TOOLS bool            isDigits(const string &str);
    VMILIBRARY_API_TOOLS bool            isOdd(int val);
    VMILIBRARY_API_TOOLS bool            isEven(int val);
    VMILIBRARY_API_TOOLS bool            noCaseCompare(std::string const& a, std::string const& b);
    VMILIBRARY_API_TOOLS bool            endsWith(std::string const & value, std::string const & ending);
    VMILIBRARY_API_TOOLS std::string     to_string_with_precision(const double a_value, const int n = 6);

    VMILIBRARY_API_TOOLS void            getCPULoad(int& user, int& kernel);
    VMILIBRARY_API_TOOLS void            getMEMORYLoad(int &mem);
    VMILIBRARY_API_TOOLS void            createOneRGBFrameFile(int w, int h, const char* filename);
    VMILIBRARY_API_TOOLS void            createOneRGBAFrameFile(int w, int h, const char* filename);
    VMILIBRARY_API_TOOLS void            dumpBuffer(unsigned char * buff, int len, const char * filename, ios_base::openmode mode = ios::binary);
    VMILIBRARY_API_TOOLS unsigned char * loadBuffer(const char * filename, int &len);
    VMILIBRARY_API_TOOLS void            displayVersion();
    VMILIBRARY_API_TOOLS int             getPPCM(int a, int b);
    VMILIBRARY_API_TOOLS int             randint(int min, int max);
    VMILIBRARY_API_TOOLS int             getIPAddressFromString(const char* str);
    VMILIBRARY_API_TOOLS int             convert10bitsto8bits(unsigned char* in, int in_size, unsigned char* out);
    VMILIBRARY_API_TOOLS int             convert8bitsto10bits(unsigned char* in, int in_size, unsigned char* out);
    VMILIBRARY_API_TOOLS string          getEnv(const string & var);
    VMILIBRARY_API_TOOLS void            convertYUV8ToRGB(unsigned char* src, int src_w, int src_h, unsigned char* dest, int factor, int depth);
    VMILIBRARY_API_TOOLS void            convertRGBAToRGB(unsigned char* src, int src_w, int src_h, unsigned char* dest, int factor, int depth);

#ifndef _WIN32
    VMILIBRARY_API_TOOLS char*           createSHMSegment(int size, int shmkey, int& shmid);
    VMILIBRARY_API_TOOLS void            detachSHMSegment(char* pData);
    VMILIBRARY_API_TOOLS void            deleteSHMSegment(char* pData, int shmid);
    VMILIBRARY_API_TOOLS int             getSHMSegmentSize(int shmid);
    VMILIBRARY_API_TOOLS int             getSHMSegmentAttachNb(int shmid);
    VMILIBRARY_API_TOOLS char*           createSHMSegment_ext(int size, int& shmkey, int& shmid, bool bForceDeleteIfUnused);
    VMILIBRARY_API_TOOLS char*           getSHMSegment(int size, int shmkey, int& shmid);
#else
    VMILIBRARY_API_TOOLS char*           createSHMSegment(int size, int shmkey, HANDLE& shmid);
    VMILIBRARY_API_TOOLS void            detachSHMSegment(char* pData);
    VMILIBRARY_API_TOOLS void            deleteSHMSegment(char* pData, HANDLE shmid);
    VMILIBRARY_API_TOOLS int             getSHMSegmentSize(HANDLE shmid);
    VMILIBRARY_API_TOOLS int             getSHMSegmentAttachNb(HANDLE shmid);
    VMILIBRARY_API_TOOLS char*           createSHMSegment_ext(int size, int& shmkey, HANDLE& shmid, bool bForceDeleteIfUnused);
    VMILIBRARY_API_TOOLS char*           getSHMSegment(int size, int shmkey, HANDLE& shmid);
#endif

    VMILIBRARY_API_TOOLS int inline get10bitsWord(unsigned char* buffer, int pos10bits) {
        int posPack8bits = (int)(pos10bits / 4) * 5;
        int offset = pos10bits - (int)(pos10bits / 4) * 4;
        return ((buffer[posPack8bits + offset] & GET_IN1_MASK[offset]) << (GET_IN1_INDICES[offset])) + ((buffer[posPack8bits + offset + 1] & GET_IN2_MASK[offset]) >> GET_IN2_INDICES[offset]);
    }

    VMILIBRARY_API_TOOLS void inline set10bitsWord(unsigned char* buffer, int pos10bits, int word10bits) {
        int posPack8bits = (int)(pos10bits / 4) * 5;
        int offset = pos10bits - (int)(pos10bits / 4) * 4;
        buffer[posPack8bits + offset]     = (buffer[posPack8bits + offset]     & SET_IN1_MASK[offset*2])   | ((word10bits >> SET_INDICES[offset*2])   & SET_IN2_MASK[offset*2]);
        buffer[posPack8bits + offset + 1] = (buffer[posPack8bits + offset + 1] & SET_IN1_MASK[offset*2+1]) | ((word10bits << SET_INDICES[offset*2+1]) & SET_IN2_MASK[offset*2+1]);
    }
    VMILIBRARY_API_TOOLS void inline set20bitsWord(unsigned char* buffer, int pos10bits, int word10bits1, int word10bits2) {
        int posPack8bits = (int)(pos10bits / 4) * 5;
        int offset = pos10bits - (int)(pos10bits / 4) * 4;
        buffer[posPack8bits + offset]     = (buffer[posPack8bits + offset]     & SET_IN1_MASK[offset * 2])     | ((word10bits1 >> SET_INDICES[offset * 2])     & SET_IN2_MASK[offset * 2]);
        buffer[posPack8bits + offset + 1] = (buffer[posPack8bits + offset + 1] & SET_IN1_MASK[offset * 2 + 1]) | ((word10bits1 << SET_INDICES[offset * 2 + 1]) & SET_IN2_MASK[offset * 2 + 1]);
        //buffer[posPack8bits + offset + 2] = (buffer[posPack8bits + offset + 2] & SET_IN1_MASK[offset * 2 + 2]) | ((word10bits1 << SET_INDICES[offset * 2 + 2]) & SET_IN2_MASK[offset * 2 + 2]);
        //buffer[posPack8bits + offset + 3] = (buffer[posPack8bits + offset + 3] & SET_IN1_MASK[offset * 2 + 3]) | ((word10bits1 << SET_INDICES[offset * 2 + 3]) & SET_IN2_MASK[offset * 2 + 3]);
    }

}
#ifdef _WIN32
void usleep(__int64 usec);
#endif



#endif //_TOOLS_H
