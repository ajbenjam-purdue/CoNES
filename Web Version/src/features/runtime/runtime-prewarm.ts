import { CoNESWorkerClient } from "@/features/runtime/worker-client";
import { formatPhase } from "@/features/runner/runner-state";
import type { WorkerDiagnosticCode, WorkerTimings } from "@/features/runtime/worker-contract";

export type RuntimePrewarmState = {
  status: "idle" | "preparing" | "ready" | "failed";
  label: string;
  phase?: string;
  errorCode?: WorkerDiagnosticCode | string;
  errorMessage?: string;
  timings?: WorkerTimings;
  updatedAt?: string;
};

const runtimePrewarmEvent = "cones-runtime-prewarm";
const workerClient = new CoNESWorkerClient();
let state: RuntimePrewarmState = {
  status: "idle",
  label: "Not started",
};
let prewarmStarted = false;

export function getRuntimeWorkerClient() {
  return workerClient;
}

export function getRuntimePrewarmState() {
  return state;
}

export function subscribeRuntimePrewarm(listener: (nextState: RuntimePrewarmState) => void) {
  function handleEvent() {
    listener(state);
  }

  window.addEventListener(runtimePrewarmEvent, handleEvent);
  return () => {
    window.removeEventListener(runtimePrewarmEvent, handleEvent);
  };
}

export function startRuntimePrewarm() {
  if (prewarmStarted) {
    return;
  }

  prewarmStarted = true;
  setRuntimePrewarmState({
    status: "preparing",
    label: "Preparing runtime",
    phase: "starting",
  });

  void workerClient.prewarm({
    onStatus(message) {
      setRuntimePrewarmState({
        status: "preparing",
        label: formatPhase(message.phase || "preparing-runtime"),
        phase: message.phase,
      });
    },
  }).then((result) => {
    if (result.type === "prewarm-ready") {
      setRuntimePrewarmState({
        status: "ready",
        label: "Runtime ready",
        phase: "runtime-ready",
        timings: result.timings,
      });
      return;
    }

    setRuntimePrewarmState({
      status: "failed",
      label: result.error?.code || "Runtime failed",
      phase: "prewarm-failed",
      errorCode: result.error?.code,
      errorMessage: result.error?.message,
      timings: result.timings,
    });
  });
}

function setRuntimePrewarmState(nextState: Omit<RuntimePrewarmState, "updatedAt">) {
  state = {
    ...nextState,
    updatedAt: new Date().toISOString(),
  };
  window.dispatchEvent(new CustomEvent(runtimePrewarmEvent, { detail: state }));
}
