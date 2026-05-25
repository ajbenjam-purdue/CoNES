import { useEffect, useMemo, useRef, useState } from "react";
import { AlertCircle, Download, FileCode2, FilePlus2, Save, Upload } from "lucide-react";
import { Alert, AlertDescription, AlertTitle } from "@/components/ui/alert";
import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import { Separator } from "@/components/ui/separator";
import { cn } from "@/lib/utils";

type EditorFileMeta = {
  name: string;
  size: number;
  lastModified: number | null;
};

const initialText = "";
const defaultDownloadName = "untitled.cnes";

export function EditorShellPage() {
  const inputRef = useRef<HTMLInputElement | null>(null);
  const [text, setText] = useState(initialText);
  const [loadedText, setLoadedText] = useState(initialText);
  const [fileMeta, setFileMeta] = useState<EditorFileMeta | null>(null);
  const [status, setStatus] = useState("No file loaded");
  const [error, setError] = useState<string | null>(null);
  const [downloadUrl, setDownloadUrl] = useState<string | null>(null);

  const dirty = text !== loadedText;
  const stats = useMemo(() => buildTextStats(text), [text]);
  const downloadName = useMemo(() => buildDownloadName(fileMeta?.name), [fileMeta?.name]);

  useEffect(() => {
    const blob = new Blob([text], { type: "text/plain;charset=utf-8" });
    const url = URL.createObjectURL(blob);
    setDownloadUrl(url);

    return () => {
      URL.revokeObjectURL(url);
    };
  }, [text]);

  async function handleFileChange(file: File | null) {
    setError(null);

    if (!file) {
      return;
    }

    if (!file.name.toLowerCase().endsWith(".cnes")) {
      setError(`${file.name} is not a .cnes file`);
      clearInputSelection();
      return;
    }

    try {
      const nextText = await file.text();
      setText(nextText);
      setLoadedText(nextText);
      setFileMeta({
        name: file.name,
        size: file.size,
        lastModified: file.lastModified || null,
      });
      setStatus("Loaded");
    } catch {
      setError(`Could not read ${file.name}`);
    }
  }

  function handleNewFile() {
    setText(initialText);
    setLoadedText(initialText);
    setFileMeta(null);
    setStatus("New draft");
    setError(null);
    clearInputSelection();
  }

  function handleDownload() {
    setLoadedText(text);
    setStatus("Saved to download");
  }

  function clearInputSelection() {
    if (inputRef.current) {
      inputRef.current.value = "";
    }
  }

  return (
    <section className="flex h-full min-h-[calc(100svh-4rem)] flex-col">
      <div className="grid gap-4 border-b bg-background p-4 lg:grid-cols-[minmax(260px,1fr)_auto]">
        <div className="flex min-w-0 flex-col gap-2">
          <div className="flex flex-wrap items-center gap-2">
            <Badge variant="outline">Editor shell</Badge>
            <Badge variant={dirty ? "secondary" : "outline"}>{dirty ? "Unsaved changes" : "Clean"}</Badge>
            <Badge variant="outline">Local .cnes text</Badge>
          </div>
          <div className="min-w-0">
            <h2 className="truncate text-2xl font-semibold tracking-normal">{fileMeta?.name || "Untitled CoNES file"}</h2>
            <p className="max-w-3xl text-sm text-muted-foreground">
              Open a local <code>.cnes</code> file, edit its text, then download the current draft. Run-from-editor is intentionally
              outside this first pass.
            </p>
          </div>
        </div>

        <div className="flex flex-wrap items-end justify-start gap-2 lg:justify-end">
          <Button type="button" variant="outline" onClick={handleNewFile}>
            <FilePlus2 aria-hidden="true" />
            New
          </Button>
          <Button asChild type="button" variant="outline">
            <label className="cursor-pointer">
              <Upload aria-hidden="true" />
              Open
              <input
                ref={inputRef}
                type="file"
                accept=".cnes"
                className="sr-only"
                onChange={(event) => void handleFileChange(event.target.files?.[0] ?? null)}
              />
            </label>
          </Button>
          <Button asChild type="button" disabled={!downloadUrl}>
            <a href={downloadUrl || undefined} download={downloadName} onClick={handleDownload}>
              <Save aria-hidden="true" />
              Save .cnes
            </a>
          </Button>
        </div>
      </div>

      {error ? (
        <Alert variant="destructive" className="m-4 mb-0">
          <AlertCircle aria-hidden="true" />
          <AlertTitle>File rejected</AlertTitle>
          <AlertDescription>{error}</AlertDescription>
        </Alert>
      ) : null}

      <div className="grid flex-1 grid-cols-[minmax(0,1fr)_320px] max-xl:grid-cols-1">
        <main className="flex min-w-0 flex-col">
          <label className="sr-only" htmlFor="cnes-editor-textarea">
            CoNES source text
          </label>
          <textarea
            id="cnes-editor-textarea"
            className={cn(
              "min-h-[640px] flex-1 resize-none border-0 bg-background p-5 font-mono text-sm leading-6 text-foreground outline-none",
              "placeholder:text-muted-foreground focus-visible:ring-0",
            )}
            spellCheck={false}
            value={text}
            placeholder="Open a .cnes file or start typing CoNES source here."
            onChange={(event) => {
              setText(event.target.value);
              if (status !== "Editing") {
                setStatus("Editing");
              }
            }}
          />
        </main>

        <aside className="border-l bg-muted/30 max-xl:border-l-0 max-xl:border-t">
          <div className="grid grid-cols-2 border-b">
            <MetaCell label="Status" value={status} />
            <MetaCell label="Dirty" value={dirty ? "Yes" : "No"} />
            <MetaCell label="Lines" value={String(stats.lines)} />
            <MetaCell label="Chars" value={String(stats.characters)} />
          </div>

          <div className="flex flex-col gap-4 p-4 text-sm">
            <div className="flex items-center gap-2">
              <FileCode2 className="text-muted-foreground" aria-hidden="true" />
              <div>
                <h3 className="font-semibold tracking-normal">Local file authoring</h3>
                <p className="text-muted-foreground">This editor does not send source text anywhere.</p>
              </div>
            </div>

            <Separator />

            <div className="grid gap-3">
              <DetailRow label="File name" value={fileMeta?.name || "untitled.cnes"} />
              <DetailRow label="File size" value={fileMeta ? formatBytes(fileMeta.size) : "-"} />
              <DetailRow label="Last modified" value={formatLastModified(fileMeta?.lastModified ?? null)} />
              <DetailRow label="Download as" value={downloadName} />
            </div>

            <Alert>
              <Download aria-hidden="true" />
              <AlertTitle>First pass boundary</AlertTitle>
              <AlertDescription>
                Save downloads the current editor text as <code>.cnes</code>. Running edited source will be wired by a later
                integration packet.
              </AlertDescription>
            </Alert>
          </div>
        </aside>
      </div>
    </section>
  );
}

function MetaCell({ label, value }: { label: string; value: string }) {
  return (
    <div className="flex min-h-20 flex-col justify-center gap-1 border-b border-r p-4 even:border-r-0">
      <span className="text-xs font-semibold uppercase text-muted-foreground">{label}</span>
      <strong className="break-words text-sm">{value}</strong>
    </div>
  );
}

function DetailRow({ label, value }: { label: string; value: string }) {
  return (
    <div className="grid gap-1 rounded-md border bg-background p-3">
      <span className="text-xs font-semibold uppercase text-muted-foreground">{label}</span>
      <span className="break-words font-mono text-xs">{value}</span>
    </div>
  );
}

function buildTextStats(value: string) {
  return {
    characters: value.length,
    lines: value.length === 0 ? 0 : value.split(/\r\n|\r|\n/).length,
  };
}

function buildDownloadName(fileName?: string) {
  if (!fileName) {
    return defaultDownloadName;
  }

  return fileName.toLowerCase().endsWith(".cnes") ? fileName : `${fileName}.cnes`;
}

function formatLastModified(value: number | null) {
  if (!value) {
    return "-";
  }

  return new Intl.DateTimeFormat(undefined, {
    dateStyle: "medium",
    timeStyle: "short",
  }).format(new Date(value));
}

function formatBytes(bytes: number) {
  if (bytes === 0) {
    return "0 B";
  }

  const units = ["B", "KB", "MB", "GB"];
  const exponent = Math.min(Math.floor(Math.log(bytes) / Math.log(1024)), units.length - 1);
  const value = bytes / 1024 ** exponent;

  return `${value >= 10 || exponent === 0 ? value.toFixed(0) : value.toFixed(1)} ${units[exponent]}`;
}
