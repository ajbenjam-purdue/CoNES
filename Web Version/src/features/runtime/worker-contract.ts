import { z } from "zod";

export const runArgv = ["/work/input.cnes", "--json"] as const;
export const lintArgv = ["/work/input.cnes", "--lint", "--json"] as const;

export const workerDiagnosticCodes = [
  "module-load-failed",
  "module-factory-missing",
  "module-init-failed",
  "wasm-load-failed",
  "manifest-fetch-failed",
  "manifest-invalid",
  "manifest-entry-invalid",
  "runtime-file-fetch-failed",
  "byte-mismatch",
  "sha256-mismatch",
  "missing-file",
  "invalid-file",
  "unsupported-message",
  "worker-response-invalid",
  "worker-create-failed",
  "worker-error",
  "file-read-failed",
  "request-cancelled",
  "process-exit-nonzero",
  "json-parse-failed",
  "prewarm-failed",
  "runtime-error",
] as const;

export const workerDiagnosticCodeSchema = z.enum(workerDiagnosticCodes);

export const workerStatusSchema = z.object({
  type: z.literal("status"),
  requestId: z.string(),
  phase: z.string(),
});

export const workerTimingsSchema = z.object({
  moduleMs: z.number().optional(),
  runtimeDataMs: z.number().optional(),
  runtimeDataFiles: z.number().optional(),
  solveMs: z.number().optional(),
  wallMs: z.number().optional(),
});

export const workerDiagnosticSchema = z.object({
  code: workerDiagnosticCodeSchema.or(z.string()),
  message: z.string().optional(),
  details: z.unknown().optional(),
});

export const workerResultSchema = z.object({
  type: z.union([z.literal("success"), z.literal("failure")]),
  requestId: z.string(),
  exitCode: z.number().nullable().optional(),
  stdout: z.string().optional(),
  stderr: z.string().optional(),
  parsedJson: z.unknown().optional(),
  parseError: workerDiagnosticSchema.optional(),
  timings: workerTimingsSchema.optional(),
  error: workerDiagnosticSchema.optional(),
});

export const workerPrewarmResultSchema = z.object({
  type: z.union([z.literal("prewarm-ready"), z.literal("prewarm-failure")]),
  requestId: z.string(),
  version: z.string().optional(),
  timings: workerTimingsSchema.optional(),
  error: workerDiagnosticSchema.optional(),
});

export type WorkerStatusMessage = z.infer<typeof workerStatusSchema>;
export type WorkerResultMessage = z.infer<typeof workerResultSchema>;
export type WorkerPrewarmResultMessage = z.infer<typeof workerPrewarmResultSchema>;
export type WorkerDiagnosticCode = z.infer<typeof workerDiagnosticCodeSchema>;
export type WorkerTimings = z.infer<typeof workerTimingsSchema>;

export type WorkerRunResult = WorkerResultMessage & {
  stdout: string;
  stderr: string;
};

export type WorkerLintResult = WorkerResultMessage & {
  stdout: string;
  stderr: string;
};
