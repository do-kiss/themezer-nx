#include "gfx.h"
#include <unistd.h>

static const char *GetPackThemeTargetLabel(const ThemeInfo_t *theme){
    if (theme->target < 0 || theme->target >= 7)
        return "未知";

    return targetOptions[theme->target + 1];
}

static void ShowPackDetailsMessage(char *title, char *message){
    ShapeLinker_t *menu = CreateBaseMessagePopup(title, message);
    ShapeLinkAdd(&menu, ButtonCreate(POS(250, 470, 780, 50), COLOR_BTNIDLE, COLOR_INSTALLBTNPRS, COLOR_WHITE, COLOR_INSTALLBTN, 0, ButtonStyleBottomStrip, "确定", FONT_TEXT[FSize28], exitFunc), ButtonType);
    MakeMenu(menu, ButtonHandlerBExit, NULL);
    ShapeLinkDispose(&menu);
}

static ShapeLinker_t *CreatePackProgressMenu(char *message, TextCentered_t **progressText){
    ShapeLinker_t *out = NULL;

    SDL_Texture *screenshot = ScreenshotToTexture();
    ShapeLinkAdd(&out, ImageCreate(screenshot, POS(0, 0, SCREEN_W, SCREEN_H), IMAGE_CLEANUPTEX), ImageType);
    ShapeLinkAdd(&out, RectangleCreate(POS(0, 0, SCREEN_W, SCREEN_H), COLOR(0,0,0,200), 1), RectangleType);
    *progressText = TextCenteredCreate(POS(0, 0, SCREEN_W, SCREEN_H), message, COLOR_WHITE, FONT_TEXT[FSize45]);
    ShapeLinkAdd(&out, *progressText, TextCenteredType);

    return out;
}

static int DownloadPackTheme(ThemeInfo_t *theme){
    char *path = GetThemePath(theme->creator, theme->name, theme->target);
    int res = DownloadThemeFromUrl(CopyTextUtil(theme->downloadLink), path);
    free(path);

    return res;
}

static int EnsurePackThemeDownloaded(ThemeInfo_t *theme){
    char *path = GetThemePath(theme->creator, theme->name, theme->target);
    int res = 0;

    if (access(path, F_OK) == -1)
        res = DownloadThemeFromUrl(CopyTextUtil(theme->downloadLink), path);

    free(path);
    return res;
}

static int ShowPackTargetChoice(ThemeInfo_t *themes, int themeCount, int target){
    ShapeLinker_t *menu = NULL;
    ShapeLinker_t *items = NULL;
    int *candidateIndexes = calloc(themeCount, sizeof(int));
    int candidateCount = 0;

    if (candidateIndexes == NULL)
        return -1;

    for (int i = 0; i < themeCount; i++){
        if (themes[i].target == target){
            candidateIndexes[candidateCount] = i;
            ShapeLinkAdd(&items, ListItemCreate(COLOR_WHITE, COLOR_VERYLIGHTGREY_RGBA, NULL, themes[i].name, themes[i].creator), ListItemType);
            candidateCount++;
        }
    }

    SDL_Texture *screenshot = ScreenshotToTexture();
    char *title = CopyTextArgsUtil("选择 %s 主题", targetOptions[target + 1]);
    ShapeLinkAdd(&menu, ImageCreate(screenshot, POS(0, 0, SCREEN_W, SCREEN_H), IMAGE_CLEANUPTEX), ImageType);
    ShapeLinkAdd(&menu, RectangleCreate(POS(0, 0, SCREEN_W, SCREEN_H), COLOR(0,0,0,200), 1), RectangleType);
    ShapeLinkAdd(&menu, RectangleCreate(POS(250, 120, SCREEN_W - 500, 50), COLOR_TOPBAR, 1), RectangleType);
    ShapeLinkAdd(&menu, TextCenteredCreate(POS(260, 120, 0, 50), title, COLOR_WHITE, FONT_TEXT[FSize25]), TextCenteredType);
    ShapeLinkAdd(&menu, RectangleCreate(POS(250, 170, SCREEN_W - 500, 380), COLOR_MAINBG, 1), RectangleType);
    ShapeLinkAdd(&menu, ListViewCreate(POS(250, 170, SCREEN_W - 500, 380), 60, COLOR_MAINBG, COLOR_CURSOR, COLOR_CURSORPRESS, COLOR_SCROLLBAR, COLOR_SCROLLBARTHUMB, LIST_CENTERLEFT, items, exitFunc, NULL, FONT_TEXT[FSize25]), ListViewType);
    ShapeLinkAdd(&menu, ButtonCreate(POS(SCREEN_W - 300, 120, 50, 50), COLOR_TOPBAR, COLOR_RED, COLOR_WHITE, COLOR_TOPBARCURSOR, 0, ButtonStyleFlat, NULL, NULL, exitFunc), ButtonType);
    ShapeLinkAdd(&menu, ImageCreate(XIcon, POS(SCREEN_W - 300, 120, 50, 50), 0), ImageType);
    free(title);

    Context_t menuCtx = MakeMenu(menu, ButtonHandlerBExit, NULL);
    int selectedIndex = -1;

    if (menuCtx.selected != NULL && menuCtx.selected->type == ListViewType && menuCtx.origin == OriginFunction){
        ListView_t *lv = menuCtx.selected->item;
        if (lv->highlight >= 0 && lv->highlight < candidateCount)
            selectedIndex = candidateIndexes[lv->highlight];
    }

    ShapeLinkDispose(&menu);
    free(candidateIndexes);

    return selectedIndex;
}

static int ConfirmPackInstallOverwrite(const int *selectedThemes){
    int conflictCount = 0;

    for (int target = 0; target < 7; target++){
        if (selectedThemes[target] >= 0 && !CheckIfInstallSlotIsFree(target))
            conflictCount++;
    }

    if (conflictCount == 0)
        return 1;

    char *message = CopyTextArgsUtil("这将替换 %d 个安装队列项目%s。是否继续?", conflictCount, conflictCount == 1 ? "" : "项");
    ShapeLinker_t *menu = CreateBaseMessagePopup("替换安装队列?", message);
    free(message);

    ShapeLinkAdd(&menu, ButtonCreate(POS(640, 470, 390, 50), COLOR_BTNIDLE, COLOR_INSTALLBTNPRS, COLOR_WHITE, COLOR_INSTALLBTN, 0, ButtonStyleBottomStrip, "否", FONT_TEXT[FSize28], exitFunc), ButtonType);
    ShapeLinkAdd(&menu, ButtonCreate(POS(250, 470, 390, 50), COLOR_BTNIDLE, COLOR_INSTALLBTNPRS, COLOR_WHITE, COLOR_INSTALLBTN, 0, ButtonStyleBottomStrip, "是", FONT_TEXT[FSize28], exitFunc), ButtonType);

    Context_t menuCtx = MakeMenu(menu, ButtonHandlerBExit, NULL);
    ShapeLinkDispose(&menu);

    return menuCtx.curOffset == 7 && menuCtx.origin == OriginFunction;
}

int DownloadPackButton(Context_t *ctx){
    RequestInfo_t *rI = ShapeLinkFind(ctx->all, DataType)->item;
    TextCentered_t *progressText = NULL;
    ShapeLinker_t *progress = CreatePackProgressMenu("正在下载主题...", &progressText);
    int failures = 0;

    for (int i = 0; i < rI->curPageItemCount; i++){
        char *message = CopyTextArgsUtil("正在下载主题... %d/%d", i + 1, rI->curPageItemCount);
        free(progressText->text.text);
        progressText->text.text = CopyTextUtil(message);
        free(message);
        RenderShapeLinkList(progress);

        if (DownloadPackTheme(&rI->themes[i]))
            failures++;
    }

    ShapeLinkDispose(&progress);

    if (failures){
        char *message = CopyTextArgsUtil("%d 个主题下载失败。", failures);
        ShowPackDetailsMessage("下载未完成", message);
        free(message);
    }
    else {
        ShowPackDetailsMessage("下载完成", "此包中的所有主题已下载完成。");
    }

    return 0;
}

int InstallPackButton(Context_t *ctx){
    RequestInfo_t *rI = ShapeLinkFind(ctx->all, DataType)->item;
    int selectedThemes[7];
    int selectedCount = 0;

    for (int i = 0; i < 7; i++)
        selectedThemes[i] = -1;

    for (int target = 0; target < 7; target++){
        int count = 0;
        int firstIndex = -1;

        for (int i = 0; i < rI->curPageItemCount; i++){
            if (rI->themes[i].target == target){
                if (firstIndex < 0)
                    firstIndex = i;
                count++;
            }
        }

        if (count == 1){
            selectedThemes[target] = firstIndex;
            selectedCount++;
        }
        else if (count > 1){
            int selectedIndex = ShowPackTargetChoice(rI->themes, rI->curPageItemCount, target);
            if (selectedIndex < 0)
                return 0;

            selectedThemes[target] = selectedIndex;
            selectedCount++;
        }
    }

    if (selectedCount == 0){
        ShowPackDetailsMessage("未加入队列", "此包不包含任何可安装的主题。");
        return 0;
    }

    if (!ConfirmPackInstallOverwrite(selectedThemes))
        return 0;

    TextCentered_t *progressText = NULL;
    ShapeLinker_t *progress = CreatePackProgressMenu("正在加入安装队列...", &progressText);
    int failures = 0;
    int queued = 0;
    int processed = 0;

    for (int target = 0; target < 7; target++){
        if (selectedThemes[target] < 0)
            continue;

        ThemeInfo_t *theme = &rI->themes[selectedThemes[target]];
        processed++;
        char *message = CopyTextArgsUtil("正在加入安装队列... %d/%d", processed, selectedCount);
        free(progressText->text.text);
        progressText->text.text = CopyTextUtil(message);
        free(message);
        RenderShapeLinkList(progress);

        if (EnsurePackThemeDownloaded(theme)){
            failures++;
            continue;
        }

        char *path = GetThemePath(theme->creator, theme->name, theme->target);
        SetInstallSlot(theme->target, path);
        free(path);
        queued++;
    }

    ShapeLinkDispose(&progress);

    if (failures){
        char *message = CopyTextArgsUtil("%d 个已加入安装队列。%d 个主题下载失败。", queued, failures);
        ShowPackDetailsMessage("安装未完成", message);
        free(message);
    }
    else {
        char *message = CopyTextArgsUtil("%d 个已加入安装队列。退出应用以应用主题。\n你可以按 + 键退出应用。", queued);
        ShowPackDetailsMessage("已加入安装队列", message);
        free(message);
    }

    return 0;
}

ShapeLinker_t *CreatePackDetailsMenu(ShapeLinker_t *items, RequestInfo_t *rI){
    ShapeLinker_t *out = NULL;
    const int contentX = 160;
    const int contentW = SCREEN_W - 320;
    const int topBarY = 50;
    const int topBarH = 50;
    const int actionBarY = SCREEN_H - 110;
    const int actionBarH = 60;
    const int gridY = topBarY + topBarH;
    const int gridH = actionBarY - gridY;

    SDL_Texture *screenshot = ScreenshotToTexture();
    ShapeLinkAdd(&out, ImageCreate(screenshot, POS(0, 0, SCREEN_W, SCREEN_H), IMAGE_CLEANUPTEX), ImageType);
    ShapeLinkAdd(&out, RectangleCreate(POS(0, 0, SCREEN_W, SCREEN_H), COLOR(0,0,0,200), 1), RectangleType);

    // Grid fills most of the screen
    ShapeLinkAdd(&out, ListGridCreate(POS(contentX, topBarY, contentW, gridH + topBarH), 3, 260, COLOR_MAINBG, COLOR_CARDCURSOR, COLOR_CARDCURSORPRESS, COLOR_SCROLLBAR, COLOR_SCROLLBARTHUMB, (items) ? GRID_NOSIDEESC : LIST_DISABLED, items, ThemeSelect, NULL, FONT_TEXT[FSize23]), ListGridType);

    // X close button (top-right, touch-only, no d-pad focus) — placed outside the grid to avoid touch conflicts
    ShapeLinkAdd(&out, ButtonCreate(POS(contentX + contentW + 5, topBarY, 50, topBarH), COLOR_MAINBG, COLOR_RED, COLOR_WHITE, COLOR_CURSOR, BUTTON_NOJOYSEL, ButtonStyleFlat, NULL, NULL, exitFunc), ButtonType);
    ShapeLinkAdd(&out, ImageCreate(XIcon, POS(contentX + contentW + 5, topBarY, 50, topBarH), 0), ImageType);

    // Action bar: Install All / Download All
    ShapeLinkAdd(&out, RectangleCreate(POS(contentX, actionBarY, contentW, actionBarH), COLOR_TOPBAR, 1), RectangleType);
    ShapeLinkAdd(&out, ButtonCreate(POS(contentX + 30, actionBarY + 5, 470, 50), COLOR_BTNIDLE, COLOR_INSTALLBTNPRS, COLOR_WHITE, COLOR_INSTALLBTN, GetInstallButtonState() ? 0 : BUTTON_DISABLED, ButtonStyleFlat, "全部安装", FONT_TEXT[FSize25], InstallPackButton), ButtonType);
    ShapeLinkAdd(&out, ButtonCreate(POS(contentX + contentW - 500, actionBarY + 5, 470, 50), COLOR_BTNIDLE, COLOR_DOWNLOADBTNPRS, COLOR_WHITE, COLOR_DOWNLOADBTN, 0, ButtonStyleFlat, "全部下载", FONT_TEXT[FSize25], DownloadPackButton), ButtonType);
    ShapeLinkAdd(&out, rI, DataType);

    return out;
}

int ShowPackDetails(Context_t *ctx){
    ListGrid_t *gv = ShapeLinkFind(ctx->all, ListGridType)->item;
    RequestInfo_t *rI = ShapeLinkFind(ctx->all, DataType)->item;
    // target = -1 (not 0): prevents ThemeSelect from treating these as a pack listing and
    // recursively calling ShowPackDetails with a NULL packs array
    RequestInfo_t customRI = {12, -1, 0, 0, 0, 0, NULL, false, 0, 0, rI->packs[gv->highlight].themeCount, NULL, rI->packs[gv->highlight].themes, {NULL, 0, NULL, 1}, NULL};

    printf("Showing pack details...\nCount: %d\nEntry: %d\n", rI->packs[gv->highlight].themeCount, gv->highlight);

    ShapeLinker_t *items = GenListItemList(&customRI);
    if (!rI->packs[gv->highlight].isDlDone)
        AddThemeImagesToDownloadQueue(&customRI, true);

    ShapeLinker_t *menu = CreatePackDetailsMenu(items, &customRI);
    MakeMenu(menu, ButtonHandlerBExit, HandleDownloadQueue);

    rI->packs[gv->highlight].isDlDone = customRI.tInfo.finished; 

    if (!rI->packs[gv->highlight].isDlDone)
        CleanupTransferInfo(&customRI);

    ShapeLinkDispose(&menu);

    return 0;
}