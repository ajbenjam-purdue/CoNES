/* global createCoNESModule */
"use strict";

const MODULE_SCRIPT_URL = "/runtime/cnes-module.js";
const RUNTIME_BASE_URL = "/runtime/";
const MANIFEST_URLS = [
  "/runtime-data/materials/manifest.json",
  "/runtime-data/libs/manifest.json",
];
const PROGRAM_PATH = "/app/cnes";
const INPUT_PATH = "/work/input.cnes";
const SOLVE_ARGV = [INPUT_PATH, "--json"];
const LINT_ARGV = [INPUT_PATH, "--lint", "--json"];

let moduleFactoryPromise = null;
let runtimePromise = null;
let runtimeRecord = null;
let runQueue = Promise.resolve();
let activeStdoutLines = null;
let activeStderrLines = null;

class WorkerRuntimeError extends Error {
  constructor(code, message, details = undefined) {
    super(message);
    this.name = "WorkerRuntimeError";
    this.code = code;
    this.details = details;
  }
}

self.onmessage = (event) => {
  runQueue = runQueue
    .then(() => handleMessage(event.data))
    .catch((error) => {
      self.postMessage({
        type: "failure",
        requestId: event.data && event.data.requestId ? event.data.requestId : null,
        exitCode: null,
        stdout: "",
        stderr: "",
        error: normalizeError(error),
      });
    });
};

async function handleMessage(message) {
  if (message && message.type === "prewarm") {
    const response = await prewarmRuntime(message);
    self.postMessage(response);
    return;
  }

  if (!message || (message.type !== "run" && message.type !== "lint")) {
    self.postMessage({
      type: "failure",
      requestId: message && message.requestId ? message.requestId : null,
      exitCode: null,
      stdout: "",
      stderr: "",
      error: {
        code: "unsupported-message",
        message: "Worker only supports prewarm, run, and lint requests.",
      },
    });
    return;
  }

  const response = await runSolver(message, message.type);
  self.postMessage(response);
}

async function prewarmRuntime(request) {
  try {
    const runtime = await ensureRuntime(request.requestId);
    return {
      type: "prewarm-ready",
      requestId: request.requestId,
      timings: runtime.prewarmTimings,
    };
  } catch (error) {
    return {
      type: "prewarm-failure",
      requestId: request.requestId,
      timings: runtimeRecord ? runtimeRecord.prewarmTimings : undefined,
      error: normalizeError(error),
    };
  }
}

async function runSolver(request, mode) {
  const startedAt = performance.now();
  const stdoutLines = [];
  const stderrLines = [];
  activeStdoutLines = stdoutLines;
  activeStderrLines = stderrLines;

  try {
    const runtime = await ensureRuntime(request.requestId);
    const module = runtime.module;
    const timings = {
      moduleMs: runtime.ready ? 0 : runtime.prewarmTimings.moduleMs,
      runtimeDataMs: runtime.ready ? 0 : runtime.prewarmTimings.runtimeDataMs,
      runtimeDataFiles: runtime.prewarmTimings.runtimeDataFiles,
    };

    module.FS.writeFile(INPUT_PATH, inputFileContents(request.file));

    postStatus(request.requestId, mode === "lint" ? "linting" : "running");
    const solveStartedAt = performance.now();
    const exitCode = callMain(module, mode === "lint" ? LINT_ARGV : SOLVE_ARGV);
    timings.solveMs = elapsedSince(solveStartedAt);
    timings.wallMs = elapsedSince(startedAt);

    const stdout = stdoutLines.join("\n");
    const stderr = stderrLines.join("\n");
    const parseResult = parseStdoutJson(stdout);
    const response = {
      type: exitCode === 0 ? "success" : "failure",
      requestId: request.requestId,
      exitCode,
      stdout,
      stderr,
      timings,
    };

    if (parseResult.ok) {
      response.parsedJson = parseResult.value;
    } else {
      response.parseError = parseResult.error;
    }

    if (exitCode !== 0) {
      response.error = {
        code: "process-exit-nonzero",
        message: `CoNES exited with code ${exitCode}.`,
      };
    }

    return response;
  } catch (error) {
    return {
      type: "failure",
      requestId: request.requestId,
      exitCode: null,
      stdout: stdoutLines.join("\n"),
      stderr: stderrLines.join("\n"),
      error: normalizeError(error),
      timings: { wallMs: elapsedSince(startedAt) },
    };
  } finally {
    activeStdoutLines = null;
    activeStderrLines = null;
  }
}

async function ensureRuntime(requestId) {
  if (!runtimePromise) {
    runtimePromise = createRuntime(requestId)
      .then((runtime) => {
        runtimeRecord = runtime;
        return runtime;
      })
      .catch((error) => {
        runtimePromise = null;
        throw error;
      });
  } else if (!runtimeRecord) {
    postStatus(requestId, "waiting-runtime");
  }

  const runtime = await runtimePromise;
  runtime.ready = true;
  return runtime;
}

async function createRuntime(requestId) {
  const startedAt = performance.now();
  const prewarmTimings = {};

  postStatus(requestId, "loading-runtime");
  const factoryStartedAt = performance.now();
  const createModule = await loadModuleFactory();
  const module = await createRuntimeModule(createModule);
  prewarmTimings.moduleMs = elapsedSince(factoryStartedAt);

  createRuntimeDirectories(module.FS);

  postStatus(requestId, "loading-assets");
  const runtimeDataStartedAt = performance.now();
  const mountedFiles = await mountRuntimeData(module.FS);
  prewarmTimings.runtimeDataMs = elapsedSince(runtimeDataStartedAt);
  prewarmTimings.runtimeDataFiles = mountedFiles;
  prewarmTimings.wallMs = elapsedSince(startedAt);

  postStatus(requestId, "runtime-ready");
  return {
    module,
    prewarmTimings,
    ready: true,
  };
}

async function loadModuleFactory() {
  if (!moduleFactoryPromise) {
    moduleFactoryPromise = new Promise((resolve, reject) => {
      try {
        importScripts(MODULE_SCRIPT_URL);
        if (typeof createCoNESModule !== "function") {
          reject(
            new WorkerRuntimeError(
              "module-factory-missing",
              "cnes-module.js did not expose createCoNESModule.",
            ),
          );
          return;
        }
        resolve(createCoNESModule);
      } catch (error) {
        reject(
          new WorkerRuntimeError(
            "module-load-failed",
            `Failed to load ${MODULE_SCRIPT_URL}.`,
            { cause: errorMessage(error) },
          ),
        );
      }
    });
  }

  return moduleFactoryPromise;
}

async function createRuntimeModule(createModule) {
  try {
    return await createModule({
      thisProgram: PROGRAM_PATH,
      locateFile(path) {
        return new URL(path, self.location.origin + RUNTIME_BASE_URL).toString();
      },
      print(text) {
        if (activeStdoutLines) {
          activeStdoutLines.push(String(text));
        }
      },
      printErr(text) {
        if (activeStderrLines) {
          activeStderrLines.push(String(text));
        }
      },
    });
  } catch (error) {
    throw new WorkerRuntimeError(
      wasmLikeError(error) ? "wasm-load-failed" : "module-init-failed",
      wasmLikeError(error) ? "Failed to load or instantiate cnes-module.wasm." : "Failed to initialize the CoNES runtime module.",
      { cause: errorMessage(error) },
    );
  }
}

function createRuntimeDirectories(FS) {
  ensureDirectory(FS, "/app");
  ensureDirectory(FS, "/app/materials");
  ensureDirectory(FS, "/app/libs");
  ensureDirectory(FS, "/work");
}

async function mountRuntimeData(FS) {
  let mountedFiles = 0;

  for (const manifestUrl of MANIFEST_URLS) {
    const manifest = await fetchJson(manifestUrl, "manifest-fetch-failed");
    if (!manifest || !Array.isArray(manifest.files)) {
      throw new WorkerRuntimeError(
        "manifest-invalid",
        `Runtime data manifest is invalid: ${manifestUrl}`,
      );
    }

    if (manifest.mountRoot) {
      ensureDirectory(FS, manifest.mountRoot);
    }

    for (const entry of manifest.files) {
      await mountManifestEntry(FS, entry);
      mountedFiles += 1;
    }
  }

  return mountedFiles;
}

async function mountManifestEntry(FS, entry) {
  if (!entry || !entry.url || !entry.vfsPath) {
    throw new WorkerRuntimeError(
      "manifest-entry-invalid",
      "Runtime data manifest contains an entry without url or vfsPath.",
    );
  }

  const bytes = await fetchBytes(entry.url, "runtime-file-fetch-failed");
  if (Number.isFinite(entry.bytes) && bytes.byteLength !== entry.bytes) {
    throw new WorkerRuntimeError(
      "byte-mismatch",
      `${entry.url} expected ${entry.bytes} bytes but fetched ${bytes.byteLength}.`,
    );
  }

  if (entry.sha256) {
    const actualHash = await sha256Hex(bytes);
    if (actualHash !== entry.sha256) {
      throw new WorkerRuntimeError(
        "sha256-mismatch",
        `${entry.url} expected sha256 ${entry.sha256} but fetched ${actualHash}.`,
      );
    }
  }

  ensureDirectory(FS, parentPath(entry.vfsPath));
  FS.writeFile(entry.vfsPath, bytes);
}

async function fetchJson(url, code) {
  const response = await fetch(url, { cache: "no-store" });
  if (!response.ok) {
    throw new WorkerRuntimeError(
      code,
      `Failed to fetch ${url}: ${response.status} ${response.statusText}`,
    );
  }

  try {
    return await response.json();
  } catch (error) {
    throw new WorkerRuntimeError(
      "manifest-invalid",
      `Runtime data manifest is not valid JSON: ${url}`,
      { cause: errorMessage(error) },
    );
  }
}

async function fetchBytes(url, code) {
  const response = await fetch(url, { cache: "no-store" });
  if (!response.ok) {
    if (response.status === 404) {
      throw new WorkerRuntimeError(
        "missing-file",
        `Failed to fetch ${url}: ${response.status} ${response.statusText}`,
      );
    }
    throw new WorkerRuntimeError(
      code,
      `Failed to fetch ${url}: ${response.status} ${response.statusText}`,
    );
  }
  return new Uint8Array(await response.arrayBuffer());
}

async function sha256Hex(bytes) {
  const digest = await crypto.subtle.digest("SHA-256", bytes);
  return Array.from(new Uint8Array(digest))
    .map((byte) => byte.toString(16).padStart(2, "0"))
    .join("");
}

function inputFileContents(file) {
  if (!file) {
    throw new WorkerRuntimeError("missing-file", "Run request did not include a file.");
  }

  if (file.bytes instanceof ArrayBuffer) {
    return new Uint8Array(file.bytes);
  }

  if (ArrayBuffer.isView(file.bytes)) {
    return new Uint8Array(file.bytes.buffer, file.bytes.byteOffset, file.bytes.byteLength);
  }

  if (typeof file.text === "string") {
    return file.text;
  }

  throw new WorkerRuntimeError(
    "invalid-file",
    "Run request file must include bytes as an ArrayBuffer or text as a string.",
  );
}

function callMain(module, argv) {
  try {
    const result = module.callMain(argv);
    return Number.isInteger(result) ? result : 0;
  } catch (error) {
    if (Number.isInteger(error)) {
      return error;
    }

    if (error && Number.isInteger(error.status)) {
      return error.status;
    }

    throw error;
  }
}

function parseStdoutJson(stdout) {
  const trimmed = stdout.trim();
  if (!trimmed) {
    return {
      ok: false,
      error: {
        code: "json-parse-failed",
        message: "CoNES stdout was empty.",
      },
    };
  }

  try {
    return { ok: true, value: JSON.parse(trimmed) };
  } catch (error) {
    return {
      ok: false,
      error: {
        code: "json-parse-failed",
        message: error instanceof Error ? error.message : String(error),
      },
    };
  }
}

function ensureDirectory(FS, path) {
  if (!path || path === "/") {
    return;
  }

  let current = "";
  for (const part of path.split("/").filter(Boolean)) {
    current += `/${part}`;
    if (!FS.analyzePath(current).exists) {
      FS.mkdir(current);
    }
  }
}

function parentPath(path) {
  const index = path.lastIndexOf("/");
  return index <= 0 ? "/" : path.slice(0, index);
}

function elapsedSince(startedAt) {
  return Math.round((performance.now() - startedAt) * 100) / 100;
}

function normalizeError(error) {
  if (error instanceof WorkerRuntimeError) {
    const normalized = {
      code: error.code,
      message: error.message,
    };
    if (error.details) {
      normalized.details = error.details;
    }
    return normalized;
  }

  return {
    code: "runtime-error",
    message: error instanceof Error ? error.message : String(error),
  };
}

function postStatus(requestId, phase) {
  self.postMessage({
    type: "status",
    requestId,
    phase,
  });
}

function wasmLikeError(error) {
  return /wasm|webassembly|instantiate|compile|fetch/i.test(errorMessage(error));
}

function errorMessage(error) {
  return error instanceof Error ? error.message : String(error);
}
