/*
 * inaes67.h
 *
 *  Created on: Nov 3, 2017
 *      Author: mhawari
 */

#ifndef INAES67_H_
#define INAES67_H_

#include <pins/pins.h>
#include "tcp_basic.h"
#include "moduleconfiguration.h"


class CInAES67 : public CIn{
protected:
    UDP *_udpSock;
    int _lastSeq;
    int _lastTimestamp;
    bool _audioParametersDetected;
    int _port;
    const char* _zmqip;
    const char* _ip;
    CFrameHeaders       _headers;

public:
    CInAES67(CModuleConfiguration* pMainCfg, int nIndex);
    virtual ~CInAES67();
    int  read(CvMIFrame* frame);
    void reset();
    virtual void stop();

private:
    constexpr static unsigned int MaxAudioChannelCount = 16;
    constexpr static unsigned int MaxAudioSamplingRate = 96;
    constexpr static unsigned int AudioPCMDepth = 3;
    constexpr static unsigned int AudioFrameDuration = 20;
    constexpr static unsigned int MaxAudioFrameSize = MaxAudioChannelCount * MaxAudioSamplingRate * AudioPCMDepth * AudioFrameDuration;
#ifdef _WIN32
    static unsigned int SamplesPerMs[4];
#else
    constexpr static unsigned int SamplesPerMs[] = {0 , 0, 48, 96};
#endif //_WIN32
};




#endif /*INAES67_H_ */
