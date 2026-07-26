# Phase 12 Regression Baseline

This document freezes the Phase 11 behavioral baseline used while Phase 12
changes configuration, packaging, public APIs, and platform boundaries.

## Focused Gate

Run after every major Phase 12 refactor:

```bat
corpus-run.bat -Manifest Tools\corpus\phase12-smoke-manifest.json -OutRoot Runs\Phase12Smoke
```

The gate covers the same game on both built-in profiles. The T310 run injects
one menu input; the T610 run exercises no-input startup. Both runs record frame
and audio artifacts.

Accept the gate only when:

- the build succeeds and both runs have process exit code `0`;
- neither run times out;
- terminal VM state and error match the pre-change focused run;
- logs contain no new invalid opcode, missing syscall, memory fault, or fatal VM
  event;
- both recording directories contain `recording.txt` and captured frames;
- encoded videos are present when `ffmpeg` is available;
- input generation remains present for the T310 `start_menu` scenario.

Store each result under a new timestamped `Runs/Phase12Smoke/` directory. Do not
overwrite an earlier gate when comparing a major change.

Initial focused result: `Runs/Phase12Smoke/20260719_143402`.

- T310 `start_menu`: exit code `0`, no timeout, `state=0 error=0`, 3,765,110
  steps, frames/audio/video present;
- T610 `none`: exit code `0`, no timeout, `state=0 error=0`, 3,659,879 steps,
  frames/audio/video present;
- neither log contains an invalid opcode, missing syscall, memory fault, or
  fatal VM event.

Step counts are diagnostic only because this gate is wall-clock bounded. Treat
terminal state/events and artifact presence as the stable acceptance signals.

## Full-Corpus Freeze

The Phase 11 close-out batches are:

- T310: `Runs/RefInputT310/20260718_233046` (21/21 completed);
- T610: `Runs/RefInputT610/20260718_232534` (13/13 completed);
- comparison and visual analysis: `Runs/RefCompare_20260718/analysis.md`;
- machine-readable exceptional outcomes: `Tools/corpus/classifications.json`.

Both close-out batches completed without runner timeout, invalid opcode, or
missing syscall. The saved T310 baseline contains one `snowboardx_T310`
`LDBU addr OOB` terminal failure (`exit=2`, `state=5 error=4`) that was omitted
from the original prose summary. Run the full T310 and T610 reference-input
manifests again at Phase 12 milestone/freeze points, especially before closing
the phase.

Pre-MCU Phase 12 full regression checkpoint (2026-07-19):

- T310: `Runs/Phase12FullT310/20260719_154109` (21/21 completed);
- T610: `Runs/Phase12FullT610/20260719_154108` (13/13 completed);
- comparison: `Runs/Phase12FullRegression_20260719/analysis.md`;
- no integration regression was found, all current recording artifacts are
  present, and the old timing-sensitive `snowboardx_T310` memory fault did not
  reproduce.

## Accepted Baseline Differences

The following are existing defects, not automatic Phase 12 regressions:

- system-font and renderer parity defects listed in `ROADMAP.md`, including
  DeepAbyss, HoneyCave/HoneyCave2, FiveStones, SpaceExplorer, and rally titles;
- VM timing/cadence and scripted-input progression differences from the
  reference emulator;
- certificate/date/policy gaps and guest-controlled short exits recorded in
  `Tools/corpus/classifications.json`;
- unsupported `prhstrkmn_T610` device/profile behavior;
- reference-artifact gaps documented in the Phase 11 analysis.

Do not update classifications for an integration refactor unless the behavior
change is deliberate and separately reviewed.
