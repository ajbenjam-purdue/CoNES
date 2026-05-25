import { useEffect, useMemo, useState } from "react";
import {
  AlertTriangle,
  CheckCircle2,
  Clock3,
  Database,
  FileCode2,
  HardDrive,
  type LucideIcon,
  RefreshCcw,
  ShieldCheck,
} from "lucide-react";
import { Alert, AlertDescription, AlertTitle } from "@/components/ui/alert";
import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import { Separator } from "@/components/ui/separator";
import { readRuntimeLastRun, runtimeLastRunEvent, type RuntimeLastRunSummary } from "@/features/runtime/runtime-last-run";
import {
  getRuntimePrewarmState,
  startRuntimePrewarm,
  subscribeRuntimePrewarm,
  type RuntimePrewarmState,
} from "@/features/runtime/runtime-prewarm";
import {
  formatBytes,
  normalizeArtifacts,
  normalizeManifests,
  runtimeContract,
  runtimeMetadataUrl,
  shortHash,
  type RuntimeManifestRecord,
  type RuntimeMetadataState,
} from "@/features/tools/runtime-metadata";

export function RuntimeStatusPage() {
  const [state, setState] = useState<RuntimeMetadataState>({
    status: "loading",
    metadata: null,
    error: null,
  });
  const [lastRun, setLastRun] = useState<RuntimeLastRunSummary | null>(() => readRuntimeLastRun());
  const [prewarmState, setPrewarmState] = useState<RuntimePrewarmState>(() => getRuntimePrewarmState());
  const [reloadToken, setReloadToken] = useState(0);

  useEffect(() => {
    let isMounted = true;

    setState({ status: "loading", metadata: null, error: null });
    fetch(runtimeMetadataUrl, { cache: "no-store" })
      .then(async (response) => {
        if (!response.ok) {
          throw new Error(`${response.status} ${response.statusText || "metadata unavailable"}`);
        }
        return response.json();
      })
      .then((metadata) => {
        if (isMounted) {
          setState({ status: "ready", metadata, error: null });
        }
      })
      .catch((error) => {
        if (!isMounted) {
          return;
        }
        const message = error instanceof Error ? error.message : String(error);
        setState({
          status: message.includes("404") ? "missing" : "invalid",
          metadata: null,
          error: message,
        });
      });

    return () => {
      isMounted = false;
    };
  }, [reloadToken]);

  useEffect(() => {
    function handleLastRunUpdate() {
      setLastRun(readRuntimeLastRun());
    }

    window.addEventListener(runtimeLastRunEvent, handleLastRunUpdate);
    window.addEventListener("storage", handleLastRunUpdate);
    return () => {
      window.removeEventListener(runtimeLastRunEvent, handleLastRunUpdate);
      window.removeEventListener("storage", handleLastRunUpdate);
    };
  }, []);

  useEffect(() => {
    return subscribeRuntimePrewarm(setPrewarmState);
  }, []);

  const contract = useMemo(() => runtimeContract(state.metadata), [state.metadata]);
  const artifacts = useMemo(() => normalizeArtifacts(state.metadata), [state.metadata]);
  const manifests = useMemo(() => normalizeManifests(state.metadata), [state.metadata]);

  return (
    <section className="flex min-h-[calc(100svh-4rem)] flex-col" data-runtime-status-page>
      <div className="flex items-center justify-between gap-4 border-b px-6 py-4 max-sm:flex-col max-sm:items-start">
        <div className="min-w-0">
          <p className="text-xs font-semibold uppercase text-muted-foreground">Tools</p>
          <h2 className="text-lg font-semibold tracking-normal">Runtime status</h2>
        </div>
        <Button type="button" variant="outline" onClick={() => setReloadToken((value) => value + 1)}>
          <RefreshCcw data-icon="inline-start" aria-hidden="true" />
          Refresh
        </Button>
      </div>

      <div className="grid flex-1 grid-cols-[minmax(0,1fr)_360px] max-xl:grid-cols-1">
        <div className="flex min-w-0 flex-col gap-5 p-6">
          <RuntimePrewarmAlert state={prewarmState} />
          <RuntimeStateAlert state={state} />

          <section className="rounded-md border bg-background" aria-labelledby="contract-title">
            <div className="flex items-center justify-between gap-3 border-b p-4">
              <div>
                <h3 id="contract-title" className="font-semibold tracking-normal">Worker contract</h3>
                <p className="text-sm text-muted-foreground">The accepted invocation shape Packet 004 must preserve.</p>
              </div>
              <Badge variant="secondary">
                <ShieldCheck aria-hidden="true" />
                Client only
              </Badge>
            </div>
            <div className="grid gap-0 sm:grid-cols-2">
              <StatusRow label="Worker path" value={contract.workerPath} />
              <StatusRow label="Module script" value={contract.moduleScriptPath} />
              <StatusRow label="WASM module" value={contract.wasmPath} />
              <StatusRow label="thisProgram" value={contract.thisProgram} />
              <StatusRow label="Input path" value={contract.inputPath} />
              <StatusRow label="callMain args" value={JSON.stringify(contract.callMainArgs)} />
            </div>
            <Separator />
            <div className="grid gap-2 p-4 text-sm">
              <span className="font-medium">Runtime-data manifests</span>
              <div className="flex flex-col gap-2">
                {contract.runtimeDataManifests.map((path) => (
                  <code key={path} className="break-all rounded-md bg-muted px-2 py-1 text-xs">{path}</code>
                ))}
              </div>
            </div>
          </section>

          <section className="rounded-md border bg-background" aria-labelledby="artifact-title">
            <div className="border-b p-4">
              <h3 id="artifact-title" className="font-semibold tracking-normal">Runtime artifacts</h3>
              <p className="text-sm text-muted-foreground">Sizes and sha256 values from runtime metadata.</p>
            </div>
            <div className="overflow-x-auto">
              <table className="w-full min-w-[680px] text-sm">
                <thead className="bg-muted/50 text-left text-xs uppercase text-muted-foreground">
                  <tr>
                    <th className="px-4 py-3 font-semibold">Artifact</th>
                    <th className="px-4 py-3 font-semibold">Path</th>
                    <th className="px-4 py-3 font-semibold">Size</th>
                    <th className="px-4 py-3 font-semibold">sha256</th>
                  </tr>
                </thead>
                <tbody>
                  {artifacts.length ? (
                    artifacts.map((artifact) => (
                      <tr key={`${artifact.label}-${artifact.path}`} className="border-t">
                        <td className="px-4 py-3 font-medium">{artifact.label}</td>
                        <td className="px-4 py-3"><code className="break-all text-xs">{artifact.path}</code></td>
                        <td className="px-4 py-3">{formatBytes(artifact.bytes)}</td>
                        <td className="px-4 py-3"><code className="text-xs">{shortHash(artifact.sha256)}</code></td>
                      </tr>
                    ))
                  ) : (
                    <EmptyTableRow columns={4} message="No artifact metadata available." />
                  )}
                </tbody>
              </table>
            </div>
          </section>

          <section className="rounded-md border bg-background" aria-labelledby="manifest-title">
            <div className="border-b p-4">
              <h3 id="manifest-title" className="font-semibold tracking-normal">Manifest summaries</h3>
              <p className="text-sm text-muted-foreground">Runtime-data counts, mount roots, byte totals, and manifest checksums.</p>
            </div>
            <div className="grid gap-0 md:grid-cols-2">
              {manifests.length ? (
                manifests.map((manifest) => <ManifestPanel key={`${manifest.label}-${manifest.path}`} manifest={manifest} />)
              ) : (
                <div className="p-4 text-sm text-muted-foreground">No manifest metadata available.</div>
              )}
            </div>
          </section>
        </div>

        <aside className="border-l bg-muted/30 max-xl:border-l-0 max-xl:border-t">
          <div className="grid grid-cols-2 border-b max-sm:grid-cols-1">
            <MetaCell label="Runtime" value={prewarmState.label} dataAttr="prewarm" />
            <MetaCell label="Metadata" value={state.status} />
            <MetaCell label="Schema" value={state.metadata?.schemaVersion ? `v${state.metadata.schemaVersion}` : "Unavailable"} />
            <MetaCell label="Project" value={state.metadata?.project?.name || "Unavailable"} />
            <MetaCell label="Version" value={state.metadata?.project?.version || "Unavailable"} />
          </div>

          <div className="flex flex-col gap-4 p-4 text-sm">
            <div className="flex items-start gap-2">
              <Clock3 className="text-muted-foreground" aria-hidden="true" />
              <div>
                <h3 className="font-semibold tracking-normal">Last-run timings</h3>
                <p className="text-muted-foreground">
                  {lastRun ? `${lastRun.fileName} completed as ${lastRun.status}.` : "Run a .cnes file to populate timings."}
                </p>
              </div>
            </div>

            <div className="grid gap-2">
              <DetailRow icon={Clock3} label="prewarm module" value={formatMs(prewarmState.timings?.moduleMs)} />
              <DetailRow icon={Database} label="prewarm runtime-data" value={formatMs(prewarmState.timings?.runtimeDataMs)} />
              <DetailRow icon={HardDrive} label="prewarm wall" value={formatMs(prewarmState.timings?.wallMs)} />
              <DetailRow icon={Clock3} label="module load" value={formatMs(lastRun?.timings?.moduleMs)} />
              <DetailRow icon={Database} label="runtime-data mount" value={formatMs(lastRun?.timings?.runtimeDataMs)} />
              <DetailRow icon={FileCode2} label="solve" value={formatMs(lastRun?.timings?.solveMs)} />
              <DetailRow icon={HardDrive} label="wall time" value={formatMs(lastRun?.timings?.wallMs)} />
            </div>

            <Separator />

            <div className="grid gap-2">
              <span className="text-xs font-semibold uppercase text-muted-foreground">Build command</span>
              <code className="max-h-44 overflow-auto whitespace-pre-wrap rounded-md bg-background p-3 text-xs">
                {state.metadata?.build?.command || state.metadata?.build?.wasmCommand || "Unavailable"}
              </code>
            </div>

            <Alert>
              <ShieldCheck aria-hidden="true" />
              <AlertTitle>Runtime boundary</AlertTitle>
              <AlertDescription>
                This page reads static browser assets only. It does not run CoNES or change worker behavior.
              </AlertDescription>
            </Alert>
          </div>
        </aside>
      </div>
    </section>
  );
}

function RuntimePrewarmAlert({ state }: { state: RuntimePrewarmState }) {
  if (state.status === "ready") {
    return (
      <Alert data-runtime-ready-state="ready">
        <CheckCircle2 aria-hidden="true" />
        <AlertTitle>Runtime ready before run</AlertTitle>
        <AlertDescription>
          The Worker has loaded WASM, verified runtime-data, and mounted assets. Run should go straight to solve work.
        </AlertDescription>
      </Alert>
    );
  }

  if (state.status === "failed") {
    return (
      <Alert variant="destructive" data-runtime-ready-state="failed">
        <AlertTriangle aria-hidden="true" />
        <AlertTitle>Runtime prewarm failed</AlertTitle>
        <AlertDescription>{state.errorMessage || state.errorCode || "The runtime could not prepare in the background."}</AlertDescription>
      </Alert>
    );
  }

  return (
    <Alert data-runtime-ready-state={state.status}>
      <RefreshCcw aria-hidden="true" />
      <AlertTitle>{state.status === "idle" ? "Runtime prewarm not started" : state.label}</AlertTitle>
      <AlertDescription>
        The app prepares the Worker and runtime-data in the background before file selection.
        {state.status === "idle" ? (
          <Button type="button" variant="link" className="h-auto px-1 py-0" onClick={startRuntimePrewarm}>
            Start now
          </Button>
        ) : null}
      </AlertDescription>
    </Alert>
  );
}

function RuntimeStateAlert({ state }: { state: RuntimeMetadataState }) {
  if (state.status === "ready") {
    return (
      <Alert data-runtime-metadata-state="ready">
        <CheckCircle2 aria-hidden="true" />
        <AlertTitle>Runtime metadata loaded</AlertTitle>
        <AlertDescription>
          Read <code>{runtimeMetadataUrl}</code>. Artifact and manifest checksums below come from that file.
        </AlertDescription>
      </Alert>
    );
  }

  if (state.status === "loading") {
    return (
      <Alert data-runtime-metadata-state="loading">
        <RefreshCcw aria-hidden="true" />
        <AlertTitle>Loading runtime metadata</AlertTitle>
        <AlertDescription>Reading <code>{runtimeMetadataUrl}</code>.</AlertDescription>
      </Alert>
    );
  }

  return (
    <Alert variant="destructive" data-runtime-metadata-state={state.status}>
      <AlertTriangle aria-hidden="true" />
      <AlertTitle>{state.status === "missing" ? "Runtime metadata missing" : "Runtime metadata unreadable"}</AlertTitle>
      <AlertDescription>
        Could not read <code>{runtimeMetadataUrl}</code>: {state.error}. The accepted defaults are shown where possible.
      </AlertDescription>
    </Alert>
  );
}

function StatusRow({ label, value }: { label: string; value: string }) {
  return (
    <div className="min-w-0 border-b border-r p-4 even:border-r-0">
      <span className="text-xs font-semibold uppercase text-muted-foreground">{label}</span>
      <code className="mt-1 block break-all text-sm">{value}</code>
    </div>
  );
}

function MetaCell({ label, value, dataAttr }: { label: string; value: string; dataAttr?: "prewarm" }) {
  const dataProps = dataAttr === "prewarm" ? { "data-runtime-prewarm-label": true } : {};

  return (
    <div className="flex min-h-20 flex-col justify-center gap-1 border-b border-r p-4 even:border-r-0">
      <span className="text-xs font-semibold uppercase text-muted-foreground">{label}</span>
      <strong className="break-words text-sm" {...dataProps}>{value}</strong>
    </div>
  );
}

function DetailRow({ icon: Icon, label, value }: { icon: LucideIcon; label: string; value: string }) {
  return (
    <div className="flex items-start justify-between gap-3">
      <span className="flex items-center gap-2 text-muted-foreground">
        <Icon aria-hidden="true" />
        {label}
      </span>
      <Badge variant="secondary">{value}</Badge>
    </div>
  );
}

function ManifestPanel({ manifest }: { manifest: RuntimeManifestRecord }) {
  return (
    <article className="border-b border-r p-4 even:border-r-0">
      <div className="mb-3 flex items-center justify-between gap-3">
        <h4 className="font-semibold tracking-normal">{manifest.label}</h4>
        <Badge variant="outline">{manifest.integrity || "unknown"}</Badge>
      </div>
      <div className="grid gap-2 text-sm">
        <StatusLine label="Path" value={manifest.path || "Unavailable"} code />
        <StatusLine label="Kind" value={manifest.kind || "Unavailable"} />
        <StatusLine label="Mount root" value={manifest.mountRoot || "Unavailable"} code />
        <StatusLine label="Files" value={String(manifest.count ?? "Unavailable")} />
        <StatusLine label="Total bytes" value={formatBytes(manifest.totalBytes)} />
        <StatusLine label="Manifest sha256" value={shortHash(manifest.manifestSha256 || manifest.sha256)} code />
        <StatusLine
          label="Per-file sha256"
          value={manifest.perFileSha256?.complete ? "Complete" : "Unavailable"}
        />
      </div>
    </article>
  );
}

function StatusLine({ label, value, code = false }: { label: string; value: string; code?: boolean }) {
  return (
    <div className="flex items-start justify-between gap-3">
      <span className="text-muted-foreground">{label}</span>
      {code ? (
        <code className="max-w-[220px] break-all text-right text-xs">{value}</code>
      ) : (
        <span className="max-w-[220px] break-words text-right font-medium">{value}</span>
      )}
    </div>
  );
}

function EmptyTableRow({ columns, message }: { columns: number; message: string }) {
  return (
    <tr className="border-t">
      <td className="px-4 py-8 text-center text-muted-foreground" colSpan={columns}>{message}</td>
    </tr>
  );
}

function formatMs(value: number | undefined) {
  return value !== undefined && Number.isFinite(value) ? `${Math.round(value)} ms` : "Unavailable";
}
