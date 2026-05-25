# Packet 002 Browser Runner Verification

Status date: 2026-05-24

This artifact is the Packet 002 verification checklist and report. It is intentionally narrow: verify the browser runner over local HTTP, compare browser output to native `cnes --json`, and record what passed, what is blocked, and what still needs human review.

## Local HTTP Command

Use HTTP, not `file://`.

```bash
cd "/Users/rj/Documents/Benjamins Project/Web Version/public"
python3 -m http.server 4173 --bind 127.0.0.1
```

Expected URL:

```text
http://127.0.0.1:4173/runner/
```

Runtime assets must resolve from the same origin:

```text
http://127.0.0.1:4173/runtime/cnes-module.js
http://127.0.0.1:4173/runtime/cnes-module.wasm
http://127.0.0.1:4173/runtime-data/materials/manifest.json
http://127.0.0.1:4173/runtime-data/libs/manifest.json
```

The served root is `Web Version/public` because the runner, worker runtime, WASM runtime, and runtime data are all static assets there.

The verification-only helper below serves the canonical static root `Web Version/public`.

```bash
node "/Users/rj/Documents/Benjamins Project/Web Version/runner verification/serve-local-http.mjs"
```

## Repeatable Disk Checks

Run this before browser smoke testing:

```bash
node "/Users/rj/Documents/Benjamins Project/Web Version/runner verification/verify-runtime-data.mjs"
```

Expected result:

```text
PASS materials: 240 files, 2163600 bytes, sha256 verified
PASS libs: 1 files, 1276 bytes, sha256 verified
```

The script verifies manifest `count`, summed `totalBytes`, per-file byte counts, per-file `sha256`, URL paths, VFS paths, manifest kind, integrity mode, and mount root.

## Repeatable Chrome Browser Check

Run this with the local HTTP server active and a temporary Chrome instance exposing DevTools on port `9223`:

```bash
/Applications/Google\ Chrome.app/Contents/MacOS/Google\ Chrome \
  --headless=new \
  --disable-gpu \
  --no-first-run \
  --user-data-dir=/private/tmp/cones-chrome-cdp \
  --remote-debugging-address=127.0.0.1 \
  --remote-debugging-port=9223 \
  about:blank

node "/Users/rj/Documents/Benjamins Project/Web Version/runner verification/verify-browser-runner.mjs"
```

Latest automated result:

```text
PASS examples/test.cnes: Success, exit 0, JSON Parsed
PASS tests/test_units.cnes: Success, exit 0, JSON Parsed
PASS examples/HVAC_example.cnes: Success, exit 0, JSON Parsed
PASS invalid parser smoke: Failure, exit 1, JSON Parsed
```

This drives the browser runner DOM in Chrome, selects synthetic `.cnes` `File` objects, clicks Run, waits for terminal UI state, and validates stdout JSON against the native expectations below. It does not replace a human file-picker/download review.

## Native Expectations

Run from `/Users/rj/Documents/Benjamins Project/CoNES`:

```bash
./cnes tests/test_units.cnes --json
./cnes examples/test.cnes --json
./cnes examples/HVAC_example.cnes --json
printf 'x :=\n' | ./cnes /dev/stdin --json
```

Expected native baselines:

| Case | Input | Expected native result |
| --- | --- | --- |
| Unit smoke | `CoNES/tests/test_units.cnes` | Exit `0`, JSON `success: true`, variable `Power` is `50.0000000000` with unit `W`. |
| Include smoke | `CoNES/examples/test.cnes` | Exit `0`, JSON `success: true`, variable `T_surface` is `40.0000000000` with unit `C`. |
| Material smoke | `CoNES/examples/HVAC_example.cnes` | Exit `0`, JSON `success: true`, variable `COP_HP` is `11.7950669027`. |
| Failure smoke | invalid script `x :=` | Nonzero exit, captured JSON `success: false`, parser error text present, page must not freeze in browser. |

## Browser Smoke Checklist

These checks were split between repeatable Chrome automation and final human interaction review.

| Status | Check | Acceptance |
| --- | --- | --- |
| Automated pass | Load runner over local HTTP | Open `http://127.0.0.1:4173/runner/`; do not use `file://`. |
| Automated pass | Worker starts | Worker fetches `/runtime/cnes-module.js` and `/runtime/cnes-module.wasm` without surfacing an error to the UI. |
| Automated pass | Runtime data mounts | Worker fetches both manifests and all listed assets; byte and sha256 verification pass before `callMain()`. |
| Automated pass | Unit smoke | Upload `CoNES/tests/test_units.cnes`; stdout JSON has `success: true` and `Power = 50 W`. |
| Automated pass | Include smoke | Upload `CoNES/examples/test.cnes`; stdout JSON has `success: true` and `T_surface = 40 C`, proving `/app/libs` is mounted. |
| Automated pass | Material smoke | Upload `CoNES/examples/HVAC_example.cnes`; stdout JSON has `success: true` and `COP_HP = 11.7950669027`, proving `/app/materials` is mounted. |
| Automated pass | Failure smoke | Upload or enter `x :=`; UI reports failure, stdout/stderr remain visible separately, controls recover, and the page stays responsive. |
| Human accepted | File picker and download | Native browser file picker works for a human-selected file, and downloaded result matches displayed raw stdout JSON for successful runs. |

## Current Report

Passed:

- Native unit smoke passed with `Power = 50.0000000000 W`.
- Native include smoke passed with `T_surface = 40.0000000000 C`.
- Native material smoke passed with `COP_HP = 11.7950669027`.
- Native invalid-script smoke returned nonzero with JSON `success: false` and a parser error.
- Runtime-data disk verification passed for `materials` and `libs`.
- Runtime and UI source files are present: `Web Version/public/runner`, `Web Version/public/runtime/cnes-worker.js`, `Web Version/public/runtime/cnes-module.js`, and `Web Version/public/runtime/cnes-module.wasm`.
- `Web Version/public` served over local HTTP returned 200 for `/runner/`, `/runner/runner.js`, `/runner/worker-client.js`, `/runtime/cnes-worker.js`, `/runtime/cnes-module.js`, `/runtime/cnes-module.wasm`, `/runtime-data/materials/manifest.json`, and `/runtime-data/libs/manifest.json`.
- `serve-local-http.mjs` returned 200 for `/`, `/runner.js`, `/worker-client.js`, `/runtime/cnes-worker.js`, `/runtime/cnes-module.js`, `/runtime/cnes-module.wasm`, `/runtime-data/materials/manifest.json`, and `/runtime-data/libs/manifest.json`.
- Headless Chrome render produced a nonblank runner screenshot with upload, run, reset, stdout, stderr, and metadata regions visible.
- `verify-browser-runner.mjs` passed in Chrome DevTools automation for unit, include, material, and invalid parser smoke cases.
- Human review confirmed the current runner works as intended for Packet 002, including native file picker and JSON download behavior.

Blocked:

- No disk-level or static-route blocker is present in the latest tree.

Not carried forward:

- Full Vite/React app shell migration.
- Rich result viewer, editor, linter UI, docs page, and deploy hardening.

## Human Acceptance Checklist

Packet 002 is accepted. The accepted contract is:

- Runner is opened from `http://127.0.0.1:4173/runner/`, not `file://`.
- Runtime artifacts load from `/runtime`.
- Materials and libs load from `/runtime-data`, pass byte and sha256 verification, and mount to `/app/materials` and `/app/libs`.
- Worker invokes `callMain(["/work/input.cnes", "--json"])`.
- `stdout` and `stderr` are visible separately.
- Unit, include, material, and invalid-script smoke cases match the native expectations above.
- The UI remains responsive after the invalid-script failure.
- Downloaded JSON matches displayed raw stdout for successful runs.

## Packet 003 Handoff

The next phase should package this accepted proof inside a Vite, React, Tailwind v4, and shadcn/ui app shell. Keep the runtime paths and worker contract stable while moving UI code into React components.
