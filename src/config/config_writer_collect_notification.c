/**
 * @file config_writer_collect_notification.c
 * @brief Notification configuration collection
 */
#include "config_writer_internal.h"

#include "config.h"

static const char* NotificationTypeToString(NotificationType type) {
    switch (type) {
        case NOTIFICATION_TYPE_SYSTEM_MODAL: return "SYSTEM_MODAL";
        case NOTIFICATION_TYPE_OS: return "OS";
        case NOTIFICATION_TYPE_CATIME:
        default: return "CATIME";
    }
}

BOOL ConfigWriter_CollectNotification(ConfigItemBuilder* builder) {
    NotificationConfig* notification = &g_AppConfig.notification;

    if (!ConfigWriter_AppendString(
            builder, INI_SECTION_NOTIFICATION,
            "CLOCK_TIMEOUT_MESSAGE_TEXT",
            notification->messages.timeout_message) ||
        !ConfigWriter_AppendInt(builder, INI_SECTION_NOTIFICATION,
                                "NOTIFICATION_TIMEOUT_MS",
                                notification->display.timeout_ms) ||
        !ConfigWriter_AppendInt(builder, INI_SECTION_NOTIFICATION,
                                "NOTIFICATION_MAX_OPACITY",
                                notification->display.max_opacity) ||
        !ConfigWriter_AppendInt(builder, INI_SECTION_NOTIFICATION,
                                "NOTIFICATION_CORNER_RADIUS",
                                notification->display.corner_radius) ||
        !ConfigWriter_AppendInt(builder, INI_SECTION_NOTIFICATION,
                                "NOTIFICATION_FONT_SIZE",
                                notification->display.font_size)) {
        return FALSE;
    }

    if (!ConfigWriter_AppendString(
            builder, INI_SECTION_NOTIFICATION, "NOTIFICATION_TYPE",
            NotificationTypeToString(notification->display.type)) ||
        !ConfigWriter_AppendString(
            builder, INI_SECTION_NOTIFICATION, "NOTIFICATION_SOUND_FILE",
            notification->sound.sound_file) ||
        !ConfigWriter_AppendInt(builder, INI_SECTION_NOTIFICATION,
                                "NOTIFICATION_SOUND_VOLUME",
                                notification->sound.volume) ||
        !ConfigWriter_AppendBool(builder, INI_SECTION_NOTIFICATION,
                                 "NOTIFICATION_DISABLED",
                                 notification->display.disabled) ||
        !ConfigWriter_AppendBool(builder, INI_SECTION_NOTIFICATION,
                                 "NOTIFICATION_FULLSCREEN_TIMEOUT",
                                 notification->display.fullscreen_timeout) ||
        !ConfigWriter_AppendInt(builder, INI_SECTION_NOTIFICATION,
                                "NOTIFICATION_FS_OPACITY",
                                notification->display.fullscreen_opacity)) {
        return FALSE;
    }
    {
        /* COLORREF -> "RRGGBB" */
        char hex[16] = "";
        _snprintf_s(hex, sizeof(hex), _TRUNCATE, "%02X%02X%02X",
                    GetRValue(notification->display.fullscreen_bgcolor),
                    GetGValue(notification->display.fullscreen_bgcolor),
                    GetBValue(notification->display.fullscreen_bgcolor));
        char fontA[64] = "";
        WideCharToMultiByte(CP_UTF8, 0, notification->display.fullscreen_fontname, -1,
                            fontA, sizeof(fontA), NULL, NULL);
        if (!ConfigWriter_AppendString(builder, INI_SECTION_NOTIFICATION,
                                       "NOTIFICATION_FS_BGCOLOR", hex) ||
            !ConfigWriter_AppendString(builder, INI_SECTION_NOTIFICATION,
                                       "NOTIFICATION_FS_FONTNAME", fontA[0] ? fontA : "Microsoft YaHei") ||
            !ConfigWriter_AppendInt(builder, INI_SECTION_NOTIFICATION,
                                    "NOTIFICATION_FS_TITLE_PX",
                                    notification->display.fullscreen_title_px) ||
            !ConfigWriter_AppendInt(builder, INI_SECTION_NOTIFICATION,
                                    "NOTIFICATION_FS_MSG_PX",
                                    notification->display.fullscreen_msg_px)) {
            return FALSE;
        }
    }

    return ConfigWriter_AppendInt(builder, INI_SECTION_NOTIFICATION,
                                  "NOTIFICATION_WINDOW_X",
                                  notification->display.window_x) &&
           ConfigWriter_AppendInt(builder, INI_SECTION_NOTIFICATION,
                                  "NOTIFICATION_WINDOW_Y",
                                  notification->display.window_y) &&
           ConfigWriter_AppendInt(builder, INI_SECTION_NOTIFICATION,
                                  "NOTIFICATION_WINDOW_WIDTH",
                                  notification->display.window_width) &&
           ConfigWriter_AppendInt(builder, INI_SECTION_NOTIFICATION,
                                  "NOTIFICATION_WINDOW_HEIGHT",
                                  notification->display.window_height);
}
