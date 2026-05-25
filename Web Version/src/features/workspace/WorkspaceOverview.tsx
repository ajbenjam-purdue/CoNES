import {
  BookOpen,
  Braces,
  CircleDot,
  FileJson2,
  ListChecks,
  PanelsTopLeft,
  type LucideIcon,
} from "lucide-react";
import { Badge } from "@/components/ui/badge";
import { Separator } from "@/components/ui/separator";

type WorkspaceLane = {
  label: string;
  status: string;
  owner: string;
  detail: string;
  icon: LucideIcon;
};

const lanes: WorkspaceLane[] = [
  {
    label: "Workbench",
    status: "Operational",
    owner: "ide",
    detail: "Edit source in Monaco, run the current buffer, inspect raw and structured output.",
    icon: PanelsTopLeft,
  },
  {
    label: "Results",
    status: "Operational",
    owner: "output",
    detail: "Structure existing --json output without replacing raw artifacts.",
    icon: FileJson2,
  },
  {
    label: "Linter",
    status: "Deferred",
    owner: "diagnostics",
    detail: "Wait for stable results and editor surfaces before lint UI work.",
    icon: ListChecks,
  },
  {
    label: "Docs",
    status: "Integrated",
    owner: "guidance",
    detail: "Keep local docs close to examples, runtime limits, and exports.",
    icon: BookOpen,
  },
];

const guardrails = [
  "Client-side execution only",
  "No CoNES core rewrite",
  "Raw solver output remains inspectable",
  "Static Vercel deployment",
];

const workflow = ["Edit .cnes", "Run", "Inspect results", "Save source or JSON"];

export function WorkspaceOverview() {
  return (
    <section className="flex min-h-[calc(100svh-4rem)] flex-col gap-5 p-5 text-sm">
      <header className="flex flex-wrap items-start justify-between gap-4">
        <div className="min-w-0">
          <h2 className="text-2xl font-semibold tracking-normal">IDE workspace overview</h2>
        </div>
        <div className="grid min-w-48 gap-1 text-xs text-muted-foreground">
          <span>Runtime: /runtime</span>
          <span>Data: /runtime-data</span>
          <span>Program: /app/cnes</span>
        </div>
      </header>

      <Separator />

      <div className="grid gap-5 xl:grid-cols-[minmax(0,1fr)_320px]">
        <div className="min-w-0 overflow-hidden rounded-md border">
          <div className="grid grid-cols-[44px_140px_120px_120px_minmax(220px,1fr)] border-b bg-muted/40 px-4 py-3 text-xs font-semibold uppercase text-muted-foreground max-lg:hidden">
            <span aria-hidden="true" />
            <span>Lane</span>
            <span>Status</span>
            <span>Owner</span>
            <span>Target</span>
          </div>

          <div className="divide-y">
            {lanes.map((lane) => {
              const Icon = lane.icon;

              return (
                <article
                  key={lane.label}
                  className="grid grid-cols-[44px_140px_120px_120px_minmax(220px,1fr)] items-center gap-0 px-4 py-3 max-lg:grid-cols-[36px_minmax(0,1fr)] max-lg:gap-x-3 max-lg:gap-y-1"
                >
                  <div className="flex size-9 items-center justify-center rounded-md bg-muted text-muted-foreground">
                    <Icon aria-hidden="true" />
                  </div>
                  <h3 className="font-medium tracking-normal max-lg:col-start-2">{lane.label}</h3>
                  <div className="max-lg:col-start-2">
                    <Badge variant={lane.status === "Operational" ? "default" : "outline"}>{lane.status}</Badge>
                  </div>
                  <span className="font-mono text-xs text-muted-foreground max-lg:col-start-2">{lane.owner}</span>
                  <p className="text-muted-foreground max-lg:col-start-2">{lane.detail}</p>
                </article>
              );
            })}
          </div>
        </div>

        <aside className="grid content-start gap-5">
          <section className="rounded-md border p-4">
            <div className="mb-3 flex items-center gap-2">
              <Braces className="text-muted-foreground" aria-hidden="true" />
              <h3 className="font-semibold tracking-normal">Guardrails</h3>
            </div>
            <div className="grid gap-2">
              {guardrails.map((guardrail) => (
                <div key={guardrail} className="flex items-center gap-2 text-muted-foreground">
                  <CircleDot className="size-3 text-primary" aria-hidden="true" />
                  <span>{guardrail}</span>
                </div>
              ))}
            </div>
          </section>

          <section className="rounded-md border p-4">
            <div className="mb-3 flex items-center gap-2">
              <PanelsTopLeft className="text-muted-foreground" aria-hidden="true" />
              <h3 className="font-semibold tracking-normal">Current workflow</h3>
            </div>
            <ol className="grid gap-3">
              {workflow.map((step, index) => (
                <li key={step} className="grid grid-cols-[28px_minmax(0,1fr)] items-center gap-2">
                  <span className="flex size-7 items-center justify-center rounded-md bg-muted font-mono text-xs">
                    {index + 1}
                  </span>
                  <span className="text-muted-foreground">{step}</span>
                </li>
              ))}
            </ol>
          </section>
        </aside>
      </div>
    </section>
  );
}
