import { useEffect, useMemo, useRef, useState } from "react";

export type WorkbenchSourceMeta = {
  name: string;
  size: number;
  lastModified: number | null;
};

const defaultSource = `// CoNES Web V0.2.3 Copyright (c) 2026 ajbenjam-purdue
// Inclusions work similar to c++ header files. On cones.dev, you may use prebuilt libraries; on the desktop version, you can build and distribute your own libraries in addition.
// The prebuilt libraries include (from Bergman et. al) "1D_SS", "conduction", "fins", "geometry", "shape_factors", and (from a variety of sources) "thermo_lib"

// Variables defined with := and an optional unit cast (e.g. variable_name := 5 [C]) are fixed and excluded from the jacobian
// Variables defined with = aren't fixed and will attempt to be solved

// The following is an example script for a Brayton Cycle
// As converted from the EES code: https://fchartsoftware.com/assets/downloads/eesysolns/eesysol43.pdf

// Input conditions (Fixed)
T_amb := 20 [C]
P_atm := 101.325 [kPa]
PR := 5
T_t_in := 1400 [K]
m_dot := 1.0 [kg/s]

// State 1
T_1 = T_amb
P_1 = P_atm
s_1 = Entropy(Air, T=T_1, P=P_1)
h_1 = Enthalpy(Air, T=T_1)

// State 2
P_2 = P_1 * PR
s_2 = s_1
h_2 = Enthalpy(Air, s=s_2, P=P_2)
W_dot_c = (h_2 - h_1) * m_dot
T_2 = Temperature(Air, h=h_2)

// State 3
T_3 = T_t_in
P_3 = P_2
h_3 = Enthalpy(Air, T=T_3)
s_3 = Entropy(Air, T=T_3, P=P_3)
Q_dot_c = (h_3 - h_2) * m_dot

// State 4
s_4 = s_3
h_4 = Enthalpy(Air, s=s_4, P=P_4)
W_dot_gt = (h_3 - h_4) * m_dot
W_dot_gt = W_dot_c // Gas turbine drives compressor
T_4 = Temperature(Air, h=h_4)

// State 5
P_5 = P_atm
s_5 = s_4
h_5 = Enthalpy(Air, s=s_5, P=P_5)
W_dot_pt = (h_4 - h_5) * m_dot
T_5 = Temperature(Air, h=h_5)

eta = W_dot_pt / Q_dot_c`;

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
