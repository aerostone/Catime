# Changelog

All notable changes to this fork (`aerostone/Catime`) are documented here.
Upstream releases live under `vladelaina/Catime`; see that repository for
history before the fork point.

## [v1.6.13] - 2026-10-20

番茄钟/超时提醒的全屏遮罩在多显示器下跨屏，文字只看到一半。

### Fixed
- 多显示器环境下，超时提醒与番茄钟结束的遮罩层原来按 **所有显示器的并集** 铺开，
  卡片再在并集里居中 —— 于是两屏并排时卡片正好落在屏幕交界处，左边一半、右边
  一半，两边都只能看到半句话。现在改为只锚定 **一块屏**：优先光标所在的那块
  （用户正看着的地方），取不到就用主屏，最后才退化到整个虚拟桌面。
- 遮罩铺满所选屏幕、卡片在其中居中，整段文字始终在同一块屏上可读。

### Changed
- 显示器选择逻辑拆到 `src/todo/timeout_fullscreen_geom.c`，与绘制代码分开，
  两边都保持在 300 行门禁内；首次选择显示器时屏幕坐标偏移也做了显式说明
  （卡片是 `WS_POPUP`，坐标是屏幕坐标而非父窗口客户坐标）。

## [v1.6.12] - 2026-10-20

拉动便签任务面板窗口大小后出现残影，必须把鼠标移上去才刷新。

### Fixed
- 拉伸/缩小任务面板窗口时出现「残影」：窗口变大后新露出的区域保留着旧像素，
  鼠标划过才恢复。两个原因叠加：
  1. 卡片是分层窗口（`WS_EX_LAYERED` + `LWA_ALPHA`），而重绘用的是
     `InvalidateRect(hwnd, NULL, FALSE)`（bErase = FALSE），不会擦背景；
     同时任务行绘制只画了标题栏、过滤行和奇数行的浅色条纹，卡片主体底色
     `COL_BODY` 从未被填充过 —— 于是没画到的像素原样留在屏幕上。
  2. `WM_SIZE` 只保存几何、不失效重绘，窗口尺寸变化后完全没有触发重绘。
  现在任务行与最小化圆点两处绘制都会自己铺满自己的矩形（不再依赖外部先擦
  背景），并在 `WM_SIZE` 中失效整个客户区，残影随之消失。

## [v1.6.11] - 2026-10-19

任务本管理的入口失效、TODO 对话框的控件错位，以及两类回归的自动门禁。

### Fixed
- 托盘菜单里的 **“任务本管理…”** 和 **“显示全部任务本”** 点了没有任何反应。
  根因是命令 ID 撞号：`CLOCK_IDM_TODO_BOARDS` 被定义成 151，与既有的
  `CLOCK_IDM_24HOUR_FORMAT` 相同；命令分发是一张线性表、取第一个命中项，
  于是这两个点击都落在了 24 小时制切换上。已改用 170 / 169。
- TODO 列表对话框“添加行”的标题输入框、截止日期选择器和重要程度下拉框
  高度与纵坐标各不相同（12/14/12 与 258/256/258），日期控件还与下拉框横向
  重叠 16 个对话框单位。现在四者统一 y=256、h=14，间距 4 个单位、互不重叠。
- 任务本对话框底部的按钮行间距不一致（6/8 个单位混用），且对话框比内容高
  出 52 个单位形成大片空白；已统一为 4 个单位间距并把高度从 250 收到 206。
- 设置对话框的 URL / Token / 轮询间隔输入框高度为 12，与同一行 h=14 的
  按钮底边错开 2 个单位；已统一为 14。

### Added
- `tools/rc_layout_lint.py`：离线检查 `.rc` 对话框几何，报出同行纵坐标不一致、
  同行高度不一致、控件重叠、越出对话框边界和底部空白。
- `cmake/CatimeRcLayout.cmake`：把上述检查接进 CMake 配置阶段，几何退化的
  对话框直接让构建失败，而不是等到肉眼在界面上发现。
- `.github/scripts/ci/check_command_ids.py`：拦截命令 ID 撞号。只检查
  `CLOCK_IDM_*`（`CLOCK_IDC_*` 是各对话框私有的控件 ID，跨对话框复用是合法的），
  并额外校验命令分发表内部没有重复项，已接入 `build.yml` 的 `source-policy` 作业。

### Changed
- 说明：门禁为 `.rc` 中运行时重排的对话框（关于、通知、颜色选择器）保留跳过名单，
  它们的坐标只是初始值，真值在窗口初始化时计算，误报已在跳过名单中说明。

## [v1.6.10] - 2026-10-14

便签卡片的溢出行、同步任务本的提示文字与一处占位符残留修复。

### Fixed
- 便签卡片底部的 “还有 N 项，双击打开列表” 提示带之前会被当作它上方那一行的
  任务：单击选中错行、双击直接切换了错行的完成状态，而列表根本不会打开。
  现在绘制与命中测试共用同一份布局计算（`BoardLayout_ComputeRows` /
  `BoardLayout_HitRow`），提示带上方不再被任务行覆盖，点击提示带即打开
  任务列表，悬停也不会高亮到别的行。
- 在 tweek（同步任务本）上选中后直接点 “添加” 会真的新建一个标题为提示语
  “在此新建将创建到 tweek（下次同步）” 的任务，并把这句话推送回 tweek。
  提示语现在用 edit 的 cue banner（`EM_SETCUEBANNER`）展示，不再被写进
  输入框内容，切换任务本也不会把它带过去。
- 折叠便签的提示文字里残留了一个 `TODO` 标记（显示为 “点击展开TODO”）。

### Changed
- `EM_SETCUEBANNER` / `EM_SETCUEBANNER_W` 宏从 `dialog_todo_combos.c`
  迁移到共享头 `dialog/dialog_todo_parts.h`，避免两个对话框各写一份。

## [v1.6.9] - 2026-09-22

Editing a row now syncs back to tweek, both ways.

### Added
- Row-level edits (title / due date / priority), done toggles and
  deletes on the sync book are pushed back to tweek. New tasks added
  while the sync book is selected are created on tweek instead of
  being rerouted to the local book.
- The server returns an authoritative timestamp for every row a push
  touched; the local side adopts it, so a skewed local clock can
  neither win forever nor silently drop later remote edits.

### Fixed
- The push guard compared the todo.txt mtime, and the pull rewrites
  that same file — every poll therefore pushed back rows the server
  had just sent and wrote a conflict snapshot each time. The guard now
  compares the payload itself.
- Sync rows and the sync book were read-only, so edits raised "this
  row is a read-only view" and never reached tweek.
- Edit timestamps are monotonic now: a row pulled with a server stamp
  always gets a strictly newer stamp when edited locally.
- tweek: updated_after=0 means a full first sync (a new device used to
  receive nothing), the cursor compare is >= so changes made in the
  same second as the previous server_time are not skipped, and a
  full page advances the cursor to the last row so backlogs beyond
  200 rows drain over successive polls.
- The sync worker now runs with a 4 MiB stack and keeps the push body
  out of the stack — the first full sync holds both 200-row passes at
  once.

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
