import { AlertCircle, CheckCircle2, FileJson2, ListFilter, Terminal } from "lucide-react";
import { Alert, AlertDescription, AlertTitle } from "@/components/ui/alert";
import { Badge } from "@/components/ui/badge";
import { ScrollArea } from "@/components/ui/scroll-area";
import { Separator } from "@/components/ui/separator";

type ResultsViewerProps = {
  stdout: string;
  stderr: string;
  isRunning: boolean;
};

type CnesVariable = {
  name: string;
  value: unknown;
  unit: string;
  status: "fixed" | "solved" | "unknown";
};

type ResultsModel =
  | {
      state: "waiting" | "running";
      title: string;
      description: string;
      variables: CnesVariable[];
      performance: Record<string, unknown>;
      error: string | null;
      version: string | null;
    }
  | {
      state: "success" | "failed-json";
      title: string;
      description: string;
      variables: CnesVariable[];
      performance: Record<string, unknown>;
      error: string | null;
      version: string | null;
    }
  | {
      state: "invalid-json" | "runtime-error";
      title: string;
      description: string;
      variables: CnesVariable[];
      performance: Record<string, unknown>;
      error: string | null;
      version: string | null;
    };

export function ResultsViewer({ stdout, stderr, isRunning }: ResultsViewerProps) {
  const model = buildResultsModel(stdout, stderr, isRunning);

  return (
    <section className="border-b bg-background" data-results-viewer data-results-state={model.state}>
      <div className="flex items-center justify-between gap-3 border-b px-4 py-3">
        <div className="flex min-w-0 items-center gap-2">
          {model.state === "success" ? (
            <CheckCircle2 className="text-primary" aria-hidden="true" />
          ) : model.state === "failed-json" || model.state === "invalid-json" || model.state === "runtime-error" ? (
            <AlertCircle className="text-destructive" aria-hidden="true" />
          ) : (
            <FileJson2 className="text-muted-foreground" aria-hidden="true" />
          )}
          <div className="min-w-0">
            <h2 className="truncate text-sm font-semibold tracking-normal">{model.title}</h2>
            <p className="truncate text-xs text-muted-foreground">{model.description}</p>
          </div>
        </div>
        <div className="flex shrink-0 items-center gap-2">
          {model.version ? <Badge variant="secondary">{model.version}</Badge> : null}
          <Badge variant={model.state === "success" ? "default" : "outline"} data-results-variable-count>
            {model.variables.length} variables
          </Badge>
        </div>
      </div>

      {model.error ? (
        <Alert variant={model.state === "success" ? "default" : "destructive"} className="m-4" data-results-error-summary>
          <AlertCircle aria-hidden="true" />
          <AlertTitle>{model.state === "failed-json" ? "CoNES reported a JSON failure" : "Result summary"}</AlertTitle>
          <AlertDescription>{model.error}</AlertDescription>
        </Alert>
      ) : null}

      <div className="grid min-h-[260px] grid-cols-[minmax(0,1fr)_220px] max-lg:grid-cols-1">
        <ScrollArea className="h-[min(34svh,360px)] min-h-[260px]">
          {model.variables.length > 0 ? (
            <table className="w-full text-left text-sm">
              <thead className="sticky top-0 bg-muted text-xs uppercase text-muted-foreground">
                <tr>
                  <th className="px-4 py-2 font-semibold">Name</th>
                  <th className="px-4 py-2 font-semibold">Value</th>
                  <th className="px-4 py-2 font-semibold">Unit</th>
                  <th className="px-4 py-2 font-semibold">Status</th>
                </tr>
              </thead>
              <tbody>
                {model.variables.map((variable) => (
                  <tr
                    className="border-b last:border-b-0"
                    data-result-variable-row
                    data-variable-name={variable.name}
                    key={variable.name}
                  >
                    <td className="px-4 py-2 font-mono text-xs">{variable.name}</td>
                    <td className="px-4 py-2 font-mono text-xs" data-result-variable-value>
                      {formatValue(variable.value)}
                    </td>
                    <td className="px-4 py-2 text-muted-foreground" data-result-variable-unit>
                      {variable.unit || "-"}
                    </td>
                    <td className="px-4 py-2">
                      <Badge variant={variable.status === "solved" ? "default" : "secondary"} data-result-variable-status>
                        {variable.status}
                      </Badge>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          ) : (
            <div className="flex h-full min-h-[260px] flex-col justify-center gap-2 px-4 py-6 text-sm text-muted-foreground">
              <ListFilter aria-hidden="true" />
              <p data-results-empty>{model.state === "running" ? "Waiting for CoNES output." : "Run a .cnes file to populate variables."}</p>
            </div>
          )}
        </ScrollArea>

        <aside className="border-l p-4 text-sm max-lg:border-l-0 max-lg:border-t">
          <div className="flex items-center gap-2">
            <Terminal className="text-muted-foreground" aria-hidden="true" />
            <h3 className="font-semibold tracking-normal">JSON summary</h3>
          </div>
          <Separator className="my-3" />
          <dl className="flex flex-col gap-3">
            <SummaryRow label="state" value={model.state} />
            <SummaryRow label="success" value={model.state === "success" ? "true" : model.state === "failed-json" ? "false" : "-"} />
            {Object.entries(model.performance).map(([key, value]) => (
              <SummaryRow label={key} value={formatValue(value)} key={key} />
            ))}
          </dl>
        </aside>
      </div>
    </section>
  );
}

function buildResultsModel(stdout: string, stderr: string, isRunning: boolean): ResultsModel {
  const trimmedStdout = stdout.trim();
  const trimmedStderr = stderr.trim();

  if (!trimmedStdout && !trimmedStderr) {
    return {
      state: isRunning ? "running" : "waiting",
      title: isRunning ? "Running CoNES" : "Results viewer",
      description: isRunning ? "Structured results will appear when stdout arrives." : "Structured view of the existing --json stdout.",
      variables: [],
      performance: {},
      error: null,
      version: null,
    };
  }

  if (!trimmedStdout && trimmedStderr) {
    return {
      state: "runtime-error",
      title: "Runtime stderr",
      description: "No JSON stdout was available; raw stderr remains below.",
      variables: [],
      performance: {},
      error: trimmedStderr,
      version: null,
    };
  }

  const parsed = parseJsonObject(trimmedStdout);
  if (!parsed.ok) {
    return {
      state: "invalid-json",
      title: "JSON parse warning",
      description: "stdout was not valid JSON; inspect raw output below.",
      variables: [],
      performance: {},
      error: parsed.message,
      version: null,
    };
  }

  const success = parsed.value.success === true;
  const variables = parseVariables(parsed.value.variables);
  const version = typeof parsed.value.version === "string" ? parsed.value.version : null;
  const performance = parsePerformance(parsed.value.performance);
  const error = typeof parsed.value.error === "string" ? parsed.value.error : null;

  return {
    state: success ? "success" : "failed-json",
    title: success ? "Solve results" : "CoNES failure",
    description: success ? "Variables parsed from raw --json stdout." : "The solver returned JSON with success false.",
    variables,
    performance,
    error,
    version,
  };
}

function parseJsonObject(value: string): { ok: true; value: Record<string, unknown> } | { ok: false; message: string } {
  try {
    const parsed: unknown = JSON.parse(value);
    if (parsed && typeof parsed === "object" && !Array.isArray(parsed)) {
      return { ok: true, value: parsed as Record<string, unknown> };
    }
    return { ok: false, message: "stdout JSON was not an object." };
  } catch (error) {
    return { ok: false, message: error instanceof Error ? error.message : "stdout JSON could not be parsed." };
  }
}

function parseVariables(value: unknown): CnesVariable[] {
  if (!Array.isArray(value)) {
    return [];
  }

  return value.flatMap((entry) => {
    if (!entry || typeof entry !== "object" || Array.isArray(entry)) {
      return [];
    }

    const record = entry as Record<string, unknown>;
    if (typeof record.name !== "string" || !record.name) {
      return [];
    }

    return [
      {
        name: record.name,
        value: record.value,
        unit: typeof record.unit === "string" ? record.unit : "",
        status: typeof record.is_fixed === "boolean" ? (record.is_fixed ? "fixed" : "solved") : "unknown",
      },
    ];
  });
}

function parsePerformance(value: unknown): Record<string, unknown> {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    return {};
  }

  return Object.fromEntries(
    Object.entries(value as Record<string, unknown>).filter(([, entryValue]) => {
      const entryType = typeof entryValue;
      return entryType === "number" || entryType === "string" || entryType === "boolean";
    }),
  );
}

function SummaryRow({ label, value }: { label: string; value: string }) {
  return (
    <div className="flex items-start justify-between gap-3">
      <dt className="text-muted-foreground">{label}</dt>
      <dd className="max-w-[120px] truncate font-mono text-xs">{value}</dd>
    </div>
  );
}

function formatValue(value: unknown) {
  if (typeof value === "number") {
    return Number.isFinite(value) ? value.toLocaleString(undefined, { maximumFractionDigits: 10 }) : String(value);
  }

  if (typeof value === "string") {
    return value || "-";
  }

  if (typeof value === "boolean") {
    return String(value);
  }

  if (value === null || value === undefined) {
    return "-";
  }

  return JSON.stringify(value);
}
