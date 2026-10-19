# Enforce RC dialog geometry invariants: within a dialog, controls that sit
# on one visual row must share the same y and the same visible height, must
# not overlap horizontally, and must stay inside the dialog client area.
#
# This exists because a real regression shipped: the TODO list add row put
# the due picker (x 212..296) under the importance combo (x 280..340) and
# left the title edit on a different y, so the row looked misaligned and two
# controls drew on top of each other. Nothing in the build noticed, because
# layout is purely declarative RC geometry with no runtime relayout.
#
# Dialogs that DO recompute child geometry at WM_INITDIALOG (about /
# notification / color) are excluded: their .rc numbers are only starting
# values, so overlap there is not a defect.
#
# Mirror of tools/rc_layout_lint.py, which is the developer-facing checker.

set(CATIME_RC_MAX_DROPDOWN_H 100)   # CBS dropdown length limit in DU

if(CMAKE_SCRIPT_MODE_FILE)
    get_filename_component(_catime_rc_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
    set(_catime_rc_glob_options)
else()
    set(_catime_rc_root "${CMAKE_CURRENT_SOURCE_DIR}")
    set(_catime_rc_glob_options CONFIGURE_DEPENDS)
endif()

# Dialogs repositioned in code at WM_INITDIALOG: RC coords are not final.
set(_catime_rc_runtime_layout
    "IDD_ABOUT_DIALOG"
    "CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG"
    "IDD_MODERN_COLOR_PICKER_DIALOG"
    "CLOCK_IDD_COLOR_DIALOG"
)

file(GLOB_RECURSE _catime_rc_files
    ${_catime_rc_glob_options}
    LIST_DIRECTORIES FALSE
    "${_catime_rc_root}/resource/*.rc"
)

set(_catime_rc_violations)

foreach(_catime_rc IN LISTS _catime_rc_files)
    file(READ "${_catime_rc}" _catime_rc_text)
    # Strip block and line comments so commented-out controls are ignored.
    string(REGEX REPLACE "/\\*[^*]*\\*+([^/*][^*]*\\*+)*/" "" _catime_rc_text
        "${_catime_rc_text}")
    string(REGEX REPLACE "//[^\n]*" "" _catime_rc_text "${_catime_rc_text}")
    # Normalise whitespace so a control statement is one logical line.
    string(REGEX REPLACE "[ \t\r\n]+" " " _catime_rc_text "${_catime_rc_text}")
    # Split into logical lines on BEGIN / END keywords. END must be a whole
    # word (" END "/" END"), otherwise words like "ends" inside a quoted
    # caption would be mistaken for the dialog terminator.
    string(REGEX REPLACE " BEGIN " "\nBEGIN " _catime_rc_text "${_catime_rc_text}")
    string(REGEX REPLACE " END([ \n]|$)" "\nEND\\1" _catime_rc_text "${_catime_rc_text}")
    string(REGEX REPLACE " (PUSHBUTTON|DEFPUSHBUTTON|EDITTEXT|LTEXT|RTEXT|CTEXT|LISTBOX|COMBOBOX|GROUPBOX|CONTROL|SCROLLBAR|ICON|AUTOCHECKBOX|CHECKBOX|AUTORADIOBUTTON|RADIOBUTTON) " "\n\\1 " _catime_rc_text "${_catime_rc_text}")
    # Start each dialog header on its own line; without this the header stays
    # glued to the preceding #include lines and never matches.
    string(REGEX REPLACE " ([A-Za-z_][A-Za-z0-9_]*) DIALOG(EX)? " "\n\\1 DIALOG\\2 " _catime_rc_text "${_catime_rc_text}")

    file(RELATIVE_PATH _catime_rc_rel "${_catime_rc_root}" "${_catime_rc}")
    string(REPLACE "\\" "/" _catime_rc_rel "${_catime_rc_rel}")

    get_filename_component(_catime_rc_name "${_catime_rc}" NAME)
    string(REPLACE ".rc" "" _catime_rc_name "${_catime_rc_name}")

    set(_catime_dlg_name "")
    set(_catime_dlg_w 0)
    set(_catime_dlg_h 0)
    set(_catime_dlg_skip FALSE)
    set(_catime_ctl_ids "")
    set(_catime_ctl_x "")
    set(_catime_ctl_y "")
    set(_catime_ctl_w "")
    set(_catime_ctl_h "")
    set(_catime_ctl_kw "")

    string(REPLACE "\n" ";" _catime_rc_lines "${_catime_rc_text}")
    foreach(_catime_line IN LISTS _catime_rc_lines)
        string(STRIP "${_catime_line}" _catime_line)
        # The dialog header shares its logical line with STYLE/CAPTION/FONT,
        # so match it without a trailing anchor.
        if(_catime_line MATCHES "^([A-Za-z_][A-Za-z0-9_]*) DIALOG(EX)? [0-9-]+,? +[0-9-]+,? +([0-9]+),? +([0-9]+)")
            set(_catime_dlg_name "${CMAKE_MATCH_1}")
            set(_catime_dlg_w "${CMAKE_MATCH_3}")
            set(_catime_dlg_h "${CMAKE_MATCH_4}")
            set(_catime_ctl_ids "")
            set(_catime_ctl_x "")
            set(_catime_ctl_y "")
            set(_catime_ctl_w "")
            set(_catime_ctl_h "")
            set(_catime_ctl_kw "")
            # Portable membership test: IN_LIST needs CMP0057, which is not
            # set when this module runs under `cmake -P`.
            set(_catime_dlg_skip FALSE)
            foreach(_catime_rt IN LISTS _catime_rc_runtime_layout)
                if(_catime_dlg_name STREQUAL _catime_rt)
                    set(_catime_dlg_skip TRUE)
                endif()
            endforeach()
            continue()
        endif()

        if(_catime_line MATCHES "^(PUSHBUTTON|DEFPUSHBUTTON|EDITTEXT|LTEXT|RTEXT|CTEXT|LISTBOX|COMBOBOX|CONTROL|SCROLLBAR|AUTOCHECKBOX|CHECKBOX|AUTORADIOBUTTON|RADIOBUTTON) ")
            set(_catime_kw "${CMAKE_MATCH_1}")
            # Drop quoted strings (they may contain digits) then take the
            # last four integers as x, y, w, h.
            string(REGEX REPLACE "\"[^\"]*\"" "" _catime_rest "${_catime_line}")
            string(REGEX MATCHALL "[0-9]+" _catime_nums "${_catime_rest}")
            list(LENGTH _catime_nums _catime_num_count)
            if(_catime_num_count LESS 4)
                continue()
            endif()
            math(EXPR _catime_last "${_catime_num_count} - 1")
            math(EXPR _catime_first "${_catime_num_count} - 4")
            list(GET _catime_nums ${_catime_first} _catime_cx)
            math(EXPR _catime_second "${_catime_first} + 1")
            math(EXPR _catime_third "${_catime_first} + 2")
            list(GET _catime_nums ${_catime_second} _catime_cy)
            list(GET _catime_nums ${_catime_third} _catime_cw)
            list(GET _catime_nums ${_catime_last} _catime_ch)

            # Control id: strip the leading keyword, the window class and all
            # quoted captions, then the first identifier left is the id.
            set(_catime_cid "")
            string(REGEX REPLACE "^[A-Za-z_][A-Za-z0-9_]*[ \t]+" "" _catime_head
                "${_catime_rest}")
            string(REGEX REPLACE "\"[^\"]*\"" "" _catime_head "${_catime_head}")
            string(REGEX MATCHALL "[A-Za-z_][A-Za-z0-9_]*" _catime_toks
                "${_catime_head}")
            foreach(_catime_tok IN LISTS _catime_toks)
                string(TOUPPER "${_catime_tok}" _catime_tok_up)
                if(_catime_tok_up STREQUAL "BUTTON" OR
                   _catime_tok_up STREQUAL "EDIT" OR
                   _catime_tok_up STREQUAL "STATIC" OR
                   _catime_tok_up STREQUAL "COMBOBOX" OR
                   _catime_tok_up STREQUAL "LISTBOX" OR
                   _catime_tok_up STREQUAL "SCROLLBAR" OR
                   _catime_tok_up STREQUAL "ICON")
                    continue()
                endif()
                set(_catime_cid "${_catime_tok}")
                break()
            endforeach()
            if(_catime_cid STREQUAL "" OR _catime_cw LESS 1 OR _catime_ch LESS 1)
                continue()
            endif()
            if(_catime_line MATCHES "NOT WS_VISIBLE")
                continue()
            endif()

            if(_catime_dlg_skip)
                continue()
            endif()

            # Bounds check (skip dropdown whose stated height is really the
            # expanded list length).
            if(NOT _catime_kw STREQUAL "COMBOBOX" AND
               NOT _catime_kw STREQUAL "LISTBOX" AND
               _catime_dlg_w GREATER 0 AND _catime_dlg_h GREATER 0)
                math(EXPR _catime_right "${_catime_cx} + ${_catime_cw}")
                math(EXPR _catime_bottom "${_catime_cy} + ${_catime_ch}")
                math(EXPR _catime_dlg_w1 "${_catime_dlg_w} + 1")
                math(EXPR _catime_dlg_h1 "${_catime_dlg_h} + 1")
                if(_catime_right GREATER _catime_dlg_w1)
                    list(APPEND _catime_rc_violations
                        "${_catime_rc_rel}: ${_catime_dlg_name}: ${_catime_cid} right edge ${_catime_right} exceeds dialog width ${_catime_dlg_w}")
                endif()
                if(_catime_bottom GREATER _catime_dlg_h1)
                    list(APPEND _catime_rc_violations
                        "${_catime_rc_rel}: ${_catime_dlg_name}: ${_catime_cid} bottom edge ${_catime_bottom} exceeds dialog height ${_catime_dlg_h}")
                endif()
            endif()

            list(APPEND _catime_ctl_ids "${_catime_cid}")
            list(APPEND _catime_ctl_x "${_catime_cx}")
            list(APPEND _catime_ctl_y "${_catime_cy}")
            list(APPEND _catime_ctl_w "${_catime_cw}")
            list(APPEND _catime_ctl_h "${_catime_ch}")
            list(APPEND _catime_ctl_kw "${_catime_kw}")
            continue()
        endif()

        # Dialog END: cross-check every pair collected for this dialog.
        if(_catime_line MATCHES "^END" AND NOT _catime_dlg_name STREQUAL "")
            list(LENGTH _catime_ctl_ids _catime_ctl_n)
            if(_catime_ctl_n GREATER 1)
                math(EXPR _catime_ctl_last "${_catime_ctl_n} - 1")
                foreach(_catime_i RANGE 0 ${_catime_ctl_last})
                    list(GET _catime_ctl_ids ${_catime_i} _catime_id_a)
                    list(GET _catime_ctl_x ${_catime_i} _catime_x_a)
                    list(GET _catime_ctl_y ${_catime_i} _catime_y_a)
                    list(GET _catime_ctl_w ${_catime_i} _catime_w_a)
                    list(GET _catime_ctl_h ${_catime_i} _catime_h_a)
                    list(GET _catime_ctl_kw ${_catime_i} _catime_kw_a)
                    if(_catime_kw_a STREQUAL "GROUPBOX")
                        continue()
                    endif()
                    foreach(_catime_j RANGE 0 ${_catime_ctl_last})
                        if(_catime_j LESS_EQUAL _catime_i)
                            continue()
                        endif()
                        list(GET _catime_ctl_ids ${_catime_j} _catime_id_b)
                        list(GET _catime_ctl_x ${_catime_j} _catime_x_b)
                        list(GET _catime_ctl_y ${_catime_j} _catime_y_b)
                        list(GET _catime_ctl_w ${_catime_j} _catime_w_b)
                        list(GET _catime_ctl_h ${_catime_j} _catime_h_b)
                        list(GET _catime_ctl_kw ${_catime_j} _catime_kw_b)
                        if(_catime_kw_b STREQUAL "GROUPBOX")
                            continue()
                        endif()

                        # Order the pair left-to-right.
                        if(_catime_x_a GREATER _catime_x_b)
                            set(_catime_tmp "${_catime_x_a}")
                            set(_catime_x_a "${_catime_x_b}")
                            set(_catime_x_b "${_catime_tmp}")
                            set(_catime_tmp "${_catime_y_a}")
                            set(_catime_y_a "${_catime_y_b}")
                            set(_catime_y_b "${_catime_tmp}")
                            set(_catime_tmp "${_catime_w_a}")
                            set(_catime_w_a "${_catime_w_b}")
                            set(_catime_w_b "${_catime_tmp}")
                            set(_catime_tmp "${_catime_h_a}")
                            set(_catime_h_a "${_catime_h_b}")
                            set(_catime_h_b "${_catime_tmp}")
                            set(_catime_tmp "${_catime_id_a}")
                            set(_catime_id_a "${_catime_id_b}")
                            set(_catime_id_b "${_catime_tmp}")
                            set(_catime_tmp "${_catime_kw_a}")
                            set(_catime_kw_a "${_catime_kw_b}")
                            set(_catime_kw_b "${_catime_tmp}")
                        endif()

                        math(EXPR _catime_dy "${_catime_y_a} - ${_catime_y_b}")
                        if(_catime_dy LESS 0)
                            math(EXPR _catime_dy "0 - ${_catime_dy}")
                        endif()
                        if(_catime_dy GREATER 2)
                            continue()
                        endif()
                        math(EXPR _catime_right_a "${_catime_x_a} + ${_catime_w_a}")
                        if(_catime_x_b GREATER _catime_right_a)
                            math(EXPR _catime_gap "${_catime_x_b} - ${_catime_right_a}")
                        else()
                            math(EXPR _catime_gap "0")
                        endif()

                        # Horizontal overlap on the same visual row.
                        if(_catime_x_b GREATER _catime_x_a AND
                           _catime_x_b LESS _catime_right_a)
                            math(EXPR _catime_ov "${_catime_right_a} - ${_catime_x_b}")
                            list(APPEND _catime_rc_violations
                                "${_catime_rc_rel}: ${_catime_dlg_name}: ${_catime_id_a} x ${_catime_x_a}..${_catime_right_a} overlaps ${_catime_id_b} x ${_catime_x_b} by ${_catime_ov} DU")
                        endif()

                        # Same row must share y and visible height.
                        if(_catime_gap LESS_EQUAL 40)
                            if(NOT _catime_y_a EQUAL _catime_y_b AND
                               NOT _catime_kw_a STREQUAL "LTEXT"
                               AND NOT _catime_kw_b STREQUAL "LTEXT"
                               AND NOT _catime_kw_a STREQUAL "RTEXT"
                               AND NOT _catime_kw_b STREQUAL "RTEXT"
                               AND NOT _catime_kw_a STREQUAL "CTEXT"
                               AND NOT _catime_kw_b STREQUAL "CTEXT")
                                list(APPEND _catime_rc_violations
                                    "${_catime_rc_rel}: ${_catime_dlg_name}: ${_catime_id_a} y=${_catime_y_a} differs from ${_catime_id_b} y=${_catime_y_b} on one row")
                            endif()
                            if(_catime_y_a EQUAL _catime_y_b AND
                               NOT _catime_kw_a STREQUAL "COMBOBOX" AND
                               NOT _catime_kw_b STREQUAL "COMBOBOX" AND
                               NOT _catime_kw_a STREQUAL "LISTBOX" AND
                               NOT _catime_kw_b STREQUAL "LISTBOX" AND
                               NOT _catime_h_a EQUAL _catime_h_b)
                                list(APPEND _catime_rc_violations
                                    "${_catime_rc_rel}: ${_catime_dlg_name}: ${_catime_id_a} h=${_catime_h_a} differs from ${_catime_id_b} h=${_catime_h_b} on one row")
                            endif()
                        endif()
                    endforeach()
                endforeach()
            endif()
            set(_catime_dlg_name "")
            set(_catime_ctl_ids "")
            continue()
        endif()
    endforeach()
endforeach()

if(_catime_rc_violations)
    string(JOIN "\n  " _catime_rc_text ${_catime_rc_violations})
    message(FATAL_ERROR
        "RC dialog controls on one row must share y and height, must not "
        "overlap, and must fit the dialog:\n  ${_catime_rc_text}\n"
        "Check with: python3 tools/rc_layout_lint.py resource/*.rc"
    )
endif()

unset(_catime_rc_runtime_layout)
unset(_catime_rc_root)
unset(_catime_rc_glob_options)
unset(_catime_rc_files)
unset(_catime_rc_violations)
unset(_catime_rc_text)
unset(_catime_rc_lines)
unset(_catime_rc_rel)
unset(_catime_rc_name)
