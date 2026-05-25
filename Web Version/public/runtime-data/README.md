# CoNES Web Runtime Data

This directory contains static runtime data for the browser-based CoNES runner.

## Layout

- `materials/`: copied `.cnesbin` material tables mounted into the WASM filesystem at `/app/materials`.
- `libs/`: copied standard `.cnes` library files mounted into the WASM filesystem at `/app/libs`.
- `materials/manifest.json`: browser URLs and VFS mount paths for every material table.
- `libs/manifest.json`: browser URLs and VFS mount paths for every standard library file.

Each manifest entry includes `bytes` and `sha256`. The worker should verify both after fetching a file and before writing it into the WASM filesystem.

## Runner Contract

Before invoking `callMain(["/work/input.cnes", "--json"])`, the worker must:

1. Create `/app`, `/app/materials`, `/app/libs`, and `/work`.
2. Fetch `/runtime-data/materials/manifest.json`.
3. Fetch every listed material file and write it to its `vfsPath`.
4. Fetch `/runtime-data/libs/manifest.json`.
5. Fetch every listed library file and write it to its `vfsPath`.
6. Write the uploaded script to `/work/input.cnes`.

Do not omit `/app/materials`. If that directory is missing, the current CoNES CLI attempts the Python substance-table build fallback, which is not browser-compatible and can pollute stdout.

The Emscripten module must also be created with a program identity equivalent to:

```js
thisProgram: "/app/cnes"
```

CoNES derives executable-relative `materials` and `libs` paths from `argv[0]`. Setting `thisProgram` keeps those paths aligned with the VFS layout above.

## Regeneration

The current package was copied from:

- `CoNES/materials/*.cnesbin`
- `CoNES/libs/*.cnes`

Regenerate manifests after changing those source assets.
