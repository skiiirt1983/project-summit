// Browser test of the device web UI against the PC simulator (real WebApi, settings,
// update engine with a fake flash writer). Usage:
//   node tests/web/e2e.mjs build/sim/summit_sim [out-dir]
// Proves the page and API on a PC, not on the device.
import { spawn } from "node:child_process";
import { mkdirSync, writeFileSync, readFileSync } from "node:fs";
import { createRequire } from "node:module";
const require = createRequire(import.meta.url);
const { chromium } = require("playwright");

const sim = process.argv[2] || "build/sim/summit_sim";
const out = process.argv[3] || "build/web-out";
mkdirSync(out, { recursive: true });
const PORT = 18000 + Math.floor(Math.random() * 1000);
const base = `http://127.0.0.1:${PORT}`;

let failures = 0, passes = 0;
function check(cond, what) { if (cond) passes++; else { failures++; console.log("  FAIL " + what); } }

// An OTA image the update engine accepts: ESP image header, ESP32-P4, chip rev 3.01-3.99, project summit.
function image(version, size = 200 * 1024) {
  const b = Buffer.alloc(size);
  for (let i = 0; i < size; i++) b[i] = (i * 13) & 0xff;
  b.fill(0, 0, 0x70);
  b[0] = 0xe9; b.writeUInt16LE(0x12, 12); b.writeUInt16LE(301, 15); b.writeUInt16LE(399, 17);
  b.writeUInt32LE(0xabcd5432, 0x20); b.write(version, 0x30, "ascii"); b.write("summit", 0x50, "ascii");
  return b;
}

const proc = spawn(sim, ["--web", String(PORT)], { stdio: ["ignore", "pipe", "inherit"] });
await new Promise((resolve, reject) => {
  proc.stdout.on("data", (d) => { if (String(d).includes("web UI:")) resolve(); });
  proc.on("exit", (c) => reject(new Error("simulator exited " + c)));
  setTimeout(() => reject(new Error("simulator did not start")), 20000);
});

const browser = await chromium.launch();
try {
  const page = await browser.newPage({ viewport: { width: 1100, height: 900 } });
  const dialogs = [];
  page.on("dialog", async (d) => { dialogs.push(d.message()); d.message().startsWith("Go back") ? await d.dismiss() : await d.accept(); });
  const tab = (name) => page.click(`nav button[data-tab=${name}]`);
  const shot = (name) => page.screenshot({ path: `${out}/${name}.png`, fullPage: true });

  // Status
  await page.goto(base + "/");
  await page.waitForSelector("#status-dl dd");
  const status = await page.textContent("#status-dl");
  check(status.includes("0.0.1") && status.includes("waveshare-p4-wifi6-lcd5-rev3") && status.includes("85%"), "status shows firmware, hardware, battery");
  await shot("01-status");

  // Wi-Fi
  await tab("wifi");
  await page.waitForSelector("#wifi-networks tr");
  check((await page.locator("#wifi-networks tr").count()) === 2, "scan results listed");
  await page.click("#wifi-networks tr:nth-child(2)");
  check((await page.inputValue("#wifi-ssid")) === "cafe", "clicking a network fills the SSID");
  await page.fill("#wifi-pass", "short");
  await page.click("#wifi-form button[type=submit]");
  await page.waitForSelector("#wifi-msg.err");
  check((await page.textContent("#wifi-msg")).includes("8-63"), "short password rejected");
  await page.fill("#wifi-pass", "");
  await page.click("#wifi-form button[type=submit]");
  await page.waitForSelector("#wifi-msg.ok");
  await shot("02-wifi");

  // Firmware: check, catalog, install, restart
  await tab("firmware");
  await page.click("#fw-check");
  await page.waitForSelector("#fw-available:not([hidden])");
  check((await page.textContent("#fw-available-title")).includes("0.1.0"), "0.1.0 available on stable");
  const catalog = await page.textContent("#fw-catalog");
  check(catalog.includes("0.2.0-beta.1") && catalog.includes("channel not followed"), "catalog lists beta as not followed");
  check(!catalog.includes("rev1"), "other chip revision not listed");
  await shot("03-firmware-available");
  await page.click("#fw-install");
  await page.waitForSelector("#fw-ready:not([hidden])", { timeout: 15000 });
  await shot("04-firmware-ready");
  await page.click("#fw-reboot");
  await page.waitForFunction(() => document.querySelector("#fw-dl").textContent.includes("0.1.0"), null, { timeout: 5000 });
  check(true, "restart runs 0.1.0");

  // Previous version asks first (dismissed here).
  await page.click("#fw-previous");
  check(dialogs.some((m) => m.startsWith("Go back to v0.0.1-alpha.3? Settings from the newer version may become read-only.")), "previous version confirm text");

  // Upload: a valid image, then a file that isn't firmware.
  writeFileSync(`${out}/good-ota.bin`, image("0.3.0"));
  writeFileSync(`${out}/bad.bin`, Buffer.alloc(4096, 7));
  await page.setInputFiles("#fw-file", `${out}/good-ota.bin`);
  await page.click("#fw-upload");
  await page.waitForSelector("#fw-upload-msg.ok", { timeout: 15000 });
  check((await page.textContent("#fw-upload-msg")).includes("0.3.0"), "upload installs 0.3.0");
  await page.setInputFiles("#fw-file", `${out}/bad.bin`);
  await page.click("#fw-upload");
  await page.waitForSelector("#fw-upload-msg.err", { timeout: 15000 });
  check((await page.textContent("#fw-upload-msg")).includes("not an ESP firmware image"), "bad upload rejected with a reason");
  await shot("05-firmware-upload-rejected");

  // Upload without the custom header is refused by the server (cross-site protection).
  const plain = await page.evaluate(async () => (await fetch("/api/firmware/upload", { method: "POST", body: "x" })).status);
  check(plain === 403, "upload without X-Summit-Upload refused (403)");
  const form = await page.evaluate(async () => (await fetch("/api/settings/reset", { method: "POST", headers: { "Content-Type": "text/plain" }, body: '{"confirm":true}' })).status);
  check(form === 415, "non-JSON POST refused");

  // Settings: edit, export, reset, import
  await tab("settings");
  await page.waitForSelector("#set-display-brightness");
  await page.fill("#set-display-brightness", "42");
  await page.click("#set-save");
  await page.waitForSelector("#set-msg.ok");
  await tab("status"); await tab("settings");
  await page.waitForSelector("#set-display-brightness");
  check((await page.inputValue("#set-display-brightness")) === "42", "brightness saved");
  await shot("06-settings");
  const [dl] = await Promise.all([page.waitForEvent("download"), page.click("#set-export")]);
  const exported = `${out}/exported-settings.json`;
  await dl.saveAs(exported);
  const ex = JSON.parse(readFileSync(exported, "utf8"));
  check(ex.format === "summit-settings" && ex.values["display/brightness"] === 42, "export contains the setting");
  check(!JSON.stringify(ex).includes("password"), "export has no Wi-Fi password");
  await page.click("#set-reset");
  await page.waitForSelector("#set-backup-msg.ok");
  check((await page.inputValue("#set-display-brightness")) === "80", "reset restores defaults");
  await page.setInputFiles("#set-import-file", exported);
  await page.waitForFunction(() => document.querySelector("#set-backup-msg").textContent.startsWith("Imported"));
  await page.waitForFunction(() => document.querySelector("#set-display-brightness").value === "42");
  check(true, "import restores the exported settings");

  // Diagnostics
  await tab("diagnostics");
  await page.waitForSelector("#diag-warnings li");
  check((await page.textContent("#diag-warnings")).includes("LIS2DW12"), "add-on warning shown");
  await shot("07-diagnostics");

  // Phone width
  await page.setViewportSize({ width: 390, height: 844 });
  await tab("firmware");
  await page.waitForTimeout(300);
  const overflow = await page.evaluate(() => document.documentElement.scrollWidth > window.innerWidth + 1);
  check(!overflow, "no horizontal scroll at 390 px");
  const navVisible = await page.locator("nav button[data-tab=diagnostics]").isVisible();
  const navBox = await page.locator("nav button[data-tab=diagnostics]").boundingBox();
  check(navVisible && navBox.x + navBox.width <= 390, "all tabs visible at 390 px");
  await shot("08-phone-firmware");
} finally {
  await browser.close();
  proc.kill();
}
console.log(`web e2e: ${passes} passed, ${failures} failed`);
process.exit(failures ? 1 : 0);
