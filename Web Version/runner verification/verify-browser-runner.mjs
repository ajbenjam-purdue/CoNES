import crypto from "node:crypto";
import fs from "node:fs/promises";
import net from "node:net";

const DEVTOOLS_BASE_URL = process.env.CHROME_DEVTOOLS_URL || "http://127.0.0.1:9223";
const RUNNER_URL = process.env.CONES_RUNNER_URL || "http://127.0.0.1:4173/runner/";
const REPO_ROOT = new URL("../../", import.meta.url);
const SHOW_DEBUG_SURFACES = false;
const APP_TITLE = "CoNES Studio";

const cases = [
  {
    label: "examples/test.cnes",
    fileName: "test.cnes",
    path: new URL("examples/test.cnes", REPO_ROOT),
    expect: ({ parsed }) => parsed?.success === true && findVariable(parsed, "T_surface")?.value === 40,
  },
  {
    label: "tests/test_units.cnes",
    fileName: "test_units.cnes",
    path: new URL("tests/test_units.cnes", REPO_ROOT),
    expect: ({ parsed }) => {
      const power = findVariable(parsed, "Power");
      return parsed?.success === true && power?.unit === "W" && power?.value === 50;
    },
  },
  {
    label: "examples/HVAC_example.cnes",
    fileName: "HVAC_example.cnes",
    path: new URL("examples/vapor_compression_cycle.cnes", REPO_ROOT),
    expect: ({ parsed }) => {
      const cop = findVariable(parsed, "COP_HP");
      return parsed?.success === true && Math.abs(cop?.value - 11.7950669027) < 0.0000001;
    },
  },
  {
    label: "invalid parser smoke",
    fileName: "invalid.cnes",
    contents: "x :=\n",
    expect: ({ status, parsed }) => status === "Failure"
      && parsed?.success === false
      && /parser|expect expression/i.test(parsed?.error || ""),
  },
];

const target = await createTarget("about:blank");
const cdp = createCdpClient(target.webSocketDebuggerUrl);
await cdp.ready;
await cdp.send("Page.enable");
await cdp.send("Runtime.enable");

try {
  await cdp.send("Page.navigate", { url: RUNNER_URL });
  await cdp.waitEvent("Page.loadEventFired", 15_000).catch(() => null);
  await new Promise((resolve) => setTimeout(resolve, 750));
  const initial = await evaluate(cdp, `(async () => {
    const startedAt = performance.now();
    while (performance.now() - startedAt < 60000) {
      const snapshot = {
        title: document.title,
        body: document.body.innerText,
        runtimeState: document.querySelector("[data-runtime-prewarm-state]")?.getAttribute("data-runtime-prewarm-state") || "",
        hasWorkbench: Boolean(document.querySelector("[data-workbench-page]")),
        hasMonaco: Boolean(document.querySelector("[data-workbench-monaco-editor] .monaco-editor")),
        hasRuntimeGate: Boolean(document.querySelector("[data-workbench-runtime-gate]")),
        hasActions: Boolean(document.querySelector("[data-workbench-actions]")),
        hasLintButton: Boolean(document.querySelector("[data-workbench-lint-button]")),
        hasRunButton: Boolean(document.querySelector("[data-workbench-run-button]"))
      };

      if (
        snapshot.title === ${JSON.stringify(APP_TITLE)}
        && snapshot.runtimeState === "ready"
        && snapshot.hasWorkbench
        && snapshot.hasMonaco
        && snapshot.hasActions
        && snapshot.hasLintButton
        && snapshot.hasRunButton
      ) {
        return snapshot;
      }

      await new Promise((resolve) => setTimeout(resolve, 100));
    }

    return {
      title: document.title,
      body: document.body.innerText,
      runtimeState: document.querySelector("[data-runtime-prewarm-state]")?.getAttribute("data-runtime-prewarm-state") || "",
      hasWorkbench: Boolean(document.querySelector("[data-workbench-page]")),
      hasMonaco: Boolean(document.querySelector("[data-workbench-monaco-editor] .monaco-editor")),
      hasRuntimeGate: Boolean(document.querySelector("[data-workbench-runtime-gate]")),
      hasActions: Boolean(document.querySelector("[data-workbench-actions]")),
      hasLintButton: Boolean(document.querySelector("[data-workbench-lint-button]")),
      hasRunButton: Boolean(document.querySelector("[data-workbench-run-button]"))
    };
  })()`);

  if (
    initial.title !== APP_TITLE
    || initial.runtimeState !== "ready"
    || !initial.hasWorkbench
    || !initial.hasMonaco
    || !initial.hasActions
    || !initial.hasLintButton
    || !initial.hasRunButton
  ) {
    throw new Error(`Workbench shell did not initialize: ${JSON.stringify(initial)}`);
  }

  const prewarmStatus = await verifyRuntimePrewarm(cdp);
  const renameStatus = await verifyActiveBufferRename(cdp);
  const results = [];
  for (const testCase of cases) {
    const contents = testCase.contents ?? await fs.readFile(testCase.path, "utf8");
    const result = await runCase(cdp, testCase.fileName, contents);
    const parsed = parseJson(result.stdout);
    const passed = testCase.expect({ ...result, parsed });

    results.push({
      label: testCase.label,
      status: result.status,
      exit: result.exit,
      json: result.json,
      resultsState: result.resultsState,
      resultVariableRows: result.resultVariableRows,
      resultErrorSummary: result.resultErrorSummary,
      stdoutBytes: Buffer.byteLength(result.stdout),
      stderrBytes: Buffer.byteLength(result.stderr),
      rawOutputOpen: result.rawOutputOpen,
      diagnosticsActive: result.diagnosticsActive,
      passed: passed && verifyResultUi(result, parsed) && verifyRawOutputUi(result),
    });

    if (!passed || !verifyResultUi(result, parsed) || !verifyRawOutputUi(result)) {
      throw new Error(`Browser workbench case failed: ${testCase.label}\n${JSON.stringify(result, null, 2)}`);
    }
  }

  const negativeResults = await runWorkerNegativeChecks(cdp);
  const lintStatus = await verifyWorkbenchLint(cdp);
  const workbenchStatus = await verifyWorkbenchRun(cdp);
  const toolsStatus = await verifyToolsStatus(cdp);

  console.log(JSON.stringify({ runnerUrl: RUNNER_URL, prewarmStatus, renameStatus, results, negativeResults, lintStatus, workbenchStatus, toolsStatus }, null, 2));
} finally {
  await cdp.send("Page.close").catch(() => {});
  cdp.close();
}

async function verifyWorkbenchLint(cdp) {
  const validSource = `include "heat_transfer_lib"

h := 100 [W/m^2*K]
h.unit := [W/m^2*K]
A_c := 0.5
A_c.unit := [m^2]
T_fluid := 20 [C]
Q := 1 [kW]
NewtonCooling(h, A_c, T_surface, T_fluid, Q)
T_surface.unit := [C]
`;

  const parserInvalidSource = `h := 100 [W/m^2*K]
broken :=
`;
  const missingIncludeSource = `include "missing_library_for_lint"

x := 1
`;

  const expression = `async () => {
    const readJsonDownload = async () => {
      const link = document.querySelector("[data-workbench-download-json]");
      const fallback = document.querySelector("[data-stdout-output]")?.textContent || "";
      const href = link?.href || "";
      if (!href || link.getAttribute("aria-disabled") === "true") {
        return fallback;
      }
      try {
        const response = await fetch(href);
        return await response.text();
      } catch {
        return fallback;
      }
    };

    const workbenchButton = document.querySelector('[data-route-trigger="workbench"]');
    workbenchButton?.click();
    const startedRouteAt = performance.now();
    while (performance.now() - startedRouteAt < 10000) {
      if (
        document.querySelector("[data-workbench-page]")
        && document.querySelector("[data-workbench-source-test-input]")
        && document.querySelector("[data-workbench-lint-button]")
        && document.querySelector("[data-workbench-run-button]")
      ) {
        break;
      }
      await new Promise((resolve) => setTimeout(resolve, 100));
    }

    const sourceInput = document.querySelector("[data-workbench-source-test-input]");
    const lintButton = document.querySelector("[data-workbench-lint-button]");
    const runButton = document.querySelector("[data-workbench-run-button]");
    if (!sourceInput || !lintButton || !runButton) {
      throw new Error("Workbench lint controls were not available.");
    }

    const valueSetter = Object.getOwnPropertyDescriptor(window.HTMLTextAreaElement.prototype, "value").set;
    const setSource = async (contents) => {
      valueSetter.call(sourceInput, contents);
      sourceInput.dispatchEvent(new Event("input", { bubbles: true }));
      sourceInput.dispatchEvent(new Event("change", { bubbles: true }));
      await new Promise((resolve) => setTimeout(resolve, 150));
    };

    const waitForLint = async (terminalStates, timeoutMs = 45000) => {
      const startedAt = performance.now();
      while (performance.now() - startedAt < timeoutMs) {
        const status = document.querySelector("[data-workbench-lint-status]")?.textContent?.trim();
        if (terminalStates.some((state) => state instanceof RegExp ? state.test(status || "") : state === status)) {
          return await lintSnapshot(status);
        }
        await new Promise((resolve) => setTimeout(resolve, 100));
      }
      throw new Error("Timed out waiting for lint status.");
    };

    const lintSnapshot = async (status) => ({
      status,
      mode: document.querySelector("[data-workbench-lint-mode]")?.textContent?.trim() || "",
      count: Number(document.querySelector("[data-workbench-diagnostics-panel]")?.getAttribute("data-workbench-diagnostics-count") || "0"),
      diagnostics: Array.from(document.querySelectorAll("[data-workbench-diagnostic]")).map((row) => ({
        line: row.getAttribute("data-diagnostic-line") || "",
        text: row.textContent || "",
      })),
      raw: document.querySelector("[data-workbench-lint-raw-output]")?.textContent || "",
      markerCount: document.querySelectorAll("[data-workbench-monaco-editor] .squiggly-error").length,
      runStatus: document.querySelector("[data-workbench-run-status]")?.textContent?.trim() || "",
      stdout: await readJsonDownload(),
    });

    await setSource(${JSON.stringify(validSource)});
    lintButton.click();
    const valid = await waitForLint(["Clean"]);

    await setSource(${JSON.stringify(parserInvalidSource)});
    lintButton.click();
    const parserInvalid = await waitForLint([/^\\d+ errors?$/]);

    await setSource(${JSON.stringify(missingIncludeSource)});
    lintButton.click();
    const missingInclude = await waitForLint([/^\\d+ errors?$/]);

    await setSource(${JSON.stringify(parserInvalidSource)});
    await new Promise((resolve) => setTimeout(resolve, 100));
    await setSource(${JSON.stringify(validSource)});
    const staleAuto = await waitForLint(["Clean"], 50000);

    runButton.click();
    const runStartedAt = performance.now();
    while (performance.now() - runStartedAt < 45000) {
      const runStatus = document.querySelector("[data-workbench-run-status]")?.textContent?.trim();
      const stdout = await readJsonDownload();
      if (runStatus === "Success" && stdout) {
        return { valid, parserInvalid, missingInclude, staleAuto, runAfterLint: { runStatus, stdout } };
      }
      await new Promise((resolve) => setTimeout(resolve, 100));
    }

    throw new Error("Timed out waiting for run after lint.");
  }`;

  const result = await evaluate(cdp, `(${expression})()`, 180_000);
  const parsedRun = parseJson(result.runAfterLint.stdout);
  const parserHasLine = result.parserInvalid.diagnostics.some((diagnostic) => diagnostic.line);
  const parserText = result.parserInvalid.raw + result.parserInvalid.diagnostics.map((diagnostic) => diagnostic.text).join("\\n");
  const missingIncludeText = result.missingInclude.raw + result.missingInclude.diagnostics.map((diagnostic) => diagnostic.text).join("\\n");
  const parserHasDiagnostic = /\[Line\\s+\\d+\]|parser|expect/i.test(parserText);
  const missingIncludeHasDiagnostic = /missing_library_for_lint|include|file|library/i.test(missingIncludeText);

  const passed = result.valid.status === "Clean"
    && result.valid.count === 0
    && result.parserInvalid.count > 0
    && parserHasLine
    && parserHasDiagnostic
    && result.missingInclude.count > 0
    && missingIncludeHasDiagnostic
    && result.staleAuto.status === "Clean"
    && result.staleAuto.count === 0
    && result.runAfterLint.runStatus === "Success"
    && parsedRun?.success === true;

  if (!passed) {
    throw new Error(`Workbench lint smoke failed:\n${JSON.stringify(result, null, 2)}`);
  }

  return {
    valid: { status: result.valid.status, count: result.valid.count },
    parserInvalid: { status: result.parserInvalid.status, count: result.parserInvalid.count, hasLineMarker: parserHasLine },
    missingInclude: { status: result.missingInclude.status, count: result.missingInclude.count },
    staleAuto: { status: result.staleAuto.status, count: result.staleAuto.count },
    runAfterLint: { status: result.runAfterLint.runStatus, success: parsedRun?.success === true },
  };
}

async function verifyActiveBufferRename(cdp) {
  const expression = `async () => {
    const sourceNameButton = document.querySelector("[data-workbench-source-name]");
    if (!sourceNameButton) {
      throw new Error("Could not find active buffer name.");
    }

    sourceNameButton.click();
    const startedAt = performance.now();
    while (performance.now() - startedAt < 5000) {
      const input = document.querySelector("[data-workbench-rename-input]");
      if (input) {
        const valueSetter = Object.getOwnPropertyDescriptor(window.HTMLInputElement.prototype, "value").set;
        valueSetter.call(input, "coil-study");
        input.dispatchEvent(new Event("input", { bubbles: true }));
        input.dispatchEvent(new Event("change", { bubbles: true }));
        input.dispatchEvent(new KeyboardEvent("keydown", { key: "Enter", bubbles: true }));
        break;
      }
      await new Promise((resolve) => setTimeout(resolve, 100));
    }

    const renamedAt = performance.now();
    while (performance.now() - renamedAt < 5000) {
      const sourceName = document.querySelector("[data-workbench-source-name]")?.textContent?.trim() || "";
      const sourceDownloadName = document.querySelector("[data-workbench-download-source]")?.getAttribute("download") || "";
      if (sourceName.includes("coil-study.cnes") && sourceDownloadName === "coil-study.cnes") {
        document.querySelector("[data-workbench-new-button]")?.click();
        return { status: "renamed", sourceName, sourceDownloadName };
      }
      await new Promise((resolve) => setTimeout(resolve, 100));
    }

    return {
      status: "failed",
      sourceName: document.querySelector("[data-workbench-source-name]")?.textContent?.trim() || "",
      sourceDownloadName: document.querySelector("[data-workbench-download-source]")?.getAttribute("download") || "",
    };
  }`;

  const result = await evaluate(cdp, `(${expression})()`, 15_000);
  if (result.status !== "renamed") {
    throw new Error(`Active buffer rename failed: ${JSON.stringify(result)}`);
  }
  return result;
}

async function verifyRuntimePrewarm(cdp) {
  const expression = `async () => {
    const startedAt = performance.now();
    while (performance.now() - startedAt < 60000) {
      const state = document.querySelector("[data-runtime-prewarm-state]")?.getAttribute("data-runtime-prewarm-state");
      const body = document.body.innerText;
      if (state === "ready" && body.includes("Runtime ready")) {
        return {
          status: "ready",
          bodyHasReadyLabel: true,
        };
      }
      if (state === "failed") {
        return {
          status: "failed",
          body,
        };
      }
      await new Promise((resolve) => setTimeout(resolve, 150));
    }

    return {
      status: document.querySelector("[data-runtime-prewarm-state]")?.getAttribute("data-runtime-prewarm-state") || "missing",
      body: document.body.innerText,
    };
  }`;

  const result = await evaluate(cdp, `(${expression})()`, 65_000);
  if (result.status !== "ready") {
    throw new Error(`Runtime prewarm did not become ready before run: ${JSON.stringify(result)}`);
  }
  return result;
}

async function runWorkerNegativeChecks(cdp) {
  const expression = `async () => {
    async function probe(message, timeoutMs = 45000) {
      return new Promise((resolve, reject) => {
        const worker = new Worker("/runtime/cnes-worker.js");
        const timer = setTimeout(() => {
          worker.terminate();
          reject(new Error("Timed out waiting for negative worker probe."));
        }, timeoutMs);

        worker.addEventListener("message", (event) => {
          if (event.data?.type === "status") {
            return;
          }
          clearTimeout(timer);
          worker.terminate();
          resolve(event.data);
        });

        worker.addEventListener("error", (event) => {
          clearTimeout(timer);
          worker.terminate();
          reject(new Error(event.message || "Worker negative probe errored."));
        });

        worker.postMessage(message);
      });
    }

    const unsupported = await probe({ type: "inspect", requestId: "negative-unsupported" });
    return [
      {
        label: "unsupported message",
        passed: unsupported.type === "failure" && unsupported.error?.code === "unsupported-message",
        code: unsupported.error?.code,
      }
    ];
  }`;

  const results = await evaluate(cdp, `(${expression})()`, 55_000);
  const failed = results.find((result) => !result.passed);
  if (failed) {
    throw new Error(`Negative worker runtime check failed: ${JSON.stringify(failed)}`);
  }
  return results;
}

async function verifyToolsStatus(cdp) {
  if (!SHOW_DEBUG_SURFACES) {
    return { status: "hidden", reason: "Debug-only Tools route is hidden in product mode." };
  }

  const expression = `async () => {
    const toolsButton = document.querySelector('[data-route-trigger="tools"]')
      || Array.from(document.querySelectorAll("button")).find((button) => button.textContent?.trim() === "Tools");
    if (!toolsButton) {
      throw new Error("Could not find Tools route button.");
    }

    toolsButton.click();
    const startedAt = performance.now();
    while (performance.now() - startedAt < 10000) {
      const page = document.querySelector("[data-runtime-status-page]");
      const ready = document.querySelector("[data-runtime-metadata-state='ready']");
      const runtimeReady = document.querySelector("[data-runtime-ready-state='ready']");
      const body = document.body.innerText;
      if (
        page
        && ready
        && runtimeReady
        && body.includes("/app/cnes")
        && body.includes("/runtime/cnes-module.wasm")
        && body.includes('["/work/input.cnes","--json"]')
      ) {
        return {
          status: "ready",
          hasRuntimeStatusPage: true,
          hasMetadataReady: true,
          hasRuntimeReady: true,
        };
      }
      await new Promise((resolve) => setTimeout(resolve, 100));
    }

    return {
      status: "not-ready",
      hasRuntimeStatusPage: Boolean(document.querySelector("[data-runtime-status-page]")),
      hasMetadataReady: Boolean(document.querySelector("[data-runtime-metadata-state='ready']")),
      hasRuntimeReady: Boolean(document.querySelector("[data-runtime-ready-state='ready']")),
      body: document.body.innerText,
    };
  }`;

  const result = await evaluate(cdp, `(${expression})()`, 15_000);
  if (result.status !== "ready") {
    throw new Error(`Tools runtime status check failed: ${JSON.stringify(result)}`);
  }
  return result;
}

async function verifyWorkbenchRun(cdp) {
  const source = `// Workbench smoke source
include "heat_transfer_lib"

h := 100 [W/m^2*K]
h.unit := [W/m^2*K]
A_c := 0.5
A_c.unit := [m^2]
T_fluid := 20 [C]
Q := 2 [kW]
NewtonCooling(h, A_c, T_surface, T_fluid, Q)
T_surface.unit := [C]
`;

  const expression = `async () => {
    const readJsonDownload = async () => {
      const link = document.querySelector("[data-workbench-download-json]");
      const fallback = document.querySelector("[data-stdout-output]")?.textContent || "";
      const href = link?.href || "";
      if (!href || link.getAttribute("aria-disabled") === "true") {
        return fallback;
      }
      try {
        const response = await fetch(href);
        return await response.text();
      } catch {
        return fallback;
      }
    };

    const routeButton = document.querySelector('[data-route-trigger="workbench"]');
    routeButton?.click();
    const startedAt = performance.now();
    while (performance.now() - startedAt < 15000) {
      const page = document.querySelector("[data-workbench-page]");
      const editorHost = document.querySelector("[data-workbench-monaco-editor] .monaco-editor");
      const testInput = document.querySelector("[data-workbench-source-test-input]");
      if (page && editorHost && testInput) {
        break;
      }
      await new Promise((resolve) => setTimeout(resolve, 100));
    }

    const page = document.querySelector("[data-workbench-page]");
    const editorHost = document.querySelector("[data-workbench-monaco-editor] .monaco-editor");
    const testInput = document.querySelector("[data-workbench-source-test-input]");
    const runButton = document.querySelector("[data-workbench-run-button]");
    const resetButton = document.querySelector("[data-workbench-reset-output]");
    if (!page || !editorHost || !testInput || !runButton || !resetButton) {
      return {
        status: "not-ready",
        hasPage: Boolean(page),
        hasMonaco: Boolean(editorHost),
        hasTestInput: Boolean(testInput),
        hasRunButton: Boolean(runButton),
        hasResetButton: Boolean(resetButton),
        body: document.body.innerText,
      };
    }

    resetButton.click();
    await new Promise((resolve) => requestAnimationFrame(resolve));

    const valueSetter = Object.getOwnPropertyDescriptor(window.HTMLTextAreaElement.prototype, "value").set;
    valueSetter.call(testInput, ${JSON.stringify(source)});
    testInput.dispatchEvent(new Event("input", { bubbles: true }));
    testInput.dispatchEvent(new Event("change", { bubbles: true }));
    await new Promise((resolve) => setTimeout(resolve, 250));
    runButton.click();

    const terminalStates = new Set(["Success", "Failure"]);
    const runStartedAt = performance.now();
    while (performance.now() - runStartedAt < 45000) {
      const status = document.querySelector("[data-workbench-run-status]")?.textContent?.trim();
      const stdout = await readJsonDownload();
      if (terminalStates.has(status) && stdout) {
        return {
          status,
          hasMonaco: true,
          sourceName: document.querySelector("[data-workbench-source-name]")?.textContent?.trim(),
          worker: document.querySelector("[data-workbench-worker-state]")?.textContent?.trim(),
          json: document.querySelector("[data-workbench-json-state]")?.textContent?.trim(),
          exit: document.querySelector("[data-workbench-exit-code]")?.textContent?.trim(),
          stdout,
          stderr: document.querySelector("[data-stderr-output]")?.textContent || "",
          rawOutputOpen: Boolean(document.querySelector("[data-raw-output-section]")?.open),
          diagnosticsActive: document.querySelector("[data-raw-output-section]")?.getAttribute("data-diagnostics-active") || "",
          rawOutputText: document.querySelector("[data-raw-output-section] summary")?.textContent || "",
          resultsState: document.querySelector("[data-results-viewer]")?.getAttribute("data-results-state") || "",
          resultVariableRows: Array.from(document.querySelectorAll("[data-result-variable-row]")).map((row) => ({
            name: row.getAttribute("data-variable-name") || "",
            text: row.textContent || "",
          })),
        };
      }
      await new Promise((resolve) => setTimeout(resolve, 100));
    }

    throw new Error("Timed out waiting for workbench result.");
  }`;

  const result = await evaluate(cdp, `(${expression})()`, 65_000);
  const parsed = parseJson(result.stdout);
  const surface = findVariable(parsed, "T_surface");
  const passed = result.status === "Success"
    && result.hasMonaco === true
    && result.resultsState === "success"
    && parsed?.success === true
    && Math.abs(surface?.value - 60) < 0.0000001;

  if (!passed) {
    throw new Error(`Workbench edit-run-results smoke failed:\n${JSON.stringify(result, null, 2)}`);
  }

  return {
    status: result.status,
    hasMonaco: result.hasMonaco,
    sourceName: result.sourceName,
    worker: result.worker,
    json: result.json,
    exit: result.exit,
    tSurface: surface?.value,
    resultsState: result.resultsState,
  };
}

async function createTarget(url) {
  const response = await fetch(`${DEVTOOLS_BASE_URL}/json/new?${encodeURIComponent(url)}`, { method: "PUT" });
  if (!response.ok) {
    throw new Error(`Could not create Chrome target: ${response.status} ${await response.text()}`);
  }
  return response.json();
}

async function runCase(cdp, fileName, contents) {
  const expression = `async () => {
    const readJsonDownload = async () => {
      const link = document.querySelector("[data-workbench-download-json]");
      const fallback = document.querySelector("[data-stdout-output]")?.textContent || "";
      const href = link?.href || "";
      if (!href || link.getAttribute("aria-disabled") === "true") {
        return fallback;
      }
      try {
        const response = await fetch(href);
        return await response.text();
      } catch {
        return fallback;
      }
    };

    const workbenchButton = document.querySelector('[data-route-trigger="workbench"]');
    workbenchButton?.click();
    const startedRouteAt = performance.now();
    while (performance.now() - startedRouteAt < 10000) {
      if (
        document.querySelector("[data-workbench-page]")
        && document.querySelector("[data-workbench-source-test-input]")
        && document.querySelector("[data-workbench-run-button]")
      ) {
        break;
      }
      await new Promise((resolve) => setTimeout(resolve, 100));
    }

    const sourceInput = document.querySelector("[data-workbench-source-test-input]");
    const runButton = document.querySelector("[data-workbench-run-button]");
    const resetButton = document.querySelector("[data-workbench-reset-output]");
    if (!sourceInput || !runButton || !resetButton) {
      throw new Error("Workbench controls were not available.");
    }

    resetButton.click();
    await new Promise((resolve) => requestAnimationFrame(resolve));

    const valueSetter = Object.getOwnPropertyDescriptor(window.HTMLTextAreaElement.prototype, "value").set;
    valueSetter.call(sourceInput, ${JSON.stringify(contents)});
    sourceInput.dispatchEvent(new Event("input", { bubbles: true }));
    sourceInput.dispatchEvent(new Event("change", { bubbles: true }));
    await new Promise((resolve) => setTimeout(resolve, 150));
    runButton.click();

    const terminalStates = new Set(["Success", "Failure"]);
    const startedAt = performance.now();
    while (performance.now() - startedAt < 45000) {
      const status = document.querySelector("[data-status-label]")?.textContent?.trim();
      const workbenchStatus = document.querySelector("[data-workbench-run-status]")?.textContent?.trim();
      if (terminalStates.has(workbenchStatus || status)) {
        return {
          status: workbenchStatus || status,
          worker: document.querySelector("[data-workbench-worker-state]")?.textContent?.trim() || document.querySelector("[data-worker-state]")?.textContent?.trim(),
          json: document.querySelector("[data-workbench-json-state]")?.textContent?.trim() || document.querySelector("[data-json-state]")?.textContent?.trim(),
          exit: document.querySelector("[data-workbench-exit-code]")?.textContent?.trim() || document.querySelector("[data-exit-code]")?.textContent?.trim(),
          time: document.querySelector("[data-wall-time]")?.textContent?.trim(),
          stdout: await readJsonDownload(),
          stderr: document.querySelector("[data-stderr-output]")?.textContent || "",
          rawOutputOpen: Boolean(document.querySelector("[data-raw-output-section]")?.open),
          diagnosticsActive: document.querySelector("[data-raw-output-section]")?.getAttribute("data-diagnostics-active") || "",
          rawOutputText: document.querySelector("[data-raw-output-section] summary")?.textContent || "",
          downloadEnabled: document.querySelector("[data-workbench-download-json]")?.getAttribute("aria-disabled") !== "true",
          resultsState: document.querySelector("[data-results-viewer]")?.getAttribute("data-results-state") || "",
          resultVariableRows: Array.from(document.querySelectorAll("[data-result-variable-row]")).map((row) => ({
            name: row.getAttribute("data-variable-name") || "",
            text: row.textContent || "",
          })),
          resultErrorSummary: document.querySelector("[data-results-error-summary]")?.textContent || "",
        };
      }
      await new Promise((resolve) => setTimeout(resolve, 100));
    }

    throw new Error("Timed out waiting for workbench result.");
  }`;

  return evaluate(cdp, `(${expression})()`, 50_000);
}

async function evaluate(cdp, expression, timeoutMs = 15_000) {
  let lastError = null;

  for (let attempt = 0; attempt < 3; attempt += 1) {
    try {
      const result = await cdp.send("Runtime.evaluate", {
        expression,
        awaitPromise: true,
        returnByValue: true,
        timeout: timeoutMs,
      });

      if (result.exceptionDetails) {
        throw new Error(JSON.stringify(result.exceptionDetails, null, 2));
      }
      return result.result.value;
    } catch (error) {
      lastError = error;
      if (!isNavigationInterruption(error) || attempt === 2) {
        throw error;
      }
      await new Promise((resolve) => setTimeout(resolve, 750));
    }
  }

  throw lastError;
}

function isNavigationInterruption(error) {
  return /Inspected target navigated or closed/.test(error instanceof Error ? error.message : String(error));
}

function parseJson(stdout) {
  try {
    return JSON.parse(stdout);
  } catch {
    return null;
  }
}

function findVariable(parsed, name) {
  return parsed?.variables?.find((variable) => variable.name === name);
}

function verifyResultUi(result, parsed) {
  if (!parsed) {
    return result.resultsState === "invalid-json" || result.resultsState === "runtime-error";
  }

  if (parsed.success === true) {
    return result.resultsState === "success"
      && result.resultVariableRows.length > 0
      && parsed.variables.every((variable) => result.resultVariableRows.some((row) => row.name === variable.name));
  }

  return result.resultsState === "failed-json"
    && /parser|error|failure/i.test(result.resultErrorSummary)
    && result.resultVariableRows.length >= 0;
}

function verifyRawOutputUi(result) {
  if (!SHOW_DEBUG_SURFACES) {
    return true;
  }

  const hasOutput = typeof result.stdout === "string" && result.stdout.trim().length > 0;
  const hasDiagnostics = typeof result.stderr === "string" && result.stderr.trim().length > 0;
  const failed = result.status === "Failure";

  return hasOutput
    && result.rawOutputText?.includes("Raw output") !== false
    && (failed || hasDiagnostics ? result.rawOutputOpen === true && result.diagnosticsActive === "true" : true);
}

function createCdpClient(wsUrl) {
  const url = new URL(wsUrl);
  const key = crypto.randomBytes(16).toString("base64");
  const socket = net.createConnection(Number(url.port), url.hostname);
  let buffer = Buffer.alloc(0);
  let handshakeComplete = false;
  let nextId = 1;
  const pending = new Map();
  const eventWaiters = new Map();

  const ready = new Promise((resolve, reject) => {
    socket.on("connect", () => {
      socket.write([
        `GET ${url.pathname}${url.search} HTTP/1.1`,
        `Host: ${url.host}`,
        "Upgrade: websocket",
        "Connection: Upgrade",
        `Sec-WebSocket-Key: ${key}`,
        "Sec-WebSocket-Version: 13",
        "",
        "",
      ].join("\r\n"));
    });

    socket.on("data", (chunk) => {
      buffer = Buffer.concat([buffer, chunk]);
      if (!handshakeComplete) {
        const headerEnd = buffer.indexOf("\r\n\r\n");
        if (headerEnd === -1) {
          return;
        }

        const headers = buffer.subarray(0, headerEnd).toString("utf8");
        if (!headers.includes(" 101 ")) {
          reject(new Error(headers));
          return;
        }

        buffer = buffer.subarray(headerEnd + 4);
        handshakeComplete = true;
        resolve();
      }

      parseFrames();
    });

    socket.on("error", reject);
    socket.on("end", rejectPending);
    socket.on("close", rejectPending);
  });

  return {
    ready,
    send(method, params = {}) {
      const id = nextId++;
      const message = JSON.stringify({ id, method, params });
      return new Promise((resolve, reject) => {
        pending.set(id, { resolve, reject });
        socket.write(createWsFrame(message));
      });
    },
    waitEvent(method, timeoutMs = 15_000) {
      return new Promise((resolve, reject) => {
        const timer = setTimeout(() => reject(new Error(`Timed out waiting for ${method}`)), timeoutMs);
        const wrapped = (params) => {
          clearTimeout(timer);
          resolve(params);
        };

        const waiters = eventWaiters.get(method) || [];
        waiters.push(wrapped);
        eventWaiters.set(method, waiters);
      });
    },
    close() {
      socket.end();
    },
  };

  function parseFrames() {
    while (buffer.length >= 2) {
      const first = buffer[0];
      const second = buffer[1];
      let offset = 2;
      let length = second & 0x7f;

      if (length === 126) {
        if (buffer.length < offset + 2) {
          return;
        }
        length = buffer.readUInt16BE(offset);
        offset += 2;
      } else if (length === 127) {
        if (buffer.length < offset + 8) {
          return;
        }
        length = Number(buffer.readBigUInt64BE(offset));
        offset += 8;
      }

      const masked = Boolean(second & 0x80);
      let mask;
      if (masked) {
        if (buffer.length < offset + 4) {
          return;
        }
        mask = buffer.subarray(offset, offset + 4);
        offset += 4;
      }

      if (buffer.length < offset + length) {
        return;
      }

      let payload = buffer.subarray(offset, offset + length);
      buffer = buffer.subarray(offset + length);

      if (masked) {
        const unmasked = Buffer.alloc(payload.length);
        for (let index = 0; index < payload.length; index += 1) {
          unmasked[index] = payload[index] ^ mask[index % 4];
        }
        payload = unmasked;
      }

      const opcode = first & 0x0f;
      if (opcode === 1) {
        handleMessage(payload.toString("utf8"));
      } else if (opcode === 8) {
        socket.end();
      }
    }
  }

  function handleMessage(text) {
    const message = JSON.parse(text);

    if (message.id && pending.has(message.id)) {
      const { resolve, reject } = pending.get(message.id);
      pending.delete(message.id);

      if (message.error) {
        reject(new Error(JSON.stringify(message.error)));
      } else {
        resolve(message.result);
      }
      return;
    }

    if (message.method && eventWaiters.has(message.method)) {
      const waiters = eventWaiters.get(message.method);
      eventWaiters.delete(message.method);
      for (const resolve of waiters) {
        resolve(message.params || {});
      }
    }
  }

  function rejectPending() {
    const error = new Error("Chrome DevTools socket closed.");
    for (const { reject } of pending.values()) {
      reject(error);
    }
    pending.clear();
  }
}

function createWsFrame(text) {
  const payload = Buffer.from(text);
  const mask = crypto.randomBytes(4);
  let header;

  if (payload.length < 126) {
    header = Buffer.alloc(2);
    header[1] = 0x80 | payload.length;
  } else if (payload.length < 65536) {
    header = Buffer.alloc(4);
    header[1] = 0x80 | 126;
    header.writeUInt16BE(payload.length, 2);
  } else {
    header = Buffer.alloc(10);
    header[1] = 0x80 | 127;
    header.writeBigUInt64BE(BigInt(payload.length), 2);
  }

  header[0] = 0x81;
  const masked = Buffer.alloc(payload.length);
  for (let index = 0; index < payload.length; index += 1) {
    masked[index] = payload[index] ^ mask[index % 4];
  }

  return Buffer.concat([header, mask, masked]);
}
