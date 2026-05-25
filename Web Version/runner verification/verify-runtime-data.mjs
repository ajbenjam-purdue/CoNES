#!/usr/bin/env node
import { createHash } from "node:crypto";
import { readFile, readdir, stat } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const here = path.dirname(fileURLToPath(import.meta.url));
const runtimeDataRoot = path.resolve(here, "../public/runtime-data");

const manifestChecks = [
  {
    label: "materials",
    manifestPath: path.join(runtimeDataRoot, "materials/manifest.json"),
    assetDir: path.join(runtimeDataRoot, "materials"),
    extension: ".cnesbin",
    expectedMountRoot: "/app/materials",
    expectedKind: "cones-materials",
  },
  {
    label: "libs",
    manifestPath: path.join(runtimeDataRoot, "libs/manifest.json"),
    assetDir: path.join(runtimeDataRoot, "libs"),
    extension: ".cnes",
    expectedMountRoot: "/app/libs",
    expectedKind: "cones-libs",
  },
];

function fail(message) {
  throw new Error(message);
}

async function verifyManifest(check) {
  const manifest = JSON.parse(await readFile(check.manifestPath, "utf8"));
  const errors = [];

  if (manifest.kind !== check.expectedKind) {
    errors.push(`kind is ${manifest.kind}, expected ${check.expectedKind}`);
  }

  if (manifest.mountRoot !== check.expectedMountRoot) {
    errors.push(`mountRoot is ${manifest.mountRoot}, expected ${check.expectedMountRoot}`);
  }

  if (manifest.integrity !== "sha256") {
    errors.push(`integrity is ${manifest.integrity}, expected sha256`);
  }

  if (!Array.isArray(manifest.files)) {
    fail(`${check.label}: files must be an array`);
  }

  if (manifest.count !== manifest.files.length) {
    errors.push(`count is ${manifest.count}, files.length is ${manifest.files.length}`);
  }

  const diskFiles = (await readdir(check.assetDir)).filter((name) => name.endsWith(check.extension));
  if (diskFiles.length !== manifest.files.length) {
    errors.push(`disk has ${diskFiles.length} ${check.extension} files, manifest has ${manifest.files.length}`);
  }

  let totalBytes = 0;
  const seenNames = new Set();

  for (const entry of manifest.files) {
    if (seenNames.has(entry.name)) {
      errors.push(`duplicate entry ${entry.name}`);
      continue;
    }
    seenNames.add(entry.name);

    const expectedUrl = `/runtime-data/${check.label}/${entry.name}`;
    const expectedVfsPath = `${check.expectedMountRoot}/${entry.name}`;

    if (entry.url !== expectedUrl) {
      errors.push(`${entry.name}: url is ${entry.url}, expected ${expectedUrl}`);
    }

    if (entry.vfsPath !== expectedVfsPath) {
      errors.push(`${entry.name}: vfsPath is ${entry.vfsPath}, expected ${expectedVfsPath}`);
    }

    const assetPath = path.join(check.assetDir, entry.name);
    const assetStat = await stat(assetPath).catch(() => null);
    if (!assetStat) {
      errors.push(`${entry.name}: missing on disk`);
      continue;
    }

    const bytes = await readFile(assetPath);
    const sha256 = createHash("sha256").update(bytes).digest("hex");
    totalBytes += bytes.byteLength;

    if (entry.bytes !== assetStat.size || entry.bytes !== bytes.byteLength) {
      errors.push(`${entry.name}: bytes manifest=${entry.bytes} stat=${assetStat.size} read=${bytes.byteLength}`);
    }

    if (entry.sha256 !== sha256) {
      errors.push(`${entry.name}: sha256 manifest=${entry.sha256} actual=${sha256}`);
    }
  }

  if (manifest.totalBytes !== totalBytes) {
    errors.push(`totalBytes is ${manifest.totalBytes}, actual sum is ${totalBytes}`);
  }

  if (errors.length) {
    fail(`${check.label}: ${errors.join("; ")}`);
  }

  return {
    label: check.label,
    count: manifest.files.length,
    totalBytes,
    manifest: path.relative(process.cwd(), check.manifestPath),
  };
}

try {
  const results = [];
  for (const check of manifestChecks) {
    results.push(await verifyManifest(check));
  }

  for (const result of results) {
    console.log(
      `PASS ${result.label}: ${result.count} files, ${result.totalBytes} bytes, sha256 verified (${result.manifest})`,
    );
  }
} catch (error) {
  console.error(`FAIL runtime-data verification: ${error.message}`);
  process.exitCode = 1;
}
