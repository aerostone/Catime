/**
 * @file config_applier.c
 * @brief Apply configuration snapshot to global variables
 *
 * Orchestrator: ApplyConfigSnapshot fans out to per-domain appliers
 * (display, notification, and the timer/general appliers below).
 * Timer/general appliers stay here; display lives in
 * config_applier_display.c, general/notification/color in
 * config_applier_notification.c.
 */
#include "config/config_applier.h"
#include "config.h"
#include "config/config_defaults.h"
#include "language.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <ctype.h>

BOOL g_ForceApplyConfig = FALSE;

static int LanguageNameToEnum(const char* langName) {
    if (!langName) return APP_LANG_ENGLISH;
    return GetLanguageFromConfigKey(langName);
}

static void ApplyDefaultPomodoroTimes(void) {
    static const int defaultTimes[] = {
        DEFAULT_POMODORO_WORK,
        DEFAULT_POMODORO_SHORT_BREAK,
        DEFAULT_POMODORO_WORK,
        DEFAULT_POMODORO_LONG_BREAK
    };
    int count = (int)_countof(defaultTimes);
    if (count > (int)_countof(g_AppConfig.pomodoro.times)) {
        count = (int)_countof(g_AppConfig.pomodoro.times);
    }
    ZeroMemory(g_AppConfig.pomodoro.times, sizeof(g_AppConfig.pomodoro.times));
    memcpy(g_AppConfig.pomodoro.times, defaultTimes, (size_t)count * sizeof(defaultTimes[0]));
    g_AppConfig.pomodoro.times_count = count;
    g_AppConfig.pomodoro.work_time = g_AppConfig.pomodoro.times[0];
    g_AppConfig.pomodoro.short_break = g_AppConfig.pomodoro.times[1];
    g_AppConfig.pomodoro.long_break = g_AppConfig.pomodoro.times[2];
}

static int ClampPomodoroLoopCountForApply(int loopCount) {
    if (loopCount < MIN_POMODORO_LOOP_COUNT) return MIN_POMODORO_LOOP_COUNT;
    if (loopCount > MAX_POMODORO_LOOP_COUNT) return MAX_POMODORO_LOOP_COUNT;
    return loopCount;
}

static void ApplyDefaultQuickCountdownOptions(void) {
    static const int defaultOptions[] = {
        DEFAULT_QUICK_COUNTDOWN_1,
        DEFAULT_QUICK_COUNTDOWN_2,
        DEFAULT_QUICK_COUNTDOWN_3
    };
    int count = (int)_countof(defaultOptions);
    if (count > MAX_TIME_OPTIONS) {
        count = MAX_TIME_OPTIONS;
    }
    ZeroMemory(time_options, sizeof(time_options));
    for (int i = 0; i < count; i++) {
        time_options[i] = defaultOptions[i];
    }
    time_options_count = count;
}

static void ApplyQuickCountdownOptions(const ConfigSnapshot* snapshot) {
    int optionsCount = snapshot->timeOptionsCount;
    if (optionsCount <= 0 || optionsCount > MAX_TIME_OPTIONS) {
        LOG_WARNING("Invalid quick countdown options count %d while applying config, using defaults",
                    optionsCount);
        ApplyDefaultQuickCountdownOptions();
        return;
    }
    for (int i = 0; i < optionsCount; i++) {
        if (snapshot->timeOptions[i] <= 0 ||
            snapshot->timeOptions[i] > MAX_TIME_OPTION_SECONDS) {
            LOG_WARNING("Invalid quick countdown preset %d while applying config, using defaults",
                        snapshot->timeOptions[i]);
            ApplyDefaultQuickCountdownOptions();
            return;
        }
    }
    ZeroMemory(time_options, sizeof(time_options));
    for (int i = 0; i < optionsCount; i++) {
        time_options[i] = snapshot->timeOptions[i];
    }
    time_options_count = optionsCount;
}

void ApplyTimerSettings(const ConfigSnapshot* snapshot) {
    if (!snapshot) return;
    g_AppConfig.timer.default_start_time = snapshot->defaultStartTime;
    CLOCK_TOTAL_TIME = snapshot->defaultStartTime;
    CLOCK_USE_24HOUR = snapshot->use24Hour;
    CLOCK_SHOW_SECONDS = snapshot->showSeconds;
    g_AppConfig.display.time_format.format = snapshot->timeFormat;
    g_AppConfig.display.time_format.show_milliseconds = snapshot->showMilliseconds;
    if (CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_SHUTDOWN &&
        CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_RESTART &&
        CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_SLEEP) {
        CLOCK_TIMEOUT_ACTION = snapshot->timeoutAction;
    }
    strncpy(CLOCK_TIMEOUT_TEXT, snapshot->timeoutText, sizeof(CLOCK_TIMEOUT_TEXT) - 1);
    CLOCK_TIMEOUT_TEXT[sizeof(CLOCK_TIMEOUT_TEXT) - 1] = '\0';
    strncpy(CLOCK_TIMEOUT_FILE_PATH, snapshot->timeoutFilePath, MAX_PATH - 1);
    CLOCK_TIMEOUT_FILE_PATH[MAX_PATH - 1] = '\0';
    strncpy(CLOCK_TIMEOUT_WEBSITE_URL, snapshot->timeoutWebsiteUrl, MAX_PATH - 1);
    CLOCK_TIMEOUT_WEBSITE_URL[MAX_PATH - 1] = '\0';
    ApplyQuickCountdownOptions(snapshot);
    strncpy(CLOCK_STARTUP_MODE, snapshot->startupMode, sizeof(CLOCK_STARTUP_MODE) - 1);
    CLOCK_STARTUP_MODE[sizeof(CLOCK_STARTUP_MODE) - 1] = '\0';
}

void ApplyPomodoroSettings(const ConfigSnapshot* snapshot) {
    if (!snapshot) return;
    int loopCount = ClampPomodoroLoopCountForApply(snapshot->pomodoroLoopCount);
    int timesCount = snapshot->pomodoroTimesCount;
    if (timesCount <= 0 || timesCount > (int)_countof(g_AppConfig.pomodoro.times)) {
        LOG_WARNING("Invalid Pomodoro times count %d while applying config, using defaults",
                    timesCount);
        ApplyDefaultPomodoroTimes();
        g_AppConfig.pomodoro.loop_count = loopCount;
        return;
    }
    for (int i = 0; i < timesCount; i++) {
        if (snapshot->pomodoroTimes[i] <= 0 ||
            snapshot->pomodoroTimes[i] > MAX_POMODORO_OPTION_SECONDS) {
            LOG_WARNING("Invalid Pomodoro interval %d while applying config, using defaults",
                        snapshot->pomodoroTimes[i]);
            ApplyDefaultPomodoroTimes();
            g_AppConfig.pomodoro.loop_count = loopCount;
            return;
        }
    }
    g_AppConfig.pomodoro.times_count = timesCount;
    ZeroMemory(g_AppConfig.pomodoro.times, sizeof(g_AppConfig.pomodoro.times));
    for (int i = 0; i < timesCount; i++) {
        g_AppConfig.pomodoro.times[i] = snapshot->pomodoroTimes[i];
    }
    g_AppConfig.pomodoro.work_time = g_AppConfig.pomodoro.times[0];
    if (timesCount > 1) g_AppConfig.pomodoro.short_break = g_AppConfig.pomodoro.times[1];
    if (timesCount > 2) g_AppConfig.pomodoro.long_break = g_AppConfig.pomodoro.times[2];
    g_AppConfig.pomodoro.loop_count = loopCount;
}

void ApplyHotkeySettings(const ConfigSnapshot* snapshot) {
    if (!snapshot) return;
}

void ApplyRecentFilesSettings(const ConfigSnapshot* snapshot) {
    if (!snapshot) return;
    int recentFilesCount = snapshot->recentFilesCount;
    if (recentFilesCount < 0) {
        recentFilesCount = 0;
    }
    if (recentFilesCount > MAX_RECENT_FILES) {
        recentFilesCount = MAX_RECENT_FILES;
    }
    ZeroMemory(g_AppConfig.recent_files.files, sizeof(g_AppConfig.recent_files.files));
    g_AppConfig.recent_files.count = recentFilesCount;
    for (int i = 0; i < recentFilesCount; i++) {
        memcpy(&g_AppConfig.recent_files.files[i], &snapshot->recentFiles[i], sizeof(RecentFile));
    }
}

void ApplyConfigSnapshot(const ConfigSnapshot* snapshot) {
    if (!snapshot) {
        LOG_WARNING("ApplyConfigSnapshot called with NULL snapshot");
        return;
    }
    ApplyGeneralSettings(snapshot);
    ApplyDisplaySettings(snapshot);
    ApplyTimerSettings(snapshot);
    ApplyPomodoroSettings(snapshot);
    ApplyNotificationSettings(snapshot);
    ApplyColorSettings(snapshot);
    ApplyHotkeySettings(snapshot);
    ApplyRecentFilesSettings(snapshot);
    int languageEnum = LanguageNameToEnum(snapshot->language);
    if (languageEnum < 0 || languageEnum >= APP_LANG_COUNT) {
        LOG_WARNING("Invalid language enum %d, using English", languageEnum);
        languageEnum = APP_LANG_ENGLISH;
    }
    SetLanguage((AppLanguage)languageEnum);
    ReloadAnimationSpeedFromConfig();
    g_AppConfig.last_config_time = time(NULL);
}
