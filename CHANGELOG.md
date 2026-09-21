# Changelog

All notable changes to this fork (`aerostone/Catime`) are documented here.
Upstream releases live under `vladelaina/Catime`; see that repository for
history before the fork point.

## [v1.6.8] - 2026-09-21

Task-book rework: 便签 becomes 任务本, bigger cards, a dedicated
manager, date pickers, and select-to-edit.

### Added
- Task-book manager dialog (任务本管理): book list + New/Rename/Del
  + show/hide panel + Sync Now + jump to sync settings. All
  book-level controls move here; the tray and task manager keep
  only per-task navigation and editing.
- Date picker (SysDateTimePick32, DTS_SHOWNONE) for the task due
  date and the task-list range start/end; empty checkbox means no
  bound, no more YYYY-MM-DD typing.
- Save button in the task manager: selecting a row refills the edit
  row (title + due + priority) and Save writes back. New tasks are
  auto-preselected after adding.
- Minimized cards render as a 52px semi-transparent dot (alpha 140)
  tinted by peak open-task importance with the open count centered.

### Changed
- All UI copy: 便签 -> 任务本 / TODO panels; English Stickies ->
  Task books / Show panel.
- Board cards +30%: title bar 30->39px, filter row 26->34px,
  task rows 22->29px, default card 320x260 -> 416x338, title font
  -16 -> -21, body -14 -> -18.
- Tray TODO menu: task rows, New Task, settings, and Sync Now are
  gone — only 任务管理 / 任务本管理 / 显示任务本 plus the
  sync-state and conflict rows remain.
- Double-click collapse is gone; only the title-bar fold button
  collapses, and row double-click toggles done.
- In-card search box removed; the filter row keeps this-week/all
  toggle and the add shortcut.
- Task-list scope: Week/Month no longer persist into the card; new
  tasks no longer trigger a fake Custom scope.
- MinGW release size budget 1180 -> 1200 KiB for the new dialog,
  picker, and dot paint code.

## [v1.6.7] - 2026-09-21

Single-editor cleanup: the sticky card selects instead of completing,
the board menu hides behind `...`, and the tray menu gets slimmer.

### Fixed
- Sticky card: single click now selects a row (+hover band, focus rect);
  done needs double-click, row menu, or Space/Enter — no more
  accidental completions, and nothing syncs outward by mistake.
- Sticky card: rows that overflow the card paint a
  `...还有 n 项，双击打开列表` hint instead of silently clipping.
- Row menu: removed the fake date/importance "editors" that jumped to
  the list dialog; added an honest `在列表中编辑...` entry.
- Title bar: hide leaves the bar (card menu only, tasks kept) — no
  more one-click accidental hides or close-X confusion.
- Card keyboard support: arrows move selection, Space/Enter toggles
  done, Esc hides the card.
- Title glyph wording: pin tips and card menu are honest about
  show/hide semantics.
- Task-list dialog: New/Rename/Del board buttons collapse into a `...`
  manage menu; sync board disables the Add row (read-only) instead of
  scolding with a modal.
- Tray: Top5 -> Top3 + `更多 (n)项...` overflow, sync status merges
  into Sync Now (`Sync Now · 同步·正常 12:03`).
- Overdue marks: `[!]` is now independent of `[A]` importance; tray,
  list, and card share one `TodoTask_IsOverdue` predicate.
- Pomodoro label on cards reads `番茄 MM:SS` (localized).
- MinGW: doubled backslash in the tray overflow label.

## [v1.6.6] - 2026-09-21

Sticky boards: one desktop card per task board, rebuilt task-list dialog,
and new tray entries.

### Added
- Sticky boards: named task groupings (`board:NAME` in todo.txt) with one
  desktop card per board — title bar (fold / open list / hide), filter row
  (week-all toggle + keyword + add button), click-to-toggle rows, full
  row menu (done, pomodoro, due, importance, move, delete).
- Task-list dialog edits one board at a time: board selector combo, New /
  Rename / Del board buttons, sticky show/hide button, per-board scope
  and keyword persistence.
- Sync board: pull-only board that shows server tasks from tweek; local
  tasks added while it is selected land on the default board.
- Tray TODO menu: top-5 open tasks, checkable Stickies submenu, sync
  status line, conflict entry, plus a Show-all-stickies command.
- Sticky settings: global topmost default, per-board topmost override,
  opacity 30-100% (`[Sticky] Opacity`), live re-apply without restart.
- Dot collapse: collapsing a card shrinks it to a 32x32 dot tinted by
  peak importance (hollow when done); click the dot to expand.

### Changed
- Countdown window no longer draws todo overlay text (sticky is the
  only on-screen task surface; countdown stays clean).
- Tray `New Sticky Note` renamed `New Task...`; the list dialog is the
  only create/edit entry, focused on the title edit.
- Sticky cards are display + lightweight actions only (done, pomodoro,
  fold, topmost, hide); rename/delete live in the list dialog.
- Pomodoro countdown on the card now reads `番茄 M:SS`.
- Layout audit fixes: filter buttons moved out of the GROUPBOX,
  board-row commands gated on `CBN_SELCHANGE`, shared conflict id macro
  (`TODO_MENU_CONFLICT_BASE`), cue-banner macro centralized.

### Fixed
- MSVC C2065 batch: shared sticky class/timers declarations
  (`STICKY_CLASS`, `StickyProc`, `STICKY_TIMER_POMO/KW`), `WC_EDITW` /
  `EM_SETCUEBANNER` include handling, board-command `code` parameter.

## [v1.6.5]
- TODO full set (task list, sync with tweek, sticky notes, tray menu).

## [v1.6.4]
- Week/Month/Custom due-date scope presets for the task list.

## [v1.6.3]
- Fork release baseline.
