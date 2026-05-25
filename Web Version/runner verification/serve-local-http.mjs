#!/usr/bin/env node
import { createServer } from "node:http";
import { createReadStream } from "node:fs";
import { stat } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const here = path.dirname(fileURLToPath(import.meta.url));
const webRoot = path.resolve(here, "..");
const publicRoot = path.join(webRoot, "public");
const host = "127.0.0.1";
const port = Number.parseInt(process.env.PORT || "4173", 10);

const mimeTypes = new Map([
  [".html", "text/html; charset=utf-8"],
  [".js", "text/javascript; charset=utf-8"],
  [".css", "text/css; charset=utf-8"],
  [".json", "application/json; charset=utf-8"],
  [".wasm", "application/wasm"],
  [".cnes", "text/plain; charset=utf-8"],
  [".cnesbin", "application/octet-stream"],
]);

const server = createServer(async (request, response) => {
  if (!request.url || request.method !== "GET") {
    sendText(response, 405, "Method not allowed\n");
    return;
  }

  const url = new URL(request.url, `http://${host}:${port}`);
  const filePath = resolveStaticPath(url.pathname);

  if (!filePath) {
    sendText(response, 404, "Not found\n");
    return;
  }

  const fileStat = await stat(filePath).catch(() => null);
  if (!fileStat || !fileStat.isFile()) {
    sendText(response, 404, "Not found\n");
    return;
  }

  response.writeHead(200, {
    "Content-Type": mimeTypes.get(path.extname(filePath)) || "application/octet-stream",
    "Content-Length": fileStat.size,
    "Cache-Control": "no-store",
  });
  createReadStream(filePath).pipe(response);
});

server.listen(port, host, () => {
  console.log(`Packet 002 local HTTP server: http://${host}:${port}/`);
});

function resolveStaticPath(pathname) {
  const cleanPath = decodeURIComponent(pathname);
  if (cleanPath.includes("\0")) {
    return null;
  }

  if (cleanPath === "/" || cleanPath === "/index.html") {
    return path.join(publicRoot, "runner/index.html");
  }

  if (cleanPath === "/runner" || cleanPath === "/runner/") {
    return path.join(publicRoot, "runner/index.html");
  }

  if (
    cleanPath.startsWith("/runtime/") ||
    cleanPath.startsWith("/runtime-data/") ||
    cleanPath.startsWith("/runner/")
  ) {
    return safeJoin(publicRoot, cleanPath.slice(1));
  }

  return null;
}

function safeJoin(root, relativePath) {
  const resolved = path.resolve(root, relativePath);
  return resolved === root || resolved.startsWith(`${root}${path.sep}`) ? resolved : null;
}

function sendText(response, status, body) {
  response.writeHead(status, {
    "Content-Type": "text/plain; charset=utf-8",
    "Content-Length": Buffer.byteLength(body),
    "Cache-Control": "no-store",
  });
  response.end(body);
}
