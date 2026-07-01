import {
  CheckCircle2,
  Cloud,
  Code2,
  FileCode2,
  FileJson2,
  LockKeyhole,
  PlaySquare,
  ServerOff,
  ShieldCheck,
  Terminal,
} from "lucide-react";
import { Alert, AlertDescription, AlertTitle } from "@/components/ui/alert";
import { Separator } from "@/components/ui/separator";

const reviewUrl = "https://cones.dev/runner/";

const workflowSteps = [
  {
    label: "Edit",
    detail: "Work in the Monaco source editor.",
    icon: FileCode2,
  },
  {
    label: "Prewarm",
    detail: "WASM and runtime-data load before the user clicks Run.",
    icon: ShieldCheck,
  },
  {
    label: "Run",
    detail: "The Worker runs the current buffer with --json.",
    icon: PlaySquare,
  },
  {
    label: "Export",
    detail: "stdout JSON remains downloadable and inspectable.",
    icon: FileJson2,
  },
];

const runtimeFacts = [
  ["Execution", "Browser Worker only"],
  ["Worker path", "/runtime/cnes-worker.js"],
  ["Runtime data", "/runtime-data/materials and /runtime-data/libs"],
  ["Program", 'thisProgram: "/app/cnes"'],
  ["Arguments", 'callMain(["/work/input.cnes", "--json"])'],
  ["Review URL", reviewUrl],
];

const guardrails = [
  "No backend solve API, Railway queue, Redis worker, auth layer, or server actions.",
  "Do not rewrite the CoNES C++ solver for the browser surface.",
  "Preserve raw stdout, stderr, exit code, JSON download, and worker diagnostics.",
  "Treat Tools runtime status as the place to verify assets, manifests, checksums, and prewarm readiness.",
];

export function DocsPage() {
  return (
    <section className="flex min-h-[calc(100svh-4rem)] flex-col" data-docs-page>
      <div className="border-b px-6 py-5">
        <div className="mt-4 max-w-4xl">
          <h2 className="text-2xl font-semibold tracking-normal">Docs</h2>
        </div>
      </div>

      <div className="grid flex-1 grid-cols-[minmax(0,1fr)_340px] max-xl:grid-cols-1">
        <div className="flex min-w-0 flex-col gap-6 p-6">
          <Alert>
            <CheckCircle2 aria-hidden="true" />
            <AlertTitle>Current posture</AlertTitle>
            <AlertDescription>
              CoNES runs locally in the browser. Vercel serves static assets, and the Worker executes the existing CLI-shaped
              runtime with <code>--json</code>.
            </AlertDescription>
          </Alert>

          <section aria-labelledby="workflow-title" className="grid gap-4">
            <div>
              <h3 id="workflow-title" className="text-base font-semibold tracking-normal">Run workflow</h3>
            </div>
            <div className="grid gap-0 overflow-hidden rounded-md border bg-background md:grid-cols-4">
              {workflowSteps.map((step, index) => {
                const Icon = step.icon;
                return (
                  <article key={step.label} className="min-h-36 border-b p-4 md:border-b-0 md:border-r md:last:border-r-0">
                    <div className="flex items-center justify-between gap-3">
                      <Icon className="text-primary" aria-hidden="true" />
                      <span className="text-xs font-semibold text-muted-foreground">{String(index + 1).padStart(2, "0")}</span>
                    </div>
                    <h4 className="mt-5 font-semibold tracking-normal">{step.label}</h4>
                    <p className="mt-2 text-sm leading-5 text-muted-foreground">{step.detail}</p>
                  </article>
                );
              })}
            </div>
          </section>

          <section aria-labelledby="runtime-title" className="grid gap-4">
            <div>
              <h3 id="runtime-title" className="text-base font-semibold tracking-normal">Runtime facts</h3>
            </div>
            <div className="overflow-x-auto rounded-md border bg-background">
              <table className="w-full min-w-[680px] text-sm">
                <thead className="bg-muted/50 text-left text-xs uppercase text-muted-foreground">
                  <tr>
                    <th className="px-4 py-3 font-semibold">Surface</th>
                    <th className="px-4 py-3 font-semibold">Current value</th>
                  </tr>
                </thead>
                <tbody>
                  {runtimeFacts.map(([label, value]) => (
                    <tr key={label} className="border-t">
                      <td className="w-48 px-4 py-3 font-medium">{label}</td>
                      <td className="px-4 py-3">
                        {value === reviewUrl ? (
                          <a className="break-all font-medium text-primary underline-offset-4 hover:underline" href={reviewUrl}>
                            {reviewUrl}
                          </a>
                        ) : (
                          <code className="break-all text-xs">{value}</code>
                        )}
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </section>

          <section aria-labelledby="guardrails-title" className="grid gap-4">
            <div>
              <h3 id="guardrails-title" className="text-base font-semibold tracking-normal">Guardrails</h3>
            </div>
            <div className="grid gap-0 rounded-md border bg-background">
              {guardrails.map((item) => (
                <div key={item} className="flex gap-3 border-b p-4 last:border-b-0">
                  <LockKeyhole className="mt-0.5 shrink-0 text-muted-foreground" aria-hidden="true" />
                  <p className="text-sm leading-6">{item}</p>
                </div>
              ))}
            </div>
          </section>
        </div>

        <aside className="border-l bg-muted/30 max-xl:border-l-0 max-xl:border-t">
          <div className="grid grid-cols-2 border-b max-sm:grid-cols-1">
            <MetaCell icon={ServerOff} label="Solve API" value="None" />
            <MetaCell icon={Cloud} label="Deploy" value="Vercel static" />
            <MetaCell icon={Terminal} label="Output" value="stdout JSON" />
            <MetaCell icon={FileCode2} label="Input" value="Workbench buffer" />
          </div>

          <div className="flex flex-col gap-4 p-4 text-sm">
            <div>
              <h3 className="font-semibold tracking-normal">Where to verify</h3>
              <p className="mt-1 text-muted-foreground">
                Workbench verifies edit, run, and output behavior. Tools verifies runtime metadata, checksums, and prewarm readiness.
              </p>
            </div>

            <Separator />

            <div className="grid gap-2">
              <span className="text-xs font-semibold uppercase text-muted-foreground">Expected commands</span>
              <code className="whitespace-pre-wrap rounded-md bg-background p-3 text-xs">{`npm run verify:runtime-metadata
npm run verify:runtime-data
npm run build
npm run verify:browser`}</code>
            </div>

            <Alert>
              <Code2 aria-hidden="true" />
              <AlertTitle>Docs boundary</AlertTitle>
              <AlertDescription>
                This page documents current behavior only. It does not call the Worker, fetch manifests, or change runtime state.
              </AlertDescription>
            </Alert>
          </div>
        </aside>
      </div>
    </section>
  );
}

function MetaCell({ icon: Icon, label, value }: { icon: typeof ServerOff; label: string; value: string }) {
  return (
    <div className="flex min-h-24 flex-col justify-center gap-2 border-b border-r p-4 even:border-r-0">
      <div className="flex items-center gap-2 text-xs font-semibold uppercase text-muted-foreground">
        <Icon aria-hidden="true" />
        {label}
      </div>
      <strong className="break-words text-sm">{value}</strong>
    </div>
  );
}
