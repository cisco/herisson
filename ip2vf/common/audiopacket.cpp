#include <cstdio>
#include <cstdlib>
#include <cstring>      // strerror
#include <string>
#include <iostream>     // for file saving

#include "common.h"
#include "log.h"
#include "tools.h"
#include "audiopacket.h"

// Initialise the audio map... to identify valid audio channel and keep only valid data
std::vector<struct AudioChannelsGroup> CreateAudioMap()
{
    std::vector<struct AudioChannelsGroup> v;
    v.push_back(AudioChannelsGroup(AUDIO_PACKET_GROUP1, AUDIO_CONTROL_PACKET_GROUP1, 0));
    v.push_back(AudioChannelsGroup(AUDIO_PACKET_GROUP2, AUDIO_CONTROL_PACKET_GROUP2, 1));
    v.push_back(AudioChannelsGroup(AUDIO_PACKET_GROUP3, AUDIO_CONTROL_PACKET_GROUP3, 2));
    v.push_back(AudioChannelsGroup(AUDIO_PACKET_GROUP4, AUDIO_CONTROL_PACKET_GROUP4, 3));
    return v;
}
std::vector<struct AudioChannelsGroup> AudioPacket::_audioMap = CreateAudioMap();



AudioPacket::AudioPacket() {
    _interlaced = INTERLACED_MODE::NOT_DEFINED;
    _DID = 0;
    _DBN10bits = 0;
    _DBN = 0;
    _DC10bits = 0;
    _DC = 0;
    _CLK = 0;
    _packetSize = sizeof(_packet)/sizeof(int);
    _isValid = true;
    _isControlPacket = false;
}

AudioPacket::AudioPacket(INTERLACED_MODE interlaced) : AudioPacket() {
    _interlaced = interlaced;
} 

AudioPacket::~AudioPacket() {
}

int AudioPacket::DetectADF10bits(unsigned char* buffer, int buffersize_8bits, int offset_10bits) {
    if (_interlaced != INTERLACED_MODE::NOT_DEFINED) {
        return _detectADF10bits(buffer, buffersize_8bits, offset_10bits, _interlaced);
    }
    else {
        // First, try interlaced
        int pos = _detectADF10bits(buffer, buffersize_8bits, offset_10bits, INTERLACED_MODE::INTERLACED);
        if (pos != -1) {
            LOG_INFO("DETECT INTERLACED_MODE::INTERLACED");
            _interlaced = INTERLACED_MODE::INTERLACED;
            return pos;
        }
        else {
            pos = _detectADF10bits(buffer, buffersize_8bits, offset_10bits, INTERLACED_MODE::NO_INTERLACED);
            if (pos != -1) {
                LOG_INFO("DETECT INTERLACED_MODE::NO_INTERLACED");
                _interlaced = INTERLACED_MODE::NO_INTERLACED;
                return pos;
            }
            else {
                // Pb
                LOG("NO DETECT AUDIO");
                return -1;
            }
        }
    }
}

int ADF_Interlaced[6] = { 0x000, 0x000, 0x3ff, 0x3ff, 0x3ff, 0x3ff };
int len_ADF_Interlaced = sizeof(ADF_Interlaced) / sizeof(int);
int ADF_NoInterlaced[3] = { 0x000, 0x3ff, 0x3ff };
int len_ADF_NoInterlaced = sizeof(ADF_NoInterlaced) / sizeof(int);
int AudioPacket::_detectADF10bits(unsigned char* buffer, int buffersize_8bits, int offset_10bits, INTERLACED_MODE interlaced) {
    int size10bits = 8 * buffersize_8bits / 10;
    //LOG_INFO("size=%d, size10=%d, len=%d", size8bits, size10bits, len_ADF);
    int* ADF = (interlaced ? ADF_Interlaced : ADF_NoInterlaced);
    int lenADF = (interlaced ? len_ADF_Interlaced : len_ADF_NoInterlaced);
    int pos10bits = offset_10bits;
    int w;
    //while (pos10bits <= size10bits - lenADF * 2) {
        w = tools::get10bitsWord(buffer, pos10bits);
        if (w == ADF[0]) {
            //LOG_INFO("pos=%d, w=0x%03x, ADF[0]=0x%03x", pos10bits, w, ADF[0]);
            for (int i = 1; i < lenADF; i++) {
                w = tools::get10bitsWord(buffer, pos10bits + i * 2);
                //LOG_INFO("(2)pos=%d, w=0x%03x, ADF[%d]=0x%03x", pos10bits, w, i, ADF[i]);
                if (tools::get10bitsWord(buffer, pos10bits + i * 2) != ADF[i])
                    break;
                if (i == lenADF - 1) {
                    //LOG_INFO("... pos10=%d", pos10bits);
                    return pos10bits;
                }
            }
            //LOG_INFO("... pos10=%d", pos10bits);
            //return pos10bits;
        }
        pos10bits++;
    //}
    //LOG_INFO("can't found ANC packet start seq");
    return -1;
}

// Ectract channel value on 24bits
int AudioPacket::_extractChannelValue(int pos) {
    int audioDta = ((_packet[pos] & 0b0011110000) >> 4) +
        ((_packet[pos+1] & 0b0011111111) << 4) +
        ((_packet[pos+2] & 0b0011111111) << 12) +
        ((_packet[pos+3] & 0b0000001111) << 20);
    return audioDta;
}

AudioChannelsGroup* AudioPacket::_getAudioGroupForDID(int DID) {
    for (int i = 0; i < 4; i++) {
        if (AudioPacket::_audioMap[i]._group_id == DID)
            return &(AudioPacket::_audioMap[i]);
        if (AudioPacket::_audioMap[i]._control_id == DID)
            return &(AudioPacket::_audioMap[i]);
    }
    return NULL;
}

/*
*
* ReadSMPTEAudioData() and WriteSMPTEAudioData() methods allows to read from SMPTE or write to SMPTE buffer.
*
*/
int AudioPacket::WriteSMPTEAudioData(unsigned char* buffer, int startoffset_10bits)
{
    // Note: always use AUDIO_PACKET_MAX_LENGTH as size 
    int sizeToWrite = IsControlPacket() ? AUDIO_CONTROL_PACKET_MAX_LENGTH : AUDIO_PACKET_MAX_LENGTH;
    for (int i = 0; i < sizeToWrite; i++) {
         tools::set10bitsWord(buffer, startoffset_10bits + i * 2, _packet[i]);
    }
    return 0;
}
int AudioPacket::ReadSMPTEAudioData(unsigned char* buffer, int startoffset_10bits) {

    // Reset some parameters
    _isValid = true;
    _isControlPacket = false;

    // Keep audio data packet. Save 10bits word on int for simpler processing
    int factor = (_interlaced == INTERLACED_MODE::INTERLACED) ? 2 : 1;
    for (int i = 0; i < AUDIO_PACKET_MAX_LENGTH; i++) {
        _packet[i] = tools::get10bitsWord(buffer, startoffset_10bits + i * 2 * factor);
    }

    // firstly, get DID value for this audio packet
    _DID = _packet[3];
    //LOG_INFO(" DID=0x%03x", _DID);

    // Decode the packet
    if (IS_AUDIO_CONTROL_PACKET(_DID)) 
        return _decodeAudioControlPacket(_DID);
    else if (IS_AUDIO_PACKET(_DID))
        return _decodeAudioPacket(_DID);
    else {
        LOG("Invalid DID for audio packet... DID=0x%03x", _DID);
        _isValid = false;
    }

    return 0;
}
int AudioPacket::_decodeAudioControlPacket(int DID) {

    _isControlPacket = true;
    _packetSize = AUDIO_CONTROL_PACKET_MAX_LENGTH;

    // Get AudioGroup Object corresponding to the DID
    AudioChannelsGroup* pAudioGroup = _getAudioGroupForDID(_DID);
    if (pAudioGroup == NULL) {
        LOG_ERROR("Can't detect audio group for DID=0x%03x", _DID);
        return 0;
    }

    if (pAudioGroup->_valid)
        return _packetSize;

    int _af = (_packet[6] & 0b11111111);
    int _x0rate = (_packet[7] & 0b10) >> 1;
    int _x1rate = (_packet[7] & 0b100) >> 2;
    int _x2rate = (_packet[7] & 0b1000) >> 3;
    if (_x2rate == 0 && _x1rate == 0 && _x0rate == 0)
        pAudioGroup->_sampling_rate = AUDIO_PACKET_SAMPLING_RATE_48_0_KHZ;
    else if (_x2rate == 0 && _x1rate == 0 && _x0rate == 1)
        pAudioGroup->_sampling_rate = AUDIO_PACKET_SAMPLING_RATE_44_1_KHZ;
    else if (_x2rate == 0 && _x1rate == 1 && _x0rate == 0)
        pAudioGroup->_sampling_rate = AUDIO_PACKET_SAMPLING_RATE_32_0_KHZ;
    else if (_x2rate == 1 && _x1rate == 0 && _x0rate == 0)
        pAudioGroup->_sampling_rate = AUDIO_PACKET_SAMPLING_RATE_96_0_KHZ;
    pAudioGroup->_channels[0]._active = _packet[8] & 0b1;
    pAudioGroup->_channels[1]._active = !!((_packet[8] & 0b10) >> 1);     // to prevent "warning C4800: 'int': forcing value to bool 'true' or 'false' (performance warning)"
    pAudioGroup->_channels[2]._active = !!((_packet[8] & 0b100) >> 2);    // to prevent "warning C4800: 'int': forcing value to bool 'true' or 'false' (performance warning)"
    pAudioGroup->_channels[3]._active = !!((_packet[8] & 0b1000) >> 3);   // to prevent "warning C4800: 'int': forcing value to bool 'true' or 'false' (performance warning)"
    LOG_INFO("audio control packet detected DID=0x%03x, _af=%d, rate=%d, ch active=%d/%d/%d/%d", _DID, _af, pAudioGroup->_sampling_rate, pAudioGroup->_channels[0]._active, 
        pAudioGroup->_channels[1]._active, pAudioGroup->_channels[2]._active, pAudioGroup->_channels[3]._active);

    pAudioGroup->_valid = true;

    return _packetSize;
}
int AudioPacket::_decodeAudioPacket(int DID) {

    _isControlPacket = false;
    _packetSize = AUDIO_PACKET_MAX_LENGTH;

    // get the corresponding AudioChannelsGroup 
    AudioChannelsGroup* pAudioGroup = _getAudioGroupForDID(DID);
    if (pAudioGroup == NULL) {
        LOG_ERROR("Can't detect audio group for DID=0x%03x", DID);
        return 0;
    }
    //LOG_INFO("Use AudioChannelsGroup %d for DID=0x%03x", pAudioGroup->_group_index, DID);

    if (pAudioGroup->_valid) {
        // Scan this audiogroup
        _DBN10bits = _packet[4];  
        _DBN = (_DBN10bits & 0b11111111);
        _DC10bits = _packet[5];
        _DC = (_DC10bits & 0b11111111); // must be 24
        _CLK = (_packet[6] & 0b11111111) + ((_packet[7] & 0b1111) << 8) + ((_packet[7] & 0b100000) << 12);
        int _mpf = (_packet[7] & 0b10000) >> 4;

        // Check validity for each channel of this audio group...
        for (int i = 0; i < 4; i++) 
            if( pAudioGroup->_channels[i]._active ) {
                int channelOffset = 8 + i * 4;  // 8 for channel 1, 12 for ch2, 16 = ch3, 20=ch4
                pAudioGroup->_channels[i]._chDta = _extractChannelValue(channelOffset);
                pAudioGroup->_channels[i]._v = (_packet[channelOffset + 3] & 0b0000010000) >> 4;   // AES Sample validity bit
                pAudioGroup->_channels[i]._u = (_packet[channelOffset + 3] & 0b0000100000) >> 5;   // AES user bit
                pAudioGroup->_channels[i]._c = (_packet[channelOffset + 3] & 0b0001000000) >> 6;   // AES channel status bit
                pAudioGroup->_channels[i]._p = (_packet[channelOffset + 3] & 0b0010000000) >> 7;   // AES parity bit
                                                                                                   // if data, v, u, c and p equal to 0 for a channel => inactive channel
                                                                                                   // if (v == 0 && u == 0 && c == 0 && p == 0)
                //pAudioGroup->_channels[i].write();
            //LOG_INFO("_DID=0x%03x, v, u, c, p = (%d, %d, %d, %d), chDta=0x%x", _DID, pAudioGroup->_channels[i]._v, pAudioGroup->_channels[i]._u, pAudioGroup->_channels[i]._c, pAudioGroup->_channels[i]._p, pAudioGroup->_channels[i]._chDta);
        }
        //LOG_INFO("Validity [%d,%d,%d,%d]", pAudioGroup->_channels[0]._active, pAudioGroup->_channels[1]._active, pAudioGroup->_channels[2]._active, pAudioGroup->_channels[3]._active);
        //LOG("line ADF=0x%03x 0x%03x 0x%03x, DID=0x%03x, DBN=0x%03x(%d), DC=0x%03x(%d), ch1Dta=0x%x", /_packet[0], _packet[1], _packet[2], _DID, _DBN10bits, _DBN, _DC10bits, _DC, ch1Dta);
    }
    //else
    //   LOG_ERROR("Channelgroup not valid DID=0x%03x", DID);
    return _packetSize;
}


/*
*
* Read() and write() methods allows are used to read from/write to Internal IP2VF buffer...
* Do not use such funtion to read from SMPTE or write to SMPTE buffer. 
*
*/
int AudioPacket::Read(unsigned char* buffer)
{
    memcpy(_packet, buffer, AUDIO_PACKET_MAX_LENGTH*sizeof(int));

    // firstly, get DID value for this audio packet
    _DID = _packet[3];
    //LOG_INFO(" DID=0x%03x", _DID);

    // Decode the packet
    if (IS_AUDIO_CONTROL_PACKET(_DID))
        return _decodeAudioControlPacket(_DID);
    else if (IS_AUDIO_PACKET(_DID))
        return _decodeAudioPacket(_DID);
    else {
        LOG("Invalid DID for audio packet... DID=0x%03x", _DID);
        _isValid = false;
    }

    return 0;
}
int AudioPacket::Write(unsigned char* buffer, int line)
{
    if (!_isValid)
        return 0;

    int writed = 0;
    unsigned char* p = buffer;

    // Write the line number
    *((int*)p) = line;
    writed += sizeof(int);
    p += sizeof(int);

    // Write the packet content. Note that we write always the full buffer, event if the one for control packet is smaller
    // No matter about kind of audio packet (i.e. data or control) as it is mentioned on DID parameter (_packet[3])
    memcpy(p, _packet, AUDIO_PACKET_MAX_LENGTH*sizeof(int));
    writed += AUDIO_PACKET_MAX_LENGTH*sizeof(int);
    return writed;

    //memcpy(buffer, _packet, AUDIO_PACKET_MAX_LENGTH*sizeof(int));
    //return AUDIO_PACKET_MAX_LENGTH*sizeof(int);
}


int AudioPacket::DumpChannelData(int chanId, int bits) {
#ifdef DEBUG_AUDIO    
    if (bits != 16 && bits != 24)
        return 0;

    //if (!_isValid)
    //    return 0;
    if (_DID != AudioPacket::AUDIO_GROUP1)
        return 0;

    unsigned char dta=0;
    int ch1Dta = _extractChannelValue(8+(chanId-1)*4);

    char filename[64];
    SNPRINTF(filename, 64, "audio%d.pcm", bits);
    static std::ofstream f;
    if( !f.is_open())
        f.open(filename, ios::out | ios::binary);

    if (bits == 16) {
        dta = (ch1Dta & 0b1111111100000000) >> 8;
        f.write((char*)&dta, 1);
        dta = (ch1Dta & 0b0000000011111111);
        f.write((char*)&dta, 1);
    }
    else if (bits == 24) {
        dta = (ch1Dta & 0b111111110000000000000000) >> 16;
        f.write((char*)&dta, 1);
        dta = (ch1Dta & 0b000000001111111100000000) >> 8;
        f.write((char*)&dta, 1);
        dta = (ch1Dta & 0b000000000000000011111111);
        f.write((char*)&dta, 1);
    }

    static int count = 0;
    count++;
    if (count > 100) {
        f.flush();
        count = 0;
    }
#endif  //DEBUG_AUDIO
    return 0;
}


