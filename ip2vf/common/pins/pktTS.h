#ifndef _PktTS_H
#define _PktTS_H

typedef long long Time;

typedef struct pktTSaggr pktTSaggr;
struct pktTSaggr {                  /* aggregate information about a frame */
    int    handle;                  /* libvMI_pin_handle inputHandle */
    int    pinId;                   /*  */
    unsigned int identifier[2];     /* of first[0] and last[1] packet */
    Time timestamp[2];              /* of first[0] and last[1] packet */

    int     count;                  /* of packets */
    double  mean;                   /* IPG accumulated mean */
    double  m2;                     /* aggregate squared distance from mean */

    Time min;                       /* IPG */
    Time max;                       /* IPG */
};

class PktTS: public UDP {
    PinConfiguration* _pConfig;
    int _pktTS;
    int _mHandle;
    int _pinId;
    int _dbg;

    int _state;
    pktTSaggr* _aggr;

public:
    PktTS(PinConfiguration*);
    virtual ~PktTS();

    virtual int openSocket(const char*, const char*, int, bool, const char* = 0);
    virtual int readSocket(char*, int*);
    virtual void pktTSctl(int, unsigned int = 0, Time = 0);

    PinConfiguration* getConfiguration(void){
        return _pConfig;
    };
};

PktTS* pktTSconstruct(PinConfiguration*);

extern int                  g_isProbe;
extern CQueue<pktTSaggr>    g_pktTSq;

#endif /* _PktTS_H */
