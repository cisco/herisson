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

#ifndef YUV_H
#define YUV_H

#ifdef _WIN32

#ifdef VMILIBRARY_EXPORTS
#pragma once
#define VMILIBRARY_API_YUV __declspec(dllexport)
#else
#pragma once
#define VMILIBRARY_API_YUV __declspec(dllimport)
#endif

#else
#define VMILIBRARY_API_YUV 
#endif

/* 
 * YUV to RGB conversion
 * caller must allocate correct memory !!!! 
 *  -- the out buffer must be of size npixels * 3
 * LUT initialization must be done by user
 */
 
 
/* used for LUT versions */
VMILIBRARY_API_YUV void initYUVConversion ();

/* yuv422 interleaved -> RGB */
VMILIBRARY_API_YUV void YCbCr2RGB(char* out, const char* in, int npixels);
VMILIBRARY_API_YUV void YCbCr2RGBA(char* out, const char* in, int npixels);
VMILIBRARY_API_YUV void RGB2YUV422_(char* out, const char* in, int npixels);
VMILIBRARY_API_YUV void RGB2YUV444_(char* out, const char* in, int npixels);

/* yuv420 planar -> RGB */
VMILIBRARY_API_YUV void yuv420ToRGB(char* out, const char* in, int w, int h);


/* cover for YCbCr2RGB with minimal sanity checking to be user friendly */
VMILIBRARY_API_YUV int convertYCbCrToRGB (char* out, const char* in, int npixels);


VMILIBRARY_API_YUV void
yuvPromoteImage (char* src, int srcW, int srcH, char* dest, int destW, int destH);

#endif /* YUV_H */

