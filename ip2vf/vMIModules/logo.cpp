/*
 * * File:   logo.h
 * Author: aviva vaknin
 *
 * Created on June 29, 2016, 1:21 PM
 * 
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

#include <malloc.h>
#include "logo.h"
#include "tools.h"
#include "pngtools.h"
#include "yuv.h"
#include <fstream>

//#define USE_VIPS

#ifdef USE_VIPS
#include "vips/vips8"
#endif
using namespace std;


Logo::Logo (char* file) {
    _png = NULL;
    loadlogo (file);
    _x0 = -1;
    _y0 = -1;
    _file = file;
}

Logo::Logo (char* file, int left, int top) {
    _png = NULL;
    loadlogo (file);
    _x0 = left;
    _y0 = top;
    _file = file;
}

Logo::~Logo () {
    if (_rgbbytes)
        free (_rgbbytes);    
    if (_yuvbytes)
        free (_yuvbytes);
    if (_alpha)
        free (_alpha);
    if (_png)
        tools::destroyPNGImage(_png);
    
#   ifdef USE_VIPS
    vips_shutdown();
#   endif
}

int  Logo::loadlogo(char* file) {
#   ifdef USE_VIPS
    if (VIPS_INIT ("./dist/Debug/GNU-Linux/logoinsertor")) {
        printf( "error: failed to initialize vips" );
    }
    VipsImage* vimg = vips_image_new_from_file  (file, NULL);    
    _png->pixels = (unsigned char*)vips_image_get_data(vimg);        
    _png->width = vimg->Xsize;
    _png->height = vimg->Ysize;
    _png->bpp = vimg->Bands;
    _png->size = vimg->Xsize * vimg->Ysize * vimg->Bands;
    _npixels = _png->width * _png->height;
#   else    
    _png = tools::loadPNGImage(file);
    if (_png == NULL)
        return 0;
    _npixels = _png->height * _png->width;
#   endif
    
    _rgbbytes = (char*) malloc (_npixels*3);
    _yuvbytes = (char*) malloc (_npixels*2);
    _alpha = (char*) malloc (_npixels);
    extractChannels ();
    convertToYUV ();

    return 0;
}

int Logo::convertToYUV () {
    if (_png == NULL)
        return 0;
    /* this will be yuv specific, decision here */
    RGB2YUV422_( _yuvbytes, _rgbbytes, _npixels);

    return 0;
}

int Logo::extractChannels () {
    
    if (_png == NULL)
        return 0;
    unsigned char* pngbytes = _png->pixels;
    unsigned char* end = pngbytes + _npixels * 4; //_png->bpp;
    
    char* rgb = this->_rgbbytes;
    char* alpha = this->_alpha;
    
    while (pngbytes < end) {
        *rgb++ = *pngbytes++;
        *rgb++ = *pngbytes++;
        *rgb++ = *pngbytes++;
        *alpha++ = *pngbytes++;
    }
    return 0;
}

void Logo::setDefaultPosition (int imgWidth, int imgHeight) {
    if (_png == NULL)
        return;
    /* default is upper right corner with border = 50*/
    _x0 = imgWidth > (int)(_png->width + BORDER) ? BORDER : imgWidth - _png->width;
    if (_x0 < 0) 
        _x0 = 0;
    
    _y0 = imgHeight >(int)(_png->height + BORDER) ? BORDER : imgHeight - _png->height;
    if (_y0 < 0) 
        _y0 = 0;
}


unsigned char* Logo::overlayLogo (unsigned char* image, int w, int h, int bpp, int x, int y) {
    
    if (_png == NULL)
        return NULL;
    if ((x == -1 || y==-1)  && _x0 == -1)
        setDefaultPosition(w, h);
    if (x == -1)
        x = _x0;
    if (y==-1)
        y = _y0;
    
    //unsigned char* a = _alpha;
    
    /* uyvy 422 interlace = none  8 bit */
    unsigned char* l = (unsigned char*)this->_yuvbytes;
    int n = _png->width * 2 ;
    for (int j=0; j < (int)_png->height; j++) {
        unsigned char* p = image + (y+j)*w*bpp + x * bpp;
        for (int i = 0; i < n; i++) {
            *p++ = *l; 
            ++l;
         }
    }
    
    return image;
}

