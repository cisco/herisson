#ifndef _PNGTOOLS_H
#define _PNGTOOLS_H

typedef struct
{
    unsigned int   width;
    unsigned int   height;
    unsigned int   bpp;
    unsigned int   size;
    unsigned char* pixels;
} tools_png_t;

namespace tools
{
    tools_png_t*  loadPNGImage(const char* filename);
    void          destroyPNGImage(tools_png_t* image);
}

#endif //_PNGTOOLS_H
