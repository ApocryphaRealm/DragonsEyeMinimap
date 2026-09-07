# -*- coding: utf-8 -*-
"""lang-patch.py - one-shot, re-runnable language-support patch for Dragon's Eye Minimap.

Applies the consumer-side mechanism from the translation rollout plan, section 2:

  * include/utils/Strings.h is vendored separately (copied from the template, unchanged);
  * include/SKSEMenuFramework.h gets "!ApocryphaMenuFramework" as the FIRST module lookup;
  * source/MessageListeners.cpp calls strings::Configure("DragonsEyeMinimap") at kDataLoaded
    (this mod's message handler lives there, not in main.cpp);
  * source/UI.cpp calls strings::Tick() as the first drawing line of SettingsPanel::Render()
    and routes every drawn literal through strings::TR("DEM_...", "English");
  * source/DevBenchTool.cpp gains an op=strings on dem.control returning strings::StatusJson().

Every edit is a must-match anchor replace: an anchor that is not found EXACTLY ONCE raises, so a
stale run against changed source fails loudly instead of leaving the code half patched. Nothing is
written until every anchor for that file has matched. Each patch step is skipped when its
done-marker is already present, so the script is re-runnable.

Run: `python tools/lang-patch.py`, or `python tools/lang-patch.py --check` to verify every anchor
still matches exactly once without writing anything (used while the patch was built up section by
section).

Encoding note: source files are read and written as UTF-8 with newline='' in BOTH directions, so a
raw CR inside a string literal survives and the existing line endings are preserved exactly (logic
library, 2026-09-02 - "Reading source with Python's universal newlines corrupts string literals").
This repo's own sources are LF (MessageListeners.cpp carries a UTF-8 BOM, which round-trips
untouched); the vendored SKSEMenuFramework.h is CRLF, which is what fit() below is for.

NEVER routed through TR, deliberately:
  * the framework's section and page names ("Dragon's Eye Minimap", "Settings") - registry identity;
  * the frame-theme dropdown's entries - those are the SWF FILE STEMS found in
    Data/Interface/DragonsEyeMinimapThemes, i.e. data, not text (only the combo's own label and the
    "Built-in frame" entry this mod adds are translated);
  * INI keys, the INI path, folder and file names, the scan-code table's key names
    ("ESCAPE", "F1", ...) which are matched against SKSEMenuFramework.ini, the XInput mask numbers;
  * slider display formats ("%.0f px", "%.2f", "%.3f") - numeric formats, "px" is a unit symbol;
  * every logger:: line and the "(?)"/"<-->" ornament markers.
"""
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CHECK_ONLY = "--check" in sys.argv


def read(path):
    with open(path, "r", encoding="utf-8", newline="") as f:
        return f.read()


def write(path, text):
    if CHECK_ONLY:
        return
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(text)


def fit(s, crlf):
    """Anchors are written with LF; a file kept in CRLF gets the CRLF form of the same text."""
    return s.replace("\n", "\r\n") if crlf else s


def apply_one(text, anchor, replacement, label, crlf=False):
    anchor, replacement = fit(anchor, crlf), fit(replacement, crlf)
    n = text.count(anchor)
    if n != 1:
        raise RuntimeError("[{}] anchor found {} time(s), expected exactly 1:\n{!r}".format(label, n, anchor))
    return text.replace(anchor, replacement, 1)


def dominant_crlf(text):
    """True when the file is KEPT in CRLF. Counted, not merely detected: source/UI.cpp is an LF
    file with a handful of stray CRLF lines in it (the "Show location name" block), and a plain
    `"\\r\\n" in text` would call the whole file CRLF and every multi-line anchor would miss."""
    return text.count("\r\n") * 2 > text.count("\n")


def apply_all(text, pairs, label):
    crlf = dominant_crlf(text)
    for i, (anchor, replacement) in enumerate(pairs):
        text = apply_one(text, anchor, replacement, "{}[{}]".format(label, i), crlf)
    return text


# ------------------------------------------------------------------------------------------------
# 1) include/SKSEMenuFramework.h - "!ApocryphaMenuFramework" first, ahead of the alias name.
# ------------------------------------------------------------------------------------------------
def patch_skse_menu_framework_h():
    path = os.path.join(REPO, "include", "SKSEMenuFramework.h")
    text = read(path)
    if 'GetModuleHandleW(L"!ApocryphaMenuFramework")' in text and not CHECK_ONLY:
        print("  SKSEMenuFramework.h: already patched")
        return
    anchor = (
        '        menuFramework = GetModuleHandleW(L"ApocryphaMenuFramework");\n'
        '        if (!menuFramework) {\n'
        '            menuFramework = GetModuleHandleW(L"SKSEMenuFramework");\n'
        '        }\n'
    )
    replacement = (
        '        menuFramework = GetModuleHandleW(L"!ApocryphaMenuFramework");\n'
        '        if (!menuFramework) {\n'
        '            menuFramework = GetModuleHandleW(L"ApocryphaMenuFramework");\n'
        '        }\n'
        '        if (!menuFramework) {\n'
        '            menuFramework = GetModuleHandleW(L"SKSEMenuFramework");\n'
        '        }\n'
    )
    if 'GetModuleHandleW(L"!ApocryphaMenuFramework")' in text:
        print("  SKSEMenuFramework.h: already patched")
        return
    text = apply_one(text, anchor, replacement, "SKSEMenuFramework.h:GetMenuFrameworkModule", dominant_crlf(text))
    write(path, text)
    print("  SKSEMenuFramework.h: patched")


# ------------------------------------------------------------------------------------------------
# 2) source/MessageListeners.cpp - strings::Configure(...) at kDataLoaded.
# ------------------------------------------------------------------------------------------------
ML_PAIRS = [
    ('#include "UI.h"\n',
     '#include "UI.h"\n\n#include "utils/Strings.h"\n'),
    ('\tif (a_msg->type == SKSE::MessagingInterface::kDataLoaded)\n'
     '\t{\n'
     '\t\tDEM::devbench::Init(true);\n'
     '\t}\n',
     '\tif (a_msg->type == SKSE::MessagingInterface::kDataLoaded)\n'
     '\t{\n'
     '\t\t// Language first (translation rollout plan, section 2.1): the settings page reads its\n'
     '\t\t// text from Data/Interface/Translations/DragonsEyeMinimap_<language>.txt for whatever\n'
     '\t\t// language the Apocrypha Menu Framework reports, before anything is drawn.\n'
     '\t\tstrings::Configure("DragonsEyeMinimap");\n'
     '\n'
     '\t\tDEM::devbench::Init(true);\n'
     '\t}\n'),
]


def patch_message_listeners_cpp():
    path = os.path.join(REPO, "source", "MessageListeners.cpp")
    text = read(path)
    if 'strings::Configure("DragonsEyeMinimap")' in text:
        print("  MessageListeners.cpp: already patched")
        return
    text = apply_all(text, ML_PAIRS, "MessageListeners.cpp")
    write(path, text)
    print("  MessageListeners.cpp: patched")


# ------------------------------------------------------------------------------------------------
# 3) source/DevBenchTool.cpp - op=strings on dem.control, and its descriptor line.
#
# The op is answered BEFORE the minimap-singleton guard: the language a page draws in is knowable
# whether or not the minimap has been built yet, and the proof run reads it at the main menu.
# ------------------------------------------------------------------------------------------------
DEVBENCH_PAIRS = [
    ('#include "MiniMap.h"\n#include "Settings.h"',
     '#include "MiniMap.h"\n#include "Settings.h"\n\n#include "utils/Strings.h"'),
    ('\t\t\tconst std::string op = JsonStr(args, "op");\n'
     '\n'
     '\t\t\tauto* mini = Minimap::GetSingleton();',
     '\t\t\tconst std::string op = JsonStr(args, "op");\n'
     '\n'
     '\t\t\t// op=strings: which language the settings page is drawing in, where that came from\n'
     '\t\t\t// and how many texts were read - the proof a translation file actually loaded,\n'
     '\t\t\t// readable without a capture. Answered before the minimap guard below, because it\n'
     '\t\t\t// does not depend on the minimap existing yet.\n'
     '\t\t\tif (op == "strings")\n'
     '\t\t\t{\n'
     '\t\t\t\tconst std::string reply = "{\\"ok\\":true,\\"op\\":\\"strings\\",\\"strings\\":" + strings::StatusJson() + "}";\n'
     '\t\t\t\ta_write(a_sink, reply.c_str());\n'
     '\t\t\t\treturn;\n'
     '\t\t\t}\n'
     '\n'
     '\t\t\tauto* mini = Minimap::GetSingleton();'),
    ('\t\t\ta_write(a_sink, "{\\"ok\\":false,\\"error\\":\\"op must be show|hide|state\\"}");',
     '\t\t\ta_write(a_sink, "{\\"ok\\":false,\\"error\\":\\"op must be show|hide|state|strings\\"}");'),
    ('\t\t\t"never persisted), state (ready/shown/visible, the stage rect, compass toggles).\\","',
     '\t\t\t"never persisted), state (ready/shown/visible, the stage rect, compass toggles), "\n'
     '\t\t\t"strings (the language the settings page is drawn in, where it came from and how many "\n'
     '\t\t\t"translated texts were loaded).\\","'),
]


def patch_devbench_tool_cpp():
    path = os.path.join(REPO, "source", "DevBenchTool.cpp")
    text = read(path)
    if '\\"op\\":\\"strings\\"' in text:
        print("  DevBenchTool.cpp: already patched")
        return
    text = apply_all(text, DEVBENCH_PAIRS, "DevBenchTool.cpp")
    write(path, text)
    print("  DevBenchTool.cpp: patched")


# ------------------------------------------------------------------------------------------------
# 4) source/UI.cpp - Tick() in the page callback, TR() on every drawn literal.
#
# Conventions (plan 2.2): Text/TextWrapped/TextDisabled with a plain literal become
# X("%s", TR(...)); a literal that IS a printf format keeps its specifiers inside the TR text; an
# ImGui id suffix (##hide, ##zoom, ##default, ##zoomedin) stays OUTSIDE the translated text; combo
# option lists are rebuilt per frame from TR'd entries (ComboTR); the shape / corner / log-level /
# reserved-reason tables become parallel key/label arrays translated at the use site; status
# strings assigned into statusMessage are built from TR at the moment they are set. The helpers
# (HelpMarker, NudgeableSlider, KeyBindRow) take ALREADY-TR'd text, so every key is visible at the
# call site in the exact TR("KEY", "English") shape the generator and the check grep for.
# ------------------------------------------------------------------------------------------------
UI_PAIRS = [
    # --- includes -------------------------------------------------------------------------------
    ('#include "utils/Logger.h"\n#include "utils/Toggle.h"',
     '#include "utils/Logger.h"\n#include "utils/Strings.h"\n#include "utils/Toggle.h"'),
    ('#include <map>\n#include <vector>',
     '#include <map>\n#include <string>\n#include <vector>'),

    # --- the option tables become parallel key/label arrays, plus the two helpers ----------------
    ('\t\tconstexpr const char* kShapeNames[] = { "Squared", "Round" };\n'
     '\t\tconstexpr int kShapeCount = 2;\n'
     '\n'
     '\t\tconstexpr const char* kAnchorNames[] = { "Top left", "Top right", "Bottom left", "Bottom right" };\n'
     '\t\tconstexpr int kAnchorCount = 4;\n'
     '\n'
     '\t\tconstexpr const char* kLogLevelNames[] = { "Trace", "Debug", "Info", "Warning", "Error", "Critical", "Off" };\n'
     '\t\tconstexpr int kLogLevelCount = 7;\n',
     '\t\t// Every list of drawn options is a parallel key/label pair: the key array names the\n'
     '\t\t// translation key, the label array holds the compiled English fallback. ComboTR() below\n'
     '\t\t// pairs them by index and rebuilds the option list from TR\'d entries every frame\n'
     '\t\t// (translation rollout plan, section 2.2). The labels stay the log\'s vocabulary.\n'
     '\t\tconstexpr const char* const kShapeKeys[] = { "DEM_Shape_Squared", "DEM_Shape_Round" };\n'
     '\t\tconstexpr const char* const kShapeLabels[] = { "Squared", "Round" };\n'
     '\t\tconstexpr int kShapeCount = 2;\n'
     '\n'
     '\t\tconstexpr const char* const kAnchorKeys[] = { "DEM_Corner_TopLeft", "DEM_Corner_TopRight", "DEM_Corner_BottomLeft", "DEM_Corner_BottomRight" };\n'
     '\t\tconstexpr const char* const kAnchorLabels[] = { "Top left", "Top right", "Bottom left", "Bottom right" };\n'
     '\t\tconstexpr int kAnchorCount = 4;\n'
     '\n'
     '\t\tconstexpr const char* const kLogLevelKeys[] = { "DEM_Log_Trace", "DEM_Log_Debug", "DEM_Log_Info", "DEM_Log_Warning", "DEM_Log_Error", "DEM_Log_Critical", "DEM_Log_Off" };\n'
     '\t\tconstexpr const char* const kLogLevelLabels[] = { "Trace", "Debug", "Info", "Warning", "Error", "Critical", "Off" };\n'
     '\t\tconstexpr int kLogLevelCount = 7;\n'
     '\n'
     '\t\t// Why a key cannot be bound. ReservedKeyReason() returns an INDEX into these rather than\n'
     '\t\t// a string, so one answer serves both the English log line and the translated status\n'
     '\t\t// line the player reads.\n'
     '\t\tconstexpr const char* const kReservedKeys[] = { "DEM_Res_Framework", "DEM_Res_Tab", "DEM_Res_Escape", "DEM_Res_MenuKey", "DEM_Res_Arrows", "DEM_Res_Enter", "DEM_Res_Space" };\n'
     '\t\tconstexpr const char* const kReservedLabels[] = {\n'
     '\t\t\t"the menu framework uses it",\n'
     '\t\t\t"Tab opens the Tween menu",\n'
     '\t\t\t"Escape closes menus",\n'
     '\t\t\t"it opens the mod configuration menu",\n'
     '\t\t\t"arrow keys drive menu navigation",\n'
     '\t\t\t"Enter activates the focused control",\n'
     '\t\t\t"Space activates the focused control"\n'
     '\t\t};\n'
     '\t\tconstexpr int kReservedNone = -1;\n'
     '\n'
     '\t\t// A Combo whose option list is rebuilt from TR\'d entries every frame: store owns the\n'
     '\t\t// translated bytes for the duration of the call, so the pointers stay valid.\n'
     '\t\tbool ComboTR(const char* a_label, int* a_current,\n'
     '\t\t\t\t\t const char* const* a_keys, const char* const* a_labels, int a_count)\n'
     '\t\t{\n'
     '\t\t\tstd::vector<std::string> store;\n'
     '\t\t\tstore.reserve(static_cast<std::size_t>(a_count));\n'
     '\t\t\tfor (int i = 0; i < a_count; ++i) { store.emplace_back(strings::TR(a_keys[i], a_labels[i])); }\n'
     '\t\t\tstd::vector<const char*> items;\n'
     '\t\t\titems.reserve(store.size());\n'
     '\t\t\tfor (const auto& s : store) { items.push_back(s.c_str()); }\n'
     '\t\t\treturn ImGuiMCP::Combo(a_label, a_current, items.data(), a_count);\n'
     '\t\t}\n'
     '\n'
     '\t\t// Substitutes one already-translated word into an already-translated sentence carrying a\n'
     '\t\t// single %s. Done by find-and-replace rather than snprintf so no runtime format string is\n'
     '\t\t// ever handed to a printf, and a translation that drops the %s truncates nothing.\n'
     '\t\tstd::string FormatOne(const char* a_format, const char* a_arg)\n'
     '\t\t{\n'
     '\t\t\tstd::string out = a_format ? a_format : "";\n'
     '\t\t\tconst auto at = out.find("%s");\n'
     '\t\t\tif (at != std::string::npos) { out.replace(at, 2, a_arg ? a_arg : ""); }\n'
     '\t\t\treturn out;\n'
     '\t\t}\n'),

    # --- ReservedKeyReason returns an index -----------------------------------------------------
    ('\t\t// nullptr means the key is fine to bind; anything else is the reason it is not, phrased\n'
     '\t\t// for the player rather than for the log.\n'
     '\t\tconst char* ReservedKeyReason(std::int32_t a_code)',
     '\t\t// kReservedNone means the key is fine to bind; anything else is an index into\n'
     '\t\t// kReservedKeys / kReservedLabels - the reason it is not, phrased for the player rather\n'
     '\t\t// than for the log. An index rather than a string so the caller can log the English and\n'
     '\t\t// show the player the translation from the one answer.\n'
     '\t\tint ReservedKeyReason(std::int32_t a_code)'),
    ('\t\t\t\tif (std::find(reported.begin(), reported.end(), a_code) != reported.end())\n'
     '\t\t\t\t{\n'
     '\t\t\t\t\treturn "the menu framework uses it";\n'
     '\t\t\t\t}\n'
     '\n'
     '\t\t\t\treturn nullptr;\n'
     '\t\t\t}\n',
     '\t\t\t\tif (std::find(reported.begin(), reported.end(), a_code) != reported.end())\n'
     '\t\t\t\t{\n'
     '\t\t\t\t\treturn 0;\n'
     '\t\t\t\t}\n'
     '\n'
     '\t\t\t\treturn kReservedNone;\n'
     '\t\t\t}\n'),
    ('\t\t\t\treturn "Tab opens the Tween menu";', '\t\t\t\treturn 1;'),
    ('\t\t\t\treturn "Escape closes menus";', '\t\t\t\treturn 2;'),
    ('\t\t\t\treturn "it opens the mod configuration menu";', '\t\t\t\treturn 3;'),
    ('\t\t\t\t\treturn "arrow keys drive menu navigation";', '\t\t\t\t\treturn 4;'),
    ('\t\t\t\t\treturn "Enter activates the focused control";', '\t\t\t\t\treturn 5;'),
    ('\t\t\t\t\treturn "Space activates the focused control";', '\t\t\t\t\treturn 6;'),
    ('\t\t\t\tdefault:\n'
     '\t\t\t\t\tbreak;\n'
     '\t\t\t\t}\n'
     '\t\t\t}\n'
     '\n'
     '\t\t\treturn nullptr;\n'
     '\t\t}\n',
     '\t\t\t\tdefault:\n'
     '\t\t\t\t\tbreak;\n'
     '\t\t\t\t}\n'
     '\t\t\t}\n'
     '\n'
     '\t\t\treturn kReservedNone;\n'
     '\t\t}\n'),

    # --- the two bind-refusal status lines ------------------------------------------------------
    ('\t\t\t\tif (const char* reason = ReservedKeyReason(code))\n'
     '\t\t\t\t{\n'
     '\t\t\t\t\tlogger::info("Refusing to bind key code {} - {}", code, reason);\n'
     '\t\t\t\t\tstatusMessage = std::string{ "That key is reserved (" } + reason +\n'
     '\t\t\t\t\t\t\t\t\t"). Press a different key.";\n'
     '\t\t\t\t\treturn false;\n'
     '\t\t\t\t}\n',
     '\t\t\t\tif (const int reason = ReservedKeyReason(code); reason != kReservedNone)\n'
     '\t\t\t\t{\n'
     '\t\t\t\t\tlogger::info("Refusing to bind key code {} - {}", code, kReservedLabels[reason]);\n'
     '\t\t\t\t\tstatusMessage = FormatOne(strings::TR("DEM_StatusKeyReserved", "That key is reserved (%s). Press a different key."),\n'
     '\t\t\t\t\t\t\t\t\t\t\t  strings::TR(kReservedKeys[reason], kReservedLabels[reason]));\n'
     '\t\t\t\t\treturn false;\n'
     '\t\t\t\t}\n'),
    ('\t\t\t\tconst char* otherName = (target == BindTarget::kHide) ? "zoom toggle" : "hide";\n'
     '\n'
     '\t\t\t\tif (code != 0 && code == otherCode)\n'
     '\t\t\t\t{\n'
     '\t\t\t\t\tlogger::info("Refusing to bind key code {} - already the {} key", code, otherName);\n'
     '\t\t\t\t\tstatusMessage = std::string{ "That key is already the " } + otherName +\n'
     '\t\t\t\t\t\t\t\t\t" key. Press a different key.";\n'
     '\t\t\t\t\treturn false;\n'
     '\t\t\t\t}\n',
     '\t\t\t\t// The English name goes to the log; the translated one goes to the player.\n'
     '\t\t\t\tconst char* otherName = (target == BindTarget::kHide) ? "zoom toggle" : "hide";\n'
     '\t\t\t\tconst char* otherNameText = (target == BindTarget::kHide)\n'
     '\t\t\t\t\t\t\t\t\t\t\t\t? strings::TR("DEM_KeyRoleZoomToggle", "zoom toggle")\n'
     '\t\t\t\t\t\t\t\t\t\t\t\t: strings::TR("DEM_KeyRoleHide", "hide");\n'
     '\n'
     '\t\t\t\tif (code != 0 && code == otherCode)\n'
     '\t\t\t\t{\n'
     '\t\t\t\t\tlogger::info("Refusing to bind key code {} - already the {} key", code, otherName);\n'
     '\t\t\t\t\tstatusMessage = FormatOne(strings::TR("DEM_StatusKeyTaken", "That key is already the %s key. Press a different key."), otherNameText);\n'
     '\t\t\t\t\treturn false;\n'
     '\t\t\t\t}\n'),

    # --- KeyBindRow: TR'd label in, English name for the log, id suffix kept outside ------------
    ('\t\tvoid KeyBindRow(const char* a_label, std::int32_t* a_keyCode, BindTarget a_target, const char* a_bindButtonId)\n'
     '\t\t{\n'
     '\t\t\tint keyCode = *a_keyCode;\n'
     '\t\t\tif (ImGuiMCP::InputInt(a_label, &keyCode))\n'
     '\t\t\t{\n'
     '\t\t\t\t*a_keyCode = keyCode < 0 ? 0 : keyCode;\n'
     '\t\t\t\tlogger::debug("{} set to {} (typed)", a_label, *a_keyCode);\n'
     '\t\t\t}\n'
     '\n'
     '\t\t\tImGuiMCP::SameLine();\n'
     '\n'
     '\t\t\tif (bindTarget.load() == a_target)\n'
     '\t\t\t{\n'
     '\t\t\t\tif (ImGuiMCP::Button("Press a key... (cancel)"))\n'
     '\t\t\t\t{\n'
     '\t\t\t\t\tbindTarget.store(BindTarget::kNone);\n'
     '\t\t\t\t\tlogger::debug("{} bind cancelled", a_label);\n'
     '\t\t\t\t}\n'
     '\t\t\t}\n'
     '\t\t\telse if (ImGuiMCP::Button(a_bindButtonId))\n'
     '\t\t\t{\n'
     '\t\t\t\tbindTarget.store(a_target);\n'
     '\t\t\t\tlogger::debug("{} bind started; waiting for a keypress", a_label);\n'
     '\t\t\t}\n'
     '\t\t}\n',
     '\t\t// a_label arrives ALREADY translated (its key is at the call site); a_logName is the\n'
     '\t\t// English name the log keeps; a_idSuffix ("##hide", "##zoom") is the ImGui id\n'
     '\t\t// disambiguator and is never part of the translated text.\n'
     '\t\tvoid KeyBindRow(const char* a_label, const char* a_logName, std::int32_t* a_keyCode, BindTarget a_target, const char* a_idSuffix)\n'
     '\t\t{\n'
     '\t\t\tconst std::string inputLabel = std::string(a_label) + a_idSuffix;\n'
     '\n'
     '\t\t\tint keyCode = *a_keyCode;\n'
     '\t\t\tif (ImGuiMCP::InputInt(inputLabel.c_str(), &keyCode))\n'
     '\t\t\t{\n'
     '\t\t\t\t*a_keyCode = keyCode < 0 ? 0 : keyCode;\n'
     '\t\t\t\tlogger::debug("{} set to {} (typed)", a_logName, *a_keyCode);\n'
     '\t\t\t}\n'
     '\n'
     '\t\t\tImGuiMCP::SameLine();\n'
     '\n'
     '\t\t\tif (bindTarget.load() == a_target)\n'
     '\t\t\t{\n'
     '\t\t\t\tconst std::string cancelLabel = std::string(strings::TR("DEM_PressAKey", "Press a key... (cancel)")) + a_idSuffix;\n'
     '\t\t\t\tif (ImGuiMCP::Button(cancelLabel.c_str()))\n'
     '\t\t\t\t{\n'
     '\t\t\t\t\tbindTarget.store(BindTarget::kNone);\n'
     '\t\t\t\t\tlogger::debug("{} bind cancelled", a_logName);\n'
     '\t\t\t\t}\n'
     '\t\t\t}\n'
     '\t\t\telse\n'
     '\t\t\t{\n'
     '\t\t\t\tconst std::string bindLabel = std::string(strings::TR("DEM_Bind", "Bind")) + a_idSuffix;\n'
     '\t\t\t\tif (ImGuiMCP::Button(bindLabel.c_str()))\n'
     '\t\t\t\t{\n'
     '\t\t\t\t\tbindTarget.store(a_target);\n'
     '\t\t\t\t\tlogger::debug("{} bind started; waiting for a keypress", a_logName);\n'
     '\t\t\t\t}\n'
     '\t\t\t}\n'
     '\t\t}\n'),

    # --- Display section ------------------------------------------------------------------------
    ('\t\t\tImGuiMCP::SeparatorText("Display");',
     '\t\t\tImGuiMCP::SeparatorText(strings::TR("DEM_SecDisplay", "Display"));'),
    ('\t\t\t\tlabels.push_back("Built-in frame");\n'
     '\t\t\t\tif (ImGuiMCP::Combo("Frame theme", &current, labels.data(), static_cast<int>(labels.size())))',
     '\t\t\t\t// The theme entries above are SWF FILE STEMS from the themes folder - data, not\n'
     '\t\t\t\t// text, and never translated. Only this mod\'s own "built-in" entry and the combo\'s\n'
     '\t\t\t\t// label are.\n'
     '\t\t\t\tlabels.push_back(strings::TR("DEM_BuiltInFrame", "Built-in frame"));\n'
     '\t\t\t\tif (ImGuiMCP::Combo(strings::TR("DEM_FrameTheme", "Frame theme"), &current, labels.data(), static_cast<int>(labels.size())))'),
    ('\t\t\t\t\tstatusMessage = "Frame theme selected. Press Save to keep it.";',
     '\t\t\t\t\tstatusMessage = strings::TR("DEM_StatusThemeSelected", "Frame theme selected. Press Save to keep it.");'),
    ('\t\t\t\tHelpMarker("Replaces the minimap\'s frame artwork. Themes are SWF files in Data/Interface/DragonsEyeMinimapThemes - drop one in and it appears here on the next game start. \\"Built-in frame\\" uses the artwork the mod ships, which is also what a frame-reskin mod replaces.");',
     '\t\t\t\tHelpMarker(strings::TR("DEM_HelpFrameTheme", "Replaces the minimap\'s frame artwork. Themes are SWF files in Data/Interface/DragonsEyeMinimapThemes - drop one in and it appears here on the next game start. \\"Built-in frame\\" uses the artwork the mod ships, which is also what a frame-reskin mod replaces."));'),
    ('\t\t\tif (ImGuiMCP::Combo("Corner", &anchor, kAnchorNames, kAnchorCount))',
     '\t\t\tif (ComboTR(strings::TR("DEM_Corner", "Corner"), &anchor, kAnchorKeys, kAnchorLabels, kAnchorCount))'),
    ('\t\t\tHelpMarker("Which screen corner the minimap sits in. With both offsets at 0 the artwork lines up flush with that corner.");',
     '\t\t\tHelpMarker(strings::TR("DEM_HelpCorner", "Which screen corner the minimap sits in. With both offsets at 0 the artwork lines up flush with that corner."));'),
    ('\t\t\tchanged |= NudgeableSlider("Offset X", &display::offsetX[offsetCorner], -600.0F, 600.0F, "%.0f px", 1.0F);\n'
     '\t\t\tHelpMarker("Nudge from the corner, in screen pixels. Positive is always rightwards, whichever corner is anchored. Each corner remembers its own pair.");',
     '\t\t\tchanged |= NudgeableSlider(strings::TR("DEM_OffsetX", "Offset X"), &display::offsetX[offsetCorner], -600.0F, 600.0F, "%.0f px", 1.0F);\n'
     '\t\t\tHelpMarker(strings::TR("DEM_HelpOffsetX", "Nudge from the corner, in screen pixels. Positive is always rightwards, whichever corner is anchored. Each corner remembers its own pair."));'),
    ('\t\t\tchanged |= NudgeableSlider("Offset Y", &display::offsetY[offsetCorner], -600.0F, 600.0F, "%.0f px", 1.0F);\n'
     '\t\t\tHelpMarker("Nudge from the corner, in screen pixels. Positive is always downwards, whichever corner is anchored. Each corner remembers its own pair.");',
     '\t\t\tchanged |= NudgeableSlider(strings::TR("DEM_OffsetY", "Offset Y"), &display::offsetY[offsetCorner], -600.0F, 600.0F, "%.0f px", 1.0F);\n'
     '\t\t\tHelpMarker(strings::TR("DEM_HelpOffsetY", "Nudge from the corner, in screen pixels. Positive is always downwards, whichever corner is anchored. Each corner remembers its own pair."));'),
    ('\t\t\tImGuiMCP::Text("Editing the %s offset.", kAnchorNames[offsetCorner]);',
     '\t\t\tImGuiMCP::Text(strings::TR("DEM_EditingOffset", "Editing the %s offset."),\n'
     '\t\t\t\t\t\t   strings::TR(kAnchorKeys[offsetCorner], kAnchorLabels[offsetCorner]));'),
    ('\t\t\tchanged |= NudgeableSlider("Scale", &display::scale, display::kScaleSliderMin, maxScale, "%.2f", 0.01F);\n'
     '\t\t\tHelpMarker("Size of the minimap. 1.00 is the size the artwork was drawn at. The top of the range is capped so the minimap stays within a quarter of the screen.");',
     '\t\t\tchanged |= NudgeableSlider(strings::TR("DEM_Scale", "Scale"), &display::scale, display::kScaleSliderMin, maxScale, "%.2f", 0.01F);\n'
     '\t\t\tHelpMarker(strings::TR("DEM_HelpScale", "Size of the minimap. 1.00 is the size the artwork was drawn at. The top of the range is capped so the minimap stays within a quarter of the screen."));'),
    ('\t\t\tImGuiMCP::Text("Largest allowed: %.2f (a quarter of the screen)", maxScale);',
     '\t\t\tImGuiMCP::Text(strings::TR("DEM_LargestAllowed", "Largest allowed: %.2f (a quarter of the screen)"), maxScale);'),
    ('\t\t\tif (ImGuiMCP::Combo("Shape", &shape, kShapeNames, kShapeCount))',
     '\t\t\tif (ComboTR(strings::TR("DEM_Shape", "Shape"), &shape, kShapeKeys, kShapeLabels, kShapeCount))'),
    ('\t\t\tHelpMarker("Whether the minimap is drawn as a square or as a circle.");',
     '\t\t\tHelpMarker(strings::TR("DEM_HelpShape", "Whether the minimap is drawn as a square or as a circle."));'),
    ('\t\t\t\tif (ImGuiMCP::Toggle("Show minimap", &shown))',
     '\t\t\t\tif (ImGuiMCP::Toggle(strings::TR("DEM_ShowMinimap", "Show minimap"), &shown))'),
    ('\t\t\t\tHelpMarker("Hides or shows the minimap right now, and remembers the choice for the next time you play.");',
     '\t\t\t\tHelpMarker(strings::TR("DEM_HelpShowMinimap", "Hides or shows the minimap right now, and remembers the choice for the next time you play."));'),
    ('\t\t\t\tif (ImGuiMCP::Toggle("Show minimap on game start", &display::showOnGameStart))',
     '\t\t\t\tif (ImGuiMCP::Toggle(strings::TR("DEM_ShowOnStart", "Show minimap on game start"), &display::showOnGameStart))'),
    ('\t\t\t\tHelpMarker("The minimap has not been built yet, so this only sets what happens once it is.");',
     '\t\t\t\tHelpMarker(strings::TR("DEM_HelpShowOnStart", "The minimap has not been built yet, so this only sets what happens once it is."));'),

    # --- Zoom section ---------------------------------------------------------------------------
    ('\t\t\tImGuiMCP::SeparatorText("Map zoom");',
     '\t\t\tImGuiMCP::SeparatorText(strings::TR("DEM_SecMapZoom", "Map zoom"));'),
    ('\t\t\tKeyBindRow("Zoom toggle key", &controls::zoomToggleKeyCode, BindTarget::kZoom, "Bind##zoom");\n'
     '\t\t\tHelpMarker("Press this key to jump between the two zoom levels below, instead of holding the control key and scrolling. 0 disables it.");',
     '\t\t\tKeyBindRow(strings::TR("DEM_ZoomToggleKey", "Zoom toggle key"), "Zoom toggle key", &controls::zoomToggleKeyCode, BindTarget::kZoom, "##zoom");\n'
     '\t\t\tHelpMarker(strings::TR("DEM_HelpZoomToggleKey", "Press this key to jump between the two zoom levels below, instead of holding the control key and scrolling. 0 disables it."));'),
    ('\t\t\t\tImGuiMCP::Text("No zoom key set.");',
     '\t\t\t\tImGuiMCP::Text("%s", strings::TR("DEM_NoZoomKey", "No zoom key set."));'),
    ('\t\t\tif (NudgeableSlider("Default zoom", &controls::zoomDefault, 0.0F, 1.0F, "%.3f", 0.01F))',
     '\t\t\tif (NudgeableSlider(strings::TR("DEM_DefaultZoom", "Default zoom"), &controls::zoomDefault, 0.0F, 1.0F, "%.3f", 0.01F))'),
    ('\t\t\t\tif (ImGuiMCP::Button("Set to current##default"))',
     '\t\t\t\tif (ImGuiMCP::Button((std::string(strings::TR("DEM_SetToCurrent", "Set to current")) + "##default").c_str()))'),
    ('\t\t\tif (NudgeableSlider("Zoomed in", &controls::zoomZoomedIn, 0.0F, 1.0F, "%.3f", 0.01F))',
     '\t\t\tif (NudgeableSlider(strings::TR("DEM_ZoomedIn", "Zoomed in"), &controls::zoomZoomedIn, 0.0F, 1.0F, "%.3f", 0.01F))'),
    ('\t\t\t\tif (ImGuiMCP::Button("Set to current##zoomedin"))',
     '\t\t\t\tif (ImGuiMCP::Button((std::string(strings::TR("DEM_SetToCurrent", "Set to current")) + "##zoomedin").c_str()))'),
    ('\t\t\tHelpMarker("The zoom toggle key alternates between these two. Zoom the map where you want it, then press \\"Set to current\\" to store that level rather than typing a number in units the game does not document.");',
     '\t\t\tHelpMarker(strings::TR("DEM_HelpZoomLevels", "The zoom toggle key alternates between these two. Zoom the map where you want it, then press \\"Set to current\\" to store that level rather than typing a number in units the game does not document."));'),
    ('\t\t\t\tImGuiMCP::TextWrapped("Zoom the map in game and use \\"Set to current\\" once the minimap is running - "\n'
     '\t\t\t\t\t\t\t\t\t  "the numbers above are in the camera\'s own units, which are not documented.");',
     '\t\t\t\tImGuiMCP::TextWrapped("%s", strings::TR("DEM_ZoomNotReady", "Zoom the map in game and use \\"Set to current\\" once the minimap is running - the numbers above are in the camera\'s own units, which are not documented."));'),
    ('\t\t\tif (NudgeableSlider("Live zoom", &live, 0.0F, 1.0F, "%.3f", 0.01F))',
     '\t\t\tif (NudgeableSlider(strings::TR("DEM_LiveZoom", "Live zoom"), &live, 0.0F, 1.0F, "%.3f", 0.01F))'),
    ('\t\t\tHelpMarker("How far the minimap is zoomed in, right now. The game applies its own limits, so the value can settle somewhere other than where you left it.");',
     '\t\t\tHelpMarker(strings::TR("DEM_HelpLiveZoom", "How far the minimap is zoomed in, right now. The game applies its own limits, so the value can settle somewhere other than where you left it."));'),

    # --- Compass section ------------------------------------------------------------------------
    ('\t\t\tImGuiMCP::SeparatorText("Compass");',
     '\t\t\tImGuiMCP::SeparatorText(strings::TR("DEM_SecCompass", "Compass"));'),
    ('\t\t\tImGuiMCP::Toggle("Compass ring", &compass::compassRing);\n'
     '\t\t\tHelpMarker("The compass ring that takes the minimap\'s corner while the map is hidden. Off = nothing is drawn there when the map is hidden.");',
     '\t\t\tImGuiMCP::Toggle(strings::TR("DEM_CompassRing", "Compass ring"), &compass::compassRing);\n'
     '\t\t\tHelpMarker(strings::TR("DEM_HelpCompassRing", "The compass ring that takes the minimap\'s corner while the map is hidden. Off = nothing is drawn there when the map is hidden."));'),
    ('\t\t\tImGuiMCP::Toggle("Quest pointer", &compass::questPointer);\n'
     '\t\t\tHelpMarker("The vanilla-style quest marker with the distance readout, riding the ring or the visible map. Off = never drawn.");',
     '\t\t\tImGuiMCP::Toggle(strings::TR("DEM_QuestPointer", "Quest pointer"), &compass::questPointer);\n'
     '\t\t\tHelpMarker(strings::TR("DEM_HelpQuestPointer", "The vanilla-style quest marker with the distance readout, riding the ring or the visible map. Off = never drawn."));'),
    ('\t\t\tImGuiMCP::Toggle("Metric units", &compass::metricUnits);\n'
     '\t\t\tHelpMarker("Distance readout in metres instead of feet.");',
     '\t\t\tImGuiMCP::Toggle(strings::TR("DEM_MetricUnits", "Metric units"), &compass::metricUnits);\n'
     '\t\t\tHelpMarker(strings::TR("DEM_HelpMetricUnits", "Distance readout in metres instead of feet."));'),

    # --- Controls section -----------------------------------------------------------------------
    ('\t\t\tImGuiMCP::SeparatorText("Controls");',
     '\t\t\tImGuiMCP::SeparatorText(strings::TR("DEM_SecControls", "Controls"));'),
    ('\t\t\tKeyBindRow("Hide key", &controls::hideKeyCode, BindTarget::kHide, "Bind##hide");\n'
     '\t\t\tHelpMarker("Press this key to show or hide the minimap immediately. 0 disables it.");',
     '\t\t\tKeyBindRow(strings::TR("DEM_HideKey", "Hide key"), "Hide key", &controls::hideKeyCode, BindTarget::kHide, "##hide");\n'
     '\t\t\tHelpMarker(strings::TR("DEM_HelpHideKey", "Press this key to show or hide the minimap immediately. 0 disables it."));'),
    ('\t\t\t\tImGuiMCP::Text("No hide key set.");',
     '\t\t\t\tImGuiMCP::Text("%s", strings::TR("DEM_NoHideKey", "No hide key set."));'),
    ('\t\t\tif (ImGuiMCP::Toggle("Controller: tap to hide, hold to pan", &controls::gamepadHideButtonEnabled))',
     '\t\t\tif (ImGuiMCP::Toggle(strings::TR("DEM_ControllerButton", "Controller: tap to hide, hold to pan"), &controls::gamepadHideButtonEnabled))'),
    ('\t\t\tHelpMarker("Off by default. On: tapping the controller button below hides/shows the minimap, holding it pans the map with the RIGHT stick. Pick a button that is free in your layout.");',
     '\t\t\tHelpMarker(strings::TR("DEM_HelpControllerButton", "Off by default. On: tapping the controller button below hides/shows the minimap, holding it pans the map with the RIGHT stick. Pick a button that is free in your layout."));'),
    ('\t\t\t\tif (ImGuiMCP::InputInt("Controller button (XInput mask)", &code))',
     '\t\t\t\tif (ImGuiMCP::InputInt(strings::TR("DEM_ControllerMask", "Controller button (XInput mask)"), &code))'),
    ('\t\t\t\tHelpMarker("XInput button masks: 128 = R3 (right stick click), 64 = L3, 256 = LB, 512 = RB, 16 = Start, 32 = Back, 4096 A, 8192 B, 16384 X, 32768 Y.");',
     '\t\t\t\tHelpMarker(strings::TR("DEM_HelpControllerMask", "XInput button masks: 128 = R3 (right stick click), 64 = L3, 256 = LB, 512 = RB, 16 = Start, 32 = Back, 4096 A, 8192 B, 16384 X, 32768 Y."));'),
    ('\t\t\tif (ImGuiMCP::Toggle("Show location name", &display::showLocationName))',
     '\t\t\tif (ImGuiMCP::Toggle(strings::TR("DEM_ShowLocationName", "Show location name"), &display::showLocationName))'),
    ('\t\t\tHelpMarker("The location name under the map. Turn this off if your game\'s language shows missing characters or text running past the frame - the title uses the game\'s own interface font, which this mod cannot change.");',
     '\t\t\tHelpMarker(strings::TR("DEM_HelpShowLocationName", "The location name under the map. Turn this off if your game\'s language shows missing characters or text running past the frame - the title uses the game\'s own interface font, which this mod cannot change."));'),
    ('\t\t\tif (ImGuiMCP::Toggle("Rotate with the player", &controls::followPlayerCameraRotation))',
     '\t\t\tif (ImGuiMCP::Toggle(strings::TR("DEM_RotateWithPlayer", "Rotate with the player"), &controls::followPlayerCameraRotation))'),
    ('\t\t\tHelpMarker("On: the minimap turns to face where the player is looking. Off: north is always up, like the local map.");',
     '\t\t\tHelpMarker(strings::TR("DEM_HelpRotateWithPlayer", "On: the minimap turns to face where the player is looking. Off: north is always up, like the local map."));'),

    # --- Debug section --------------------------------------------------------------------------
    ('\t\t\tImGuiMCP::SeparatorText("Debug");',
     '\t\t\tImGuiMCP::SeparatorText(strings::TR("DEM_SecDebug", "Debug"));'),
    ('\t\t\tif (ImGuiMCP::Combo("Log level", &logLevel, kLogLevelNames, kLogLevelCount))',
     '\t\t\tif (ComboTR(strings::TR("DEM_LogLevel", "Log level"), &logLevel, kLogLevelKeys, kLogLevelLabels, kLogLevelCount))'),
    ('\t\t\t\tlogger::debug("Log level set to {}", kLogLevelNames[logLevel]);',
     '\t\t\t\tlogger::debug("Log level set to {}", kLogLevelLabels[logLevel]);'),
    ('\t\t\tHelpMarker("How much detail the plugin writes to its log. Leave this on Info unless you are chasing a problem.");',
     '\t\t\tHelpMarker(strings::TR("DEM_HelpLogLevel", "How much detail the plugin writes to its log. Leave this on Info unless you are chasing a problem."));'),

    # --- the buttons row ------------------------------------------------------------------------
    ('\t\t\tif (ImGuiMCP::Button("Save"))\n'
     '\t\t\t{\n'
     '\t\t\t\tstatusMessage = "Saving...";\n'
     '\t\t\t\tOnMainThread([]() {\n'
     '\t\t\t\t\tstatusMessage = settings::Save() ? "Settings saved." : "Could not write the INI. See the log for why.";\n'
     '\t\t\t\t});\n'
     '\t\t\t}\n'
     '\t\t\tHelpMarker("Writes every setting on this page to the plugin\'s INI so it survives a restart.");',
     '\t\t\tif (ImGuiMCP::Button(strings::TR("DEM_Save", "Save")))\n'
     '\t\t\t{\n'
     '\t\t\t\tstatusMessage = strings::TR("DEM_StatusSaving", "Saving...");\n'
     '\t\t\t\tOnMainThread([]() {\n'
     '\t\t\t\t\tstatusMessage = settings::Save() ? strings::TR("DEM_StatusSaved", "Settings saved.")\n'
     '\t\t\t\t\t\t\t\t\t\t\t\t\t : strings::TR("DEM_StatusSaveFailed", "Could not write the INI. See the log for why.");\n'
     '\t\t\t\t});\n'
     '\t\t\t}\n'
     '\t\t\tHelpMarker(strings::TR("DEM_HelpSave", "Writes every setting on this page to the plugin\'s INI so it survives a restart."));'),
    ('\t\t\tif (ImGuiMCP::Button("Reload from INI"))\n'
     '\t\t\t{\n'
     '\t\t\t\tstatusMessage = "Reloading...";',
     '\t\t\tif (ImGuiMCP::Button(strings::TR("DEM_ReloadFromIni", "Reload from INI")))\n'
     '\t\t\t{\n'
     '\t\t\t\tstatusMessage = strings::TR("DEM_StatusReloading", "Reloading...");'),
    ('\t\t\t\t\t\tstatusMessage = "Settings reloaded from the INI.";',
     '\t\t\t\t\t\tstatusMessage = strings::TR("DEM_StatusReloaded", "Settings reloaded from the INI.");'),
    ('\t\t\t\t\t\tstatusMessage = "Could not read the INI. See the log for why.";',
     '\t\t\t\t\t\tstatusMessage = strings::TR("DEM_StatusReloadFailed", "Could not read the INI. See the log for why.");'),
    ('\t\t\tHelpMarker("Throws away any change made here since the last save and re-reads the INI from disk. Also picks up edits made to the file by hand.");',
     '\t\t\tHelpMarker(strings::TR("DEM_HelpReload", "Throws away any change made here since the last save and re-reads the INI from disk. Also picks up edits made to the file by hand."));'),
    ('\t\t\tif (ImGuiMCP::Button("Restore defaults"))',
     '\t\t\tif (ImGuiMCP::Button(strings::TR("DEM_RestoreDefaults", "Restore defaults")))'),
    ('\t\t\t\tstatusMessage = "Defaults restored. Press Save to keep them.";',
     '\t\t\t\tstatusMessage = strings::TR("DEM_StatusDefaultsRestored", "Defaults restored. Press Save to keep them.");'),
    ('\t\t\tHelpMarker("Puts every setting back to the value it has on a fresh install. Nothing is written until you press Save.");',
     '\t\t\tHelpMarker(strings::TR("DEM_HelpRestoreDefaults", "Puts every setting back to the value it has on a fresh install. Nothing is written until you press Save."));'),

    # --- the page callback: Tick() first, then the intro line -----------------------------------
    ('\t\tMarkPanelDrawn();\n'
     '\n'
     '\t\tImGuiMCP::TextWrapped("Changes apply as soon as you make them. Press Save to keep them for the next time you play.");',
     '\t\tMarkPanelDrawn();\n'
     '\n'
     '\t\t// Follows the Apocrypha Menu Framework\'s Language setting: a string compare per drawn\n'
     '\t\t// frame, and a reload of this mod\'s translation file only when that language changed.\n'
     '\t\tstrings::Tick();\n'
     '\n'
     '\t\tImGuiMCP::TextWrapped("%s", strings::TR("DEM_Intro", "Changes apply as soon as you make them. Press Save to keep them for the next time you play."));'),
]


def patch_ui_cpp():
    path = os.path.join(REPO, "source", "UI.cpp")
    text = read(path)
    if "strings::Tick();" in text:
        print("  UI.cpp: already patched")
        return
    text = apply_all(text, UI_PAIRS, "UI.cpp")
    write(path, text)
    print("  UI.cpp: {} ({} anchors)".format("anchors all match" if CHECK_ONLY else "patched", len(UI_PAIRS)))


def main():
    print("lang-patch.py - Dragon's Eye Minimap{}".format(" (--check, nothing written)" if CHECK_ONLY else ""))
    patch_skse_menu_framework_h()
    patch_message_listeners_cpp()
    patch_devbench_tool_cpp()
    patch_ui_cpp()
    print("done")


if __name__ == "__main__":
    main()
