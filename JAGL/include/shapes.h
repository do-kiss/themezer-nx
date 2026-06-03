#pragma once

typedef unsigned char u8;
typedef unsigned short u16;

#include <switch.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL2_gfxPrimitives.h>

#define BUTTON_A            0xE0E0
#define BUTTON_B            0xE0E1
#define BUTTON_X            0xE0E2
#define BUTTON_Y            0xE0E3
#define BUTTON_L            0xE0E4
#define BUTTON_R            0xE0E5
#define BUTTON_ZL           0xE0E6
#define BUTTON_ZR           0xE0E7
#define BUTTON_SL           0xE0E8
#define BUTTON_SR           0xE0E9
#define BUTTON_UP           0xE0EB
#define BUTTON_DOWN         0xE0EC
#define BUTTON_LEFT         0xE0ED
#define BUTTON_RIGHT        0xE0EE
#define BUTTON_PLUS         0xE0EF
#define BUTTON_MINUS        0xE0F0

#define CAST(type, tocast) ((type *) tocast)

typedef struct {
    int w, h;
} SizeInfo_t;

enum ShapeTypes{
    RectangleType = 0,
    TextType,
    GlyphType,
    TextCenteredType,
    TextBoxType,
    ImageType,
    RectangleOutlineType,
    ProgressBarType,
    ListItemType,
    DataType, // This is an invalid type, this also won't get cleaned up. this is just that you can safely add an entry to the list and read (and find) it back later.
    ListViewType,
    ListGridType,
    ButtonType,
};

typedef struct {
    SDL_Rect pos;
    SDL_Color color;
    u8 flood;
} Rectangle_t;

typedef struct {
    int x, y;
    char *text;
    SDL_Color color;
    TTF_Font *font;
} Text_t;

#define TEXT_GLYPH_NO_RENDER BIT(0)

typedef struct {
    int x, y;
    u16 glyph;
    SDL_Color color;
    TTF_Font *font;
    u8 options;
} Glyph_t;

typedef struct {
    int w, h;
    Text_t text;
} TextCentered_t;

#define IMAGE_CLEANUPTEX BIT(0)

typedef struct {
    SDL_Texture *texture;
    SDL_Rect pos;
    u8 options;
} Image_t;

typedef struct {
    Rectangle_t rect;
    int borderSize;
} RectangleOutline_t;

typedef struct ShapeLinker_s{
    u8 type;
    void *item;
    struct ShapeLinker_s *next;
} ShapeLinker_t;

enum ctxOrigin {
    OriginButtonPress = 0,
    OriginFunction,
    OriginPlus
};

typedef struct {
    int curOffset, origin;
    ShapeLinker_t *selected, *all;
    u64 kDown, kUp, kHeld;
} Context_t;

typedef struct {
    SDL_Color leftColor, rightColor;
    SDL_Texture *leftImg;
    char *leftText, *rightText;
} ListItem_t;

typedef int (*func_ptr)(Context_t *ctx);

#define SETBIT(object, shift, value) ((value) ? (object |= shift) : (object &= ~shift))
#define BUTTON_DISABLED BIT(0)
#define BUTTON_PRESSED BIT(1)
#define BUTTON_HIGHLIGHT BIT(2)
#define BUTTON_NOJOYSEL BIT(3)

enum {
    ButtonStyleFlat = 0,
    ButtonStyleBottomStrip,
    ButtonStyleTopStrip,
    ButtonStyleBorder,
};

typedef struct {
    SDL_Rect pos;
    SDL_Color primary, secondary, textcolor, highlight;
    u8 options;
    u8 style;
    char *text;
    TTF_Font *font;
    func_ptr function;
} Button_t;

#define LIST_DISABLED BIT(0)
#define LIST_AUTO BIT(1)
#define LIST_PRESSED BIT(2)
#define LIST_SELECTED BIT(3)
#define LIST_ALWAYSRENDERSELECTED BIT(4)
#define LIST_CENTERLEFT BIT(5)
#define LIST_CLEANUPTEX BIT(6)
#define GRID_NOSIDEESC BIT(7) // Fuck we're outta bits

typedef struct {
    SDL_Rect pos;
    int entrySize;
    SDL_Color primary, selected, pressed, scrollbarBg, scrollbarThumb;
    u8 options;
    int highlight;
    int offset;
    ShapeLinker_t *text;
    func_ptr function;
    func_ptr changeSelection;
    TTF_Font *font;
} ListView_t;

typedef struct {
    SDL_Rect pos;
    u8 fitOnX;
    int entryYSize;
    SDL_Color primary, selected, pressed, scrollbarBg, scrollbarThumb;
    u8 options;
    int highlight;
    int offset;
    ShapeLinker_t *text;
    func_ptr function;
    func_ptr changeSelection;
    TTF_Font *font;
} ListGrid_t;

enum {
    ProgressBarStyleFlat = 0,
    ProgressBarStyleBorder,
    ProgressBarStyleSize,
    ProgressBarStyleCompleteBorder
};

typedef struct {
    SDL_Rect pos;
    SDL_Color primary, secondary;
    u8 style;
    u8 percentage;
} ProgressBar_t;

#define COLOR(r, g, b, a) (SDL_Color) {r, g, b, a}
#define POS(x, y, w, h) (SDL_Rect) {x, y, w, h}

Rectangle_t *RectangleCreate(SDL_Rect pos, SDL_Color color, int flood);
RectangleOutline_t *RectangleOutlineCreate(SDL_Rect pos, SDL_Color color, int flood, int borderSize);
Image_t *ImageCreate(SDL_Texture *tex, SDL_Rect pos, u8 options);
Text_t *TextCreate(int x, int y, const char *text, SDL_Color color, TTF_Font *font);
TextCentered_t *TextCenteredCreate(SDL_Rect pos, const char *text, SDL_Color color, TTF_Font *font);
Button_t *ButtonCreate(SDL_Rect pos, SDL_Color primary, SDL_Color secondary, SDL_Color textcolor, SDL_Color highlight, u8 options, u8 style, const char *text, TTF_Font *font, func_ptr function);
void DrawButton(Button_t *button);
ListView_t *ListViewCreate(SDL_Rect pos, int entrySize, SDL_Color primary, SDL_Color selected, SDL_Color pressed, SDL_Color scrollbarBg, SDL_Color scrollbarThumb, u8 options, ShapeLinker_t *textList, func_ptr function, func_ptr selectionChanged, TTF_Font *font);
void DrawListView(ListView_t *listview);
void DrawProgressBar(ProgressBar_t *bar);
ProgressBar_t *ProgressBarCreate(SDL_Rect pos, SDL_Color primary, SDL_Color secondary, u8 style, u8 percentage);
Glyph_t *GlyphCreate(int x, int y, u16 glyph, SDL_Color color, TTF_Font *font);
void ProgressBarFree(ProgressBar_t *pb);
void ListViewFree(ListView_t *lv);
void ButtonFree(Button_t *btn);
void TextCenteredFree(TextCentered_t *text);
void TextFree(Text_t *text);
void ImageFree(Image_t *img);
void RectangleOutlineFree(RectangleOutline_t *Rectangle);
void RectangleFree(Rectangle_t *Rectangle);
void GlyphFree(Glyph_t *glyph);
ListItem_t *ListItemCreate(SDL_Color LColor, SDL_Color RColor, SDL_Texture *LImg, const char *LText, const char *RText);
void ListItemFree(ListItem_t *li);
int CheckTouchCollisionButton(Button_t *btn, int touchX, int touchY);
int CheckTouchCollisionListView(ListView_t *lv, int touchX, int touchY);
void DrawListGrid(ListGrid_t *gv);
void ListGridFree(ListGrid_t *gv);
ListGrid_t *ListGridCreate(SDL_Rect pos, u8 fitOnX, int entryYSize, SDL_Color primary, SDL_Color selected, SDL_Color pressed, SDL_Color scrollbarBg, SDL_Color scrollbarThumb, u8 options, ShapeLinker_t *textList, func_ptr function, func_ptr selectionChanged, TTF_Font *font);
int CheckTouchCollisionListGrid(ListGrid_t *gv, int touchX, int touchY);