#ifndef _HBRMPFRAME_H
#define _HBRMPFRAME_H

// Forward declaration
class CSMPTPProfile;

#define HBRMP_FIXED_LENGTH              8   // in bytes
#define HBRMP_VIDEO_TIMESTAMP_LENGTH    4   // in bytes
#define HBRMP_HEADERS_LENGTH            (HBRMP_FIXED_LENGTH + HBRMP_VIDEO_TIMESTAMP_LENGTH)      // in bytes

struct FRAME_t {
    int     code;        // Nb of column used to render the video (is < to the total nb of column in frame)
    int     horizontal_active;
    int     vertical_active;
    int     vertical_total;
    int     sampling_struct;     // 0 for interlaced, 1 for progressive
    int     transport_struct;    // 0 for interlaced, 1 for progressive, 2 for interlaced segmented
};
extern FRAME_t g_FRAME[];
extern int g_FRAME_len;

struct FRATE_t {
    int     code;        // Nb of column used to render the video (is < to the total nb of column in frame)
    float   frame_rate_in_hz;
};
extern FRATE_t g_FRATE[];
extern int g_FRATE_len;

struct SAMPLE_t {
    int     code;        // Nb of column used to render the video (is < to the total nb of column in frame)
    int   sampling_struct;
    int     bit_depth;
};
extern SAMPLE_t g_SAMPLE[];
extern int g_SAMPLE_len;

enum REF_FOR_TIMESTAMP {
    REFT_NOT_LOCKED     = 0b00,
    REFT_RESERVED       = 0b01,
    REFT_LOCKED_UTC     = 0b10,
    REFT_LOCKED_PRIVATE = 0b11,
};

enum CLOCK_FREQ {
    CF_NO_TIMESTAMP        = 0b000,
    CF_27_MHZ              = 0b001,
    CF_148_5_MHZ           = 0b010,
    CF_148_5_PER_1_001_MHZ = 0b011,
    CF_297_MHZ             = 0b100,
    CF_297_PER_1_001_MHZ   = 0b101,
    CF_RESERVED            = 0b110,
};
#define GET_CLOCK_FREQUENCY(cf) ((cf < (int)CLOCK_FREQ::CF_RESERVED) ? static_cast<CLOCK_FREQ>(cf) : CLOCK_FREQ::CF_RESERVED)


class CHBRMPFrame
{
public:
    unsigned char* _frame;

    int _ext;
    int _f;
    int _vsid;
    int _frcount;
    REF_FOR_TIMESTAMP _r;
    int _s;
    int _fec;
    CLOCK_FREQ _cf;
    int _map;
    int _frm;
    int _frate;
    int _sample;
    unsigned int _timestamp;
    static float _timestampbase;
    int _framelen;
    int _headerlen;
    int _payloadlen;

public:
    CHBRMPFrame();
    CHBRMPFrame(const unsigned char* frame, int len);
    ~CHBRMPFrame();

public:
    void setBuffer(const unsigned char* frame, int len) {
        _frame = (unsigned char*)frame; 
        _framelen = len;
    };
    void dumpHeader();
    void readHeader() { 
        extractData(); 
    };
    void dumpPayload(int size = 10);
    void dumpHeader10bits(int size = 10);

    void initFixedHBRMPValuesFromProfile(CSMPTPProfile* profile);
    void writeHeader(int frcount, unsigned int timestamp);

    // Accessors
    unsigned char* getPayload() {
        return _frame + _headerlen;
    };
    int getPayloadLen() { return _payloadlen; };
    int getFrameCounter() { return _frcount; };
    REF_FOR_TIMESTAMP getRefForTimestamp() { return _r; };
    void setRefForTimestamp(REF_FOR_TIMESTAMP ref) { _r = ref; };
    CLOCK_FREQ getClockFrequency() { return _cf; };
    void setClockFrequency(int cf) { _cf = GET_CLOCK_FREQUENCY(cf); };
    int getVideoSourceMapFormat() { return _map; };
    int getVideoSourceFrmFormat() { return _frm; };
    int getVideoSourceRateFormat() { return _frate; };
    int getVideoSourceSampleFormat() { return _sample; };
    unsigned int getTimestamp() { return _timestamp; };

private:
    void extractData();
};

#endif //_HBRMPFRAME_H
