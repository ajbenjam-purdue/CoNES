import { useEffect, useMemo, useRef, useState } from "react";
import {
  AlertCircle,
  Check,
  CheckCircle2,
  Clock3,
  Download,
  FileCode2,
  FileJson2,
  FilePlus2,
  ListChecks,
  Loader2,
  Pencil,
  Play,
  Save,
  Terminal,
  Upload,
  X,
} from "lucide-react";
import { showDebugSurfaces } from "@/app/debug-flags";
import { Alert, AlertDescription, AlertTitle } from "@/components/ui/alert";
import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import { Separator } from "@/components/ui/separator";
import { MonacoSourceEditor } from "@/features/workbench/MonacoSourceEditor";
import { ResultsViewer } from "@/features/results/ResultsViewer";
import { RunnerOutput } from "@/features/runner/RunnerOutput";
import { formatBytes, formatPhase } from "@/features/runner/runner-state";
import {
  getRuntimePrewarmState,
  getRuntimeWorkerClient,
  subscribeRuntimePrewarm,
  type RuntimePrewarmState,
} from "@/features/runtime/runtime-prewarm";
import { saveRuntimeLastRun } from "@/features/runtime/runtime-last-run";
import type { WorkerLintResult, WorkerRunResult } from "@/features/runtime/worker-contract";
import {
  normalizeLintDiagnostics,
  type LintDiagnostic,
} from "@/features/workbench/workbench-lint-diagnostics";
import { useWorkbenchSource } from "@/features/workbench/workbench-source-state";

type WorkbenchRunState = "idle" | "loading" | "running" | "success" | "failure";
type WorkbenchLintState = "idle" | "waiting" | "linting" | "clean" | "errors" | "failed";
type LintMode = "manual" | "auto";

type RunMeta = {
  worker: string;
  json: string;
  exit: string;
  time: string;
};

const initialMeta: RunMeta = {
  worker: "Idle",
  json: "Waiting",
  exit: "-",
  time: "-",
};

const runtimeWaitSteps = [
  {
    label: "Starting solver worker",
    detail: "Opening the browser runtime lane.",
  },
  {
    label: "Mounting CoNES runtime",
    detail: "Preparing the in-memory filesystem.",
  },
  {
    label: "Indexing materials",
    detail: "Making bundled material tables available.",
  },
  {
    label: "Linking libraries",
    detail: "Registering packaged CoNES includes.",
  },
  {
    label: "Checking JSON runner",
    detail: "Confirming the solver output path.",
  },
  {
    label: "Unlocking Workbench",
    detail: "Preparing Run and Lint controls.",
  },
] as const;

export function WorkbenchPage() {
  const clientRef = useRef(getRuntimeWorkerClient());
  const runTokenRef = useRef(0);
  const lintTokenRef = useRef(0);
  const sourceRevisionRef = useRef(0);
  const didMountSourceRef = useRef(false);
  const autoLintTimeoutRef = useRef<number | null>(null);
  const lastScheduledLintRevisionRef = useRef<number | null>(null);
  const ignoreNextRenameBlurRef = useRef(false);
  const renameInputRef = useRef<HTMLInputElement | null>(null);
  const source = useWorkbenchSource();
  const [prewarmState, setPrewarmState] = useState<RuntimePrewarmState>(() => getRuntimePrewarmState());
  const [runState, setRunState] = useState<WorkbenchRunState>("idle");
  const [lintState, setLintState] = useState<WorkbenchLintState>("idle");
  const [lintMode, setLintMode] = useState<LintMode>("auto");
  const [runtimeWaitStep, setRuntimeWaitStep] = useState(0);
  const [isRenaming, setIsRenaming] = useState(false);
  const [renameDraft, setRenameDraft] = useState(source.fileName);
  const [lintDiagnostics, setLintDiagnostics] = useState<LintDiagnostic[]>([]);
  const [lintStdout, setLintStdout] = useState("");
  const [lintStderr, setLintStderr] = useState("");
  const [isRunning, setIsRunning] = useState(false);
  const [isLinting, setIsLinting] = useState(false);
  const [stdout, setStdout] = useState("");
  const [stderr, setStderr] = useState("");
  const [meta, setMeta] = useState<RunMeta>(initialMeta);
  const [timings, setTimings] = useState<WorkerRunResult["timings"] | null>(null);
  const [jsonDownloadUrl, setJsonDownloadUrl] = useState<string | null>(null);

  const runtimeReady = prewarmState.status === "ready";
  const canRun = runtimeReady && source.source.trim().length > 0 && !isRunning;
  const canLint = runtimeReady && source.source.trim().length > 0 && !isRunning && !isLinting;
  const runLabel = isRunning ? "Running" : "Run";
  const lintLabel = isLinting ? "Linting" : "Lint";
  const runStatus = useMemo(() => formatRunStatus(runState), [runState]);
  const lintStatus = useMemo(() => formatLintStatus(lintState, lintDiagnostics.length), [lintState, lintDiagnostics.length]);
  const runtimeWaitCopy = useMemo(
    () => buildRuntimeWaitCopy(prewarmState, runtimeWaitStep),
    [prewarmState, runtimeWaitStep],
  );

  useEffect(() => {
    return subscribeRuntimePrewarm(setPrewarmState);
  }, []);

  useEffect(() => {
    if (prewarmState.status === "ready" || prewarmState.status === "failed") {
      return;
    }

    const interval = window.setInterval(() => {
      setRuntimeWaitStep((current) => current + 1);
    }, 1400);

    return () => {
      window.clearInterval(interval);
    };
  }, [prewarmState.status]);

  useEffect(() => {
    if (!isRenaming) {
      setRenameDraft(source.fileName);
    }
  }, [isRenaming, source.fileName]);

  useEffect(() => {
    if (!isRenaming) {
      return;
    }

    renameInputRef.current?.focus();
    renameInputRef.current?.select();
  }, [isRenaming]);

  useEffect(() => {
    if (!isRunning && (runState === "idle" || runState === "loading")) {
      setMeta((current) => ({ ...current, worker: prewarmState.label }));
    }
  }, [isRunning, prewarmState.label, runState]);

  useEffect(() => {
    if (!stdout.trim()) {
      setJsonDownloadUrl(null);
      return;
    }

    try {
      JSON.parse(stdout);
    } catch {
      setJsonDownloadUrl(null);
      return;
    }

    const blob = new Blob([stdout], { type: "application/json" });
    const url = URL.createObjectURL(blob);
    setJsonDownloadUrl(url);

    return () => {
      URL.revokeObjectURL(url);
    };
  }, [stdout]);

  function clearAutoLintTimer() {
    if (autoLintTimeoutRef.current !== null) {
      window.clearTimeout(autoLintTimeoutRef.current);
      autoLintTimeoutRef.current = null;
    }
  }

  function scheduleAutoLint(revision: number) {
    if (!source.source.trim()) {
      lintTokenRef.current += 1;
      clearAutoLintTimer();
      lastScheduledLintRevisionRef.current = null;
      setLintState("idle");
      setLintDiagnostics([]);
      setLintStdout("");
      setLintStderr("");
      return;
    }

    if (isRunning || prewarmState.status !== "ready") {
      return;
    }

    if (lastScheduledLintRevisionRef.current === revision) {
      return;
    }

    lastScheduledLintRevisionRef.current = revision;
    setLintState("waiting");
    clearAutoLintTimer();
    autoLintTimeoutRef.current = window.setTimeout(() => {
      autoLintTimeoutRef.current = null;
      void runLint("auto", revision);
    }, 1000);
  }

  useEffect(() => {
    if (!didMountSourceRef.current) {
      didMountSourceRef.current = true;
      sourceRevisionRef.current = 1;
    } else {
      sourceRevisionRef.current += 1;
    }

    scheduleAutoLint(sourceRevisionRef.current);
    return clearAutoLintTimer;
  }, [source.source]);

  useEffect(() => {
    if (isRunning) {
      clearAutoLintTimer();
      return;
    }

    scheduleAutoLint(sourceRevisionRef.current);
  }, [isRunning, prewarmState.status]);

  function resetOutput() {
    setStdout("");
    setStderr("");
    setMeta({ ...initialMeta, worker: prewarmState.label });
    setTimings(null);
  }

  async function handleRun() {
    if (!canRun) {
      return;
    }

    const activeToken = ++runTokenRef.current;
    clearAutoLintTimer();
    setIsRunning(true);
    resetOutput();
    setRunState(prewarmState.status === "ready" ? "running" : "loading");
    setMeta({ ...initialMeta, worker: prewarmState.status === "ready" ? "Running" : "Preparing runtime" });

    const result = await clientRef.current.runSource(
      {
        fileName: source.fileName,
        contents: source.source,
        lastModified: source.fileMeta?.lastModified ?? Date.now(),
      },
      {
        onStatus: (message) => {
          const phase = message.phase || "loading-runtime";
          setMeta((current) => ({ ...current, worker: formatPhase(phase) }));
          setRunState(phase === "running" ? "running" : "loading");
        },
      },
    );

    if (activeToken !== runTokenRef.current) {
      return;
    }

    setIsRunning(false);
    renderResult(result);
  }

  async function handleLint() {
    if (!canLint) {
      return;
    }

    await runLint("manual", sourceRevisionRef.current);
  }

  async function runLint(mode: LintMode, revision: number) {
    if (!source.source.trim() || isRunning) {
      return;
    }

    clearAutoLintTimer();
    const activeToken = ++lintTokenRef.current;
    setLintMode(mode);
    setIsLinting(true);
    setLintState("linting");

    const result = await clientRef.current.lintSource(
      {
        fileName: source.fileName,
        contents: source.source,
        lastModified: source.fileMeta?.lastModified ?? Date.now(),
      },
      {
        onStatus: (message) => {
          if (message.phase === "linting") {
            setLintState("linting");
          }
        },
      },
    );

    if (activeToken !== lintTokenRef.current) {
      return;
    }

    setIsLinting(false);

    if (revision !== sourceRevisionRef.current) {
      return;
    }

    renderLintResult(result);
  }

  function renderLintResult(result: WorkerLintResult) {
    const diagnostics = normalizeLintDiagnostics(result);
    const runtimeFailed = result.type === "failure" && result.exitCode === null && diagnostics.length === 0;

    setLintStdout(result.stdout || "");
    setLintStderr(result.stderr || (runtimeFailed ? result.error?.message || "" : ""));
    setLintDiagnostics(diagnostics);
    setLintState(runtimeFailed ? "failed" : diagnostics.length > 0 ? "errors" : "clean");
  }

  function renderResult(result: WorkerRunResult) {
    const nextStdout = result.stdout || "";
    const nextStderr = result.stderr || "";
    const jsonState = parseJsonState(nextStdout);

    setStdout(nextStdout);
    setStderr(nextStderr || (result.type === "failure" ? result.error?.message || "" : ""));
    setTimings(result.timings || null);
    setRunState(result.type === "success" ? "success" : "failure");
    setMeta({
      worker: result.type === "success" ? "Complete" : result.error?.code || "Failed",
      json: jsonState,
      exit: result.exitCode === null || result.exitCode === undefined ? "-" : String(result.exitCode),
      time: result.timings?.wallMs ? `${Math.round(result.timings.wallMs)} ms` : "-",
    });
    saveRuntimeLastRun({
      completedAt: new Date().toISOString(),
      fileName: source.fileName,
      status: result.type,
      exitCode: result.exitCode ?? null,
      errorCode: result.error?.code,
      timings: result.timings,
    });
  }

  function handleCancelOrReset() {
    runTokenRef.current += 1;
    clearAutoLintTimer();

    if (isRunning) {
      clientRef.current.terminate();
    }

    setIsRunning(false);
    setRunState("idle");
    resetOutput();
  }

  function startRenaming() {
    ignoreNextRenameBlurRef.current = false;
    setRenameDraft(source.fileName);
    setIsRenaming(true);
  }

  function commitRename() {
    if (ignoreNextRenameBlurRef.current) {
      ignoreNextRenameBlurRef.current = false;
      return;
    }

    if (source.renameFileName(renameDraft)) {
      setIsRenaming(false);
    }
  }

  function cancelRename() {
    ignoreNextRenameBlurRef.current = true;
    setRenameDraft(source.fileName);
    setIsRenaming(false);
    source.setError(null);
  }

  function handleNewDraft() {
    setIsRenaming(false);
    source.newDraft();
  }

  function handleOpenFile(file: File | null) {
    setIsRenaming(false);
    void source.openFile(file);
  }

  return (
    <section
      className="flex h-full min-h-svh flex-col"
      data-runtime-prewarm-state={prewarmState.status}
      data-workbench-page
    >
      <div className="grid gap-3 border-b bg-background p-3 min-[1180px]:grid-cols-[minmax(260px,1fr)_auto]">
        <div className="flex min-w-0 items-center gap-3">
          <div className="min-w-0">
            <p className="text-xs font-semibold uppercase text-muted-foreground">CoNES Studio</p>
            {isRenaming ? (
              <div className="mt-1 flex min-w-0 items-center gap-2">
                <input
                  ref={renameInputRef}
                  value={renameDraft}
                  onChange={(event) => setRenameDraft(event.target.value)}
                  onBlur={commitRename}
                  onKeyDown={(event) => {
                    if (event.key === "Enter") {
                      event.preventDefault();
                      commitRename();
                    } else if (event.key === "Escape") {
                      event.preventDefault();
                      cancelRename();
                    }
                  }}
                  className="h-9 w-[min(72vw,32rem)] min-w-0 max-w-full rounded-md border border-input bg-background px-2 text-xl font-semibold tracking-normal outline-none ring-offset-background focus-visible:ring-2 focus-visible:ring-ring focus-visible:ring-offset-2"
                  aria-label="Active buffer file name"
                  data-workbench-rename-input
                />
                <Button type="button" variant="ghost" size="icon" onMouseDown={(event) => event.preventDefault()} onClick={commitRename}>
                  <Check aria-hidden="true" />
                  <span className="sr-only">Save file name</span>
                </Button>
                <Button type="button" variant="ghost" size="icon" onMouseDown={(event) => event.preventDefault()} onClick={cancelRename}>
                  <X aria-hidden="true" />
                  <span className="sr-only">Cancel rename</span>
                </Button>
              </div>
            ) : (
              <button
                type="button"
                className="mt-1 flex max-w-full items-center gap-2 rounded-md text-left text-xl font-semibold tracking-normal outline-none hover:text-primary focus-visible:ring-2 focus-visible:ring-ring focus-visible:ring-offset-2 focus-visible:ring-offset-background"
                onClick={startRenaming}
                aria-label={`Rename ${source.fileName}`}
                data-workbench-source-name
                data-workbench-rename-button
              >
                <span className="truncate">{source.fileName}</span>
                <Pencil className="size-4 shrink-0 text-muted-foreground" aria-hidden="true" />
              </button>
            )}
          </div>
          <span className="shrink-0 text-xs text-muted-foreground" data-workbench-dirty-state>
            {source.dirty ? "Unsaved" : source.status}
          </span>
        </div>

        {runtimeReady ? (
          <div className="flex flex-wrap items-center justify-start gap-2 min-[1180px]:justify-end" data-workbench-actions>
            <Button type="button" variant="outline" onClick={handleNewDraft} data-workbench-new-button>
              <FilePlus2 aria-hidden="true" />
              New
            </Button>
            <Button asChild type="button" variant="outline">
              <label className="cursor-pointer">
                <Upload aria-hidden="true" />
                Open
                <input
                  ref={source.inputRef}
                  type="file"
                  accept=".cnes"
                  className="sr-only"
                  data-workbench-open-input
                  onChange={(event) => handleOpenFile(event.target.files?.[0] ?? null)}
                />
              </label>
            </Button>
            <Button asChild type="button" variant="outline">
              <a href={source.downloadUrl || undefined} download={source.fileName} onClick={source.markDownloaded} data-workbench-download-source>
                <Save aria-hidden="true" />
                Save
              </a>
            </Button>
            <Button
              type="button"
              variant="outline"
              disabled={!canLint}
              onClick={() => void handleLint()}
              className="min-w-24"
              data-workbench-lint-button
            >
              {isLinting ? <Loader2 className="animate-spin" aria-hidden="true" /> : <ListChecks aria-hidden="true" />}
              {lintLabel}
            </Button>
            <Button
              type="button"
              disabled={!canRun}
              onClick={() => void handleRun()}
              className="min-w-24"
              data-workbench-run-button
            >
              <Play aria-hidden="true" />
              {runLabel}
            </Button>
          </div>
        ) : (
          <div
            className="flex min-h-10 items-center justify-start gap-3 text-sm text-muted-foreground min-[1180px]:justify-end"
            data-workbench-runtime-gate
          >
            <Loader2 className="size-4 animate-spin" aria-hidden="true" />
            <div className="min-w-0" aria-live="polite" data-workbench-runtime-operation={runtimeWaitCopy.label}>
              <p className="truncate font-medium text-foreground">{runtimeWaitCopy.label}</p>
              <p className="truncate text-xs">{runtimeWaitCopy.detail}</p>
            </div>
          </div>
        )}
      </div>

      {source.error ? (
        <Alert variant="destructive" className="m-4 mb-0">
          <AlertCircle aria-hidden="true" />
          <AlertTitle>File rejected</AlertTitle>
          <AlertDescription>{source.error}</AlertDescription>
        </Alert>
      ) : null}

      <div className="grid flex-1 grid-cols-[minmax(360px,0.9fr)_minmax(480px,1.1fr)_300px] max-2xl:grid-cols-[minmax(360px,0.9fr)_minmax(460px,1.1fr)] max-xl:grid-cols-1">
        <main className="flex min-w-0 flex-col border-r max-xl:border-r-0">
          <div className="flex items-center justify-between gap-3 border-b px-4 py-2">
            <div className="flex min-w-0 items-center gap-2">
              <FileCode2 className="text-muted-foreground" aria-hidden="true" />
              <div className="min-w-0">
                <h3 className="truncate text-sm font-semibold tracking-normal">Source</h3>
              </div>
            </div>
            <span className="text-xs text-muted-foreground">
              {source.stats.lines} lines
            </span>
          </div>
          <MonacoSourceEditor
            value={source.source}
            onChange={source.setSource}
            diagnostics={lintDiagnostics}
            readOnly={isRunning}
          />
        </main>

        <section className="flex min-w-0 flex-col">
          <ResultsViewer stdout={stdout} stderr={stderr} isRunning={isRunning} />
          {showDebugSurfaces ? <RunnerOutput stdout={stdout} stderr={stderr} hasFailure={runState === "failure"} /> : null}
        </section>

        <aside className="border-l bg-muted/30 max-2xl:col-span-2 max-2xl:border-l-0 max-2xl:border-t max-xl:col-span-1">
          <div className="grid grid-cols-2 border-b max-sm:grid-cols-1">
            <MetaCell label="Run" value={runStatus} dataAttr="run" />
            <MetaCell label="Lint" value={lintStatus} dataAttr="lint" />
            <MetaCell label="Worker" value={meta.worker} dataAttr="worker" />
            <MetaCell label="JSON" value={meta.json} dataAttr="json" />
            <MetaCell label="Exit" value={meta.exit} dataAttr="exit" />
          </div>

          <div className="flex flex-col gap-4 p-4 text-sm">
            <div className="flex items-center gap-2">
              {runState === "success" ? (
                <CheckCircle2 className="text-primary" aria-hidden="true" />
              ) : (
                <Terminal className="text-muted-foreground" aria-hidden="true" />
              )}
              <div>
                <h3 className="font-semibold tracking-normal">Current buffer execution</h3>
                <p className="text-muted-foreground">Run sends the Monaco source through the existing worker file path.</p>
              </div>
            </div>

            <Separator />

            {runtimeReady ? (
              <>
                <DiagnosticsPanel
                  diagnostics={lintDiagnostics}
                  lintMode={lintMode}
                  lintState={lintState}
                  lintStdout={lintStdout}
                  lintStderr={lintStderr}
                  showRawOutput={showDebugSurfaces}
                />

                <Separator />
              </>
            ) : null}

            <div className="grid gap-3">
              <DetailRow icon={FileCode2} label="source file" value={source.fileName} />
              <DetailRow icon={Clock3} label="time" value={meta.time} />
              <DetailRow icon={Clock3} label="solve" value={formatMs(timings?.solveMs)} />
              {showDebugSurfaces ? (
                <>
                  <DetailRow icon={FileJson2} label="output bytes" value={formatBytes(new TextEncoder().encode(stdout).byteLength)} />
                  <DetailRow icon={AlertCircle} label="diagnostic bytes" value={formatBytes(new TextEncoder().encode(stderr).byteLength)} />
                  <DetailRow icon={Clock3} label="module" value={formatMs(timings?.moduleMs)} />
                  <DetailRow icon={Clock3} label="runtime data" value={formatMs(timings?.runtimeDataMs)} />
                </>
              ) : null}
            </div>

            {runtimeReady ? (
              <>
                <div className="flex flex-wrap gap-2" data-workbench-output-actions>
                  <Button type="button" variant="outline" onClick={handleCancelOrReset} data-workbench-reset-output>
                    <Terminal aria-hidden="true" />
                    Reset output
                  </Button>
                  <Button asChild type="button" variant="outline">
                    <a
                      href={jsonDownloadUrl || undefined}
                      aria-disabled={!jsonDownloadUrl}
                      download={`${source.fileName.replace(/\.cnes$/i, "")}.json`}
                      data-workbench-download-json
                    >
                      <Download aria-hidden="true" />
                      JSON
                    </a>
                  </Button>
                </div>

                {showDebugSurfaces ? (
                  <Alert>
                    <Terminal aria-hidden="true" />
                    <AlertTitle>Worker contract</AlertTitle>
                    <AlertDescription>
                      Workbench delegates to the accepted runner path with <code>["/work/input.cnes","--json"]</code>; no backend or
                      C++ core changes are involved.
                    </AlertDescription>
                  </Alert>
                ) : null}
              </>
            ) : null}
          </div>
        </aside>
      </div>
    </section>
  );
}

function MetaCell({
  label,
  value,
  dataAttr,
}: {
  label: string;
  value: string;
  dataAttr: "run" | "lint" | "worker" | "json" | "exit";
}) {
  const dataProps = {
    run: { "data-workbench-run-status": true },
    lint: { "data-workbench-lint-status": true },
    worker: { "data-workbench-worker-state": true },
    json: { "data-workbench-json-state": true },
    exit: { "data-workbench-exit-code": true },
  }[dataAttr];

  return (
    <div className="flex min-h-20 flex-col justify-center gap-1 border-b border-r p-4 even:border-r-0">
      <span className="text-xs font-semibold uppercase text-muted-foreground">{label}</span>
      <strong className="break-words text-sm" {...dataProps}>
        {value}
      </strong>
    </div>
  );
}

function DiagnosticsPanel({
  diagnostics,
  lintMode,
  lintState,
  lintStdout,
  lintStderr,
  showRawOutput,
}: {
  diagnostics: LintDiagnostic[];
  lintMode: LintMode;
  lintState: WorkbenchLintState;
  lintStdout: string;
  lintStderr: string;
  showRawOutput: boolean;
}) {
  const rawText = [lintStdout, lintStderr].filter(Boolean).join("\n\n");
  const rawBytes = formatBytes(new TextEncoder().encode(rawText).byteLength);

  return (
    <section className="grid gap-3" data-workbench-diagnostics-panel data-workbench-diagnostics-count={diagnostics.length}>
      <div className="flex items-center justify-between gap-3">
        <div className="flex min-w-0 items-center gap-2">
          <ListChecks className="text-muted-foreground" aria-hidden="true" />
          <div className="min-w-0">
            <h3 className="truncate text-sm font-semibold tracking-normal">Diagnostics</h3>
            <p className="text-xs text-muted-foreground" data-workbench-lint-mode>
              {lintMode === "manual" ? "Manual lint" : "Auto lint"}
            </p>
          </div>
        </div>
        <Badge variant={lintState === "errors" || lintState === "failed" ? "destructive" : "secondary"} data-workbench-diagnostics-badge>
          {formatLintStatus(lintState, diagnostics.length)}
        </Badge>
      </div>

      {diagnostics.length > 0 ? (
        <div className="grid gap-2" data-workbench-diagnostic-list>
          {diagnostics.map((diagnostic, index) => (
            <div
              key={`${diagnostic.line || "file"}-${index}-${diagnostic.message}`}
              className="grid gap-1 border-l-2 border-destructive pl-3"
              data-workbench-diagnostic
              data-diagnostic-line={diagnostic.line || ""}
            >
              <span className="text-xs font-semibold uppercase text-destructive">
                {diagnostic.line ? `Line ${diagnostic.line}` : "File"}
              </span>
              <p className="break-words text-sm">{diagnostic.message}</p>
            </div>
          ))}
        </div>
      ) : (
        <p className="text-sm text-muted-foreground" data-workbench-diagnostics-empty>
          {lintState === "clean"
            ? "No lint diagnostics."
            : lintState === "waiting"
              ? "Waiting for edit pause."
              : lintState === "linting"
                ? "Linting current buffer."
                : lintState === "failed"
                  ? "Lint did not complete; inspect raw lint output."
                  : "No lint run yet."}
        </p>
      )}

      {showRawOutput ? (
        <details className="group rounded-md border bg-background/60" data-workbench-lint-raw-section>
          <summary className="flex cursor-pointer list-none items-center justify-between gap-2 px-3 py-2 marker:hidden">
            <span className="flex items-center gap-2 text-xs font-semibold uppercase text-muted-foreground">
              <Terminal className="size-4" aria-hidden="true" />
              Raw lint output
            </span>
            <span className="text-xs text-muted-foreground" data-workbench-lint-raw-bytes>{rawBytes}</span>
          </summary>
          <pre className="max-h-44 overflow-auto whitespace-pre-wrap border-t bg-code px-3 py-3 font-mono text-xs leading-5 text-code-foreground" data-workbench-lint-raw-output>
            {rawText || "No lint output."}
          </pre>
        </details>
      ) : null}
    </section>
  );
}

function DetailRow({ icon: Icon, label, value }: { icon: typeof Clock3; label: string; value: string }) {
  return (
    <div className="flex items-start justify-between gap-3">
      <span className="flex items-center gap-2 text-muted-foreground">
        <Icon aria-hidden="true" />
        {label}
      </span>
      <Badge variant="secondary" className="max-w-[180px] justify-end truncate">
        {value}
      </Badge>
    </div>
  );
}

function parseJsonState(stdout: string) {
  if (!stdout.trim()) {
    return "No output";
  }
  try {
    JSON.parse(stdout);
  } catch {
    return "Parse warning";
  }
  return "Parsed";
}

function buildRuntimeWaitCopy(prewarmState: RuntimePrewarmState, stepIndex: number) {
  if (prewarmState.status === "failed") {
    return {
      label: prewarmState.label,
      detail: prewarmState.errorMessage || "The browser worker could not finish startup.",
    };
  }

  return runtimeWaitSteps[stepIndex % runtimeWaitSteps.length];
}

function formatRunStatus(state: WorkbenchRunState) {
  const labels: Record<WorkbenchRunState, string> = {
    idle: "Idle",
    loading: "Loading",
    running: "Running",
    success: "Success",
    failure: "Failure",
  };

  return labels[state];
}

function formatLintStatus(state: WorkbenchLintState, count: number) {
  if (state === "errors") {
    return `${count} ${count === 1 ? "error" : "errors"}`;
  }

  const labels: Record<WorkbenchLintState, string> = {
    idle: "Idle",
    waiting: "Waiting",
    linting: "Linting",
    clean: "Clean",
    errors: "Errors",
    failed: "Failed",
  };

  return labels[state];
}

function formatMs(value: number | undefined) {
  return value !== undefined && Number.isFinite(value) ? `${Math.round(value)} ms` : "-";
}
