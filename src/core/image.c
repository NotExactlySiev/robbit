#include <string.h>
#include <stdio.h>
#include "common.h"
#include "image.h"

// all the different formats
// Exact Indexed Texture
// Exact ARchive
// Exact Model
// a clut is just an array of 256 colors
// eit_dimensions(void *data, u16 *w, u16 *h)

// img_size()
// do I even need to convert the image?
// caller must free for all these functions

Image* image_make(uint8_t* data, uint16_t w, uint16_t h, uint16_t* clut)
{
    Image* img;

    img = malloc(sizeof(Image) + w*h*sizeof(Color));
    img->max = (1 << 8) - 1;
    img->w = w;
    img->h = h;

    uint16_t col;

    for (int i = 0; i < w*h; i++) {
        col = clut[data[i]];
        // 15 bit to 24 bit
        img->bitmap[i].r = (0x1f & (col >> 0)) << 3;
        img->bitmap[i].g = (0x1f & (col >> 5)) << 3;
        img->bitmap[i].b = (0x1f & (col >> 10)) << 3;
    }

    return img;
}

Image* image_make_empty(uint16_t w, uint16_t h)
{
    Image* img;
    img = malloc(sizeof(Image) + w*h*sizeof(Color));
    //img->max = 1 << 8 - 1;
    img->w = w;
    img->h = h;
    memset(img->bitmap, 255, w*h*sizeof(Color));

    return img;
}

// from TIM?
Image* image_make_from_data(uint8_t* data, uint16_t* clut)
{
    uint16_t w, h;
    data += 4;
    READ_BE16(w, data);
    READ_BE16(h, data);

    return image_make(data, w, h, clut);
}

Color image_15_to_24(uint16_t col)
{
    Color ret = {
        .r = (0x1f & (col >> 0)) << 3,
        .g = (0x1f & (col >> 5)) << 3,
        .b = (0x1f & (col >> 10)) << 3,
    };
    //printf("converting %X, g = %d\n", col, ret.g);
    return ret;
}

// turn clut to image
Image* image_make_from_clut(uint16_t* clut)
{
    //return image_make((uint8_t*) clut, 16, 16, clut);
    Image* img;

    img = malloc(sizeof(Image) + 16*16*sizeof(Color));
    img->max = (1 << 8) - 1;
    img->w = 16;
    img->h = 16;

    uint16_t col;

    for (int i = 0; i < 256; i++) {
        //col = clut[data[i]];
        col = clut[i];
        // 15 bit to 24 bit
        img->bitmap[i].r = (0x1f & (col >> 0)) << 3;
        img->bitmap[i].g = (0x1f & (col >> 5)) << 3;
        img->bitmap[i].b = (0x1f & (col >> 10)) << 3;
    }

    return img;
}

void image_destroy(Image* img)
{
    free(img);
}

void image_save_ppm(FILE* outf, Image* img)
{
    // The header
    fprintf(outf, "P3\n%d %d\n%d\n", img->w, img->h, img->max);

    Color col;
    for (int i = 0; i < img->w*img->h; i++) {
        col = img->bitmap[i];
        fprintf(outf, "%d %d %d\n", col.r, col.g, col.b);
    }   
}

