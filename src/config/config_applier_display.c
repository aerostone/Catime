/**
 * @file config_applier_display.c
 * @brief Apply display/window settings from a configuration snapshot.
 *
 * Split from config_applier.c to keep first-party files reviewable.
 * Public entry points declared in config/config_applier.h.
 */
#include "config/config_applier.h"
#include "config.h"
#include "window.h"
#include "font.h"
#include "drawing/drawing_effect.h"
#include "text_effect.h"
#include "taskbar_monitor.h"
#include <string.h>

extern TextEffectType CLOCK_TEXT_EFFECT;

void ApplyDisplaySettings(const ConfigSnapshot* snapshot) {
    if (!snapshot) return;
    strncpy(CLOCK_TEXT_COLOR, snapshot->textColor, sizeof(CLOCK_TEXT_COLOR) - 1);
    CLOCK_TEXT_COLOR[sizeof(CLOCK_TEXT_COLOR) - 1] = '\0';
    CLOCK_BASE_FONT_SIZE = snapshot->baseFontSize;
    strncpy(FONT_FILE_NAME, snapshot->fontFileName, sizeof(FONT_FILE_NAME) - 1);
    FONT_FILE_NAME[sizeof(FONT_FILE_NAME) - 1] = '\0';
    strncpy(FONT_RUNTIME_FILE_NAME, snapshot->fontFileName,
            sizeof(FONT_RUNTIME_FILE_NAME) - 1);
    FONT_RUNTIME_FILE_NAME[sizeof(FONT_RUNTIME_FILE_NAME) - 1] = '\0';
    strncpy(FONT_INTERNAL_NAME, snapshot->fontInternalName, sizeof(FONT_INTERNAL_NAME) - 1);
    FONT_INTERNAL_NAME[sizeof(FONT_INTERNAL_NAME) - 1] = '\0';
    CLOCK_WINDOW_SCALE = snapshot->windowScale;
    CLOCK_FONT_SCALE_FACTOR = snapshot->windowScale;
    PLUGIN_FONT_SCALE_FACTOR = snapshot->pluginScale;
    CLOCK_WINDOW_TOPMOST = snapshot->windowTopmost;
    CLOCK_WINDOW_EFFECTIVE_TOPMOST = snapshot->windowTopmost;
    CLOCK_WINDOW_OPACITY = snapshot->windowOpacity;
    g_AppConfig.display.move_step_small = snapshot->moveStepSmall;
    g_AppConfig.display.move_step_large = snapshot->moveStepLarge;
    g_AppConfig.display.opacity_step_normal = snapshot->opacityStepNormal;
    g_AppConfig.display.opacity_step_fast = snapshot->opacityStepFast;
    g_AppConfig.display.scale_step_normal = snapshot->scaleStepNormal;
    g_AppConfig.display.scale_step_fast = snapshot->scaleStepFast;
    TextEffectType previousTextEffect = CLOCK_TEXT_EFFECT;
    CLOCK_TEXT_EFFECT = (TextEffectType)snapshot->textEffect;
    g_AppConfig.display.text_effect = snapshot->textEffect;
    if (TextEffect_UsesSharedEffectBuffer(previousTextEffect) &&
        !TextEffect_UsesSharedEffectBuffer(CLOCK_TEXT_EFFECT)) {
        CleanupDrawingEffects();
    }
    HWND hwnd = FindCurrentProcessMainWindow();
    if (hwnd) {
        if (CLOCK_IS_DRAGGING) {
            RECT currentRect;
            if (GetWindowRect(hwnd, &currentRect)) {
                CLOCK_WINDOW_POS_X = currentRect.left;
                CLOCK_WINDOW_POS_Y = currentRect.top;
            }
        } else if (g_ForceApplyConfig) {
            CLOCK_WINDOW_POS_Y = snapshot->windowPosY;
            if (snapshot->windowPosX != -2 && snapshot->windowPosX != -1) {
                CLOCK_WINDOW_POS_X = snapshot->windowPosX;
                SetWindowPos(hwnd, NULL, CLOCK_WINDOW_POS_X, CLOCK_WINDOW_POS_Y,
                            0, 0, SWP_NOSIZE | SWP_NOZORDER);
            }
        } else {
            RECT currentRect;
            GetWindowRect(hwnd, &currentRect);
            long long deltaX = llabs((long long)currentRect.left -
                                     snapshot->windowPosX);
            long long deltaY = llabs((long long)currentRect.top -
                                     snapshot->windowPosY);
            if (deltaX > 10 || deltaY > 10) {
                CLOCK_WINDOW_POS_X = currentRect.left;
                CLOCK_WINDOW_POS_Y = currentRect.top;
            } else {
                CLOCK_WINDOW_POS_X = snapshot->windowPosX;
                CLOCK_WINDOW_POS_Y = snapshot->windowPosY;
                SetWindowPos(hwnd, NULL, CLOCK_WINDOW_POS_X, CLOCK_WINDOW_POS_Y,
                            0, 0, SWP_NOSIZE | SWP_NOZORDER);
            }
        }
        BYTE alphaValue = (BYTE)((CLOCK_WINDOW_OPACITY * 255) / 100);
        SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), alphaValue, LWA_COLORKEY | LWA_ALPHA);
        if (!CLOCK_IS_DRAGGING) {
            RefreshWindowTopmostState(hwnd);
        }
        if (!CLOCK_IS_DRAGGING) {
            InvalidateRect(hwnd, NULL, TRUE);
        }
    } else {
        CLOCK_WINDOW_POS_X = snapshot->windowPosX;
        CLOCK_WINDOW_POS_Y = snapshot->windowPosY;
    }
    TaskbarMonitor_ApplyConfig(snapshot->taskbarMonitorEnabled,
        snapshot->taskbarMonitorCpuMemory, snapshot->taskbarMonitorNetwork);
}
