#ifndef GFXFONT_H
#define GFXFONT_H

// to use fonts w/o cleanup
#define PROGMEM

#include <stdint.h>

typedef struct {           // Data stored PER GLYPH
  uint16_t bitmapOffset;   // Pointer into GFXfont->bitmap
  uint8_t width, height;   // Bitmap dimensions in pixels
  uint8_t xAdvance;        // Distance to advance cursor (x axis)
  int8_t xOffset, yOffset; // Dist from cursor pos to UL corner
} GFXglyph;

typedef struct {       // Data stored for FONT AS A WHOLE:
  uint8_t *bitmap;     // Glyph bitmaps, concatenated
  GFXglyph *glyph;     // Glyph array
  uint8_t first, last; // ASCII extents
  uint8_t yAdvance;    // Newline distance (y axis)
} GFXfont;

#endif
