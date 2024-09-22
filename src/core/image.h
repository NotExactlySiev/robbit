#ifndef _IMAGE_H
#define _IMAGE_H

#include "robbit.h"

typedef struct Color Color;
typedef struct Image Image;

struct Color {
    uint8_t r,g,b;
};

struct Image {
    uint16_t w;
    uint16_t h;
    uint8_t max;
    Color bitmap[];
};

Image* image_make(uint8_t* data, uint16_t w, uint16_t h, uint16_t* clut);
Image* image_make_empty(uint16_t w, uint16_t h);
Image* image_make_from_data(uint8_t* data, uint16_t* clut);
Image* image_make_from_clut(uint16_t* clut);
Color image_15_to_24(uint16_t);

//int clut_to_sdl_texture(SDL_Texture *texture, uint16_t *clut);

#endif
