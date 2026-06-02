#pragma once
#include <switch.h>
#include <JAGL.h>
#include <curl/curl.h>
#include "libs/cJSON.h"

extern const char *targetOptions[], *sortOptions[], *orderOptions[];

typedef struct {
    int sort;
    int order;
    char *search;
    bool includeNSFW;
} FilterOptions_t;

typedef struct {
    unsigned char *buffer;
    size_t len;
    size_t buflen;
} get_request_t;

typedef struct {
    char *id;
    char *creator;
    char *name;
    char *description;
    char *lastUpdated;
    char *imgLink;
    char *thumbLink;
    char *downloadLink;
    int dlCount;
    int likeCount;
    int target;
    SDL_Texture *preview;
} ThemeInfo_t;

typedef struct { // We are not going to display like half of these
    //char *id;
    char *creator;
    char *name;
    //char *description;
    //char *lastUpdated;
    //int dlCount;
    //int likeCount;
    char *imgLink;
    char *thumbLink;
    SDL_Texture *preview;
    int themeCount;
    int isDlDone;
    ThemeInfo_t *themes;
} PackInfo_t;

typedef struct {
    CURL *transfer;
    get_request_t data;
    int index;
} Transfer_t;

typedef struct {
    Transfer_t *transfers;
    int queueOffset;
    CURLM *transferer;
    bool finished;
} TransferInfo_t;

typedef struct {
    int maxDls;
    int target;
    int limit;
    int page;
    int sort;
    int order;
    char *search;
    bool includeNSFW;
    int pageCount;
    int itemCount;
    int curPageItemCount;
    cJSON *response;
    ThemeInfo_t *themes;
    TransferInfo_t tInfo;
    PackInfo_t *packs;
    bool themesCached;
    int lastPageDir;
    void *pageCache;
} RequestInfo_t;

#define PAGE_CACHE_SIZE 3
#define MAX_PRELOAD_JOBS 1

typedef struct {
    int page;
    int pageCount;
    int itemCount;
    ThemeInfo_t *themes;
    PackInfo_t *packs;
    ShapeLinker_t *listItems;
    int curPageItemCount;
    bool isLoaded;
    bool isLoading;
} PageCacheEntry_t;

typedef struct {
    PageCacheEntry_t entries[PAGE_CACHE_SIZE];
    int count;
    CURLM *jsonTransferer;
    Transfer_t jsonTransfers[MAX_PRELOAD_JOBS];
    int jsonQueueOffset;
    bool jsonActive;
    int preloadPages[MAX_PRELOAD_JOBS];
    int currentTarget;
    int currentLimit;
} PageCache_t;

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))