/*
 * tr03frameparser.cpp
 *
 *  Created on: Oct 31, 2017
 *      Author: mhawari
 */
#include "tr03frameparser.h"
#include "log.h"

CTR03FrameParser::CTR03FrameParser(
            unsigned int widthInPixels,
            unsigned int heightInPixels,
            unsigned int bitsPerPixel,
            unsigned int frameBufferLength
            ) : _widthInPixels(widthInPixels)
    , _heightInPixels(heightInPixels)
    , _bitsPerPixel(bitsPerPixel)
    , _frameBufferLength(frameBufferLength) {
        auto scanLineBits = widthInPixels*bitsPerPixel;
        if ((scanLineBits % 8) != 0) {
            THROW_CRITICAL_EXCEPTION("scanlines with unaligned bits not supported yet");
        }
        _bytesPerScanLine = scanLineBits / 8;
        auto bitsInFrame = (heightInPixels * scanLineBits);
        auto bytesPerFrame = bitsInFrame / 8;
        if (bytesPerFrame > frameBufferLength) {
            LOG_ERROR("Frame is too small to contain data: vMI buffer length=%d, received length=%d", frameBufferLength, bytesPerFrame);
            THROW_CRITICAL_EXCEPTION("Frame is too small to contain data");
        }
        this->_frameBuffer.reset(new unsigned char[frameBufferLength]);
    };

void CTR03FrameParser::copyFrameContents(void *destinationAddress){
        std::memcpy(destinationAddress, _frameBuffer.get(), _frameBufferLength);

    }

void CTR03FrameParser::addRtpFrame(CRTPFrame* rtpFrame, const std::function<void() > onFrameParsed) {
     unsigned char *rtpData = rtpFrame->getData();
     auto GLOBAL_HEADER_LENGTH = 14;
     auto SUB_HEADER_LENGTH = 6;
     //utility function to iterate over all the headers:
     auto forEachSubHeader = [&](std::function<bool (unsigned char* subheader) > callback) {
         if (_discardThisFrame) {
             return;
         }
         bool continueParsingAfterThisRecord = true;
         for (unsigned int scanLineRecordIndex = 0;
                 continueParsingAfterThisRecord;
                 scanLineRecordIndex++) {

             //the offset of the record header in the RTP packet (rfc4175)
             unsigned int subHeaderOffset = GLOBAL_HEADER_LENGTH + (SUB_HEADER_LENGTH * scanLineRecordIndex);
             //protect against overflow
             if (subHeaderOffset + SUB_HEADER_LENGTH > rtpFrame->getSize()) {
                 LOG_ERROR("Corrupt header overflow");
                 discardThisFrame();
                 break;
             }
             unsigned char* subHeader = rtpData + subHeaderOffset;
             continueParsingAfterThisRecord = ((subHeader[4]&0x80) != 0);
             //which scan line do these pixels belong on
             if (!callback(subHeader)) {
                 break;
             }

         }
         return;
     };
     bool lastLineInSecondField = true;
     if (!_discardThisFrame) {
         //holds the data of the original RTP frame to be parsed according to: https://tools.ietf.org/html/rfc4175

         unsigned char* lastSubHeader = 0;
         forEachSubHeader([&](unsigned char*subHeader) {
             //find the address of the last subheader in the packet
             lastSubHeader = subHeader;
             return true;
         });


         //the place where the pixel data begins is right after the last header:
         unsigned char* pixelData = lastSubHeader + SUB_HEADER_LENGTH;
         //actually parse the data
         forEachSubHeader([&](unsigned char* subHeader) {
             //the length in bytes of this record's data
             unsigned int recordLength = PARSE_BIG_ENDIAN_UINT16(subHeader);
             //which part of the interlaced frame these pixels belong on:
             unsigned int interlaceField = ((subHeader[2]&0x80) >> 7);
             //which scan line do these pixels belong on
             unsigned int scanLineNumber = 0x7FFF & PARSE_BIG_ENDIAN_UINT16(subHeader + 2);
             //which scan line do these pixels belong on
             unsigned int offset = 0x7FFF & PARSE_BIG_ENDIAN_UINT16(subHeader + 4);
             if (interlaceField && !_interlaced) {
                 LOG_ERROR("Interlaced format detected");
                 _interlaced = true;
             }
             scanLineNumber = scanLineNumber * (1 + _interlaced) + interlaceField;
             //prevent the data from overflowing past the rtp frame boundries
             if ((pixelData + recordLength - rtpData) > rtpFrame->getSize()) {
                 LOG_ERROR("tr03 scanline part overflow");
                         discardThisFrame();
                         //do not continue iterating
                 return false;
             }
             //find out where to put the data in the accumulated raw frame:
             auto destinationAddress = this->getPixelGroupAddress(offset, scanLineNumber);
             //prevent segfaults because of bad data:
             if (
                 //bad destination address:
                 destinationAddress == 0
                     ||
                     //overflow
                     (destinationAddress - _frameBuffer.get() + recordLength>this->_frameBufferLength)
                     ) {
                 LOG_ERROR("scanline address overflow");
                         discardThisFrame();
                         //do not continue iterating
                 return false;
             }
             //copy the data from the rtp packet into the frame:
             std::memcpy(destinationAddress, pixelData, recordLength);
             //advance to the next buffer
             pixelData += recordLength;
             lastLineInSecondField = !!interlaceField; // to remove warning "warning C4800: 'unsigned int': forcing value to bool 'true' or 'false' (performance warning)"
             //continue iterating
             return true;
         });
     }
     //check if this is the last packet for the frame:
     //RTP marker bit (m) signals that the frame is complete
     if ((rtpFrame->_m != 0) && (!_interlaced || lastLineInSecondField)) {
         onEndOfFrame( onFrameParsed);
     }
 }

void CTR03FrameParser::resetFrame()
{
    this->_discardThisFrame = false;
    //debug code to color the frame where the scan lines are not set:
    //this adds compute overhead, and should remain disabled
    std::memset(_frameBuffer.get(), 0x80, this->_frameBufferLength);
}
