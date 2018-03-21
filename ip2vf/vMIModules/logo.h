/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   logo.h
 * Author: aviva vaknin
 *
 * Created on June 29, 2016, 1:21 PM
 */

#ifndef LOGO_H
#define LOGO_H

#include "pngtools.h"

class Logo {
public:
    Logo (char* file, int x0, int y0);
    Logo (char* file);
    ~Logo ();
    
    unsigned char* overlayLogo (unsigned char* image, int w, int h, int bpp, int x0, int y0);
    

    
    void setDefaultPosition (int imgWidth, int imgHeight); 
    inline int getDefaultX () { return _x0; }
    inline int getDefaultY () { return _y0; } 
    
    inline const char* getYUVbytes () {
        return _yuvbytes;
    }    
    inline const char* getRGBbytes () {
        return _rgbbytes;
    }    
    inline int getNumPixels () {
        return _npixels;
    }
    inline int getWidth () {
        return (_png ? _png->width : 0);
    }    
    inline int getHeight () {
        return (_png ? _png->height : 0);
    } 
    
#   define BORDER 50
    
private:
    tools_png_t* _png;

    char*   _file;
    char*   _rgbbytes;
    char*   _yuvbytes;
    char*   _alpha;
    int     _x0;
    int     _y0;    
    int     _npixels;
    
    int loadlogo (char* file);
    int convertToYUV ();
    int extractChannels ();

};


#endif /* LOGO_H */

