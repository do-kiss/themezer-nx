#include "curl.h"
#include <switch.h>
#include <curl/curl.h>
#include <mbedtls/base64.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "libs/cJSON.h"
#include "gfx/gfx.h"
#include <JAGL.h>
#include "thumbhash.h"
#include "utils.h"

#define THUMBHASH_CACHE_SIZE 64
static char *th_cache_keys[THUMBHASH_CACHE_SIZE];
static SDL_Texture *th_cache_tex[THUMBHASH_CACHE_SIZE];
static uint64_t th_cache_time[THUMBHASH_CACHE_SIZE];
static int th_cache_count = 0;
static uint64_t th_cache_tick = 0;

static SDL_Texture *ThumbHashCacheGet(const char *encodedThumbHash){
    th_cache_tick++;
    for (int i = 0; i < th_cache_count; i++){
        if (th_cache_keys[i] && !strcmp(th_cache_keys[i], encodedThumbHash)){
            th_cache_time[i] = th_cache_tick;
            return th_cache_tex[i];
        }
    }
    return NULL;
}

static void ThumbHashCachePut(const char *encodedThumbHash, SDL_Texture *tex){
    if (th_cache_count < THUMBHASH_CACHE_SIZE){
        th_cache_keys[th_cache_count] = strdup(encodedThumbHash);
        th_cache_tex[th_cache_count] = tex;
        th_cache_time[th_cache_count] = th_cache_tick;
        th_cache_count++;
    } else {
        // LRU eviction: find oldest entry
        int oldest = 0;
        for (int i = 1; i < th_cache_count; i++){
            if (th_cache_time[i] < th_cache_time[oldest])
                oldest = i;
        }
        free(th_cache_keys[oldest]);
        th_cache_keys[oldest] = strdup(encodedThumbHash);
        th_cache_time[oldest] = th_cache_tick;
        // keep old texture — SDL owns it, caller reuses or drops
        th_cache_tex[oldest] = tex;
    }
}

const char *requestTargets[] = {
    "ResidentMenu",
    "Entrance",
    "Flaunch",
    "Set",
    "Psl",
    "MyPage",
    "Notification"
};

const char *requestSorts[] = {
    "CREATED",
    "UPDATED",
    "DOWNLOADS",
    "SAVES"
};

const char *requestOrders[] = {
    "DESC",
    "ASC"
};

static int GetPreviewUrls(cJSON *item, const char *fieldName, cJSON **original, cJSON **thumb);
static int ParseThemeList(ThemeInfo_t **storage, int size, cJSON *themesList);
int GetIndexOfStrArr(const char **toSearch, int limit, const char *search);

static char *GenLookupByQuickIdLink(const char *quickId){
    static char request[0x1200];
    request[0] = '\0';
    const char *query = "query($quickId:String!){switch{lookupByQuickId(quickId:$quickId){__typename ... on SwitchPack{name creator{username} collageThumbHash collagePreview{jpgHdUrl jpgThumbUrl} themes{hexId creator{username} name description updatedAt downloadCount saveCount target screenshotThumbHash screenshotPreview{jpgHdUrl jpgThumbUrl} downloadUrl}} ... on SwitchTheme{hexId creator{username} name description updatedAt downloadCount saveCount target screenshotThumbHash screenshotPreview{jpgHdUrl jpgThumbUrl} downloadUrl} ... on SwitchRemoteInstallTheme{author createdAt downloadUrl name quickId target}}}}";
    char *variables = NULL;

    cJSON *variablesJson = cJSON_CreateObject();
    if (variablesJson != NULL){
        cJSON_AddStringToObject(variablesJson, "quickId", quickId);
        variables = cJSON_PrintUnformatted(variablesJson);
        cJSON_Delete(variablesJson);
    }

    CURL *curl = curl_easy_init();
    if (curl){
        char *encodedQuery = curl_easy_escape(curl, query, 0);
        char *encodedVariables = curl_easy_escape(curl, variables ? variables : "{}", 0);

        snprintf(request, sizeof(request), "https://api.themezer.net/graphql?query=%s&variables=%s", encodedQuery ? encodedQuery : query, encodedVariables ? encodedVariables : (variables ? variables : "{}"));

        if (encodedQuery)
            curl_free(encodedQuery);
        if (encodedVariables)
            curl_free(encodedVariables);
        curl_easy_cleanup(curl);
    }
    else {
        snprintf(request, sizeof(request), "https://api.themezer.net/graphql?query=%s&variables=%s", query, variables ? variables : "{}");
    }

    free(variables);

    printf("Request: %s\n\n", request);
    return request;
}

static int ParseTheme(ThemeInfo_t *themeInfo, cJSON *theme){
    cJSON *id = cJSON_GetObjectItem(theme, "hexId");
    cJSON *creator = cJSON_GetObjectItem(theme, "creator");
    cJSON *display_name = cJSON_GetObjectItem(creator, "username");
    cJSON *name = cJSON_GetObjectItem(theme, "name");
    cJSON *description = cJSON_GetObjectItem(theme, "description");
    cJSON *last_updated = cJSON_GetObjectItem(theme, "updatedAt");
    cJSON *dl_count = cJSON_GetObjectItem(theme, "downloadCount");
    cJSON *like_count = cJSON_GetObjectItem(theme, "saveCount");
    cJSON *original = NULL;
    cJSON *thumb = NULL;
    cJSON *thumb_hash = cJSON_GetObjectItem(theme, "screenshotThumbHash");
    cJSON *download = cJSON_GetObjectItem(theme, "downloadUrl");
    cJSON *target = cJSON_GetObjectItem(theme, "target");

    if (!GetPreviewUrls(theme, "screenshotPreview", &original, &thumb) || !cJSON_IsString(thumb_hash) || !cJSON_IsNumber(dl_count) || !cJSON_IsNumber(like_count) || !cJSON_IsString(last_updated) ||
        !(cJSON_IsString(description) || cJSON_IsNull(description)) || !cJSON_IsString(name) || !cJSON_IsString(display_name) || !cJSON_IsString(id) || !cJSON_IsString(download) || !cJSON_IsString(target)){
        return 1;
    }

    themeInfo->dlCount = dl_count->valueint;
    themeInfo->likeCount = like_count->valueint;
    themeInfo->lastUpdated = CopyTextUtil(last_updated->valuestring);
    if (!cJSON_IsNull(description))
        themeInfo->description = CopyTextUtil(description->valuestring);

    themeInfo->name = SanitizeString(name->valuestring);
    themeInfo->creator = SanitizeString(display_name->valuestring);
    themeInfo->id = CopyTextUtil(id->valuestring);
    themeInfo->imgLink = CopyTextUtil(original->valuestring);
    themeInfo->thumbLink = CopyTextUtil(thumb->valuestring);
    themeInfo->downloadLink = CopyTextUtil(download->valuestring);
    themeInfo->target = GetIndexOfStrArr(requestTargets, 7, target->valuestring);
    themeInfo->preview = CreateThumbHashTexture(thumb_hash->valuestring);

    return 0;
}

static int ParseRemoteTheme(ThemeInfo_t *themeInfo, cJSON *theme){
    cJSON *author = cJSON_GetObjectItem(theme, "author");
    cJSON *created_at = cJSON_GetObjectItem(theme, "createdAt");
    cJSON *download = cJSON_GetObjectItem(theme, "downloadUrl");
    cJSON *name = cJSON_GetObjectItem(theme, "name");
    cJSON *quick_id = cJSON_GetObjectItem(theme, "quickId");
    cJSON *target = cJSON_GetObjectItem(theme, "target");

    if (!cJSON_IsString(author) || !cJSON_IsString(created_at) || !cJSON_IsString(download) || !cJSON_IsString(name) || !cJSON_IsString(quick_id) || !cJSON_IsString(target))
        return 1;

    themeInfo->creator = SanitizeString(author->valuestring);
    themeInfo->name = SanitizeString(name->valuestring);
    themeInfo->id = CopyTextUtil(quick_id->valuestring);
    themeInfo->lastUpdated = CopyTextUtil(created_at->valuestring);
    themeInfo->downloadLink = CopyTextUtil(download->valuestring);
    themeInfo->target = GetIndexOfStrArr(requestTargets, 7, target->valuestring);

    return 0;
}

static int ParsePack(PackInfo_t *packInfo, cJSON *pack){
    cJSON *creator = cJSON_GetObjectItem(pack, "creator");
    cJSON *display_name = cJSON_GetObjectItem(creator, "username");
    cJSON *name = cJSON_GetObjectItem(pack, "name");
    cJSON *original = NULL;
    cJSON *thumb = NULL;
    cJSON *thumb_hash = cJSON_GetObjectItem(pack, "collageThumbHash");
    cJSON *themes = cJSON_GetObjectItem(pack, "themes");

    if (!GetPreviewUrls(pack, "collagePreview", &original, &thumb) || !cJSON_IsString(thumb_hash) || !cJSON_IsString(name) || !cJSON_IsString(display_name) || !cJSON_IsArray(themes))
        return 1;

    packInfo->creator = SanitizeString(display_name->valuestring);
    packInfo->name = SanitizeString(name->valuestring);
    packInfo->imgLink = CopyTextUtil(original->valuestring);
    packInfo->thumbLink = CopyTextUtil(thumb->valuestring);
    packInfo->preview = CreateThumbHashTexture(thumb_hash->valuestring);
    packInfo->themeCount = cJSON_GetArraySize(themes);

    if (ParseThemeList(&packInfo->themes, packInfo->themeCount, themes))
        return 2;

    return 0;
}

char *GenLink(RequestInfo_t *rI){
    char *searchQuoted;
    if (rI->search[0] != '\0')
        searchQuoted = CopyTextArgsUtil("\"%s\"", rI->search);
    else 
        searchQuoted = CopyTextUtil("null");

    char *requestTarget;
    if (rI->target == 0 || rI->target >= 8)
        requestTarget = CopyTextUtil("null");
    else 
        requestTarget = CopyTextArgsUtil("\"%s\"",requestTargets[rI->target - 1]);
    
    static char request[0x600];
    char variables[0x400];
    char *query;
    if (rI->target >= 1)
    {
        // query($target:Target,$paginationArgs:PaginationInput,$sort:ItemSort,$order:SortOrder,$query:String,$includeNSFW:Boolean!){switch{themes(target:$target,paginationArgs:$paginationArgs,sort:$sort,order:$order,query:$query,includeNSFW:$includeNSFW){nodes{hexId creator{username} name description updatedAt downloadCount saveCount target screenshotThumbHash screenshotPreview{jpgHdUrl jpgThumbUrl} downloadUrl isNSFW}pageInfo{itemCount limit page pageCount}}}}
        query = "query%28%24target%3ATarget%2C%24paginationArgs%3APaginationInput%2C%24sort%3AItemSort%2C%24order%3ASortOrder%2C%24query%3AString%2C%24includeNSFW%3ABoolean%21%29%7Bswitch%7Bthemes%28target%3A%24target%2CpaginationArgs%3A%24paginationArgs%2Csort%3A%24sort%2Corder%3A%24order%2Cquery%3A%24query%2CincludeNSFW%3A%24includeNSFW%29%7Bnodes%7BhexId%20creator%7Busername%7D%20name%20description%20updatedAt%20downloadCount%20saveCount%20target%20screenshotThumbHash%20screenshotPreview%7BjpgHdUrl%20jpgThumbUrl%7D%20downloadUrl%20isNSFW%7DpageInfo%7BitemCount%20limit%20page%20pageCount%7D%7D%7D%7D";
        snprintf(variables, 0x400,"{\"target\":%s,\"paginationArgs\":{\"page\":%d,\"limit\":%d},\"sort\":\"%s\",\"order\":\"%s\",\"query\":%s,\"includeNSFW\":%s}",\
            requestTarget, rI->page, rI->limit, requestSorts[rI->sort], requestOrders[rI->order], searchQuoted, rI->includeNSFW ? "true" : "false");
    }
    else if (rI->target == 0)
    {
        // query($paginationArgs:PaginationInput,$sort:ItemSort,$order:SortOrder,$query:String,$includeNSFW:Boolean!){switch{packs(paginationArgs:$paginationArgs,sort:$sort,order:$order,query:$query,includeNSFW:$includeNSFW){nodes{hexId creator{username} name description updatedAt downloadCount saveCount collageThumbHash collagePreview{jpgHdUrl jpgThumbUrl} themes{hexId creator{username} name description updatedAt downloadCount saveCount target screenshotThumbHash screenshotPreview{jpgHdUrl jpgThumbUrl} downloadUrl isNSFW}}pageInfo{itemCount limit page pageCount}}}}
        query = "query%28%24paginationArgs%3APaginationInput%2C%24sort%3AItemSort%2C%24order%3ASortOrder%2C%24query%3AString%2C%24includeNSFW%3ABoolean%21%29%7Bswitch%7Bpacks%28paginationArgs%3A%24paginationArgs%2Csort%3A%24sort%2Corder%3A%24order%2Cquery%3A%24query%2CincludeNSFW%3A%24includeNSFW%29%7Bnodes%7BhexId%20creator%7Busername%7D%20name%20description%20updatedAt%20downloadCount%20saveCount%20collageThumbHash%20collagePreview%7BjpgHdUrl%20jpgThumbUrl%7D%20themes%7BhexId%20creator%7Busername%7D%20name%20description%20updatedAt%20downloadCount%20saveCount%20target%20screenshotThumbHash%20screenshotPreview%7BjpgHdUrl%20jpgThumbUrl%7D%20downloadUrl%20isNSFW%7D%7DpageInfo%7BitemCount%20limit%20page%20pageCount%7D%7D%7D%7D";
        snprintf(variables, 0x400, "{\"paginationArgs\":{\"page\":%d,\"limit\":%d},\"sort\":\"%s\",\"order\":\"%s\",\"query\":%s,\"includeNSFW\":%s}",\
            rI->page, rI->limit, requestSorts[rI->sort], requestOrders[rI->order], searchQuoted, rI->includeNSFW ? "true" : "false");
    }

    CURL *curl = curl_easy_init();
    if(curl) {
        char *output = curl_easy_escape(curl, variables, 0);
        if(output) {
            printf("Encoded: %s\n", output);
            snprintf(request, 0x600, "https://api.themezer.net/graphql?query=%s&variables=%s", query, output);
            curl_free(output);
        }
        else 
        {
            snprintf(request, 0x600, "https://api.themezer.net/graphql?query=%s&variables=%s", query, variables);
        }
        curl_easy_cleanup(curl);
    }

    free(searchQuoted);
    free(requestTarget);
    
    printf("Request: %s\n\n", request);
    return request;
}

char *GenPageLink(RequestInfo_t *rI, int page){
    int savedPage = rI->page;
    rI->page = page;
    char *link = GenLink(rI);
    rI->page = savedPage;
    char *copy = malloc(strlen(link) + 1);
    if (copy)
        strcpy(copy, link);
    return copy;
}

int GetIndexOfStrArr(const char **toSearch, int limit, const char *search){
    for (int i = 0; i < limit; i++){
        if (!strcmp(search, toSearch[i]))
            return i;
    }

    return 0;
}

static int GetPreviewUrls(cJSON *item, const char *fieldName, cJSON **original, cJSON **thumb){
    cJSON *preview = cJSON_GetObjectItem(item, fieldName);
    if (!cJSON_IsObject(preview))
        return 0;

    *original = cJSON_GetObjectItem(preview, "jpgHdUrl");
    *thumb = cJSON_GetObjectItem(preview, "jpgThumbUrl");

    return cJSON_IsString(*original) && cJSON_IsString(*thumb);
}

SDL_Texture *CreateThumbHashTexture(const char *encodedThumbHash){
    if (!encodedThumbHash || !encodedThumbHash[0])
        return NULL;

    SDL_Texture *cached = ThumbHashCacheGet(encodedThumbHash);
    if (cached)
        return cached;

    size_t decodedSize = 0;
    size_t encodedLen = strlen(encodedThumbHash);
    size_t decodedCapacity = encodedLen * 3 / 4 + 4;
    unsigned char *decoded = malloc(decodedCapacity);
    uint8_t *rgba = NULL;
    int width = 0;
    int height = 0;
    SDL_Texture *texture = NULL;

    if (!decoded)
        return NULL;

    if (mbedtls_base64_decode(decoded, decodedCapacity, &decodedSize, (const unsigned char *)encodedThumbHash, encodedLen) == 0){
        if (ThumbHashToRGBA(decoded, decodedSize, 64, &rgba, &width, &height)){
            texture = LoadImageRGBASDL(rgba, width, height);
            ThumbHashCachePut(encodedThumbHash, texture);
        }
    }

    free(decoded);
    free(rgba);

    return texture;
}

#define CHUNK_SIZE 8192

static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    size_t realsize = size * nmemb; 
    get_request_t *req = userdata;

    while (req->buflen < req->len + realsize + 1)
    {
        req->buffer = realloc(req->buffer, req->buflen * 2);
        req->buflen *= 2;
    }
    memcpy(&req->buffer[req->len], ptr, realsize);
    req->len += realsize;
    req->buffer[req->len] = 0;

    return realsize;
}

char cURLErrBuff[CURL_ERROR_SIZE] = "";

CURL *CreateRequest(char *url, get_request_t *data){
    CURL *curl = NULL;

    curl = curl_easy_init();
    if (curl){
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "themezer-nx");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        data->buffer = malloc(CHUNK_SIZE);
        data->buflen = CHUNK_SIZE;

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, data);
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, cURLErrBuff);
    }

    return curl;
}

int MakeJsonRequest(char *url, cJSON **response){
    get_request_t req = {0};

    int res;
    CURL *curl = CreateRequest(url, &req);

    if (!(res = curl_easy_perform(curl))){
        if (response != NULL){
            *response = cJSON_Parse(req.buffer);
        }

        printf("Buffer: %s\n", req.buffer);
        free(req.buffer);
    }

    curl_easy_cleanup(curl);
    return res;
}

int MakeDownloadRequest(char *url, char *path){
    get_request_t req = {0};
    int res;
    CURL *curl = CreateRequest(url, &req);

    if (!(res = curl_easy_perform(curl))){
        FILE *fp = fopen(path, "wb");
        if (fp){
            fwrite(req.buffer, req.len, 1, fp);
            fclose(fp);
        }
        else {
            res = 1;
        }
    }

    curl_easy_cleanup(curl);
    return res;
}

int hasError(cJSON *root){
    cJSON *err = cJSON_GetObjectItem(root, "errors");

    if (err){
        cJSON *errItem = cJSON_GetArrayItem(err, 0);
        if (errItem){
            cJSON *messageItem = cJSON_GetObjectItem(errItem, "message");
            char *message = cJSON_GetStringValue(messageItem);
            
            if (message){
                ShapeLinker_t *menu = CreateBaseMessagePopup("请求出错", message);
                ShapeLinkAdd(&menu, ButtonCreate(POS(250, 470, 780, 50), COLOR_BTNIDLE, COLOR_INSTALLBTNPRS, COLOR_WHITE, COLOR_INSTALLBTN, 0, ButtonStyleBottomStrip, "确定", FONT_TEXT[FSize28], exitFunc), ButtonType);
                MakeMenu(menu, ButtonHandlerBExit, NULL);
                ShapeLinkDispose(&menu);
            }
        }

        return 1;
    }

    return 0;
}

int DownloadThemeFromUrl(char *url, char *path){
    int res = 1;

    if (url){
        res = MakeDownloadRequest(url, path);
        free(url);
    }

    printf("Res: %d", res);
    return res;
}

#define MIN(x, y) ((x < y) ? x : y)

void FreeThemes(RequestInfo_t *rI){
    if (!rI->themes)
        return;

    if (rI->themesCached){
        rI->themes = NULL;
        rI->packs = NULL;
        return;
    }

    for (int i = 0; i < rI->curPageItemCount; i++){
        NNFREE(rI->themes[i].id);
        NNFREE(rI->themes[i].creator);
        NNFREE(rI->themes[i].name);
        NNFREE(rI->themes[i].description);
        NNFREE(rI->themes[i].lastUpdated);
        NNFREE(rI->themes[i].imgLink);
        NNFREE(rI->themes[i].thumbLink);
        NNFREE(rI->themes[i].downloadLink);
        if (rI->themes[i].preview && rI->packs == NULL)
            SDL_DestroyTexture(rI->themes[i].preview);
        
        if (rI->packs != NULL){
            free(rI->packs[i].creator);
            free(rI->packs[i].name);
            if (rI->packs[i].preview)
                SDL_DestroyTexture(rI->packs[i].preview);
            for (int j = 0; j < rI->packs[i].themeCount; j++){
                free(rI->packs[i].themes[j].id);
                free(rI->packs[i].themes[j].creator);
                free(rI->packs[i].themes[j].name);
                free(rI->packs[i].themes[j].description);
                free(rI->packs[i].themes[j].lastUpdated);
                free(rI->packs[i].themes[j].imgLink);
                free(rI->packs[i].themes[j].thumbLink);
                free(rI->packs[i].themes[j].downloadLink);
                if  (rI->packs[i].themes[j].preview)
                    SDL_DestroyTexture(rI->packs[i].themes[j].preview);
            }
            free(rI->packs[i].themes);
        }
    }

    NNFREE(rI->themes);
    NNFREE(rI->packs);
}

int ParseThemeList(ThemeInfo_t **storage, int size, cJSON *themesList){
    *storage = calloc(sizeof(ThemeInfo_t), size);
    ThemeInfo_t *themes = *storage;

    cJSON *theme = NULL;
    int i = 0;
    cJSON_ArrayForEach(theme, themesList){
        if (ParseTheme(&themes[i], theme))
            return 1;

        i++;
    }

    return 0;
}

int ParsePackList(PackInfo_t **storage, int size, cJSON *packList){
    *storage = calloc(sizeof(PackInfo_t), size);
    PackInfo_t *packs = *storage;

    cJSON *pack = NULL;
    int i = 0;

    cJSON_ArrayForEach(pack, packList){
        if (ParsePack(&packs[i], pack))
            return 1;

        i++;
    }

    return 0;
}

void FillThemesWithPacks(RequestInfo_t *rI){
    rI->themes = calloc(sizeof(ThemeInfo_t), rI->curPageItemCount);
    for (int i = 0; i < rI->curPageItemCount; i++){
        rI->themes[i].name = CopyTextUtil(rI->packs[i].name);
        rI->themes[i].creator = CopyTextUtil(rI->packs[i].creator);
        rI->themes[i].thumbLink = CopyTextUtil(rI->packs[i].thumbLink);
        rI->themes[i].imgLink = CopyTextUtil(rI->packs[i].imgLink);
        rI->themes[i].preview = rI->packs[i].preview;
    }
}

int GenThemeArray(RequestInfo_t *rI){
    if (rI->response == NULL)
        return -1;

    int res = -1;

    if (hasError(rI->response))
        return -4;

    cJSON *data = cJSON_GetObjectItem(rI->response, "data");
    if (data){
        cJSON *switchObj = cJSON_GetObjectItem(data, "switch");
        if (switchObj) {
            cJSON *queryData;
            if (rI->target != 0){
                queryData = cJSON_GetObjectItem(switchObj, "themes");
            } else {
                queryData = cJSON_GetObjectItem(switchObj, "packs");
            }
            cJSON *pagination = cJSON_GetObjectItem(queryData, "pageInfo");
            cJSON *page_count = cJSON_GetObjectItem(pagination, "pageCount");
            cJSON *item_count = cJSON_GetObjectItem(pagination, "itemCount");

            if (cJSON_IsNumber(page_count) && cJSON_IsNumber(item_count)){
                rI->pageCount = page_count->valueint;
                rI->itemCount = item_count->valueint;
            }
            else 
            {
                return -1;
            }
                


            FreeThemes(rI);
            rI->curPageItemCount = MIN(rI->limit, rI->itemCount - rI->limit * (rI->page - 1));

            if (rI->itemCount <= 0)
                return 0;

            cJSON *nodes = cJSON_GetObjectItem(queryData, "nodes");
            if (rI->target != 0){
                if (nodes){
                    if (ParseThemeList(&rI->themes, rI->curPageItemCount, nodes))
                        return -3;

                    res = 0;
                    cJSON_Delete(rI->response);
                }
            }
            else {
                if (nodes){
                    if (ParsePackList(&rI->packs, rI->curPageItemCount, nodes)){
                        printf("Pack parser failed!");
                        return -3;
                    }
                        

                    FillThemesWithPacks(rI);

                    res = 0;
                    cJSON_Delete(rI->response);
                }
            }
        }
    }

    return res;
}

int LookupByQuickId(const char *quickId, RequestInfo_t *rI, QuickIdLookupType_t *lookupType){
    if (!quickId || !quickId[0] || !rI || !lookupType)
        return -1;

    *lookupType = QuickIdLookupNone;

    int res = MakeJsonRequest(GenLookupByQuickIdLink(quickId), &rI->response);
    if (res){
        ShowConnErrMenu(res);
        return res;
    }

    if (hasError(rI->response)){
        cJSON_Delete(rI->response);
        rI->response = NULL;
        return -4;
    }

    cJSON *data = cJSON_GetObjectItem(rI->response, "data");
    cJSON *switchObj = cJSON_GetObjectItem(data, "switch");
    cJSON *lookupData = cJSON_GetObjectItem(switchObj, "lookupByQuickId");

    if (!lookupData || cJSON_IsNull(lookupData)){
        cJSON_Delete(rI->response);
        rI->response = NULL;
        return 1;
    }

    cJSON *typename = cJSON_GetObjectItem(lookupData, "__typename");
    if (!cJSON_IsString(typename)){
        cJSON_Delete(rI->response);
        rI->response = NULL;
        return -2;
    }

    rI->maxDls = 12;
    rI->curPageItemCount = 1;

    if (!strcmp(typename->valuestring, "SwitchTheme")){
        rI->themes = calloc(sizeof(ThemeInfo_t), 1);
        if (!rI->themes)
            res = -3;
        else if (ParseTheme(&rI->themes[0], lookupData))
            res = -3;
        else
            *lookupType = QuickIdLookupTheme;
    }
    else if (!strcmp(typename->valuestring, "SwitchPack")){
        rI->packs = calloc(sizeof(PackInfo_t), 1);
        if (!rI->packs)
            res = -3;
        else if (ParsePack(&rI->packs[0], lookupData))
            res = -3;
        else {
            FillThemesWithPacks(rI);
            *lookupType = QuickIdLookupPack;
        }
    }
    else if (!strcmp(typename->valuestring, "SwitchRemoteInstallTheme")){
        rI->themes = calloc(sizeof(ThemeInfo_t), 1);
        if (!rI->themes)
            res = -3;
        else if (ParseRemoteTheme(&rI->themes[0], lookupData))
            res = -3;
        else
            *lookupType = QuickIdLookupRemoteTheme;
    }
    else {
        res = -2;
    }

    cJSON_Delete(rI->response);
    rI->response = NULL;

    return res;
}



ShapeLinker_t *GenListItemList(RequestInfo_t *rI){
    ShapeLinker_t *link = NULL;

    printf("Gen: ArraySize: %d", rI->curPageItemCount);

    for (int i = 0; i < rI->curPageItemCount; i++){
        ShapeLinkAdd(&link, ListItemCreate(COLOR_WHITE, COLOR_VERYLIGHTGREY_RGBA, rI->themes[i].preview, rI->themes[i].name, rI->themes[i].creator), ListItemType);
    }

    return link;
}

int AddThemeImagesToDownloadQueue(RequestInfo_t *rI, bool thumb){
    if (!rI->curPageItemCount)
        return 0;
        
    rI->tInfo.transfers = calloc(sizeof(Transfer_t), rI->curPageItemCount);
    rI->tInfo.transferer = curl_multi_init();
    if (!rI->tInfo.transfers || !rI->tInfo.transferer){
        free(rI->tInfo.transfers);
        if (rI->tInfo.transferer)
            curl_multi_cleanup(rI->tInfo.transferer);
        rI->tInfo.transfers = NULL;
        rI->tInfo.transferer = NULL;
        rI->tInfo.queueOffset = 0;
        rI->tInfo.finished = true;
        return 1;
    }

    rI->tInfo.queueOffset = rI->curPageItemCount;
    rI->tInfo.finished = false;
    curl_multi_setopt(rI->tInfo.transferer, CURLMOPT_MAXCONNECTS, (long)rI->maxDls);
    curl_multi_setopt(rI->tInfo.transferer, CURLMOPT_MAX_TOTAL_CONNECTIONS, (long)rI->maxDls);
    curl_multi_setopt(rI->tInfo.transferer, CURLMOPT_MAX_HOST_CONNECTIONS, (long)rI->maxDls);
    curl_multi_setopt(rI->tInfo.transferer, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX);

    for (int i = 0; i < rI->curPageItemCount; i++){
            rI->tInfo.transfers[i].transfer = CreateRequest((thumb) ? rI->themes[i].thumbLink : rI->themes[i].imgLink, &rI->tInfo.transfers[i].data);
            rI->tInfo.transfers[i].index = i;
            if (!rI->tInfo.transfers[i].transfer)
                continue;

            curl_easy_setopt(rI->tInfo.transfers[i].transfer, CURLOPT_PRIVATE, &rI->tInfo.transfers[i].index); 
            curl_multi_add_handle(rI->tInfo.transferer, rI->tInfo.transfers[i].transfer);
    }

    return 0;
}

int CleanupTransferInfo(RequestInfo_t *rI){
    if (rI->tInfo.finished)
        return 0;

    if (!rI->tInfo.transfers && !rI->tInfo.transferer){
        rI->tInfo.finished = true;
        return 0;
    }

    for (int i = 0; i < rI->tInfo.queueOffset; i++){
        if (rI->tInfo.transferer && rI->tInfo.transfers[i].transfer){
            curl_multi_remove_handle(rI->tInfo.transferer, rI->tInfo.transfers[i].transfer);
            curl_easy_cleanup(rI->tInfo.transfers[i].transfer);
        }

        free(rI->tInfo.transfers[i].data.buffer);
        rI->tInfo.transfers[i].data.buffer = NULL;
    }

    if (rI->tInfo.transferer)
        curl_multi_cleanup(rI->tInfo.transferer);
    free(rI->tInfo.transfers);
    rI->tInfo.transferer = NULL;
    rI->tInfo.transfers = NULL;
    rI->tInfo.queueOffset = 0;
    rI->tInfo.finished = true;
    return 0;
}

int HandleDownloadQueue(Context_t *ctx){
    ShapeLinker_t *all = ctx->all;
    RequestInfo_t *rI = ShapeLinkFind(all, DataType)->item;
    ShapeLinker_t *gvLink = ShapeLinkFind(all, ListGridType);
    ListGrid_t *gv = NULL;
    Image_t *img;

    if (gvLink != NULL)
        gv = gvLink->item;
    else {
        img = ShapeLinkFind(ShapeLinkFind(all, ImageType)->next, ImageType)->item;
    }


    if (rI->tInfo.finished)
        return 0;

    int running_handles = 0;
    CURLMcode multi_res = CURLM_OK;

    // Single pump per frame — let other frames handle remaining I/O
    multi_res = curl_multi_perform(rI->tInfo.transferer, &running_handles);

    int msgs_left = -1;
    struct CURLMsg *msg;
    while ((msg = curl_multi_info_read(rI->tInfo.transferer, &msgs_left))){
        if (msg->msg == CURLMSG_DONE){
            CURL *e = msg->easy_handle;
            int *index;
            curl_easy_getinfo(e, CURLINFO_PRIVATE, &index);

            if (msg->data.result != CURLE_OK){
                printf("Something went wrong with the downloader, index %d, %d\n", *index, msg->data.result);
            }
            else {
                printf("Download of index %d finished!\n", *index);
                get_request_t *req = &rI->tInfo.transfers[*index].data;
                SDL_Texture *oldPreview = rI->themes[*index].preview;
                rI->themes[*index].preview = LoadImageMemSDL(req->buffer, req->len);
                if (rI->packs != NULL)
                    rI->packs[*index].preview = rI->themes[*index].preview;

                if (rI->themesCached){
                    PageCache_t *cache = (PageCache_t *)rI->pageCache;
                    if (cache){
                        PageCacheEntry_t *entry = FindPageCache(cache, rI->page);
                        if (entry && entry->themes){
                            entry->themes[*index].preview = rI->themes[*index].preview;
                            if (entry->packs)
                                entry->packs[*index].preview = rI->themes[*index].preview;
                        }
                    }
                }

                if (gvLink != NULL && gv->text != NULL){
                    ListItem_t *li = ShapeLinkOffset(gv->text, *index)->item;
                    li->leftImg = rI->themes[*index].preview;
                }
                else {
                    img->texture = rI->themes[*index].preview;
                }
                if (oldPreview && oldPreview != rI->themes[*index].preview && !rI->themesCached)
                    SDL_DestroyTexture(oldPreview);
            }

            curl_multi_remove_handle(rI->tInfo.transferer, e);
            curl_easy_cleanup(e);
            rI->tInfo.transfers[*index].transfer = NULL;
            free(rI->tInfo.transfers[*index].data.buffer);
            rI->tInfo.transfers[*index].data.buffer = NULL;
            rI->tInfo.transfers[*index].data.len = 0;
            rI->tInfo.transfers[*index].data.buflen = 0;
        }
    }

    if (!running_handles){
        printf("Downloading done!\n");
        CleanupTransferInfo(rI);
    }

    return 0;
}

void SetDefaultsRequestInfo(RequestInfo_t *rI){
    rI->target = 8;
    rI->limit = 12;
    rI->page = 1;
    rI->sort = 0;
    rI->order = 0;
    rI->search = CopyTextUtil("");
    rI->maxDls = 12;
    rI->includeNSFW = false;
}

static void FreePageCacheEntry(PageCacheEntry_t *entry){
    if (!entry || !entry->isLoaded)
        return;

    if (entry->themes){
        for (int i = 0; i < entry->curPageItemCount; i++){
            NNFREE(entry->themes[i].id);
            NNFREE(entry->themes[i].creator);
            NNFREE(entry->themes[i].name);
            NNFREE(entry->themes[i].description);
            NNFREE(entry->themes[i].lastUpdated);
            NNFREE(entry->themes[i].imgLink);
            NNFREE(entry->themes[i].thumbLink);
            NNFREE(entry->themes[i].downloadLink);
            if (entry->themes[i].preview && entry->packs == NULL)
                SDL_DestroyTexture(entry->themes[i].preview);
        }
        NNFREE(entry->themes);
    }

    if (entry->packs){
        for (int i = 0; i < entry->curPageItemCount; i++){
            free(entry->packs[i].creator);
            free(entry->packs[i].name);
            if (entry->packs[i].preview)
                SDL_DestroyTexture(entry->packs[i].preview);
            if (entry->packs[i].themes){
                for (int j = 0; j < entry->packs[i].themeCount; j++){
                    free(entry->packs[i].themes[j].id);
                    free(entry->packs[i].themes[j].creator);
                    free(entry->packs[i].themes[j].name);
                    free(entry->packs[i].themes[j].description);
                    free(entry->packs[i].themes[j].lastUpdated);
                    free(entry->packs[i].themes[j].imgLink);
                    free(entry->packs[i].themes[j].thumbLink);
                    free(entry->packs[i].themes[j].downloadLink);
                    if (entry->packs[i].themes[j].preview)
                        SDL_DestroyTexture(entry->packs[i].themes[j].preview);
                }
                free(entry->packs[i].themes);
            }
        }
        NNFREE(entry->packs);
    }

    if (entry->listItems)
        ShapeLinkDispose(&entry->listItems);

    entry->isLoaded = false;
    entry->isLoading = false;
}

PageCache_t *InitPageCache(void){
    PageCache_t *cache = calloc(1, sizeof(PageCache_t));
    if (cache){
        cache->count = 0;
        cache->jsonTransferer = curl_multi_init();
        cache->jsonQueueOffset = 0;
        cache->jsonActive = false;
        for (int i = 0; i < PAGE_CACHE_SIZE; i++){
            cache->entries[i].page = -1;
            cache->entries[i].isLoaded = false;
            cache->entries[i].isLoading = false;
        }
        for (int i = 0; i < MAX_PRELOAD_JOBS; i++)
            cache->preloadPages[i] = -1;
    }
    return cache;
}

void FreePageCache(PageCache_t *cache){
    if (!cache)
        return;
    for (int i = 0; i < PAGE_CACHE_SIZE; i++)
        FreePageCacheEntry(&cache->entries[i]);
    if (cache->jsonTransferer){
        for (int i = 0; i < cache->jsonQueueOffset; i++){
            if (cache->jsonTransfers[i].transfer){
                curl_multi_remove_handle(cache->jsonTransferer, cache->jsonTransfers[i].transfer);
                curl_easy_cleanup(cache->jsonTransfers[i].transfer);
            }
            free(cache->jsonTransfers[i].data.buffer);
        }
        curl_multi_cleanup(cache->jsonTransferer);
    }
    free(cache);
}

void ClearPageCache(PageCache_t *cache){
    if (!cache)
        return;

    for (int i = 0; i < cache->jsonQueueOffset; i++){
        if (cache->jsonTransfers[i].transfer){
            curl_multi_remove_handle(cache->jsonTransferer, cache->jsonTransfers[i].transfer);
            curl_easy_cleanup(cache->jsonTransfers[i].transfer);
        }
        free(cache->jsonTransfers[i].data.buffer);
        cache->jsonTransfers[i].data.buffer = NULL;
        cache->jsonTransfers[i].transfer = NULL;
        cache->jsonTransfers[i].data.len = 0;
        cache->jsonTransfers[i].data.buflen = 0;
        cache->preloadPages[i] = -1;
    }
    cache->jsonQueueOffset = 0;
    cache->jsonActive = false;

    for (int i = 0; i < PAGE_CACHE_SIZE; i++){
        FreePageCacheEntry(&cache->entries[i]);
        cache->entries[i].page = -1;
    }
    cache->count = 0;
}

PageCacheEntry_t *FindPageCache(PageCache_t *cache, int page){
    if (!cache)
        return NULL;
    for (int i = 0; i < PAGE_CACHE_SIZE; i++){
        if (cache->entries[i].page == page && cache->entries[i].isLoaded)
            return &cache->entries[i];
    }
    return NULL;
}

static PageCacheEntry_t *FindEmptyCacheSlot(PageCache_t *cache){
    for (int i = 0; i < PAGE_CACHE_SIZE; i++){
        if (cache->entries[i].page == -1)
            return &cache->entries[i];
    }
    return NULL;
}

void EvictFurthestPage(PageCache_t *cache, int currentPage){
    if (!cache || cache->count == 0)
        return;

    int furthestIndex = -1;
    int furthestDist = -1;

    for (int i = 0; i < PAGE_CACHE_SIZE; i++){
        if (cache->entries[i].page != -1 && cache->entries[i].isLoaded && cache->entries[i].page != currentPage){
            int dist = abs(cache->entries[i].page - currentPage);
            if (dist > furthestDist){
                furthestDist = dist;
                furthestIndex = i;
            }
        }
    }

    if (furthestIndex >= 0 && cache->entries[furthestIndex].page != currentPage){
        FreePageCacheEntry(&cache->entries[furthestIndex]);
        cache->entries[furthestIndex].page = -1;
        cache->count--;
    }
}

int StorePageInCache(PageCache_t *cache, RequestInfo_t *rI, ShapeLinker_t *items){
    if (!cache || !rI || !rI->themes)
        return -1;

    if (FindPageCache(cache, rI->page))
        return 0;

    PageCacheEntry_t *slot = FindEmptyCacheSlot(cache);
    if (!slot){
        EvictFurthestPage(cache, rI->page);
        slot = FindEmptyCacheSlot(cache);
        if (!slot)
            return -1;
    }

    slot->page = rI->page;
    slot->pageCount = rI->pageCount;
    slot->itemCount = rI->itemCount;
    slot->curPageItemCount = rI->curPageItemCount;

    slot->themes = calloc(sizeof(ThemeInfo_t), rI->curPageItemCount);
    if (slot->themes){
        for (int i = 0; i < rI->curPageItemCount; i++){
            slot->themes[i].id = rI->themes[i].id ? CopyTextUtil(rI->themes[i].id) : NULL;
            slot->themes[i].creator = rI->themes[i].creator ? CopyTextUtil(rI->themes[i].creator) : NULL;
            slot->themes[i].name = rI->themes[i].name ? CopyTextUtil(rI->themes[i].name) : NULL;
            slot->themes[i].description = rI->themes[i].description ? CopyTextUtil(rI->themes[i].description) : NULL;
            slot->themes[i].lastUpdated = rI->themes[i].lastUpdated ? CopyTextUtil(rI->themes[i].lastUpdated) : NULL;
            slot->themes[i].imgLink = rI->themes[i].imgLink ? CopyTextUtil(rI->themes[i].imgLink) : NULL;
            slot->themes[i].thumbLink = rI->themes[i].thumbLink ? CopyTextUtil(rI->themes[i].thumbLink) : NULL;
            slot->themes[i].downloadLink = rI->themes[i].downloadLink ? CopyTextUtil(rI->themes[i].downloadLink) : NULL;
            slot->themes[i].dlCount = rI->themes[i].dlCount;
            slot->themes[i].likeCount = rI->themes[i].likeCount;
            slot->themes[i].target = rI->themes[i].target;
            slot->themes[i].preview = rI->themes[i].preview;
        }
    }

    if (rI->packs){
        slot->packs = calloc(sizeof(PackInfo_t), rI->curPageItemCount);
        if (slot->packs){
            for (int i = 0; i < rI->curPageItemCount; i++){
                slot->packs[i].creator = rI->packs[i].creator ? CopyTextUtil(rI->packs[i].creator) : NULL;
                slot->packs[i].name = rI->packs[i].name ? CopyTextUtil(rI->packs[i].name) : NULL;
                slot->packs[i].imgLink = rI->packs[i].imgLink ? CopyTextUtil(rI->packs[i].imgLink) : NULL;
                slot->packs[i].thumbLink = rI->packs[i].thumbLink ? CopyTextUtil(rI->packs[i].thumbLink) : NULL;
                slot->packs[i].preview = rI->packs[i].preview;
                slot->packs[i].themeCount = rI->packs[i].themeCount;
                slot->packs[i].isDlDone = rI->packs[i].isDlDone;
                if (rI->packs[i].themes && rI->packs[i].themeCount > 0){
                    slot->packs[i].themes = calloc(sizeof(ThemeInfo_t), rI->packs[i].themeCount);
                    if (slot->packs[i].themes){
                        for (int j = 0; j < rI->packs[i].themeCount; j++){
                            slot->packs[i].themes[j].id = rI->packs[i].themes[j].id ? CopyTextUtil(rI->packs[i].themes[j].id) : NULL;
                            slot->packs[i].themes[j].creator = rI->packs[i].themes[j].creator ? CopyTextUtil(rI->packs[i].themes[j].creator) : NULL;
                            slot->packs[i].themes[j].name = rI->packs[i].themes[j].name ? CopyTextUtil(rI->packs[i].themes[j].name) : NULL;
                            slot->packs[i].themes[j].description = rI->packs[i].themes[j].description ? CopyTextUtil(rI->packs[i].themes[j].description) : NULL;
                            slot->packs[i].themes[j].lastUpdated = rI->packs[i].themes[j].lastUpdated ? CopyTextUtil(rI->packs[i].themes[j].lastUpdated) : NULL;
                            slot->packs[i].themes[j].imgLink = rI->packs[i].themes[j].imgLink ? CopyTextUtil(rI->packs[i].themes[j].imgLink) : NULL;
                            slot->packs[i].themes[j].thumbLink = rI->packs[i].themes[j].thumbLink ? CopyTextUtil(rI->packs[i].themes[j].thumbLink) : NULL;
                            slot->packs[i].themes[j].downloadLink = rI->packs[i].themes[j].downloadLink ? CopyTextUtil(rI->packs[i].themes[j].downloadLink) : NULL;
                            slot->packs[i].themes[j].dlCount = rI->packs[i].themes[j].dlCount;
                            slot->packs[i].themes[j].likeCount = rI->packs[i].themes[j].likeCount;
                            slot->packs[i].themes[j].target = rI->packs[i].themes[j].target;
                            slot->packs[i].themes[j].preview = rI->packs[i].themes[j].preview;
                        }
                    }
                }
            }
        }
    }

    slot->listItems = items;
    slot->isLoaded = true;
    slot->isLoading = false;
    cache->count++;

    rI->themesCached = true;
    return 0;
}

int StartPagePreload(PageCache_t *cache, RequestInfo_t *rI, int page){
    if (!cache || !rI || cache->jsonQueueOffset >= MAX_PRELOAD_JOBS)
        return -1;

    if (FindPageCache(cache, page))
        return 0;

    for (int i = 0; i < PAGE_CACHE_SIZE; i++){
        if (cache->entries[i].page == page && cache->entries[i].isLoading)
            return 0;
    }

    char *url = GenPageLink(rI, page);
    if (!url)
        return -1;

    int idx = cache->jsonQueueOffset;

    CURL *curl = CreateRequest(url, &cache->jsonTransfers[idx].data);
    if (!curl){
        free(url);
        return -1;
    }

    free(url);

    cache->jsonTransfers[idx].transfer = curl;
    cache->jsonTransfers[idx].index = idx;
    cache->preloadPages[idx] = page;
    cache->jsonQueueOffset++;

    curl_multi_add_handle(cache->jsonTransferer, curl);
    cache->jsonActive = true;

    for (int i = 0; i < PAGE_CACHE_SIZE; i++){
        if (cache->entries[i].page == -1){
            cache->entries[i].page = page;
            cache->entries[i].isLoaded = false;
            cache->entries[i].isLoading = true;
            break;
        }
    }

    return 0;
}

static int ParsePageJson(const char *buffer, int target, int limit, int page,
                         ThemeInfo_t **outThemes, PackInfo_t **outPacks,
                         int *outPageCount, int *outItemCount, int *outCurPageItemCount){
    cJSON *root = cJSON_Parse(buffer);
    if (!root)
        return -1;

    if (hasError(root)){
        cJSON_Delete(root);
        return -4;
    }

    int res = -1;
    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (data){
        cJSON *switchObj = cJSON_GetObjectItem(data, "switch");
        if (switchObj){
            cJSON *queryData;
            if (target != 0)
                queryData = cJSON_GetObjectItem(switchObj, "themes");
            else
                queryData = cJSON_GetObjectItem(switchObj, "packs");

            cJSON *pagination = cJSON_GetObjectItem(queryData, "pageInfo");
            cJSON *page_count = cJSON_GetObjectItem(pagination, "pageCount");
            cJSON *item_count = cJSON_GetObjectItem(pagination, "itemCount");

            if (cJSON_IsNumber(page_count) && cJSON_IsNumber(item_count)){
                *outPageCount = page_count->valueint;
                *outItemCount = item_count->valueint;
            } else {
                cJSON_Delete(root);
                return -1;
            }

            *outCurPageItemCount = MIN(limit, *outItemCount - limit * (page - 1));

            if (*outItemCount <= 0){
                cJSON_Delete(root);
                return 0;
            }

            cJSON *nodes = cJSON_GetObjectItem(queryData, "nodes");
            if (target != 0){
                if (nodes){
                    if (ParseThemeList(outThemes, *outCurPageItemCount, nodes))
                        res = -3;
                    else
                        res = 0;
                }
            } else {
                if (nodes){
                    if (ParsePackList(outPacks, *outCurPageItemCount, nodes))
                        res = -3;
                    else {
                        *outThemes = calloc(sizeof(ThemeInfo_t), *outCurPageItemCount);
                        for (int i = 0; i < *outCurPageItemCount; i++){
                            (*outThemes)[i].name = CopyTextUtil((*outPacks)[i].name);
                            (*outThemes)[i].creator = CopyTextUtil((*outPacks)[i].creator);
                            (*outThemes)[i].thumbLink = CopyTextUtil((*outPacks)[i].thumbLink);
                            (*outThemes)[i].imgLink = CopyTextUtil((*outPacks)[i].imgLink);
                            (*outThemes)[i].preview = (*outPacks)[i].preview;
                        }
                        res = 0;
                    }
                }
            }
        }
    }

    cJSON_Delete(root);
    return res;
}

static ShapeLinker_t *BuildListItemsFromThemes(ThemeInfo_t *themes, int count){
    ShapeLinker_t *link = NULL;
    for (int i = 0; i < count; i++)
        ShapeLinkAdd(&link, ListItemCreate(COLOR_WHITE, COLOR_VERYLIGHTGREY_RGBA, themes[i].preview, themes[i].name, themes[i].creator), ListItemType);
    return link;
}

void PumpPagePreloads(PageCache_t *cache){
    if (!cache || !cache->jsonActive)
        return;

    int running_handles = 0;
    curl_multi_perform(cache->jsonTransferer, &running_handles);

    int msgs_left = -1;
    struct CURLMsg *msg;
    while ((msg = curl_multi_info_read(cache->jsonTransferer, &msgs_left))){
        if (msg->msg == CURLMSG_DONE){
            CURL *e = msg->easy_handle;
            int idx = -1;

            for (int i = 0; i < cache->jsonQueueOffset; i++){
                if (cache->jsonTransfers[i].transfer == e){
                    idx = i;
                    break;
                }
            }
            if (idx < 0) continue;

            int page = cache->preloadPages[idx];

            if (msg->data.result == CURLE_OK){
                char *jsonStr = (char *)cache->jsonTransfers[idx].data.buffer;
                ThemeInfo_t *themes = NULL;
                PackInfo_t *packs = NULL;
                int pageCount = 0, itemCount = 0, curPageItemCount = 0;

                if (!ParsePageJson(jsonStr, 
                    cache->currentTarget, cache->currentLimit, page,
                    &themes, &packs, &pageCount, &itemCount, &curPageItemCount)){
                    
                    PageCacheEntry_t *slot = NULL;
                    for (int i = 0; i < PAGE_CACHE_SIZE; i++){
                        if (cache->entries[i].page == page && cache->entries[i].isLoading){
                            slot = &cache->entries[i];
                            break;
                        }
                    }
                    if (!slot){
                        for (int i = 0; i < PAGE_CACHE_SIZE; i++){
                            if (cache->entries[i].page == -1){
                                slot = &cache->entries[i];
                                slot->page = page;
                                break;
                            }
                        }
                    }
                    if (!slot){
                        // evict furthest
                        PageCacheEntry_t *toEvict = NULL;
                        int maxDist = -1;
                        for (int i = 0; i < PAGE_CACHE_SIZE; i++){
                            if (cache->entries[i].page != -1 && cache->entries[i].isLoaded && cache->entries[i].page != page){
                                int dist = abs(cache->entries[i].page - page);
                                if (dist > maxDist){
                                    maxDist = dist;
                                    toEvict = &cache->entries[i];
                                }
                            }
                        }
                        if (toEvict){
                            FreePageCacheEntry(toEvict);
                            toEvict->page = -1;
                            cache->count--;
                            slot = toEvict;
                            slot->page = page;
                        }
                    }

                    if (slot){
                        slot->pageCount = pageCount;
                        slot->itemCount = itemCount;
                        slot->curPageItemCount = curPageItemCount;
                        slot->themes = themes;
                        slot->packs = packs;
                        slot->listItems = BuildListItemsFromThemes(themes, curPageItemCount);
                        slot->isLoaded = true;
                        slot->isLoading = false;
                        cache->count++;
                    } else {
                        // cleanup if we couldn't find a slot
                        if (themes){
                            for (int i = 0; i < curPageItemCount; i++){
                                free(themes[i].id);
                                free(themes[i].creator);
                                free(themes[i].name);
                                free(themes[i].description);
                                free(themes[i].lastUpdated);
                                free(themes[i].imgLink);
                                free(themes[i].thumbLink);
                                free(themes[i].downloadLink);
                                if (themes[i].preview && packs == NULL) SDL_DestroyTexture(themes[i].preview);
                            }
                            free(themes);
                        }
                        if (packs){
                            for (int i = 0; i < curPageItemCount; i++){
                                free(packs[i].creator);
                                free(packs[i].name);
                                if (packs[i].preview) SDL_DestroyTexture(packs[i].preview);
                                if (packs[i].themes){
                                    for (int j = 0; j < packs[i].themeCount; j++){
                                        free(packs[i].themes[j].id);
                                        free(packs[i].themes[j].creator);
                                        free(packs[i].themes[j].name);
                                        free(packs[i].themes[j].description);
                                        free(packs[i].themes[j].lastUpdated);
                                        free(packs[i].themes[j].imgLink);
                                        free(packs[i].themes[j].thumbLink);
                                        free(packs[i].themes[j].downloadLink);
                                        if (packs[i].themes[j].preview) SDL_DestroyTexture(packs[i].themes[j].preview);
                                    }
                                    free(packs[i].themes);
                                }
                            }
                            free(packs);
                        }
                    }
                }
            }

            curl_multi_remove_handle(cache->jsonTransferer, e);
            curl_easy_cleanup(e);
            cache->jsonTransfers[idx].transfer = NULL;
            free(cache->jsonTransfers[idx].data.buffer);
            cache->jsonTransfers[idx].data.buffer = NULL;
            cache->jsonTransfers[idx].data.len = 0;
            cache->jsonTransfers[idx].data.buflen = 0;
            cache->preloadPages[idx] = -1;
        }
    }

    // Compact the transfer array
    int writeIdx = 0;
    for (int i = 0; i < cache->jsonQueueOffset; i++){
        if (cache->jsonTransfers[i].transfer != NULL){
            if (writeIdx != i){
                cache->jsonTransfers[writeIdx] = cache->jsonTransfers[i];
                cache->preloadPages[writeIdx] = cache->preloadPages[i];
            }
            writeIdx++;
        }
    }
    cache->jsonQueueOffset = writeIdx;

    if (!running_handles && cache->jsonQueueOffset == 0)
        cache->jsonActive = false;
}

void TriggerPagePreloads(PageCache_t *cache, RequestInfo_t *rI){
    if (!cache || !rI || rI->pageCount <= 0)
        return;

    cache->currentTarget = rI->target;
    cache->currentLimit = rI->limit;

    // 只预加载下一页
    int nextPage = rI->page + 1;
    if (nextPage <= rI->pageCount)
        StartPagePreload(cache, rI, nextPage);
}

int HandleMainMenuFrame(Context_t *ctx){
    ShapeLinker_t *all = ctx->all;
    RequestInfo_t *rI = ShapeLinkFind(all, DataType)->item;
    PageCache_t *cache = (PageCache_t *)rI->pageCache;
    if (cache)
        PumpPagePreloads(cache);
    HandleDownloadQueue(ctx);
    return 0;
}
