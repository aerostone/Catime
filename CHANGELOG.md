# Changelog

All notable changes to this fork (`aerostone/Catime`) are documented here.
Upstream releases live under `vladelaina/Catime`; see that repository for
history before the fork point.

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
