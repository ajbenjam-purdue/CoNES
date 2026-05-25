import { useEffect, useMemo, useRef, useState } from "react";

export type WorkbenchSourceMeta = {
  name: string;
  size: number;
  lastModified: number | null;
};

const defaultSource = `// CoNES workbench draft
include "heat_transfer_lib"

h := 100 [W/m^2*K]
h.unit := [W/m^2*K]
A_c := 0.5
A_c.unit := [m^2]
T_fluid := 20 [C]
Q := 1 [kW]
NewtonCooling(h, A_c, T_surface, T_fluid, Q)
T_surface.unit := [C]
`;

const defaultFileName = "untitled.cnes";

export function useWorkbenchSource() {
  const inputRef = useRef<HTMLInputElement | null>(null);
  const [source, setSource] = useState(defaultSource);
  const [savedSource, setSavedSource] = useState(defaultSource);
  const [fileMeta, setFileMeta] = useState<WorkbenchSourceMeta | null>(null);
  const [status, setStatus] = useState("New draft");
  const [error, setError] = useState<string | null>(null);
  const [downloadUrl, setDownloadUrl] = useState<string | null>(null);

  const dirty = source !== savedSource;
  const fileName = buildDownloadName(fileMeta?.name);
  const stats = useMemo(() => buildTextStats(source), [source]);

  useEffect(() => {
    const blob = new Blob([source], { type: "text/plain;charset=utf-8" });
    const url = URL.createObjectURL(blob);
    setDownloadUrl(url);

    return () => {
      URL.revokeObjectURL(url);
    };
  }, [source]);

  function updateSource(nextSource: string) {
    setSource(nextSource);
    setStatus("Editing");
  }

  async function openFile(file: File | null) {
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
      const text = await file.text();
      setSource(text);
      setSavedSource(text);
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

  function newDraft() {
    setSource(defaultSource);
    setSavedSource(defaultSource);
    setFileMeta(null);
    setStatus("New draft");
    setError(null);
    clearInputSelection();
  }

  function markDownloaded() {
    setSavedSource(source);
    setStatus("Saved to download");
  }

  function renameFileName(nextName: string) {
    const trimmedName = nextName.trim();
    if (!trimmedName) {
      setError("File name cannot be empty");
      return false;
    }

    const normalizedName = buildDownloadName(trimmedName);
    setFileMeta((current) => ({
      name: normalizedName,
      size: current?.size ?? new TextEncoder().encode(source).byteLength,
      lastModified: current?.lastModified ?? null,
    }));
    setStatus("Renamed");
    setError(null);
    return true;
  }

  function clearInputSelection() {
    if (inputRef.current) {
      inputRef.current.value = "";
    }
  }

  return {
    source,
    savedSource,
    fileMeta,
    fileName,
    status,
    error,
    dirty,
    stats,
    inputRef,
    downloadUrl,
    setSource: updateSource,
    setError,
    openFile,
    newDraft,
    markDownloaded,
    renameFileName,
  };
}

function buildTextStats(value: string) {
  return {
    characters: value.length,
    lines: value.length === 0 ? 0 : value.split(/\r\n|\r|\n/).length,
  };
}

function buildDownloadName(fileName?: string) {
  const trimmedName = fileName?.trim();

  if (!trimmedName) {
    return defaultFileName;
  }

  return trimmedName.toLowerCase().endsWith(".cnes") ? trimmedName : `${trimmedName}.cnes`;
}
