import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { JSDOM } from "jsdom";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const rootDir = path.resolve(__dirname, "..");

const py = spawn("python3", [path.join(__dirname, "fake_firebase.py")], { stdio: ["ignore", "pipe", "inherit"] });
const serverUrl = await new Promise((resolve, reject) => {
  py.stdout.once("data", (d) => resolve(d.toString().trim()));
  py.once("error", reject);
});

try {
  await fetch(serverUrl + "/geometrium/rooms/main/players/p1.json", {
    method: "PUT",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ name: "BuilderOne", x: 48, y: 16, z: 48, live: 1 }),
  });
  await fetch(serverUrl + "/geometrium/rooms/main/blocks.json", {
    method: "PATCH",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ "48_16_48": 4, "49_16_48": 20 }),
  });

  const html = readFileSync(path.join(rootDir, "index.html"), "utf8");
  const dom = new JSDOM(html, {
    url: serverUrl + "/",
    runScripts: "dangerously",
    beforeParse(win) {
      win.fetch = globalThis.fetch;
    },
  });
  const doc = dom.window.document;
  doc.getElementById("dbUrl").value = serverUrl;

  doc.getElementById("btnRefresh").click();
  await new Promise((r) => setTimeout(r, 150));
  assert.match(doc.getElementById("roomsView").textContent, /Комната: main \| игроков: 1 \| изменённых блоков: 2/);

  doc.getElementById("btnBackup").click();
  await new Promise((r) => setTimeout(r, 120));
  assert.ok(dom.window.LAST_BACKUP && dom.window.LAST_BACKUP.rooms.main);

  doc.getElementById("btnClearBlocks").click();
  await new Promise((r) => setTimeout(r, 150));
  assert.match(doc.getElementById("roomsView").textContent, /изменённых блоков: 0/);

  doc.getElementById("btnClearLog").click();
  assert.equal(doc.getElementById("logBox").textContent, "");
  assert.equal(dom.window.GEO_ERRORS.length, 0);
  console.log("Geometrium Web Console DOM E2E: OK");
} finally {
  py.kill("SIGTERM");
}
