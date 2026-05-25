#!/usr/bin/env node
import { createHash } from "node:crypto";
import { readFile } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const here = path.dirname(fileURLToPath(import.meta.url));
const projectRoot = path.resolve(here, "..");
const publicRoot = path.join(projectRoot, "public");
const metadataPath = path.join(publicRoot, "runtime/runtime-metadata.json");

function fail(message) {
  throw new Error(message);
}

try {
  const metadata = JSON.parse(await readFile(metadataPath, "utf8"));

  const callMainArgs = metadata?.runtimeContract?.callMainArgs;
  if (!Array.isArray(callMainArgs) || callMainArgs.join("\0") !== ["/work/input.cnes", "--json"].join("\0")) {
    fail(`runtimeContract.callMainArgs is ${JSON.stringify(callMainArgs)}, expected ["/work/input.cnes", "--json"]`);
  }

  if (metadata?.runtimeContract?.thisProgram !== "/app/cnes") {
    fail(`runtimeContract.thisProgram is ${metadata?.runtimeContract?.thisProgram}, expected /app/cnes`);
  }

  for (const artifact of Object.values(metadata.artifacts || {})) {
    await verifyFileRecord(artifact, "artifact");
  }

  for (const manifest of Object.values(metadata.runtimeData || {})) {
    await verifyFileRecord(manifest, "manifest");
    const manifestJson = JSON.parse(await readFile(path.join(publicRoot, withoutLeadingSlash(manifest.path)), "utf8"));
    if (manifest.count !== manifestJson.count) {
      fail(`${manifest.label}: metadata count ${manifest.count} does not match manifest count ${manifestJson.count}`);
    }
    if (manifest.totalBytes !== manifestJson.totalBytes) {
      fail(`${manifest.label}: metadata totalBytes ${manifest.totalBytes} does not match manifest totalBytes ${manifestJson.totalBytes}`);
    }
  }

  console.log(
    `PASS runtime metadata: ${Object.keys(metadata.artifacts || {}).length} artifacts, ${Object.keys(metadata.runtimeData || {}).length} manifests verified (${path.relative(process.cwd(), metadataPath)})`,
  );
} catch (error) {
  console.error(`FAIL runtime metadata verification: ${error.message}`);
  process.exitCode = 1;
}

async function verifyFileRecord(record, kind) {
  if (!record?.path) {
    fail(`${kind} record is missing path`);
  }
  const expectedHash = record.sha256 || record.manifestSha256;
  if (!expectedHash) {
    fail(`${record.label || record.path}: missing sha256`);
  }

  const bytes = await readFile(path.join(publicRoot, withoutLeadingSlash(record.path)));
  const actualHash = createHash("sha256").update(bytes).digest("hex");

  if (record.bytes !== undefined && record.bytes !== bytes.byteLength) {
    fail(`${record.label || record.path}: bytes metadata=${record.bytes} actual=${bytes.byteLength}`);
  }
  if (expectedHash !== actualHash) {
    fail(`${record.label || record.path}: sha256 metadata=${expectedHash} actual=${actualHash}`);
  }
}

function withoutLeadingSlash(value) {
  return String(value).replace(/^\/+/, "");
}
