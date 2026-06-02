#include "gfx.h"

ShapeLinker_t *CreateSideTargetMenu(RequestInfo_t *rI){
    ShapeLinker_t *out = CreateSideBaseMenu("类型");

    ShapeLinker_t *list = NULL;
    for (int i = 0; i < 9; i++) {
        if (rI->target == i) {
            SetActiveColorTexture(targetIcons[i]);
        } else {
            SetInactiveColorTexture(targetIcons[i]);
        }
        ShapeLinkAdd(&list, ListItemCreate((rI->target == i) ? COLOR_FILTERACTIVE : COLOR_WHITE, COLOR_WHITE, targetIcons[i], targetOptions[i], NULL), ListItemType);
    }

    ShapeLinkAdd(&out, ListViewCreate(POS(0, 50, 400, SCREEN_H - 100), 60, COLOR_MAINBG, COLOR_CURSOR, COLOR_CURSORPRESS, COLOR_SCROLLBAR, COLOR_SCROLLBARTHUMB, 0, list, exitFunc, NULL, FONT_TEXT[FSize30]), ListViewType);

    // 光标默认定位到当前生效的选项
    ListView_t *lv = ShapeLinkFind(out, ListViewType)->item;
    lv->highlight = rI->target;

    ShapeLinkAdd(&out, ButtonCreate(POS(0, SCREEN_H - 50, 400, 50), COLOR_MAINBG, COLOR_RED, COLOR_WHITE, COLOR_CURSOR, 0, ButtonStyleBottomStrip, "退出 Themezer-NX", FONT_TEXT[FSize25], exitFunc), ButtonType);
    ShapeLinkAdd(&out, GlyphCreate(376, SCREEN_H - 48, BUTTON_PLUS, COLOR_WHITE, FONT_BTN[FSize20]), GlyphType);

    return out;
}

int ShowSideTargetMenu(Context_t *ctx){
    RequestInfo_t *rI = ShapeLinkFind(ctx->all, DataType)->item;
    ShapeLinker_t *menu = CreateSideTargetMenu(rI);
    Context_t menuCtx = MakeMenu(menu, ButtonHandlerBExit, NULL);

    if (menuCtx.selected->type == ListViewType && menuCtx.origin == OriginFunction){
        ListView_t *lv = menuCtx.selected->item;
        int selection = lv->highlight;
        if (rI->target != selection){
            int tempTarget = rI->target;
            int tempPage = rI->page;
            bool tempIncludeNSFW = rI->includeNSFW;
            SetDefaultsRequestInfo(rI);
            rI->target = selection;
            rI->includeNSFW = tempIncludeNSFW;

            // 切换类型后释放旧数据
            CleanupTransferInfo(rI);
            FreeThemes(rI);
            rI->themesCached = false;

            printf("Making request...\n");
            if (MakeRequestAsCtx(ctx, rI)){
                rI->target = tempTarget;
                rI->page = tempPage;
                rI->includeNSFW = tempIncludeNSFW;
            }
        }
    }

    ShapeLinkDispose(&menu);
    return (menuCtx.curOffset == 8 && menuCtx.origin == OriginFunction) ? -1 : 0; 
}