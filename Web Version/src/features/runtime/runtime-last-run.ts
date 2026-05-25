import type { WorkerRunResult } from "@/features/runtime/worker-contract";

const storageKey = "cones.runtime.lastRun";

export const runtimeLastRunEvent = "cones-runtime-last-run";

export type RuntimeLastRunSummary = {
  completedAt: string;
  fileName: string;
  status: WorkerRunResult["type"];
  exitCode: number | null;
  errorCode?: string;
  timings?: WorkerRunResult["timings"];
};

export function saveRuntimeLastRun(summary: RuntimeLastRunSummary) {
  try {
    window.localStorage.setItem(storageKey, JSON.stringify(summary));
    window.dispatchEvent(new CustomEvent(runtimeLastRunEvent, { detail: summary }));
  } catch {
    // Diagnostic state must not affect solving or raw output.
  }
}

export function readRuntimeLastRun(): RuntimeLastRunSummary | null {
  try {
    const raw = window.localStorage.getItem(storageKey);
    return raw ? JSON.parse(raw) as RuntimeLastRunSummary : null;
  } catch {
    return null;
  }
}
