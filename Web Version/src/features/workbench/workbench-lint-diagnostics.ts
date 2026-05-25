import type { WorkerLintResult } from "@/features/runtime/worker-contract";

export type LintDiagnostic = {
  severity: "error" | "warning" | "info";
  message: string;
  line?: number;
  source: "cnes-lint";
  raw?: string;
};

export function normalizeLintDiagnostics(result: WorkerLintResult): LintDiagnostic[] {
  if (isSuccessfulJsonResult(result.parsedJson)) {
    return [];
  }

  const parsedError = extractJsonError(result.parsedJson);
  const fallback = firstNonEmpty([
    result.stderr,
    result.stdout,
    result.error?.message,
    result.parseError?.message,
  ]);
  const raw = parsedError || fallback;

  if (!raw) {
    return [];
  }

  return splitDiagnosticText(raw).map((message) => {
    const line = extractLineNumber(message);
    return {
      severity: "error",
      message: cleanDiagnosticMessage(message),
      line,
      source: "cnes-lint",
      raw: message,
    };
  });
}

function isSuccessfulJsonResult(value: unknown) {
  if (!value || typeof value !== "object") {
    return false;
  }

  return (value as Record<string, unknown>).success === true;
}

function extractJsonError(value: unknown): string {
  if (!value || typeof value !== "object") {
    return "";
  }

  const record = value as Record<string, unknown>;
  if (typeof record.error === "string") {
    return record.error;
  }

  if (record.error && typeof record.error === "object") {
    const errorRecord = record.error as Record<string, unknown>;
    if (typeof errorRecord.message === "string") {
      return errorRecord.message;
    }
  }

  if (typeof record.message === "string" && record.success === false) {
    return record.message;
  }

  if (record.success === false) {
    return JSON.stringify(record);
  }

  return "";
}

function splitDiagnosticText(value: string) {
  const trimmed = value.trim();
  if (!trimmed) {
    return [];
  }

  const lineTagged = trimmed.match(/\[Line\s+\d+\][^\n\r]*/gi);
  if (lineTagged && lineTagged.length > 1) {
    return lineTagged;
  }

  return [trimmed];
}

function extractLineNumber(value: string) {
  const match = value.match(/\[Line\s+(\d+)\]/i);
  if (!match) {
    return undefined;
  }

  const line = Number(match[1]);
  return Number.isInteger(line) && line > 0 ? line : undefined;
}

function cleanDiagnosticMessage(value: string) {
  return value.replace(/\s+/g, " ").trim();
}

function firstNonEmpty(values: Array<string | undefined>) {
  return values.find((value) => value && value.trim().length > 0)?.trim() || "";
}
