#!/usr/bin/env python3
"""
RC dialog geometry linter for Catime.

Detects the failure classes behind the TODO add-row bug and other
"row looks wrong" regressions, without needing Windows:

  OVERLAP      two controls on the same visual row whose x ranges overlap
  Y_MISMATCH   controls on one visual row sitting on different y
  H_MISMATCH   adjacent controls on one row with different visible heights
               (edit h=12 next to a button h=14 -> uneven bottoms)
  OFFLOW       a control sticks out past the dialog client area
  DEAD_SPACE   the dialog is far taller than its content

COMBOBOX/LISTBOX RC heights are dropdown/inner-list lengths, not the
visible box height, so they are excluded from height comparison.

Dialogs that recompute their child geometry at runtime (WM_INITDIALOG
layout pass) are listed in RUNTIME_LAYOUT and skipped: their RC blocks are
only starting values, so overlap/height noise there is not a real defect.
That covers the about / notification / color dialogs, which reflow for
long localized strings and DPI. The TODO dialogs are NOT in that list --
they are pure RC geometry, which is why the add-row regression was real.

Usage:
    python3 tools/rc_layout_lint.py resource/*.rc
    python3 tools/rc_layout_lint.py --all resource/*.rc   # include skipped
Exit code 1 when any problem is found.
"""

import re
import sys

BOX_KW = {
    'PUSHBUTTON', 'DEFPUSHBUTTON', 'EDITTEXT', 'LTEXT', 'RTEXT', 'CTEXT',
    'LISTBOX', 'COMBOBOX', 'GROUPBOX', 'CONTROL', 'SCROLLBAR', 'ICON',
    'AUTOCHECKBOX', 'CHECKBOX', 'AUTORADIOBUTTON', 'RADIOBUTTON',
}
DROPDOWN_KW = {'COMBOBOX', 'LISTBOX'}

# Dialogs whose children are repositioned in code at WM_INITDIALOG, so the
# .rc coordinates are not the final layout. Keep in sync with the Layout*
# helpers (DialogInfo_LayoutAboutDialogControls, DialogNotificationInternal_
# Layout, ColorPickerInternal_LayoutControls, DialogFormLayout_Apply*).
RUNTIME_LAYOUT = {
    'IDD_ABOUT_DIALOG',
    'CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG',
    'IDD_MODERN_COLOR_PICKER_DIALOG',
    'CLOCK_IDD_COLOR_DIALOG',
}
CLASS_NAMES = {'BUTTON', 'EDIT', 'STATIC', 'COMBOBOX', 'LISTBOX',
               'SCROLLBAR', 'ICON'}
VER_RE = re.compile(r'^([A-Za-z_]\w*)\s+DIALOG(?:EX)?\s+'
                    r'(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)',
                    re.I)
NUM_RE = re.compile(r'(?<![\w-])-?\d+(?![\w-])')
QUOTE_RE = re.compile(r'"(?:[^"]|"")*"')


def _strip_comments(txt):
    txt = re.sub(r'/\*.*?\*/', lambda m: '\n' * m.group(0).count('\n'),
                 txt, flags=re.S)
    return '\n'.join(re.sub(r'//.*$', '', ln) for ln in txt.split('\n'))


def parse_rc(path):
    lines = _strip_comments(open(path, encoding='utf-8',
                                errors='replace').read()).split('\n')
    dialogs, cur, depth = [], None, 0
    pending = None  # (kw, text, line) 多行控件语句续行缓冲
    for ln, line in enumerate(lines, 1):
        s = line.strip()
        m = VER_RE.match(s)
        if m and pending is None:
            cur = {'name': m.group(1), 'w': int(m.group(4)),
                   'h': int(m.group(5)), 'controls': [], 'line': ln}
            continue
        if cur is None:
            continue
        if pending is not None:
            # 续行：控件语句尚未结束（RC 允许 CONTROL 跨多行）
            pending = (pending[0], pending[1] + ' ' + s, pending[2])
            if _stmt_complete(pending[0], pending[1]):
                _add_control(cur, pending[0], pending[1], pending[2])
                pending = None
            continue
        if re.match(r'^BEGIN\b', s, re.I):
            depth += 1
            continue
        if re.match(r'^END\b', s, re.I):
            depth -= 1
            if depth <= 0:
                dialogs.append(cur)
                cur = None
            continue
        if depth != 1:
            continue
        cm = re.match(r'^\s*(\w+)\s+(.*)$', line)
        if not cm or cm.group(1).upper() not in BOX_KW:
            continue
        kw, rest = cm.group(1).upper(), cm.group(2).strip()
        if _stmt_complete(kw, rest):
            _add_control(cur, kw, rest, ln)
        else:
            pending = (kw, rest, ln)
    return dialogs


def _stmt_complete(kw, text):
    """一条控件语句是否有足够的坐标数字（末 4 个 = x,y,w,h）。"""
    return len(NUM_RE.findall(text)) >= 4


def _add_control(dlg, kw, rest, ln):
    if re.search(r'NOT\s+WS_VISIBLE', rest, re.I):
        return
    nums = list(NUM_RE.finditer(rest))
    if len(nums) < 4:
        return
    x, y, w, h = (int(nums[k].group())
                  for k in range(len(nums) - 4, len(nums)))
    if w <= 0 or h <= 0:
        return
    head = QUOTE_RE.sub('', rest[:nums[-4].start()])
    toks = re.findall(r'[A-Za-z_]\w*|\d+', head)
    cid = next((t for t in toks if t.upper() not in CLASS_NAMES), '?')
    dlg['controls'].append({'kw': kw, 'x': x, 'y': y, 'w': w, 'h': h,
                            'id': cid, 'line': ln})


def lint(d):
    out = []
    ctl = [c for c in d['controls'] if c['id'] != '?']
    for c in ctl:
        if c['x'] + c['w'] > d['w'] + 1:
            out.append(('OFLOW', c['line'], '%-24s 右边缘 %d > 宽 %d'
                        % (c['id'], c['x'] + c['w'], d['w'])))
        if c['kw'] in DROPDOWN_KW:
            continue  # RC 高=下拉展开长度，不是可见框高
        if c['y'] + c['h'] > d['h'] + 1:
            out.append(('OFLOW', c['line'], '%-24s 下边缘 %d > 高 %d'
                        % (c['id'], c['y'] + c['h'], d['h'])))

    items = [c for c in ctl if c['kw'] != 'GROUPBOX']
    for i in range(len(items)):
        for j in range(i + 1, len(items)):
            a, b = items[i], items[j]
            if a['x'] > b['x']:
                a, b = b, a
            if abs(a['y'] - b['y']) > 2:
                continue
            gap = b['x'] - (a['x'] + a['w'])
            if a['x'] < b['x'] < a['x'] + a['w']:
                out.append(('OVERLAP', a['line'],
                            '%-24s x %d..%d 与 %-22s x %d..%d 重叠 %d DU'
                            % (a['id'], a['x'], a['x'] + a['w'], b['id'],
                               b['x'], b['x'] + b['w'],
                               a['x'] + a['w'] - b['x'])))
            label_pair = {a['kw'], b['kw']} & {'LTEXT', 'RTEXT', 'CTEXT'}
            if (a['y'] != b['y'] and gap <= 40
                    and abs(a['y'] - b['y']) >= 2 and not label_pair):
                out.append(('Y_MISMATCH', a['line'],
                            '%-24s y=%d 与 %-22s y=%d (差 %d)'
                            % (a['id'], a['y'], b['id'], b['y'],
                               abs(a['y'] - b['y']))))
            if (a['y'] == b['y'] and a['kw'] not in DROPDOWN_KW
                    and b['kw'] not in DROPDOWN_KW and a['h'] != b['h']
                    and gap <= 40):
                out.append(('H_MISMATCH', a['line'],
                            '%-24s h=%d 与 %-22s h=%d'
                            % (a['id'], a['h'], b['id'], b['h'])))

    if ctl:
        bottom = max(c['y'] + (min(c['h'], 20) if c['kw'] in DROPDOWN_KW
                                else c['h']) for c in ctl)
        if d['h'] - bottom > 20:
            out.append(('DEAD_SPACE', d['line'],
                        '内容到 y=%d，对话框高 %d，底部空 %d DU'
                        % (bottom, d['h'], d['h'] - bottom)))
    return out


def main(paths, show_skipped=False):
    paths = [p for p in paths if not p.startswith('--')] or \
        ['resource/todo_dialog.rc']
    total = 0
    skipped = 0
    for path in paths:
        for d in parse_rc(path):
            if d['name'] in RUNTIME_LAYOUT and not show_skipped:
                skipped += 1
                continue
            probs = lint(d)
            if not probs:
                continue
            print('\n%s :: %s (%dx%d)' % (path, d['name'], d['w'], d['h']))
            for kind, ln, msg in sorted(probs):
                print('  [%-11s] L%-4d %s' % (kind, ln, msg))
            total += len(probs)
    if skipped:
        print('\n(跳过 %d 个运行时重排的对话框: %s)'
              % (skipped, ', '.join(sorted(RUNTIME_LAYOUT))))
    print('\n共 %d 处几何问题' % total)
    return 0 if total == 0 else 1


if __name__ == '__main__':
    args = sys.argv[1:]
    sys.exit(main(args, show_skipped='--all' in args))
