/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   yuvLoader.h
 * Author: aviva
 *
 * Created on March 21, 2016, 2:35 PM
 */

#ifndef YUVLOADER_H
#define YUVLOADER_H



/* 
 * YUV to RGB conversion
 * caller must allocate correct memory !!!! 
 *  -- the out buffer must be of size npixels * 3
 * LUT initialization must be done by user
 */
 
class YUVConvert {
    
 
public:
    YUVConvert ();
    YUVConvert (int w, int h);
    ~YUVConvert ();

    /* yuv422 non-interleaved -> RGB  and back
     */
    void YCbCr2RGB(char* out, const char* in, int npixels);
    void RGB24toUYVY422 (char* yuv, char* rgb);
   
    
    /* incomplete conversions */
    void RGB2YUV422_(char* out, const char* in, int npixels);
    void yuv420ToRGB(char* out, const char* in, int w, int h);

    /* cover for YCbCr2RGB with minimal sanity checking to be user friendly */
    int convertYCbCrToRGB (char* out, const char* in, int npixels);
    
    
private:
    int                 _w;
    int                 _h;
    int                 _rgbStride[3] = { 3, 0, 0};
    int                 _yuvStride[3] = { 2, 0, 0};
    
};


#endif /* YUVLOADER_H */

