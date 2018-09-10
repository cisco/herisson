#ifndef _FRAMEHEADER_H
#define _FRAMEHEADER_H

#define FRAME_HEADER_MAGIC_1     0x00
#define FRAME_HEADER_MAGIC_2     0x00
#define FRAME_HEADER_MAGIC_3     0x00
#define FRAME_HEADER_MAGIC_4     0x01

/* Total size of vMI headers */
#define FRAME_HEADER_LENGTH      128
#define FRAME_NAME_LENGTH        48

#include "common.h"
#include "libvMI.h"

enum FRAMETYPE {
    FRAME = 0,
    FIELD = 1
};
enum FIELDTYPE {
    ODD = 0,
    EVEN = 1,
};


// Forward declaration
class CFrameHeaders;
class CSMPTPProfile;

class CFrameHeaders
{
private:
    int         _versionmajor;
    int         _versionminor;
    int         _versionpatch;
    int         _framenb;
    int         _moduleid;
    MEDIAFORMAT _mediafmt;
    int         _headerlen;
    int         _mediasize;
    unsigned int       _mediatimestamp;
    unsigned long long _srctimestamp;

    // VIDEO
    int         _w;
    int         _h;
    FRAMETYPE   _frametype;
    FIELDTYPE   _odd;
    int         _pgroupsize;
    COLORIMETRY _colorimetry;
    SAMPLINGFMT _samplingformat;
    int         _depth;
    int         _framerateCode;     // Based on code defined on g_FRATE array values
    int         _smpteframeCode;    // FRAME code of Video Source Format fields on HBRMP headers.

    // AUDIO
    int         _channelnb;
    AUDIOFMT    _audiofmt;
    SAMPLERATE  _samplerate;
    int         _packettime;

    // Ext
    unsigned long long _inputtimestamp;
    unsigned long long _outputtimestamp;
    char       _namedata[FRAME_NAME_LENGTH];

public:
    CFrameHeaders() ;
    ~CFrameHeaders() {};

public:

    //
    // Basics operations
    //

    void CopyHeaders(const CFrameHeaders* from);
    CFrameHeaders& operator=(CFrameHeaders other)
    {
        this->CopyHeaders(&other);
        return *this;
    }
    int  WriteHeaders(unsigned char* buffer, int frame_nb=-1);
    int  ReadHeaders(unsigned char* buffer);
    void DumpHeaders(unsigned char* frame=NULL);
    static int GetHeadersLength() { return FRAME_HEADER_LENGTH; };

    //
    // External initializers
    //

    void InitVideoHeadersFromSMPTE(int w, int h, SAMPLINGFMT samplingfmt, bool interlaced);
    void InitAudioHeadersFromSMPTE(AUDIOFMT audiofmt, SAMPLERATE samplerate);
    void InitVideoHeadersFromRTP(int w, int h, SAMPLINGFMT samplingfmt, int depth, bool interlaced);
    void InitVideoHeadersFromTR03(int w, int h, SAMPLINGFMT samplingfmt, int depth, bool interlaced);
    void InitVideoHeadersFromProfile(CSMPTPProfile* smpteProfile);

    //
    // Accessors
    //

    CSMPTPProfile GetProfile();

    // common GET/SET
    int  GetFrameNumber() { return _framenb; };
    void SetFrameNumber(int nbframe) { _framenb = nbframe; };
    int  GetModuleId() { return _moduleid; };
    void SetModuleId(int module_id) { _moduleid  = module_id ;};
    MEDIAFORMAT  GetMediaFormat() { return _mediafmt; };
    void SetMediaFormat(MEDIAFORMAT mediafmt) { _mediafmt = mediafmt; };
    int  GetMediaSize() { return _mediasize; };
    void SetMediaSize(int mediasize) { _mediasize = mediasize; };
    unsigned int  GetMediaTimestamp() { return _mediatimestamp; };
    void SetMediaTimestamp(unsigned int mediatimestamp) { _mediatimestamp = mediatimestamp; };
    unsigned long long  GetSrcTimestamp() { return _srctimestamp; };
    void SetSrcTimestamp(unsigned long long srctimestamp) { _srctimestamp = srctimestamp; };

    // media video GET/SET
    int  GetW() { return _w; };
    void SetW(int w) { _w = w; };
    int  GetH() { return _h; };
    void SetH(int h) { _h = h; };
    FRAMETYPE  GetFrameType() { return _frametype; };
    void SetFrameType(FRAMETYPE frametype) { _frametype = frametype; };
    FIELDTYPE  GetFieldType() { return _odd; };
    void SetFieldType(FIELDTYPE fieldtype) { _odd = fieldtype; };
    int  GetGroupSize() { return _pgroupsize; };
    void SetGroupSize(int groupsize) { _pgroupsize = groupsize; };
    COLORIMETRY  GetColorimetry() { return _colorimetry; };
    void SetColorimetry(COLORIMETRY colorimetry) { _colorimetry = colorimetry; };
    SAMPLINGFMT  GetSamplingFmt() { return _samplingformat; };
    void SetSamplingFmt(SAMPLINGFMT samplingfmt) { _samplingformat = samplingfmt; };
    int  GetDepth() { return _depth; };
    void SetDepth(int depth) { _depth = depth; };
    int  GetFramerateCode() { return _framerateCode; };
    void SetFramerateCode(int code) { _framerateCode = code; };
    int  GetSmpteframeCode() { return _smpteframeCode; };
    void SetSmpteframeCode(int code) { _smpteframeCode = code; };

    // media audio GET/SET
    int  GetChannelNb() { return _channelnb; };
    void SetChannelNb(int channelnb) { _channelnb = channelnb; };
    AUDIOFMT  GetAudioFmt() { return _audiofmt; };
    void SetAudioFmt(AUDIOFMT audiofmt) { _audiofmt = audiofmt; };
    SAMPLERATE  GetSampleRate() { return _samplerate; };
    void SetSampleRate(SAMPLERATE samplerate) { _samplerate = samplerate; };
    int  GetPacketTime() { return _packettime; };
    void SetPacketTime(int packettime) { _packettime = packettime; };

    // Ext GET/SET
    unsigned long long GetInputTimestamp() { return _inputtimestamp; };
    void SetInputTimestamp(unsigned long long timestamp) { _inputtimestamp = timestamp; };
    unsigned long long GetOutputTimestamp() { return _outputtimestamp; };
    void SetOutputTimestamp(unsigned long long timestamp) { _outputtimestamp = timestamp; };
    const char* GetName() { return (const char*)_namedata; };
    void SetName(const char* name);
};

#endif //_FRAMEHEADER_H
