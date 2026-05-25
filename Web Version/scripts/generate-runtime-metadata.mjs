#!/usr/bin/env node
import { createHash } from "node:crypto";
import { readFile, writeFile } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const webRoot = path.resolve(scriptDir, "..");
const runtimeDir = path.join(webRoot, "public/runtime");
const packageJsonPath = path.join(webRoot, "package.json");
const outputPath = path.join(runtimeDir, "runtime-metadata.json");

const buildCommand = [
  "env EM_CACHE=${EM_CACHE:-/private/tmp/emscripten-cache} em++ -O2 -std=c++20",
  "-fexceptions",
  "-I . -I ${EIGEN_INCLUDE:-/opt/homebrew/include/eigen3}",
  "src/main.cpp",
  "-o Web Version/public/runtime/cnes-module.js",
  "-sMODULARIZE=1",
  "-sEXPORT_NAME=createCoNESModule",
  "-sENVIRONMENT=worker,node",
  "-sINVOKE_RUN=0",
  "-sEXPORTED_RUNTIME_METHODS=FS,callMain",
  "-sALLOW_MEMORY_GROWTH=1",
].join(" \\\n  ");

async function readBytes(filePath) {
  return readFile(filePath);
}

function sha256(bytes) {
  return createHash("sha256").update(bytes).digest("hex");
}

async function fileSummary(publicPath) {
  const absolutePath = path.join(webRoot, "public", publicPath.replace(/^\//, ""));
  const bytes = await readBytes(absolutePath);

  return {
    path: publicPath,
    bytes: bytes.byteLength,
    sha256: sha256(bytes),
  };
}

async function manifestSummary(label, publicPath) {
  const absolutePath = path.join(webRoot, "public", publicPath.replace(/^\//, ""));
  const manifestBytes = await readBytes(absolutePath);
  const manifest = JSON.parse(manifestBytes.toString("utf8"));
  const files = Array.isArray(manifest.files) ? manifest.files : [];
  const filesWithSha256 = files.filter((entry) => typeof entry.sha256 === "string" && entry.sha256.length > 0).length;

  const manifestSha256 = sha256(manifestBytes);

  return {
    label,
    path: publicPath,
    bytes: manifestBytes.byteLength,
    sha256: manifestSha256,
    kind: manifest.kind ?? null,
    mountRoot: manifest.mountRoot ?? null,
    integrity: manifest.integrity ?? null,
    count: Number.isInteger(manifest.count) ? manifest.count : files.length,
    totalBytes: Number.isInteger(manifest.totalBytes)
      ? manifest.totalBytes
      : files.reduce((sum, entry) => sum + (Number.isInteger(entry.bytes) ? entry.bytes : 0), 0),
    manifestSha256,
    perFileSha256: {
      exists: filesWithSha256 > 0,
      complete: filesWithSha256 === files.length,
      filesWithSha256,
      fileCount: files.length,
    },
  };
}

async function main() {
  const packageJson = JSON.parse(await readFile(packageJsonPath, "utf8"));
  const worker = await fileSummary("/runtime/cnes-worker.js");
  const moduleJs = await fileSummary("/runtime/cnes-module.js");
  const moduleWasm = await fileSummary("/runtime/cnes-module.wasm");
  const materials = await manifestSummary("materials", "/runtime-data/materials/manifest.json");
  const libs = await manifestSummary("libs", "/runtime-data/libs/manifest.json");

  const metadata = {
    schemaVersion: 1,
    project: {
      name: packageJson.name,
      version: packageJson.version,
    },
    build: {
      command: buildCommand,
      emscriptenVersion: null,
      note: "Emscripten version is environment-local; regenerate after changing the WASM build toolchain or artifacts.",
    },
    artifacts: [
      { label: "cnes-worker.js", ...worker },
      { label: "cnes-module.js", ...moduleJs },
      { label: "cnes-module.wasm", ...moduleWasm },
    ],
    artifactSummary: {
      worker,
      moduleJs,
      moduleWasm,
    },
    manifests: [materials, libs],
    runtimeData: {
      materials,
      libs,
    },
    runtime: {
      thisProgram: "/app/cnes",
      callMainArgs: ["/work/input.cnes", "--json"],
    },
    runtimeContract: {
      workerPath: "/runtime/cnes-worker.js",
      moduleScriptUrl: "/runtime/cnes-module.js",
      wasmPath: "/runtime/cnes-module.wasm",
      runtimeDataManifests: ["/runtime-data/materials/manifest.json", "/runtime-data/libs/manifest.json"],
      thisProgram: "/app/cnes",
      inputPath: "/work/input.cnes",
      callMainArgs: ["/work/input.cnes", "--json"],
      outputContract: "Raw stdout and stderr remain the source of truth; stdout is expected to contain CoNES --json output on successful runs.",
    },
  };

  const serialized = `${JSON.stringify(metadata, null, 2)}\n`;
  await writeFile(outputPath, serialized, "utf8");
  console.log(`Wrote ${path.relative(process.cwd(), outputPath)}`);
}

main().catch((error) => {
  console.error(`FAIL runtime metadata generation: ${error.message}`);
  process.exitCode = 1;
});
