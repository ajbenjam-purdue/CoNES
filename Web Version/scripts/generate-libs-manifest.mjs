#!/usr/bin/env node
import { createHash } from "node:crypto";
import { readdir, readFile, writeFile } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const libsDir = path.resolve(scriptDir, "../public/runtime-data/libs");
const manifestPath = path.join(libsDir, "manifest.json");

function sha256(bytes) {
  return createHash("sha256").update(bytes).digest("hex");
}

async function generateManifest() {
  const files = await readdir(libsDir);
  const cnesFiles = files.filter((f) => f.endsWith(".cnes")).sort();
  
  const manifestFiles = [];
  let totalBytes = 0;

  for (const filename of cnesFiles) {
    const filePath = path.join(libsDir, filename);
    const content = await readFile(filePath);
    const bytesCount = content.byteLength;
    const hash = sha256(content);

    totalBytes += bytesCount;
    manifestFiles.push({
      name: filename,
      bytes: bytesCount,
      sha256: hash,
      url: `/runtime-data/libs/${filename}`,
      vfsPath: `/app/libs/${filename}`,
    });
  }

  const todayStr = new Date().toISOString().split("T")[0];

  const manifest = {
    version: todayStr,
    kind: "cones-libs",
    count: manifestFiles.length,
    totalBytes: totalBytes,
    mountRoot: "/app/libs",
    integrity: "sha256",
    files: manifestFiles,
  };

  await writeFile(manifestPath, JSON.stringify(manifest, null, 2) + "\n", "utf8");
  console.log(`Successfully generated manifest at ${manifestPath} with ${manifestFiles.length} files.`);
}

generateManifest().catch((err) => {
  console.error("Failed to generate libraries manifest:", err);
  process.exitCode = 1;
});
