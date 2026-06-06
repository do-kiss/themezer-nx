#include "model.h"

const char *targetOptions[] = {
    "主题包",
    "主页菜单",
    "锁屏界面",
    "所有应用",
    "设置界面",
    "玩家选择",
    "用户页面",
    "新闻界面",
    "全部"
};

// 文件路径用英文名，避免 Switch 文件系统中文路径写失败
const char *targetFileNames[] = {
    "HomeMenu",
    "Lockscreen",
    "AllApps",
    "Settings",
    "PlayerSelect",
    "UserPage",
    "News"
};

const char *sortOptions[] = {
    "创建时间",
    "更新时间",
    "下载量",
    "收藏量"
};

const char *orderOptions[] = {
    "降序",
    "升序"
};