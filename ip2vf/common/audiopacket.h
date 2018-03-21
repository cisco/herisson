#ifndef _AUDIOPACKET_H
#define _AUDIOPACKET_H

#include <vector>

#define AUDIO_PACKET_MAX_LENGTH             31   // 31 words of 10 bits => saved on 24 integer (we use only 10bits on 32!!)
#define AUDIO_CONTROL_PACKET_MAX_LENGTH     18   // 18 words of 10 bits 

// The Sampling rate constante
#define AUDIO_PACKET_SAMPLING_RATE_48_0_KHZ     0
#define AUDIO_PACKET_SAMPLING_RATE_44_1_KHZ     1
#define AUDIO_PACKET_SAMPLING_RATE_32_0_KHZ     2
#define AUDIO_PACKET_SAMPLING_RATE_96_0_KHZ     3

// The Audio group DID as define in SMPTE documentation
#define AUDIO_PACKET_GROUP1     0x2e7
#define AUDIO_PACKET_GROUP2     0x1e6
#define AUDIO_PACKET_GROUP3     0x1e5
#define AUDIO_PACKET_GROUP4     0x2e4

// The Audio control packet DID as define in SMPTE documentation
#define AUDIO_CONTROL_PACKET_GROUP1     0x1e3
#define AUDIO_CONTROL_PACKET_GROUP2     0x2e2
#define AUDIO_CONTROL_PACKET_GROUP3     0x2e1
#define AUDIO_CONTROL_PACKET_GROUP4     0x1e0

#define IS_AUDIO_CONTROL_PACKET(did)  (did == AUDIO_CONTROL_PACKET_GROUP1 || did == AUDIO_CONTROL_PACKET_GROUP2 || did == AUDIO_CONTROL_PACKET_GROUP3 || did == AUDIO_CONTROL_PACKET_GROUP4)
#define IS_AUDIO_PACKET(did)  (did == AUDIO_PACKET_GROUP1 || did == AUDIO_PACKET_GROUP2 || did == AUDIO_PACKET_GROUP3 || did == AUDIO_PACKET_GROUP4)

struct AudioChannel {
    bool _active;
    int _index;
    int _chDta;
    int _v;   // AES Sample validity bit
    int _u;   // AES user bit
    int _c;   // AES channel status bit
    int _p;   // AES parity bit
    std::ofstream* _f;
    int _sampleCount;
    AudioChannel(int index) {
        _active = false;
        _index = index;
        _sampleCount = 0;
        _f = new std::ofstream;
        _chDta = 0; _v = 0; _u = 0; _c = 0; _p = 0;
    };
    ~AudioChannel() {
        if (_f && _f->is_open()) {
            _f->flush();
            _f->close();
            delete _f;
            _f = NULL;
        }
    };
    bool isValid() {
        return !(_chDta == 0 && _v == 0 && _u == 0 && _c == 0 && _p == 0);
    };
    void write() {
        //LOG_INFO("dump channel(%d): v, u, c, p = (%d, %d, %d, %d), chDta=0x%x, isopen=%d", _index, _v, _u, _c, _p, _chDta, _f->is_open());
        if (!isValid())
            return;
        if (_f && !_f->is_open()) {
            char filename[64];
            SNPRINTF(filename, 64, "audio_%d.pcm", _index);
            _f->open(filename, ios::out | ios::binary);
        }
        if (_f && _f->is_open()) {
            unsigned char dta = 0;
            dta = (_chDta & 0b111111110000000000000000) >> 16;
            _f->write((char*)&dta, 1);
            dta = (_chDta & 0b000000001111111100000000) >> 8;
            _f->write((char*)&dta, 1);
            dta = (_chDta & 0b000000000000000011111111);
            _f->write((char*)&dta, 1);
            _sampleCount++;
            if (_sampleCount > 1000) {
                _f->flush();
                _sampleCount = 0;
            }
        }
    };
};

struct AudioChannelsGroup {
    int _group_id;
    int _control_id;
    int _group_index;
    int _sampling_rate;
    bool _valid;    // The AudioChannelsGroup object become valid when receive control packet for the group
    std::vector<struct AudioChannel> _channels;  // There is 4 channel per audio packet / group channel
    AudioChannelsGroup(int group_id, int control_id, int index) {
        _group_id = group_id;
        _control_id = control_id;
        _group_index = index;
        _valid = false;
        _sampling_rate = AUDIO_PACKET_SAMPLING_RATE_48_0_KHZ;
        for (int i = 1; i < 5; i++) 
            _channels.push_back(AudioChannel((_group_index * 4) + i));
    };
};

class AudioPacket
{
protected:
    int _packet[AUDIO_PACKET_MAX_LENGTH];
    int _packetSize;

    INTERLACED_MODE _interlaced;
    int _DID;
    int _DBN10bits;
    int _DBN;
    int _DC10bits;
    int _DC;
    int _CLK;
    bool _isValid;
    bool _isControlPacket;

public:
    AudioPacket();
    AudioPacket(INTERLACED_MODE interlaced);
    ~AudioPacket();

    static std::vector<struct AudioChannelsGroup> _audioMap;

public:
    INTERLACED_MODE  GetInterlacedMode() { return _interlaced; };
    void SetInterlacedMode(INTERLACED_MODE interlaced) { _interlaced = interlaced; };

    int DetectADF10bits(unsigned char* buffer, int buffersize_8bits, int offset_10bits);
    int ReadSMPTEAudioData(unsigned char* buffer, int startoffset_10bits);
    int WriteSMPTEAudioData(unsigned char* buffer, int startoffset_10bits);
    int DumpChannelData(int chanId, int bits);
    int Read(unsigned char* buffer);
    int Write(unsigned char* buffer, int line);

    int GetPacketSize() { return _packetSize; };
    int GetAudioDataPacketSize() { return AUDIO_PACKET_MAX_LENGTH; };
    int GetAudioControlPacketSize() { return AUDIO_CONTROL_PACKET_MAX_LENGTH; };
    int GetDID() { return _DID; };
    bool IsControlPacket() { return _isControlPacket; };

private:
    int _detectADF10bits(unsigned char* buffer, int buffersize_8bits, int offset_10bits, INTERLACED_MODE interlaced);
    int _extractChannelValue(int pos);
    int _decodeAudioControlPacket(int DID);
    int _decodeAudioPacket(int DID);
    AudioChannelsGroup* _getAudioGroupForDID(int DID);
};

#endif //_AUDIOPACKET_H
