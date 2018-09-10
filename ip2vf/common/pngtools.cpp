#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#else
#include <unistd.h>         //STDIN_FILENO
#endif
#include <png.h>

#include "log.h"
#include "pngtools.h"

using namespace std;


int tools_getPNGbppInfo(int ColourType)
{
    int ret;
    switch (ColourType)
    {
    case PNG_COLOR_TYPE_GRAY:
        ret = 1;
        break;
    case PNG_COLOR_TYPE_GRAY_ALPHA:
        ret = 2;
        break;
    case PNG_COLOR_TYPE_RGB:
        ret = 3;
        break;
    case PNG_COLOR_TYPE_RGB_ALPHA:
        ret = 4;
        break;
    default:
        ret = -1;
    };
    return ret;
};

tools_png_t* tools::loadPNGImage(const char* filename) {

#ifdef WIN32
#else
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    png_bytep *row_pointers = NULL;
    int bitDepth, colourType;
    unsigned int width, height;

    FILE *pngFile = fopen(filename, "rb");
    if (pngFile)
    {
        png_byte sig[8];
        int size = fread(&sig, 8, sizeof(png_byte), pngFile);
        rewind(pngFile);//so when we init io it won't bitch
        if (!png_check_sig(sig, 8)) {
            LOG_ERROR("ERROR png_check_sig() failed! \n");
            fclose(pngFile);
            return NULL;
        }

        png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (!png_ptr) {
            LOG_ERROR("ERROR png_create_read_struct() failed! \n");
            fclose(pngFile);
            return NULL;
        }

        if (setjmp(png_jmpbuf(png_ptr))) {
            LOG_ERROR("ERROR setjmp() failed! \n");
            fclose(pngFile);
            return NULL;
        }

        info_ptr = png_create_info_struct(png_ptr);
        if (!info_ptr) {
            LOG_ERROR("ERROR png_create_info_struct() failed! \n");
            fclose(pngFile);
            return NULL;
        }

        png_init_io(png_ptr, pngFile);

        png_read_info(png_ptr, info_ptr);

        bitDepth = png_get_bit_depth(png_ptr, info_ptr);

        colourType = png_get_color_type(png_ptr, info_ptr);
        if (colourType == PNG_COLOR_TYPE_PALETTE)
            png_set_palette_to_rgb(png_ptr);

        //if(colourType == PNG_COLOR_TYPE_GRAY && bitDepth < 8)
        //	png_set_gray_1_2_4_to_8(png_ptr);

        if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
            png_set_tRNS_to_alpha(png_ptr);

        if (bitDepth == 16)
            png_set_strip_16(png_ptr);
        else if (bitDepth < 8)
            png_set_packing(png_ptr);

        png_read_update_info(png_ptr, info_ptr);

        png_get_IHDR(png_ptr, info_ptr, (png_uint_32*)&width, (png_uint_32*)&height,
            &bitDepth, &colourType, NULL, NULL, NULL);

        int components = tools_getPNGbppInfo(colourType);
        if (components == -1)
        {
            if (png_ptr)
                png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
            LOG_ERROR("ERROR format not supported ! \n");
            fclose(pngFile);
            return NULL;
        }

        tools_png_t* png = new tools_png_t();
        png->width = width;
        png->height = height;
        png->bpp = components;
        png->size = width * height * components;
        png->pixels = new unsigned char[png->size];

        row_pointers = (png_bytep *)malloc(sizeof(png_bytep) * height);

        for (int i = 0; i < (int)height; ++i)
            row_pointers[i] = (png_bytep)(png->pixels + (i * width * components));

        png_read_image(png_ptr, row_pointers);
        png_read_end(png_ptr, NULL);

        LOG("components=%d, w=%d, h=%d\n", components, width, height);

        free(row_pointers);
        fclose(pngFile);

        return png;
    }
#endif
    return NULL;
}

void tools::destroyPNGImage(tools_png_t* image) {
    if (image != NULL)
    {
        delete[] image->pixels;
        delete image;
        image = NULL;
    }
}
