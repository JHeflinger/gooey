#ifndef FONTS_H
#define FONTS_H

#include <raylib.h>

#define MAX_FONT_SIZE 40 // 2x LINE_HEIGHT for 2x UI supersampling

void InitializeFonts();

Font FontAsset();

Font FontAssetItalic();

void DestroyFonts();

#endif
