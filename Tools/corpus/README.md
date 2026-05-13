# Corpus Runner

## Run

From the repository root:

```bat
corpus-run.bat
```

The batch wrapper uses:

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File Tools\corpus-run.ps1
```

All extra arguments are passed through to the PowerShell runner:

```bat
corpus-run.bat -Manifest Tools\corpus\smoke-input-manifest.json -OutRoot Runs\CorpusInputSmoke -NoBuild
```

## Button Map

Use Mophun button names in `manifest.json` input scenarios. The runner maps
them to the same VM button masks used by the interactive SDL keyboard backend.

| Manifest button | Interactive desktop keys |
| --- | --- |
| `UP` | Up Arrow, Numpad 8 |
| `DOWN` | Down Arrow, Numpad 2 |
| `LEFT` | Left Arrow, Numpad 4 |
| `RIGHT` | Right Arrow, Numpad 6 |
| `FIRE` | Left Ctrl, Right Ctrl, Left Shift, Right Shift |
| `FIRE2` | Space, Numpad Enter |
| `SELECT` | Backspace, Enter |
| `POINTER` | Synthetic-only pointer-down mask |
| `POINTER_ALT` | Synthetic-only alternate pointer-down mask |

Diagonal movement is available interactively through the numeric keypad:

| Interactive desktop key | VM buttons |
| --- | --- |
| Numpad 7 | `UP` + `LEFT` |
| Numpad 9 | `UP` + `RIGHT` |
| Numpad 1 | `DOWN` + `LEFT` |
| Numpad 3 | `DOWN` + `RIGHT` |

For scripted diagonal input, use explicit `down`/`up` pairs:

```json
[
  { "down": "UP" },
  { "down": "LEFT" },
  { "waitMs": 300 },
  { "up": "LEFT" },
  { "up": "UP" }
]
```

## Input Steps

Supported manifest input steps:

```json
{ "waitMs": 1000 }
{ "press": "FIRE", "durationMs": 120 }
{ "down": "LEFT" }
{ "up": "LEFT" }
```

Use an empty input array for a no-input smoke scenario:

```json
{
  "name": "none",
  "input": []
}
```

## Fixed Date

The manifest can define a batch default:

```json
"defaultFixedDateTime": "2005-01-01T12:00:00"
```

Individual games can override it when their data certificate has a narrow
validity window:

```json
{
  "game": "mpn/T610/VRally_T610_decrypted.mpn",
  "profiles": [ "SE_T610" ],
  "fixedDateTime": "2003-11-04T12:00:00"
}
```

Observed VRally/CMRally certificates encode validity tags such as
`M001YYYYMMDD` and `M002YYYYMMDD`; choose a date inside that range until the
runner can extract this automatically.
