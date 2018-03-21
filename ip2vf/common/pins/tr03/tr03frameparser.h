/*
 * tr03frameparser.h
 *
 *  Created on: Oct 31, 2017
 *      Author: mhawari
 */

#ifndef COMMON_TR03FRAMEPARSER_H_
#define COMMON_TR03FRAMEPARSER_H_
#include <functional>
#include <cstring>
#include <memory>
#include "log.h"
#include "rtpframe.h"
/**
 * Builds TR03Frames from a bit-stream
 */
class CTR03FrameParser {
    //Frame will be dropped if an error is detected with the data:
    bool _discardThisFrame = false;
    bool _interlaced = false;
    const unsigned int _widthInPixels;
    const unsigned int _heightInPixels;
    const unsigned int _bitsPerPixel;
    //the size of each scanline in bytes
    unsigned int _bytesPerScanLine;
    //holds the data of the frame that is being produced
    std::shared_ptr<unsigned char> _frameBuffer;
    //the size in bytes of the frame buffer
    const unsigned int _frameBufferLength;
protected:
    void onEndOfFrame(const std::function<void()>& onFrameParsed) {
        if ((!_discardThisFrame)) {
            onFrameParsed();
        }
        //clear all frame state:
        resetFrame();
    }
public:

    CTR03FrameParser(
            unsigned int widthInPixels,
            unsigned int heightInPixels,
            unsigned int bitsPerPixel,
            unsigned int frameBufferLength
            );

    /**
     * Copies the contents of the current frame to another buffer
     * @param destinationAddress a memory address to copy the contents to
     */
    void copyFrameContents(void *destinationAddress);

    /**
     * Gets the address of a pixel within the frame buffer
     * IMPORTANT: pixel position must align to pixel group (pgroup)
     * @param x
     * @param y
     * @return Address in memory of said pixel
     */
    inline unsigned char *getPixelGroupAddress(const unsigned int &x, const unsigned &y) {
        auto bitOffsetInLine = x * this->_bitsPerPixel;
        //x value must align to whole octets:
        if ((bitOffsetInLine % 8) != 0) {
            LOG_ERROR("x value not aligned with pgroup");
            return 0;
        }
        if (y > this->_heightInPixels) {
            LOG_ERROR("y is out of frame bounds");
            return 0;
        }
        if (x > this->_widthInPixels) {
            LOG_ERROR("x is out of frame bounds");
            return 0;
        }
        return _frameBuffer.get() + _bytesPerScanLine * y + (bitOffsetInLine / 8);
    }



    /**
     * Accumulate more bytes from the bitStream, eventually building a frame in the process
     * @param rtpFrame New RTP packet to ingest and parse accumulate into pixels
     * @param onEndOfFrame callback called when the frame is ready to be parsed
     * @return true if a frame is ready to be read
     */
    void addRtpFrame(CRTPFrame* rtpFrame, const std::function<void() > onFrameParsed);

    unsigned int inline PARSE_BIG_ENDIAN_UINT16(unsigned char *buffer) {
        return ((unsigned int) (buffer[0] << 8) +(unsigned char) (buffer[1]));
    }

    /**
     * Called when an error in the input has been detected, and all further data
     * for this frame should be discarded
     */
    void inline discardThisFrame() {
        this->_discardThisFrame = true;
    }

    /**
     * start parsing a new frame
     */
    void resetFrame();


};




#endif /* COMMON_TR03FRAMEPARSER_H_ */
