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

Use a short duration override when the goal is corpus-wide startup/early-exit
classification rather than long visual inspection:

```bat
corpus-run.bat -Manifest Tools\corpus\manifest.json -OutRoot Runs\CorpusEarlyExitProbe -DurationOverrideMs 30000
```

For focused follow-up on the currently known very-short exits:

```bat
corpus-run.bat -Manifest Tools\corpus\short-exit-recheck-manifest.json -OutRoot Runs\CorpusShortExitRecheck
```

During Phase 12 integration and packaging work, run the focused regression gate
after every major refactor:

```bat
corpus-run.bat -Manifest Tools\corpus\phase12-smoke-manifest.json -OutRoot Runs\Phase12Smoke
```

The frozen full-corpus paths, accepted differences, and gate criteria are in
`Tools/corpus/PHASE12_BASELINE.md`.

When classifying startup exits, prefer terminal events in the logs over video
duration alone. A run can stop early because it hit a host-side cap such as
`maxLoggedCalls` while the VM is still in `state=0`; actual guest exits show
`vTerminateVMGP()` and may expose the reason through `vMsgBox()` text.

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

## System Font Notes

Current corpus probes distinguish at least two SDK system-font faces:

- `DeepAbyss 1.4` menu text uses `vSelectFont(size=0, flags=0)`, which maps to
  the SDK `Normal` face.
- `VRally` menu text uses `vSelectFont(size=1, flags=0x10000002)`, which maps to
  `Small + Bold + ShadowLowerRight`.

The current screenshot-derived T230 atlas is only a candidate source. Visual
comparison against real-phone references shows that it is closer to a small
face than to the final normal face. Until proper reference-matched faces are
reconstructed, the runtime keeps `Normal` wired to an explicit placeholder and
`Small` wired to the current candidate atlas.

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
runner can extract this automatically. Reference-runner sessions can also carry
their own date; the generated `ref-runner-*` manifests preserve that value so
MVM and the reference emulator receive the same scripted input and date.

In the current corpus, `.mpc` sidecars exist only for `VRally`, but several
other games still reference absent sidecar names from inside their `.mpn`:

- `IcebloxPlus610.mpc`
- `lunar.mpc`
- `Huntsman.mpc`
- `prhstrkmn.mpc`

The 2026-07-18 reference-input corpus run refined the sidecar classification:
the old emulator also fails to open `IcebloxPlus610.mpc`, `lunar.mpc`, and
`Huntsman.mpc`, but continues execution. Therefore missing sidecar events should
be treated as informational unless they are followed by behavior that diverges
from the reference. The follow-up Phase 11 fix changed `vCheckDataCert` /
`vCheckDataCertFile` to return SDK-style `0` for accepted certificates instead
of the old stubbed `1`, which removed the false `license-expired` path for
`IcebloxPlus610` and `Huntsman`. Full certificate/date/policy interpretation
remains a backlog item for titles whose guest flow still differs after the
accepted-cert result. The machine-readable corpus classification list lives in
`Tools/corpus/classifications.json`; the run analysis is in
`Runs/RefCompare_20260718/analysis.md`.
