import { useEffect, useState } from "react";
import { AlertCircle, ChevronRight, Terminal } from "lucide-react";
import { Badge } from "@/components/ui/badge";
import { ScrollArea } from "@/components/ui/scroll-area";
import { formatBytes } from "@/features/runner/runner-state";
import { cn } from "@/lib/utils";

type RunnerOutputProps = {
  stdout: string;
  stderr: string;
  hasFailure?: boolean;
};

export function RunnerOutput({ stdout, stderr, hasFailure = false }: RunnerOutputProps) {
  const [isOpen, setIsOpen] = useState(false);
  const stdoutBytes = formatBytes(new TextEncoder().encode(stdout).byteLength);
  const stderrBytes = formatBytes(new TextEncoder().encode(stderr).byteLength);
  const hasDiagnostics = stderr.trim().length > 0;
  const shouldEmphasizeDiagnostics = hasDiagnostics || hasFailure;

  useEffect(() => {
    if (shouldEmphasizeDiagnostics) {
      setIsOpen(true);
    }
  }, [shouldEmphasizeDiagnostics]);

  return (
    <details
      open={isOpen}
      onToggle={(event) => setIsOpen(event.currentTarget.open)}
      className="group border-t bg-muted/20"
      data-raw-output-section
      data-diagnostics-active={shouldEmphasizeDiagnostics ? "true" : "false"}
    >
      <summary className="flex cursor-pointer list-none items-center justify-between gap-3 border-b px-4 py-3 marker:hidden">
        <span className="flex min-w-0 items-center gap-2">
          <ChevronRight className="text-muted-foreground transition-transform group-open:rotate-90" aria-hidden="true" />
          <Terminal className="text-muted-foreground" aria-hidden="true" />
          <span className="truncate text-sm font-semibold tracking-normal">Raw output</span>
          {shouldEmphasizeDiagnostics ? (
            <Badge variant={hasDiagnostics ? "destructive" : "outline"} data-diagnostics-indicator>
              Diagnostics
            </Badge>
          ) : null}
        </span>
        <span className="flex shrink-0 items-center gap-3 text-xs font-medium text-muted-foreground">
          <span data-stdout-bytes>{stdoutBytes}</span>
          <span data-stderr-bytes>{stderrBytes}</span>
        </span>
      </summary>

      <div className="grid min-h-0 grid-cols-2 max-lg:grid-cols-1">
        <TerminalPane dataAttribute="stdout" label="Output" value={stdout} />
        <TerminalPane
          dataAttribute="stderr"
          label="Diagnostics"
          value={stderr}
          emphasized={shouldEmphasizeDiagnostics}
        />
      </div>
    </details>
  );
}

function TerminalPane({
  dataAttribute,
  emphasized = false,
  label,
  value,
}: {
  dataAttribute: "stdout" | "stderr";
  emphasized?: boolean;
  label: string;
  value: string;
}) {
  const dataProps = dataAttribute === "stdout" ? { "data-stdout-output": true } : { "data-stderr-output": true };
  const emptyLabel = dataAttribute === "stderr" ? "No diagnostics." : "No output.";

  return (
    <section className={cn("min-w-0 border-r last:border-r-0 max-lg:border-r-0 max-lg:border-t", emphasized && "bg-destructive/5")}>
      <div className="flex items-center justify-between gap-2 border-b px-4 py-2">
        <h3 className="text-xs font-semibold uppercase text-muted-foreground">{label}</h3>
        {emphasized ? <AlertCircle className="text-destructive" aria-hidden="true" /> : null}
      </div>
      <ScrollArea className="h-[min(32svh,320px)] min-h-[220px]">
      <pre
        tabIndex={0}
        className={cn(
          "min-h-full whitespace-pre-wrap bg-code px-4 py-4 font-mono text-xs leading-5 text-code-foreground",
          !value && "text-code-foreground/55",
        )}
        {...dataProps}
      >
        {value}
      </pre>
      {!value ? <p className="px-4 pb-4 font-mono text-xs text-code-foreground/55">{emptyLabel}</p> : null}
      </ScrollArea>
    </section>
  );
}
