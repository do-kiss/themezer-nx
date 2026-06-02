#include "gfx.h"

/*
 * Shape offset layout for the filter menu:
 *   0-6: from CreateSideBaseMenu (Screenshot, 2x Rect, Text, Button, Image, Button)
 *   7:   DataType (options)
 *   8:   Rectangle ("搜索关键字" sub-bar)
 *   9:   TextCentered ("搜索关键字" label)
 *   10:  Button ("输入")
 *   11:  Button ("清除")
 *   12:  Rectangle ("排序方式" sub-bar)
 *   13:  TextCentered ("排序方式")
 *   14:  ListView (sort — 4 options)
 *   15:  Rectangle ("排序方向" sub-bar)
 *   16:  TextCentered ("排序方向")
 *   17:  ListView (order — "降序"/"升序")
 *   18:  Rectangle ("儿童不宜内容" sub-bar)
 *   19:  TextCentered ("儿童不宜内容")
 *   20:  ListView (NSFW — "隐藏"/"显示")
 *   21:  Button ("应用")
 */

int SideMenuNsfwSetSelection(Context_t *ctx){
    ShapeLinker_t *all = ctx->all;
    ListView_t *lv = ShapeLinkOffset(all, 20)->item;
    FilterOptions_t *options = ShapeLinkFind(all, DataType)->item;
    int selection = lv->highlight;
    bool newVal = (selection == 1);

    if (newVal != options->includeNSFW){
        options->includeNSFW = newVal;

        ListItem_t *hideItem = ShapeLinkOffset(lv->text, 0)->item;
        ListItem_t *showItem = ShapeLinkOffset(lv->text, 1)->item;
        hideItem->leftColor = options->includeNSFW ? COLOR_WHITE : COLOR_FILTERACTIVE;
        showItem->leftColor = options->includeNSFW ? COLOR_FILTERACTIVE : COLOR_WHITE;
        SetInactiveColorTexture(targetIcons[2]);
        SetInactiveColorTexture(targetIcons[5]);
        SetActiveColorTexture(options->includeNSFW ? targetIcons[5] : targetIcons[2]);
    }

    return 0;
}

int SideMenuSortSetSelection(Context_t *ctx){
    ShapeLinker_t *all = ctx->all;
    ListView_t *lv = ShapeLinkOffset(all, 14)->item;
    FilterOptions_t *options = ShapeLinkFind(all, DataType)->item;
    int selection = lv->highlight;

    if (selection != options->sort){
        ListItem_t *prevItem = ShapeLinkOffset(lv->text, options->sort)->item;
        prevItem->leftColor = COLOR_WHITE;
        SetInactiveColorTexture(sortIcons[options->sort]);

        options->sort = selection;

        ListItem_t *newItem = ShapeLinkOffset(lv->text, options->sort)->item;
        newItem->leftColor = COLOR_FILTERACTIVE;
        SetActiveColorTexture(sortIcons[options->sort]);
    }

    return 0;
}

int SideMenuOrderSetSelection(Context_t *ctx){
    ShapeLinker_t *all = ctx->all;
    ListView_t *lv = ShapeLinkOffset(all, 17)->item;
    FilterOptions_t *options = ShapeLinkFind(all, DataType)->item;
    int selection = lv->highlight;
    int newOrder = (selection == 0) ? 0 : 1;

    if (newOrder != options->order){
        options->order = newOrder;

        ListItem_t *descItem = ShapeLinkOffset(lv->text, 0)->item;
        ListItem_t *ascItem = ShapeLinkOffset(lv->text, 1)->item;
        descItem->leftColor = (options->order == 0) ? COLOR_FILTERACTIVE : COLOR_WHITE;
        ascItem->leftColor = (options->order == 1) ? COLOR_FILTERACTIVE : COLOR_WHITE;
        SetInactiveColorTexture(orderIcons[0]);
        SetInactiveColorTexture(orderIcons[1]);
        SetActiveColorTexture(orderIcons[options->order]);
    }

    return 0;
}

int SideMenuOrderSelection(Context_t *ctx){
    return 0;
}

int SideMenuClearSearch(Context_t *ctx){
    FilterOptions_t *options = ShapeLinkFind(ctx->all, DataType)->item;
    TextCentered_t *text = ShapeLinkOffset(ctx->all, 9)->item;
    if (options->search != NULL && options->search[0]){
        free(options->search);
        options->search = CopyTextUtil("");
        free(text->text.text);
        text->text.text = CopyTextUtil("搜索关键字");
        text->text.color = COLOR_WHITE;
    }

    return 0;
}

int SideMenuSetSearch(Context_t *ctx){
    FilterOptions_t *options = ShapeLinkFind(ctx->all, DataType)->item;
    TextCentered_t *text = ShapeLinkOffset(ctx->all, 9)->item;

    char *out = showKeyboard("输入搜索关键词。最多100个字符", options->search, 100);

    if (out == NULL)
        return 0;

    if (!isStringNullOrEmpty(out)){
        if (options->search != NULL)
            free(options->search);

        options->search = SanitizeString(out);
        free(text->text.text);
        text->text.text = CopyTextArgsUtil("搜索关键字: %s", options->search);
        text->text.color = COLOR_FILTERACTIVE;
    }

    free(out);
    return 0;
}

ShapeLinker_t *CreateSideFilterMenu(FilterOptions_t *options){
    ShapeLinker_t *out = CreateSideBaseMenu("搜索与筛选");

    ShapeLinkAdd(&out, options, DataType);

    char *search = options->search[0] ? CopyTextArgsUtil("搜索关键字: %s", (options->search)) : CopyTextUtil("搜索关键字");
    SDL_Color searchColor = options->search[0] ? COLOR_FILTERACTIVE : COLOR_WHITE;
    ShapeLinkAdd(&out, RectangleCreate(POS(0, 60, 400, 44), COLOR_SUBBAR, 1), RectangleType);
    ShapeLinkAdd(&out, TextCenteredCreate(POS(0, 60, 400, 44), search, searchColor, FONT_TEXT[FSize25]), TextCenteredType);
    ShapeLinkAdd(&out, ButtonCreate(POS(0, 104, 200, 46), COLOR_MAINBG, COLOR_CURSORPRESS, COLOR_WHITE, COLOR_CURSOR, 0, ButtonStyleFlat, "输入", FONT_TEXT[FSize28], SideMenuSetSearch), ButtonType);
    ShapeLinkAdd(&out, ButtonCreate(POS(200, 104, 200, 46), COLOR_MAINBG, COLOR_CURSORPRESS, COLOR_WHITE, COLOR_CURSOR, 0, ButtonStyleFlat, "清除", FONT_TEXT[FSize28], SideMenuClearSearch), ButtonType);
    free(search);

    ShapeLinkAdd(&out, RectangleCreate(POS(0, 150, 400, 44), COLOR_SUBBAR, 1), RectangleType);
    ShapeLinkAdd(&out, TextCenteredCreate(POS(0, 150, 400, 44), "排序方式", COLOR_WHITE, FONT_TEXT[FSize25]), TextCenteredType);

    ShapeLinker_t *sortList = NULL;
    for (int i = 0; i < 4; i++) {
        if (i == options->sort) {
            SetActiveColorTexture(sortIcons[i]);
        } else {
            SetInactiveColorTexture(sortIcons[i]);
        }
        ShapeLinkAdd(&sortList, ListItemCreate((i == options->sort) ? COLOR_FILTERACTIVE : COLOR_WHITE, COLOR_WHITE, sortIcons[i], sortOptions[i], NULL), ListItemType);
    }
    ShapeLinkAdd(&out, ListViewCreate(POS(0, 194, 400, 184), 46, COLOR_MAINBG, COLOR_CURSOR, COLOR_CURSORPRESS, COLOR_SCROLLBAR, COLOR_SCROLLBARTHUMB, 0, sortList, SideMenuSortSetSelection, NULL, FONT_TEXT[FSize28]), ListViewType);

    ShapeLinkAdd(&out, RectangleCreate(POS(0, 378, 400, 44), COLOR_SUBBAR, 1), RectangleType);
    char *order = CopyTextUtil("排序方向");
    ShapeLinkAdd(&out, TextCenteredCreate(POS(0, 378, 400, 44), order, COLOR_WHITE, FONT_TEXT[FSize25]), TextCenteredType);
    free(order);

    ShapeLinker_t *orderList = NULL;
    for (int i = 0; i < 2; i++) {
        if (i == options->order) {
            SetActiveColorTexture(orderIcons[i]);
        } else {
            SetInactiveColorTexture(orderIcons[i]);
        }
        ShapeLinkAdd(&orderList, ListItemCreate((i == options->order) ? COLOR_FILTERACTIVE : COLOR_WHITE, COLOR_WHITE, orderIcons[i], orderOptions[i], NULL), ListItemType);
    }
    ShapeLinkAdd(&out, ListViewCreate(POS(0, 422, 400, 100), 50, COLOR_MAINBG, COLOR_CURSOR, COLOR_CURSORPRESS, COLOR_SCROLLBAR, COLOR_SCROLLBARTHUMB, 0, orderList, SideMenuOrderSetSelection, NULL, FONT_TEXT[FSize28]), ListViewType);

    // 儿童不宜内容
    ShapeLinkAdd(&out, RectangleCreate(POS(0, 522, 400, 44), COLOR_SUBBAR, 1), RectangleType);
    char *nsfwLabel = CopyTextUtil("儿童不宜内容");
    ShapeLinkAdd(&out, TextCenteredCreate(POS(0, 522, 400, 44), nsfwLabel, COLOR_WHITE, FONT_TEXT[FSize25]), TextCenteredType);
    free(nsfwLabel);

    if (options->includeNSFW) {
        SetInactiveColorTexture(targetIcons[2]);
        SetActiveColorTexture(targetIcons[5]);
    } else {
        SetActiveColorTexture(targetIcons[2]);
        SetInactiveColorTexture(targetIcons[5]);
    }
    ShapeLinker_t *nsfwList = NULL;
    ShapeLinkAdd(&nsfwList, ListItemCreate(options->includeNSFW ? COLOR_WHITE : COLOR_FILTERACTIVE, COLOR_WHITE, targetIcons[2], "隐藏", NULL), ListItemType);
    ShapeLinkAdd(&nsfwList, ListItemCreate(options->includeNSFW ? COLOR_FILTERACTIVE : COLOR_WHITE, COLOR_WHITE, targetIcons[5], "显示", NULL), ListItemType);
    ShapeLinkAdd(&out, ListViewCreate(POS(0, 566, 400, 100), 46, COLOR_MAINBG, COLOR_CURSOR, COLOR_CURSORPRESS, COLOR_SCROLLBAR, COLOR_SCROLLBARTHUMB, 0, nsfwList, SideMenuNsfwSetSelection, NULL, FONT_TEXT[FSize28]), ListViewType);

    ShapeLinkAdd(&out, ButtonCreate(POS(0, SCREEN_H - 50, 400, 50), COLOR_MAINBG, COLOR_CARDCURSOR, COLOR_WHITE, COLOR_CURSOR, 0, ButtonStyleBottomStrip, "应用", FONT_TEXT[FSize28], exitFunc), ButtonType);

    return out;
}

int ShowSideFilterMenu(Context_t *ctx){
    RequestInfo_t *rI = ShapeLinkFind(ctx->all, DataType)->item;
    FilterOptions_t options = {rI->sort, rI->order, CopyTextUtil(rI->search), rI->includeNSFW};
    ShapeLinker_t *menu = CreateSideFilterMenu(&options);
    Context_t menuCtx = MakeMenu(menu, ButtonHandlerBExit, NULL);
    ShapeLinkDispose(&menu);

    if (menuCtx.curOffset == 21 && menuCtx.origin == OriginFunction){
        if (rI->search != NULL)
            free(rI->search);

        rI->search = options.search;
        rI->order = options.order;
        rI->sort = options.sort;
        rI->includeNSFW = options.includeNSFW;
        rI->page = 1;

        CleanupTransferInfo(rI);
        FreeThemes(rI);
        rI->themesCached = false;

        if (MakeRequestAsCtx(ctx, rI)){
            if (rI->search != NULL)
                free(rI->search);

            rI->search = CopyTextUtil("");
        }
    }
    else if (options.search != NULL)
        free(options.search);

    return 0;
}
