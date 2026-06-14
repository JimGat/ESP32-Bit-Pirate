import { mkdir, writeFile } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const VERSION = "1.7.1";
const BASE_URL = `https://cdn.jsdelivr.net/npm/@leaphy-robotics/avrdude-webassembly@${VERSION}/`;
const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const FILES = [
  ["avrdude.js", "wasm/avrdude.js"],
  ["avrdude.wasm", "wasm/avrdude.wasm"],
  ["avrdude.conf", "wasm/avrdude.conf"],
  ["avrdude-worker.js", "avrdude-worker.js"],
];

for (const [remoteName, localPath] of FILES) {
  const url = `${BASE_URL}${remoteName}`;
  const output = resolve(ROOT, localPath);
  const response = await fetch(url);
  if (!response.ok) {
    throw new Error(`Failed to download ${url}: ${response.status} ${response.statusText}`);
  }
  const data = new Uint8Array(await response.arrayBuffer());
  await mkdir(dirname(output), { recursive: true });
  await writeFile(output, data);
  console.log(`Vendored ${remoteName} -> ${localPath} (${data.length} bytes)`);
}
