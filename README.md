# Chit Chat — After Effects effect

**Chit Chat** is a native After Effects effect. Apply it to a solid (typically a phone-screen color) to draw iMessage-style rounded text bubbles. You own the right side; the other person owns the left.

Match name: `ChitChat`  
Category: **Text**

## What it does

- Stores a transcript of up to 64 messages (plain text, `me:` / `them:`).
- **Appear ID** is a hold-keyframed integer. At time 0 with value `0`, nothing is visible. A keyframe of `1` reveals message 1; `2` reveals message 2 and pushes earlier bubbles up. If a keyframe jumps (for example `1` → `5`), messages 2–5 all trigger at that time.
- New bubbles fade from 0% to 100% opacity over **Fade In Time**, starting **Appear Offset** pixels below their rest position. Older bubbles (left and right) animate upward to make room.
- Global bubble styling: corner radius, padding, spacing, type, you/them colors.

## After Effects usage

1. Create a solid the size of the phone screen. Apply **Effect > Text > Chit Chat**.
2. Click **Edit Messages...** and enter a transcript:

```
me: Hey — are you free later?
them: Yeah, what's going on?
me: Thought we could grab coffee.
```

Optional explicit IDs:

```
1 | me | Hey
3 | them | Skipping 2 still works — 1 and 3 both exist, Appear ID 3 reveals both 1 and 3
```

3. Set **Appear ID** to 0 at the start of the layer (hold interpolation is forced).
4. Add keyframes: at 1:00 value `1`, at 2:00 value `2`, and so on.
5. Tune **Fade In Time**, **Appear Offset**, radius, padding, spacing, fonts, and the You/Them colors.

### Parameters

| Group | Parameter | Role |
| --- | --- | --- |
| Messages | Edit Messages... | Opens the transcript editor |
| Messages | Appear ID | Integer 0–64, hold-keyframed trigger |
| Animation | Fade In Time | Seconds, 0 = instant |
| Animation | Appear Offset | Extra pixels downward at the start of a spawn |
| Bubbles | Corner Radius, Padding, Bubble Spacing | Shared by every bubble |
| Bubbles | Max Bubble Width % | Wrap width relative to the layer |
| Bubbles | Side / Bottom Margin | Inset from the layer edges |
| Type | Font, Font Size, Bold | Shared type |
| You (right) | Bubble + text color | Owner of the phone |
| Them (left) | Bubble + text color | The other person |

## Build (Adobe SDK)

Adobe does not allow redistributing the After Effects SDK. You need:

1. [After Effects C++ SDK](https://developer.adobe.com/console/) (Downloads tab, After Effects)
2. Visual Studio 2019 or 2022 with C++ desktop workload
3. CMake 3.20+ (optional; the SDK Skeleton project also works)

```bat
set AESDK_ROOT=C:\path\to\AfterEffectsSDK
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Embed the PiPL (required or AE will ignore the `.aex`):

1. Preprocess `src/plugin/ChitChatPiPL.r` with the SDK headers (`cl /EP /DMSWindows /DMSWindows=1 /DAE_OS_WIN /DAE_PROC_INTELx64 ...`).
2. Run the SDK `PiPLtool.exe` to produce `ChitChatPiPL.rrc`.
3. Add a resource: `16000 PiPL DISCARDABLE "ChitChatPiPL.rrc"`.

The Adobe-recommended shortcut is to copy the SDK **Skeleton** project, replace its sources with this repo’s `src/` files, keep Skeleton’s PiPL custom build step, and rename Skeleton → Chit Chat in the PiPL (`Name`, `Match_Name`, `Category`).

Copy `ChitChat.aex` to:

`C:\Program Files\Adobe\Adobe After Effects <year>\Support Files\Plug-ins\Effects\`

Restart After Effects.

## Tests (no SDK)

```bat
python tests/test_chitchat.py
```

## Security notes (v1)

- Transcript size, message count, and per-message length are capped.
- UTF-8 is validated; NULs are stripped; the editor never evaluates text as code.
- Flattened project data is magic/version checked before use; a damaged arb resets to defaults.
- Fonts are chosen from a whitelist (no user-supplied font files).

## Design choices to confirm

These are implemented; say if you want them changed:

1. **Skipped Appear IDs** (keyframe `1` → `5`) reveal every message with `id <= 5` at that time, as one fade/push group.
2. **Appear Offset** is downward (new bubble starts lower, settles up). Positive offset only.
3. Incoming fade uses a smoothstep, not linear.
4. Messages are edited in a dialog (plain text), not a text layer. A text-layer feed can be added later.
5. First version is **Windows** (Direct2D / DirectWrite). macOS would need a Core Text renderer.
6. The solid’s pixels stay as the chat background; bubbles composite on top.
7. **Max Bubble Width %**, side margin, and bottom margin were added so wrapping and phone-frame insets work.

## Project layout

```
src/core/     layout, transcript parse, arb blob (no Adobe headers)
src/win/      Direct2D renderer + transcript dialog
src/plugin/   SmartFX effect, PiPL, composite
tests/        layout/trigger unit tests
```
