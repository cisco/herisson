#ifndef _SMPTEPROFILE_H
#define _SMPTEPROFILE_H

#include <string>

using namespace std;

enum SMPTE_STANDARD_SUITE {
    SMPTE_STANDARD_SUITE_NOT_DEFINED = 0,
    SMPTE_2022_6,
    SMPTE_2110_20,
};

enum SMPTE_STANDARD {
    SMPTE_NOT_DEFINED = 0,
    SMPTE_292M       = 1, // HD_SDI
    SMPTE_372M       = 2, // Dual link
    SMPTE_425MlvlA   = 3, // 3Gb/s SDI Lvl A
    SMPTE_425MlvlBDL = 4, // 3Gb/s SDI Lvl B Dual link
    SMPTE_425MlvlBDS = 5, // 3Gb/s SDI Lvl B Dual stream
    SMPTE_435M       = 6, // 10Gb/s SDI
    SMPTE_259M       = 7, // SD SDI
};

struct SMPTEProfile {
    string name;                    // String which describe this format
    SMPTE_STANDARD  smpteStandard;  // Smpte standard
    int     map;                    // Top level structure of the data stream, as define on MAP parameter on HBRMP
    int     nActivePixelsNb;        // Nb of column used to render the video (is < to the total nb of column in frame)
    int     nActiveLineNb;          // Nb of line used to render the video (is < to the total nb of line in frame)
    int     nNbPixelsPerScanline;   // Nb of pixels per scanline
    int     nScanLinesNb;           // Total nb of scanlines on the frame (is > to the number of lines for the video)
    int     nComponentsNb;          // Nb of components for a pixel
    int     nBitDepth;              // Size in bits for a component
    int     ActiveField1_Begin;     // Active Video Field 1 range. Line numbers starting at 1.
    int     ActiveField1_End;       // SMPTE 125:2013, 296:2012, 274M-2008
    int     ActiveField2_Begin;     // Active Video Field 2 range. Line numbers starting at 1.
    int     ActiveField2_End;       // Set to 0 for progressive formats.
    float   fRate;
    bool    interlaced;         
    bool    multiplexed;
};
typedef struct SMPTEProfile SMPTEProfile;

// Forward declaration
class CFrameHeaders;
class CHBRMPFrame;

class CSMPTPProfile
{
private:
    SMPTEProfile  _smpteProfile;

    int _framelen;          // Frame length without padding: i.e. real Media Octets per frame
    int _transportframelen; // Transport frame len, i.e. 2x_framelen for SMPTE425M lvl B
    int _nScanlineSize;     // Size of a scanline
    int _offsetY;           // line offset in frame to find active video picture (line number starting at 0)
    int _offsetX;           // column offset in a scanline frame to find active scanline picture
    int _smpteframeCode;    // [vDCM request] add FRAME parameter on vMI headers

public:
    CSMPTPProfile();
    ~CSMPTPProfile();

public:
    int     getActiveWidth()    { return _smpteProfile.nActivePixelsNb;         };
    int     getActiveHeight()   { return _smpteProfile.nActiveLineNb;           };
    int     getFrameDepth()     { return _smpteProfile.nBitDepth;               };
    int     getFrameSize()      { return _framelen;                             };
    int     getScanlineWidth()  { return _smpteProfile.nNbPixelsPerScanline;    };
    int     getScanlinesNb()    { return _smpteProfile.nScanLinesNb;            };
    int     getScanlineSize()   { return _nScanlineSize;                        };
    int     getComponentsNb()   { return _smpteProfile.nComponentsNb;           };
    int     getComponentsDepth(){ return _smpteProfile.nBitDepth;               };
    int     getXOffset()        { return _offsetX;                              };
    int     getYOffsetF1()      { return _smpteProfile.ActiveField1_Begin-1;    };
    int     getYOffsetF2()      { return _smpteProfile.ActiveField2_Begin-1;    };
    float   getFramerate()      { return _smpteProfile.fRate;                   };
    bool    isMultiplexed()     { return _smpteProfile.multiplexed;             };
    bool    isInterlaced()      { return _smpteProfile.interlaced;              };
    int     getFRAMEcode()      { return _smpteframeCode;                       };
    int     getTransportFrameSize() { return _transportframelen; };

    string         getProfileName() { return _smpteProfile.name; };
    SMPTE_STANDARD getStandard(){ return _smpteProfile.smpteStandard;           };
    SMPTE_STANDARD setProfile(const char* format);
    SMPTE_STANDARD initProfileFromHBRMP(CHBRMPFrame* hbrmp);
    SMPTE_STANDARD initProfileFromIP2VF(CFrameHeaders* headers);

    void    dumpProfile();

private:
    bool _findProfile(const char* format);
};

#endif //_SMPTEPROFILE_H
