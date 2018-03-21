
/* 
 * ====================================================================================
 * File:   yuvloader.c
 * Author: aviva
 *
 * Created on March 20, 2016, 2:24 PM
 * 
 * YCbCr2RGB() maps interleaved YCbCr to RGB (4 bytes -> 2 pixels)
 * conversions are based on standard Y'UV444 to RGB888 conversion formulas.
 * 
 * ALL implementations tested show some (small) overflow in pure white shiny areas.
 * 
 * ================================================================
 * IMPORTANT!
 * 
 * Memory allocation must be done by user -- the out buffer must be of size npixels * 3
 * LUT initialization must be done by user
 * 
 * internal conversion routines YCbCr2RGB() do not ANY checking
 * ===================================================================
 */
#include <stdio.h>
#include "yuv.h"
#define CLIP_UCHAR(X)  ((X) > 255 ? 255 : (X) < 0 ? 0 : (X))
#define CLIP_CHAR(X)  ((X) > 127 ? 127 : (X) < -128 ? -128 : (X))


void initYUVConversion () {
}


/* 
 * yuv422 interleaved -> RGB directly
 * 
 * Y'=0.299* R+0.587* G+0.114* B
 * U=-0.147* R-0.289* G+0.436* B
 * V=0.615* R-0.515* G-0.100* B
 * 
 * 
 */
void
YCbCr2RGB(char* out, const char* in, int npixels)
{
    int r, g, b;
    char u, v, y1, y2;
    
    char* yuv = (char*)in;
    char* rgb = (char*)out;
    char* end = rgb + 3*npixels;
    
    while (rgb < end) {

        /* extract yuv for 2 pixels */
        u =  *yuv++;
        y1 = *yuv++;
        v =  *yuv++;
        y2 = *yuv++;
        
        u -= 128;
        v -= 128;
        
        /* --- first RGB pixel --- */

        /* yuv444 -> rgb 888 */
        r = y1 + v + (v >> 2) + (v >> 3) + (v >> 5);
        g = y1 - ((u>>2) + (u>>4) + (u>>5)) - ((v>>1) + (v >> 3) + (v >> 4) + (v >> 5));
        b = y1 + u + (u >> 1) + (u >> 2) + (u >> 6);

        *rgb++ = (r);
        *rgb++ = (g);
        *rgb++ = (b);

        /* --- second RGB pixel --- */
        r = y2 + v + (v >> 2) + (v >> 3) + (v >> 5);
        g = y2 - ((u>>2) + (u>>4) + (u>>5)) - ((v>>1) + (v >> 3) + (v >> 4) + (v >> 5));
        b = y2 + u + (u >> 1) + (u >> 2) + (u >> 6);  

        *rgb++ = (r);
        *rgb++ = (g);
        *rgb++ = (b);
    }
}

void
YCbCr2RGBA(char* out, const char* in, int npixels)
{
    int r, g, b;
    char u, v, y1, y2;

    char* yuv = (char*)in;
    char* rgb = out;
    char* end = rgb + 4 * npixels;

    while (rgb < end) {

        /* extract yuv for 2 pixels */
        u = *yuv++;
        y1 = *yuv++;
        v = *yuv++;
        y2 = *yuv++;

        u -= 128;
        v -= 128;

        /* --- first RGB pixel --- */

        /* yuv444 -> rgb 888 */
        r = y1 + v + (v >> 2) + (v >> 3) + (v >> 5);
        g = y1 - ((u >> 2) + (u >> 4) + (u >> 5)) - ((v >> 1) + (v >> 3) + (v >> 4) + (v >> 5));
        b = y1 + u + (u >> 1) + (u >> 2) + (u >> 6);

        *rgb++ = r;
        *rgb++ = g;
        *rgb++ = b;
        *rgb++ = (char)255;

        if (rgb < end) {
            /* --- second RGB pixel --- */
            r = y2 + v + (v >> 2) + (v >> 3) + (v >> 5);
            g = y2 - ((u >> 2) + (u >> 4) + (u >> 5)) - ((v >> 1) + (v >> 3) + (v >> 4) + (v >> 5));
            b = y2 + u + (u >> 1) + (u >> 2) + (u >> 6);

            *rgb++ = r;
            *rgb++ = g;
            *rgb++ = b;
            *rgb++ = (char)255;
        }
    }
}
void
YCbCr2BGRA(char* out, const char* in, int npixels)
{
    int r, g, b;
    char u, v, y1, y2;

    char* yuv = (char*)in;
    char* rgb = out;
    char* end = rgb + 4 * npixels;

    while (rgb < end) {

        /* extract yuv for 2 pixels */
        u = *yuv++;
        y1 = *yuv++;
        v = *yuv++;
        y2 = *yuv++;

        u -= 128;
        v -= 128;

        /* --- first RGB pixel --- */

        /* yuv444 -> rgb 888 */
        r = y1 + v + (v >> 2) + (v >> 3) + (v >> 5);
        g = y1 - ((u >> 2) + (u >> 4) + (u >> 5)) - ((v >> 1) + (v >> 3) + (v >> 4) + (v >> 5));
        b = y1 + u + (u >> 1) + (u >> 2) + (u >> 6);

        *rgb++ = b;
        *rgb++ = g;
        *rgb++ = r;
        *rgb++ = (char)255;

        if (rgb < end) {
            /* --- second RGB pixel --- */
            r = y2 + v + (v >> 2) + (v >> 3) + (v >> 5);
            g = y2 - ((u >> 2) + (u >> 4) + (u >> 5)) - ((v >> 1) + (v >> 3) + (v >> 4) + (v >> 5));
            b = y2 + u + (u >> 1) + (u >> 2) + (u >> 6);

            *rgb++ = b;
            *rgb++ = g;
            *rgb++ = r;
            *rgb++ = (char)255;
        }
    }
}
// RGB -> YCbCr
#define CRGB2Y(R, G, B) CLIP((19595 * R + 38470 * G + 7471 * B ) >> 16)
#define CRGB2Cb(R, G, B) CLIP((36962 * (B - CLIP((19595 * R + 38470 * G + 7471 * B ) >> 16) ) >> 16) + 128)
#define CRGB2Cr(R, G, B) CLIP((46727 * (R - CLIP((19595 * R + 38470 * G + 7471 * B ) >> 16)

#define ITUR_Y(r, g, b) (0.299 * r + 0.587*g + 0.114*b)
#define ITUR_Cb(r, g, b) (-0.169 * r -0.331*g + 0.499*b + 128)
#define ITUR_Cr(r, g, b) (0.499 * r - 0.418*g + 0.0813*b + 128)

#define NTSC_Y(r, g, b) (0.299 * r + 0.587*g + 0.114*b)
#define NTSC_Cb(r, g, b) (-0.147 * r -0.289*g + 0.436*b)
#define NTSC_Cr(r, g, b) (0.615 * r - 0.515*g - 0.*b )

void
RGB2YUV422_(char* out, const char* in, int npixels)
{
    char r, g, b;
    int u, v, y0, y1;
    
    char* yuv = out;
    char* rgb = (char*)in;
    char* end = rgb + 3*npixels;
    
    float yy, uu, vv;
    
    while (rgb < end) {
        
        r = *rgb++;
        g = *rgb++;
        b = *rgb++;

        yy = (float) ((0.257 * r) + (0.504 * g) + (0.098 * b) + 16);
        y0 = (int) (yy > 255 ? 255 : yy < 0 ? 0 : yy);
         
        r = *rgb++;
        g = *rgb++;
        b = *rgb++;

        yy = (float) ((0.257 * r) + (0.504 * g) + (0.098 * b) + 16);
        vv = (float) ((0.439 * r) - (0.368 * g) - (0.071 * b) + 128);
        uu = (float)-((0.148 * r) - (0.291 * g) + (0.439 * b) + 128);
        
        u = (int) (uu > 255 ? 255 : uu < 0 ? 0 : uu);
        v = (int) (vv > 255 ? 255 : vv < 0 ? 0 : vv);
        y1 = (int) (yy > 255 ? 255 : yy < 0 ? 0 : yy);
        
        *yuv++ = u;
        *yuv++ = y0;
        *yuv++ = v;
        *yuv++ = y1;
    }    
}



/* 
 * yuv420 planar -> RGB
 */
void
yuv420ToRGB(char* out, const char* in, int w, int h)
{
    int x, rows;
    int r, g, b;
    int y, u, v;
    
    char* yuv = (char*)in;
    char* rgb = out;
    
    int npixels = w * h;
    
    int step = w/2;
        
    /* unoptimized
     */
    for (rows = 0; rows < h; rows++)
     for (x = 0; x < w; x++)  {
         
        /* extract y,u,v */ 
        y = yuv[rows * w + x];        
        u = yuv[ (int)(npixels + (rows/2)*step  + x/2) ];
        v = yuv[ (int)(npixels*1.25 + (rows/2)*step + x/2)];
                   
        /* yuv -> rgb */
        r  = (int) (y + (u - 128) *  1.40200);
        g  = (int) (y + (v - 128) * -0.34414 + (u - 128) * -0.71414);
        b  = (int) (y + (v - 128) *  1.77200);
                
        /* write 3 RGB bytes to output */
        *rgb++ = (r <= 0) ? 0 : (r >= 255) ? 255 : r;
        *rgb++ = (g <= 0) ? 0 : (g >= 255) ? 255 : g;
        *rgb++ = (b <= 0) ? 0 : (b >= 255) ? 255 : b;       
    }
}


void
RGB2YUV444_(char* out, const char* in, int npixels)
{
    char r, g, b;
    int u, v, y;
    
    char* yuv = out;
    char* rgb = (char*)in;
    char* end = rgb + 3*npixels;
    
    while (rgb < end) {
        
        r = *rgb++;
        g = *rgb++;
        b = *rgb++;
        
        double yy = r * .299000 + g * .587000 + b* .114000;
        double uu = 0.492 * (b-yy); //r * -.168736 + g * -.331264 + b * .500000 + 128;
        double vv = 0.877 * (r-yy); //r * .500000 + g * -.418688 + b * -.081312 + 128;
        
        yy = (0.257 * r) + (0.504 * g) + (0.098 * b) + 16;
        vv = (0.439 * r) - (0.368 * g) - (0.071 * b) + 128;
        uu = -(0.148 * r) - (0.291 * g) + (0.439 * b) + 128;
        
        y = (int)NTSC_Y(r, g, b);
        u = (int)NTSC_Cb(r, g, b);
        v = (int)NTSC_Cr(r, g, b) ;
        
        u = (int)(uu > 255 ? 255 : uu < 0 ? 0 : uu);
        v = (int)(vv > 255 ? 255 : vv < 0 ? 0 : vv);
        y = (int)(yy > 255 ? 255 : yy < 0 ? 0 : yy);
        
        
        
        *yuv++ = u;
        *yuv++ = y;
        *yuv++ = v;
    }    
}


int
convertYCbCrToRGB (char* out, const char* in, int npixels) {
    if (! out || !in )
        return -1;
    
     YCbCr2RGB (out, in, npixels);
     return 0;
}

void
yuvPromoteImage (char* src, int srcW, int srcH, char* dest, int destW, int destH) {
    
    int marginLeft = (destW - srcW)/ 2;
    int marginTop = (destH - srcH) / 2;
    
    if (marginLeft < 0)
        marginLeft = 0;
    if (marginTop < 0)
        marginTop = 0;
    
    char* p = dest;
    char* q = src;
    int h, w;
    
    /* center src inside dest with black background */
    
    for (h=0; h < marginTop; h++) {
        for (int w = 0; w < destW; w+=2) {
               *p++ = (char)128;
               *p++ = 0;
               *p++ = (char)128;
               *p++ = 0;
        }
    }
    for (;h < marginTop + srcH; h++) {
        for (w = 0; w < marginLeft; w+=2) {
               *p++ = (char)128;
               *p++ = 0;
               *p++ = (char)128;
               *p++ = 0;               
        }
        for (; w < marginLeft+srcW; w+=2) {
               *p++ = *q++;
               *p++ = *q++;
               *p++ = *q++;
               *p++ = *q++;               
        }
        for (; w < destW; w+=2) {
               *p++ = (char)128;
               *p++ = 0;
               *p++ = (char)128;
               *p++ = 0;               
        }
    }   
    for (; h < destH; h++) {
        for (int w = 0; w < destW; w+=2) {
               *p++ = (char)128;
               *p++ = 0;
               *p++ = (char)128;
               *p++ = 0;               
        }
    }
}