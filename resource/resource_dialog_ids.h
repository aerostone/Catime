#ifndef CATIME_RESOURCE_DIALOG_IDS_H
#define CATIME_RESOURCE_DIALOG_IDS_H

/** @brief External notification configuration variables
 * @note NOTIFICATION_MAX_OPACITY and NOTIFICATION_TIMEOUT_MS now in g_AppConfig.notification.display
 */

/** @brief About dialog identifiers */
#define IDD_ABOUT_DIALOG 1050            /**< About dialog */
#define IDC_ABOUT_ICON 1005              /**< About dialog icon */
#define IDC_VERSION_TEXT 1006            /**< Version text control */
#define IDC_LIBS_TEXT 1007               /**< Libraries text control */
#define IDC_AUTHOR_TEXT 1008             /**< Author text control */
#define IDC_ABOUT_OK 1009                /**< About dialog OK button */
#define IDC_BUILD_DATE 1010              /**< Build date control */
#define IDC_COPYRIGHT 1011               /**< Copyright control */
#define IDC_CREDITS_LABEL 1012           /**< Credits label */
#define IDC_CREDIT_LINK 1013             /**< Credit link control */
#define IDS_CREDITS_TEXT 1014            /**< Credits text string */

/** @brief About dialog constants and text */
#define ABOUT_ICON_SIZE 200                              /**< About dialog icon size */
#define IDC_ABOUT_TITLE 1022                             /**< About dialog title control */
#define IDC_ABOUT_TITLE_TEXT L"About Catime"             /**< About dialog title text */
#define IDC_ABOUT_VERSION L"Version: %hs"                /**< Version format string */
#define IDC_BUILD_DATE_TEXT L"Build date: %hs"           /**< Build date format string */
#define IDC_COPYRIGHT_TEXT L"Copyright (C) 2025-2026 By vladelaina"  /**< Copyright text */

/** @brief About dialog link control identifiers */
#define IDC_CREDITS 1015                 /**< Credits control */
#define IDC_FEEDBACK 1016                /**< Feedback link control */
#define IDC_GITHUB 1017                  /**< GitHub link control */
#define IDC_COPYRIGHT_LINK 1018          /**< Copyright link control */
#define IDC_SUPPORT 1019                 /**< Support link control */
#define IDC_BILIBILI_LINK 1020           /**< Bilibili link control */
#define IDC_GITHUB_LINK 1021             /**< GitHub link control */
#define IDC_QQ_GROUP_LINK 1023           /**< Simplified Chinese QQ group link */

/** @brief CLI help dialog identifiers */
#define IDD_CLI_HELP_DIALOG 1100         /**< CLI help dialog */
#define IDC_CLI_HELP_EDIT 1101           /**< CLI help edit control */

/** @brief Color dialog identifiers */
#define IDD_COLOR_DIALOG 1003            /**< Color selection dialog */
#define IDC_COLOR_VALUE 1301             /**< Color value control */
#define IDC_COLOR_PANEL 1302             /**< Color panel control */
#define CLOCK_IDC_COLOR_VALUE 1301       /**< Color value control (alias) */
#define CLOCK_IDC_COLOR_PANEL 1302       /**< Color panel control (alias) */

/** @brief Modern visual color picker */
#define IDD_MODERN_COLOR_PICKER_DIALOG 2060
#define IDC_MODERN_COLOR_SV 2061
#define IDC_MODERN_COLOR_HUE 2062
#define IDC_MODERN_COLOR_PREVIEW 2063
#define IDC_MODERN_COLOR_SAVED 2064
#define IDC_MODERN_COLOR_HEX_EDIT 2065
#define IDC_MODERN_COLOR_RED_EDIT 2066
#define IDC_MODERN_COLOR_GREEN_EDIT 2067
#define IDC_MODERN_COLOR_BLUE_EDIT 2068
#define IDC_MODERN_COLOR_PICK_BUTTON 2069
#define IDC_MODERN_COLOR_SAVE_BUTTON 2070
#define IDC_MODERN_COLOR_HEX_LABEL 2071
#define IDC_MODERN_COLOR_RED_LABEL 2072
#define IDC_MODERN_COLOR_GREEN_LABEL 2073
#define IDC_MODERN_COLOR_BLUE_LABEL 2074

/** @brief Reusable modern message dialog */
#define IDD_MESSAGE_DIALOG 2080
#define IDC_MESSAGE_TEXT 2081

/** @brief Pomodoro combination live duration preview */
#define IDC_POMODORO_COMBO_HINT 2090

/** @brief Startup configuration control */
#define IDC_STARTUP_TIME 1401            /**< Startup time control */

/** @brief Error and update dialog identifiers */
#define IDD_ERROR_DIALOG 700            /**< Error message dialog */
#define IDC_ERROR_TEXT 701              /**< Error message text control */

/** @brief Update dialog identifiers */
#define IDD_UPDATE_DIALOG 710           /**< Update available dialog */
#define IDC_UPDATE_TEXT 711             /**< Update message text control */
#define IDC_UPDATE_EXIT_TEXT 712        /**< Update exit text control */
#define IDC_UPDATE_NOTES 713            /**< Update release notes text control with modern scrollbar */

/** @brief Update error dialog identifiers */
#define IDD_UPDATE_ERROR_DIALOG 720     /**< Update error dialog */
#define IDC_UPDATE_ERROR_TEXT 721       /**< Update error text control */

/** @brief No update dialog identifiers */
#define IDD_NO_UPDATE_DIALOG 730        /**< No update available dialog */
#define IDC_NO_UPDATE_TEXT 731          /**< No update text control */

/** @brief Exit dialog identifiers */
#define IDD_EXIT_DIALOG 750             /**< Application exit dialog */
#define IDC_EXIT_TEXT 751               /**< Exit message text control */

/** @brief Font menu identifier */
#define CLOCK_IDC_FONT_MENU 113         /**< Font selection submenu */

/** @brief Font menu item identifiers - Special fonts */
#define CLOCK_IDC_FONT_RECMONO 342      /**< RecMono font menu item */
#define CLOCK_IDC_FONT_DEPARTURE 320    /**< Departure font menu item */
#define CLOCK_IDC_FONT_TERMINESS 343    /**< Terminess font menu item */
#define CLOCK_IDC_FONT_YESTERYEAR 390   /**< Yesteryear font menu item */
#define CLOCK_IDC_FONT_ZCOOL_KUAILE 391 /**< ZCOOL KuaiLe font menu item */
#define CLOCK_IDC_FONT_PROFONT 392      /**< ProFont font menu item */
#define CLOCK_IDC_FONT_DADDYTIME 393    /**< DaddyTime font menu item */

/** @brief Font menu item identifiers - Google Fonts collection */
#define CLOCK_IDC_FONT_JACQUARD 361             /**< Jacquard 12 font menu item */
#define CLOCK_IDC_FONT_JACQUARDA 362            /**< Jacquarda Bastarda 9 font menu item */
#define CLOCK_IDC_FONT_PIXELIFY 373             /**< Pixelify Sans Medium font menu item */
#define CLOCK_IDC_FONT_RUBIK_BURNED 377         /**< Rubik Burned font menu item */
#define CLOCK_IDC_FONT_RUBIK_GLITCH 379         /**< Rubik Glitch font menu item */
#define CLOCK_IDC_FONT_RUBIK_MARKER_HATCH 380   /**< Rubik Marker Hatch font menu item */
#define CLOCK_IDC_FONT_RUBIK_PUDDLES 381        /**< Rubik Puddles font menu item */
#define CLOCK_IDC_FONT_WALLPOET 389             /**< Wallpoet font menu item */
#define CLOCK_IDC_FONT_ADVANCED 5103            /**< Font advanced options menu item */
#define CLOCK_IDC_FONT_LICENSE_AGREE 5104       /**< Font license agreement menu item */

/** @brief Font license agreement dialog identifiers */
#define IDD_FONT_LICENSE_DIALOG 740             /**< Font license agreement dialog */
#define IDC_FONT_LICENSE_TEXT 741               /**< Font license agreement text control */
#define IDC_FONT_LICENSE_AGREE_BTN 742          /**< Font license agreement button */
#define IDC_FONT_LICENSE_CANCEL_BTN 743         /**< Font license cancel button */

/** @brief System font picker dialog identifiers */
#define IDD_FONT_PICKER_SIMPLE 758              /**< Simple system font picker dialog */
#define IDC_FONT_LIST_SIMPLE 759                /**< System font list control */
#define IDC_FONT_PICKER_LABEL 757               /**< Font picker label */
#define CLOCK_IDM_SYSTEM_FONT_PICKER 5105       /**< System font picker menu item */

#if CLOCK_IDM_TEXT_EFFECT_BASE <= CLOCK_IDM_SYSTEM_FONT_PICKER && CLOCK_IDM_TEXT_EFFECT_END >= CLOCK_IDC_FONT_ADVANCED
#error "Text effect menu command range overlaps static font menu command IDs"
#endif

/** @brief Plugin security dialog identifiers */
#define IDD_PLUGIN_SECURITY_DIALOG 760          /**< Plugin security confirmation dialog */
#define IDC_PLUGIN_SECURITY_ICON 761            /**< Plugin security icon control */
#define IDC_PLUGIN_SECURITY_TEXT 762            /**< Plugin security text control */
#define IDC_PLUGIN_SECURITY_CANCEL_BTN 764      /**< Plugin security cancel button */
#define IDC_PLUGIN_SECURITY_RUN_ONCE_BTN 765    /**< Plugin security run once button */
#define IDC_PLUGIN_SECURITY_TRUST_BTN 766       /**< Plugin security trust button */

/** @brief TODO dialog identifiers */
#define IDD_TODO_LIST_DIALOG 780          /**< TODO task list dialog */
#define IDC_TODO_LIST_VIEW 781            /**< Task listbox control */
#define IDC_TODO_FILTER_SOURCE 782        /**< Source filter combo */
#define IDC_TODO_FILTER_IMPORTANCE 783    /**< Importance filter combo */
#define IDC_TODO_FILTER_DUE_FROM 784      /**< Due-from date edit */
#define IDC_TODO_FILTER_DUE_TO 785        /**< Due-to date edit */
#define IDC_TODO_FILTER_DONE_FROM 786     /**< Done-from date edit */
#define IDC_TODO_FILTER_DONE_TO 787       /**< Done-to date edit */
#define IDC_TODO_FILTER_KEYWORD 788       /**< Keyword search edit */
#define IDC_TODO_FILTER_DUE_SCOPE 805        /**< Due date-scope combo (week/month/custom) */
#define IDC_TODO_SHOW_DONE 789            /**< Show completed checkbox */
#define IDC_TODO_NEW_EDIT 790             /**< New task title edit */
#define IDC_TODO_NEW_DUE 791              /**< New task due edit */
#define IDC_TODO_NEW_IMPORTANCE 792       /**< New task importance combo */
#define IDC_TODO_ADD_BUTTON 793           /**< Add task button */
#define IDC_TODO_DONE_BUTTON 794          /**< Toggle done button */
#define IDC_TODO_DELETE_BUTTON 795        /**< Delete task button */
#define IDC_TODO_PIN_BUTTON 796           /**< Pin/unpin sticky button */
#define IDC_TODO_SYNC_NOW_BTN 797         /**< Sync-now button in list */
#define IDC_TODO_SETTINGS_BTN 798         /**< Open settings button */
#define IDC_TODO_SETTINGS_SYNC_BTN 799    /**< Sync-now button in settings */
#define IDD_TODO_SETTINGS_DIALOG 800      /**< TODO sync settings dialog */
#define IDC_TODO_SYNC_ENABLE 801          /**< Sync enable checkbox */
#define IDC_TODO_SYNC_URL 802             /**< Server URL edit */
#define IDC_TODO_SYNC_TOKEN 803           /**< Token edit (password) */
#define IDC_TODO_SYNC_POLL 804            /**< Poll interval edit */
#define IDC_TODO_STICKY_TOPMOST 806       /**< Sticky default-topmost checkbox */
#define IDC_TODO_SYNC_STATUS 807          /**< Sync status line in settings */
#define IDC_TODO_CONFLICT_BTN 808         /**< Open conflict dir button */
#define IDC_TODO_DEBUG_CHECK 809          /**< UI debug overlay toggle */
#define IDC_TODO_GROUP_TIME 810           /**< List dialog time-range group */
#define IDC_TODO_GROUP_STATE 811          /**< List dialog state/search group */
#define IDC_TODO_GROUP_SYNC 812           /**< Settings sync group */
#define IDC_TODO_GROUP_STICKY 813         /**< Settings sticky group */
#define IDC_TODO_STICKY_OPACITY 814       /**< Sticky opacity combo */

/** @brief Custom text display dialog identifiers */
#define IDD_CUSTOM_TEXT_DISPLAY_DIALOG 770      /**< Custom text display dialog */
#define IDC_CUSTOM_TEXT_DISPLAY_HINT 771        /**< Custom text display hint label */
#define IDC_CUSTOM_TEXT_DISPLAY_TEXT 772        /**< Custom text editor */

/** @brief Color dialog format help text control */
#define IDC_COLOR_FORMAT_HELP 767               /**< Color format help text label */

#endif /* CATIME_RESOURCE_DIALOG_IDS_H */
