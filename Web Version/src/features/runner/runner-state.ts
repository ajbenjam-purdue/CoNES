export const runnerStates = {
  noFile: "no-file",
  ready: "ready",
  loading: "loading",
  running: "running",
  success: "success",
  failure: "failure",
} as const;

export type RunnerState = (typeof runnerStates)[keyof typeof runnerStates];

export const statusLabels: Record<RunnerState, string> = {
  [runnerStates.noFile]: "No file",
  [runnerStates.ready]: "Ready",
  [runnerStates.loading]: "Loading runtime/assets",
  [runnerStates.running]: "Running",
  [runnerStates.success]: "Success",
  [runnerStates.failure]: "Failure",
};

export function formatBytes(bytes: number) {
  if (bytes < 1024) {
    return `${bytes} B`;
  }
  if (bytes < 1024 * 1024) {
    return `${(bytes / 1024).toFixed(1)} KB`;
  }
  return `${(bytes / 1024 / 1024).toFixed(1)} MB`;
}

export function formatPhase(phase: string) {
  return phase
    .split("-")
    .map((part) => part.charAt(0).toUpperCase() + part.slice(1))
    .join(" ");
}

export function isCnesFile(file: File | null | undefined) {
  return Boolean(file && file.name.toLowerCase().endsWith(".cnes"));
}

export function buildDownloadName(file: File | null) {
  const baseName = file?.name?.replace(/\.cnes$/i, "") || "cones-result";
  return `${baseName}.json`;
}
