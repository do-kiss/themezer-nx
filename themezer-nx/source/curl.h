#pragma once
#include <JAGL.h>
#include "libs/cJSON.h"
#include <curl/curl.h>
#include "model.h"

extern char cURLErrBuff[CURL_ERROR_SIZE];

typedef enum {
	QuickIdLookupNone = 0,
	QuickIdLookupTheme,
	QuickIdLookupPack,
	QuickIdLookupRemoteTheme,
} QuickIdLookupType_t;

int GetThemesList(char *url, char *data, cJSON **response);
ShapeLinker_t *GenListItemsFromJson(cJSON *json);
int MakeJsonRequest(char *url, cJSON **response);
char *GenLink(RequestInfo_t *rI);
ShapeLinker_t *GenListItemList(RequestInfo_t *rI);
int GenThemeArray(RequestInfo_t *rI);
void SetDefaultsRequestInfo(RequestInfo_t *rI);
int DownloadThemeFromUrl(char *url, char *path);
int HandleDownloadQueue(Context_t *ctx);
int AddThemeImagesToDownloadQueue(RequestInfo_t *rI, bool thumb);
int CleanupTransferInfo(RequestInfo_t *rI);
void FreeThemes(RequestInfo_t *rI);
int LookupByQuickId(const char *quickId, RequestInfo_t *rI, QuickIdLookupType_t *lookupType);
SDL_Texture *CreateThumbHashTexture(const char *encodedThumbHash);

char *GenPageLink(RequestInfo_t *rI, int page);
PageCache_t *InitPageCache(void);
void FreePageCache(PageCache_t *cache);
void ClearPageCache(PageCache_t *cache);
PageCacheEntry_t *FindPageCache(PageCache_t *cache, int page);
int StorePageInCache(PageCache_t *cache, RequestInfo_t *rI, ShapeLinker_t *items);
void EvictFurthestPage(PageCache_t *cache, int currentPage);
int StartPagePreload(PageCache_t *cache, RequestInfo_t *rI, int page);
void PumpPagePreloads(PageCache_t *cache);
void TriggerPagePreloads(PageCache_t *cache, RequestInfo_t *rI);
int HandleMainMenuFrame(Context_t *ctx);