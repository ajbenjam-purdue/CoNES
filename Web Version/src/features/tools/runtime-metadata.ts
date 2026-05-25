export type RuntimeFileRecord = {
  label?: string;
  path?: string;
  bytes?: number;
  sha256?: string;
  manifestSha256?: string;
};

export type RuntimeManifestRecord = RuntimeFileRecord & {
  kind?: string | null;
  mountRoot?: string | null;
  integrity?: string | null;
  count?: number;
  totalBytes?: number;
  perFileSha256?: {
    exists?: boolean;
    complete?: boolean;
    filesWithSha256?: number;
    fileCount?: number;
  };
};

export type RuntimeMetadata = {
  schemaVersion?: number;
  project?: {
    name?: string;
    version?: string;
  };
  build?: {
    command?: string;
    wasmCommand?: string;
    metadataCommand?: string;
    emscriptenVersion?: string | null;
    conesVersion?: string | null;
    note?: string;
  };
  runtime?: {
    workerPath?: string;
    runtimeBasePath?: string;
    runtimeDataBasePath?: string;
    moduleScriptPath?: string;
    wasmPath?: string;
    thisProgram?: string;
    inputPath?: string;
    callMainArgs?: string[];
  };
  runtimeContract?: {
    workerPath?: string;
    moduleScriptUrl?: string;
    wasmPath?: string;
    runtimeDataManifests?: string[];
    thisProgram?: string;
    inputPath?: string;
    callMainArgs?: string[];
    outputContract?: string;
  };
  artifacts?: RuntimeFileRecord[] | Record<string, RuntimeFileRecord>;
  artifactSummary?: Record<string, RuntimeFileRecord>;
  manifests?: RuntimeManifestRecord[] | Record<string, RuntimeManifestRecord>;
  runtimeData?: Record<string, RuntimeManifestRecord>;
};

export type RuntimeMetadataState =
  | { status: "loading"; metadata: null; error: null }
  | { status: "ready"; metadata: RuntimeMetadata; error: null }
  | { status: "missing"; metadata: null; error: string }
  | { status: "invalid"; metadata: null; error: string };

export const runtimeMetadataUrl = "/runtime/runtime-metadata.json";

export function normalizeArtifacts(metadata: RuntimeMetadata | null) {
  if (metadata?.artifactSummary) {
    return normalizeRecordCollection(metadata.artifactSummary);
  }
  if (!metadata?.artifacts) {
    return [];
  }
  return normalizeRecordCollection(metadata.artifacts);
}

export function normalizeManifests(metadata: RuntimeMetadata | null) {
  if (metadata?.runtimeData) {
    return normalizeRecordCollection(metadata.runtimeData);
  }
  if (metadata?.manifests) {
    return normalizeRecordCollection(metadata.manifests);
  }
  return [];
}

export function runtimeContract(metadata: RuntimeMetadata | null) {
  return {
    workerPath: metadata?.runtimeContract?.workerPath || metadata?.runtime?.workerPath || "/runtime/cnes-worker.js",
    moduleScriptPath:
      metadata?.runtimeContract?.moduleScriptUrl || metadata?.runtime?.moduleScriptPath || "/runtime/cnes-module.js",
    wasmPath: metadata?.runtimeContract?.wasmPath || metadata?.runtime?.wasmPath || "/runtime/cnes-module.wasm",
    runtimeDataManifests: metadata?.runtimeContract?.runtimeDataManifests || [
      "/runtime-data/materials/manifest.json",
      "/runtime-data/libs/manifest.json",
    ],
    thisProgram: metadata?.runtimeContract?.thisProgram || metadata?.runtime?.thisProgram || "/app/cnes",
    inputPath: metadata?.runtimeContract?.inputPath || metadata?.runtime?.inputPath || "/work/input.cnes",
    callMainArgs: metadata?.runtimeContract?.callMainArgs || metadata?.runtime?.callMainArgs || ["/work/input.cnes", "--json"],
    outputContract: metadata?.runtimeContract?.outputContract || "Raw stdout and stderr are the source of truth.",
  };
}

export function shortHash(hash: string | undefined) {
  if (!hash) {
    return "Unavailable";
  }
  return hash.length > 18 ? `${hash.slice(0, 12)}...${hash.slice(-6)}` : hash;
}

export function formatBytes(bytes: number | undefined) {
  if (bytes === undefined || !Number.isFinite(bytes)) {
    return "Unavailable";
  }
  if (bytes < 1024) {
    return `${bytes} B`;
  }
  if (bytes < 1024 * 1024) {
    return `${(bytes / 1024).toFixed(1)} KB`;
  }
  return `${(bytes / 1024 / 1024).toFixed(1)} MB`;
}

function normalizeRecordCollection<T extends RuntimeFileRecord>(collection: T[] | Record<string, T>) {
  if (Array.isArray(collection)) {
    return collection.map((item, index) => ({ ...item, label: item.label || `item-${index + 1}` }));
  }

  return Object.entries(collection).map(([key, value]) => ({
    ...value,
    label: value.label || key,
  }));
}
