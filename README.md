# themezer-nx
A Switch theme downloader that pulls from https://themezer.net/, and installs themes using the NXThemesInstaller.

## NX 主题下载器 (汉化版)
Fork 自 [suchmememanyskill/themezer-nx](https://github.com/suchmememanyskill/themezer-nx)。

## 汉化内容
- 所有菜单、按钮、弹窗、提示文字均已汉化为简体中文
- 使用 Switch 系统简体中文字体
- 支持中文关键词搜索
- 新增"儿童不宜内容"过滤开关
- 多项性能优化（ThumbHash LRU 缓存、cJSON 解析加速、网络 I/O 优化）

## 编译
1. 编译 JAGL：`cd JAGL && make clean && make`
2. 编译主程序：`cd themezer-nx && make clean && make`

## 原始 README
A switch theme downloader that pulls from https://themezer.net/, and installs themes using the NXThemesInstaller

[![Build Themezer-nx](https://github.com/suchmememanyskill/themezer-nx/workflows/Build%20Themezer-nx/badge.svg)](https://github.com/suchmememanyskill/themezer-nx/actions)
[![dlCount](https://img.shields.io/github/downloads/suchmememanyskill/themezer-nx/total?color=blue)](https://github.com/suchmememanyskill/themezer-nx/releases)
[![version](https://img.shields.io/github/v/release/suchmememanyskill/themezer-nx)](https://github.com/suchmememanyskill/themezer-nx/releases)


![screenshot](screenshot.jpg)

## How to use
1. Download the latest nro release in the [releases tab](https://github.com/suchmememanyskill/themezer-nx/releases) and put it in the switch folder 
2. Download the latest [NXThemesInstaller.nro](https://github.com/exelix11/SwitchThemeInjector/releases) and put it in the switch folder, if you haven't already. (The releases from the appstore should work as well)
3. Make sure your switch is connected to the internet (if you're looking to connect online without connecting to nintendo, see [90DNS](https://nh-server.github.io/switch-guide/extras/blocking_updates/))
4. Open the app in the homebrew menu

## "Installing" a Theme
When you select "Install theme" the app will queue the install until you exit via + or via the 'exit themezer-nx' button inside the target menu. Then themezer-nx opens the NXThemesInstaller to install your selected theme(s) after it exits.

## Support
For support you can go to the [Themezer discord](https://discord.gg/bBCw6tF)

## Credits
- [Exelix11](https://github.com/exelix11) for helping me with cURL and being awesome in general
- [cJSON](https://github.com/DaveGamble/cJSON) For the json lib used in this project
