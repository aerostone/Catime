/**
 * @file config_applier_notification.c
 * @brief Apply notification/timeout-alert settings from a snapshot.
 *
 * Split from config_applier.c to keep first-party files reviewable.
 * Public entry points declared in config/config_applier.h.
 */
#include "config/config_applier.h"
#include "config.h"
#include "config/config_plugin_security.h"
#include "color/color.h"
#include "../resource/resource.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ApplyGeneralSettings(const ConfigSnapshot* snapshot) {
    if (!snapshot) return;
    g_AppConfig.font_license.accepted = snapshot->fontLicenseAccepted;
    strncpy(g_AppConfig.font_license.version_accepted, snapshot->fontLicenseVersion,
           sizeof(g_AppConfig.font_license.version_accepted) - 1);
    g_AppConfig.font_license.version_accepted[sizeof(g_AppConfig.font_license.version_accepted) - 1] = '\0';
    LoadPluginTrustFromConfig();
}

void ApplyNotificationSettings(const ConfigSnapshot* snapshot) {
    if (!snapshot) return;
    strncpy(g_AppConfig.notification.messages.timeout_message, snapshot->timeoutMessage,
           sizeof(g_AppConfig.notification.messages.timeout_message) - 1);
    g_AppConfig.notification.messages.timeout_message[sizeof(g_AppConfig.notification.messages.timeout_message) - 1] = '\0';
    g_AppConfig.notification.display.timeout_ms = snapshot->notificationTimeoutMs;
    g_AppConfig.notification.display.max_opacity = snapshot->notificationMaxOpacity;
    g_AppConfig.notification.display.corner_radius = snapshot->notificationCornerRadius;
    g_AppConfig.notification.display.font_size = snapshot->notificationFontSize;
    g_AppConfig.notification.display.type = snapshot->notificationType;
    g_AppConfig.notification.display.disabled = snapshot->notificationDisabled;
    g_AppConfig.notification.display.fullscreen_timeout = snapshot->notificationFullscreenTimeout;
    {
        int op = snapshot->notificationFullscreenOpacity;
        if (op < MIN_FS_TIMEOUT_OPACITY) op = MIN_FS_TIMEOUT_OPACITY;
        if (op > MAX_FS_TIMEOUT_OPACITY) op = MAX_FS_TIMEOUT_OPACITY;
        g_AppConfig.notification.display.fullscreen_opacity = op;
        /* hex "RRGGBB" (allow leading '#') -> COLORREF int */
        const char *hx = snapshot->notificationFullscreenBgColor;
        if (hx && hx[0] == '#') hx++;
        unsigned long cv = 0;
        if (hx && hx[0]) {
            char *end = NULL;
            cv = strtoul(hx, &end, 16);
            if (!end || end == hx) cv = 0;
        }
        int rr = (int)((cv >> 16) & 0xFF), gg = (int)((cv >> 8) & 0xFF), bb = (int)(cv & 0xFF);
        g_AppConfig.notification.display.fullscreen_bgcolor = RGB(rr, gg, bb);
        wchar_t fn[64] = L"Microsoft YaHei";
        if (snapshot->notificationFullscreenFontName[0] &&
            !MultiByteToWideChar(CP_UTF8, 0, snapshot->notificationFullscreenFontName, -1, fn, _countof(fn))) {
            wcsncpy_s(fn, _countof(fn), L"Microsoft YaHei", _TRUNCATE);
        }
        wcsncpy_s(g_AppConfig.notification.display.fullscreen_fontname,
                  _countof(g_AppConfig.notification.display.fullscreen_fontname), fn, _TRUNCATE);
        int tp = snapshot->notificationFullscreenTitleSize;
        if (tp < MIN_FS_TIMEOUT_FONT_PX) tp = MIN_FS_TIMEOUT_FONT_PX;
        if (tp > MAX_FS_TIMEOUT_FONT_PX) tp = MAX_FS_TIMEOUT_FONT_PX;
        g_AppConfig.notification.display.fullscreen_title_px = tp;
        int mp = snapshot->notificationFullscreenMsgSize;
        if (mp < MIN_FS_TIMEOUT_FONT_PX) mp = MIN_FS_TIMEOUT_FONT_PX;
        if (mp > MAX_FS_TIMEOUT_FONT_PX) mp = MAX_FS_TIMEOUT_FONT_PX;
        g_AppConfig.notification.display.fullscreen_msg_px = mp;
    }
    g_AppConfig.notification.display.window_x = snapshot->notificationWindowX;
    g_AppConfig.notification.display.window_y = snapshot->notificationWindowY;
    g_AppConfig.notification.display.window_width = snapshot->notificationWindowWidth;
    g_AppConfig.notification.display.window_height = snapshot->notificationWindowHeight;
    char resolvedSoundPath[MAX_PATH] = {0};
    const char* soundPathToApply = snapshot->notificationSoundFile;
    if (ExpandEffectiveLocalAppDataPath(snapshot->notificationSoundFile,
                                        resolvedSoundPath,
                                        sizeof(resolvedSoundPath))) {
        soundPathToApply = resolvedSoundPath;
    }
    strncpy(g_AppConfig.notification.sound.sound_file, soundPathToApply, MAX_PATH - 1);
    g_AppConfig.notification.sound.sound_file[MAX_PATH - 1] = '\0';
    g_AppConfig.notification.sound.volume = snapshot->notificationSoundVolume;
}

void ApplyColorSettings(const ConfigSnapshot* snapshot) {
    if (!snapshot) return;
    if (!ReplaceColorOptionsFromConfigValue(snapshot->colorOptions) &&
        !ReplaceColorOptionsFromConfigValue(DEFAULT_COLOR_OPTIONS_INI)) {
        LOG_WARNING("Failed to apply color options from config and defaults");
    }
}
