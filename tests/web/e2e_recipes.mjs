// Browser test of recipe import: the real pdf.js converts the sample PDFs in
// tests/fixtures/pdf and saves them to the simulator's "SD card" through the real
// recipe web API. Usage: node tests/web/e2e_recipes.mjs build/sim/summit_sim [out-dir]
import { spawn } from "node:child_process";
import { mkdirSync } from "node:fs";
import { createRequire } from "node:module";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
const require = createRequire(import.meta.url);
const { chromium } = require("playwright");

const here = dirname(fileURLToPath(import.meta.url));
const pdf = (name) => resolve(here, "../fixtures/pdf", name);
const sim = process.argv[2] || "build/sim/summit_sim";
const out = process.argv[3] || "build/web-out";
mkdirSync(out, { recursive: true });
const PORT = 19000 + Math.floor(Math.random() * 1000);
const base = `http://127.0.0.1:${PORT}`;

let failures = 0, passes = 0;
function check(cond, what) { if (cond) passes++; else { failures++; console.log("  FAIL " + what); } }

const proc = spawn(sim, ["--web", String(PORT)], { stdio: ["ignore", "pipe", "inherit"] });
await new Promise((res, rej) => {
  proc.stdout.on("data", (d) => { if (String(d).includes("web UI:")) res(); });
  proc.on("exit", (c) => rej(new Error("simulator exited " + c)));
  setTimeout(() => rej(new Error("simulator did not start")), 20000);
});

const browser = await chromium.launch();
try {
  const page = await browser.newPage({ viewport: { width: 1100, height: 1000 } });
  page.on("pageerror", (e) => { failures++; console.log("  FAIL page error: " + e.message); });
  const shot = (name) => page.screenshot({ path: `${out}/${name}.png`, fullPage: true });
  const library = async () => (await (await fetch(base + "/api/recipes")).json());
  const lines = async (id) => (await page.inputValue(id)).split("\n").filter(Boolean);
  const upload = async (file) => {
    await page.setInputFiles("#rec-file", pdf(file));
    await page.waitForSelector("#rec-edit:not([hidden])", { timeout: 30000 });
  };
  const save = async () => {
    await page.click("#rec-save");
    await page.waitForFunction(() => document.querySelector("#rec-msg").textContent.startsWith("Saved") ||
      document.querySelector("#rec-save-msg").classList.contains("err"), null, { timeout: 30000 });
    const err = await page.textContent("#rec-save-msg");
    if (err) throw new Error("save failed: " + err);
  };

  await page.goto(base + "/");
  await page.waitForFunction(() => window.summit && window.summit.recipesReady);
  await page.click("nav button[data-tab=recipes]");
  await page.waitForSelector("#rec-list tr");
  const before = (await library()).recipes.length;
  check((await page.textContent("#rec-inbox")).includes("from-sd-card.pdf"), "inbox PDF listed");

  // 1. A text recipe: parsed into a draft, edited, saved with its page and the original PDF.
  await upload("lemon-cake.pdf");
  check((await page.inputValue("#rec-title")) === "Lemon Drizzle Cake", "title parsed: " + (await page.inputValue("#rec-title")));
  check((await page.inputValue("#rec-servings")) === "8", "servings parsed");
  const ing = await lines("#rec-ingredients");
  check(ing.length === 5 && ing[0] === "225 g butter, softened" && ing[3] === "1 1/2 cup self-raising flour", "ingredients parsed: " + JSON.stringify(ing));
  const steps = await lines("#rec-steps");
  check(steps.length === 4 && steps[2] === "Fold in the flour and zest. Bake for 45-50 minutes.", "steps parsed: " + JSON.stringify(steps));
  check((await page.textContent("#rec-note")).startsWith("Found the recipe text"), "draft note");
  check((await page.locator("#rec-pages img").count()) === 1, "page preview shown");
  await page.fill("#rec-tags", "Dessert, baking");
  await shot("10-recipes-import-draft");
  await page.fill("#rec-title", "");
  await page.click("#rec-save");
  check((await page.textContent("#rec-save-msg")) === "Give the recipe a title.", "empty title refused");
  await page.fill("#rec-title", "Lemon Drizzle Cake");
  await save();
  let lib = await library();
  let cake = lib.recipes.find((r) => r.id === "lemon-drizzle-cake");
  check(cake && cake.has_card && cake.pages === 1 && cake.tags.join() === "dessert,baking", "saved with steps, 1 page, tags: " + JSON.stringify(cake));
  const stored = await (await fetch(base + "/api/recipes/get?id=lemon-drizzle-cake")).json();
  check(stored.servings === 8 && stored.ingredients[0].qty === 225 && stored.ingredients[0].unit === "g" && stored.source.name === "lemon-cake.pdf", "recipe.json on the device");
  const dims = await page.evaluate(async () => {
    const blob = await (await fetch("/api/recipes/file?id=lemon-drizzle-cake&name=pages/001.jpg")).blob();
    const head = new Uint8Array(await blob.slice(0, 2).arrayBuffer());
    const bmp = await createImageBitmap(blob);
    return { jpeg: head[0] === 0xff && head[1] === 0xd8, w: bmp.width, h: bmp.height, bytes: blob.size };
  });
  check(dims.jpeg && Math.max(dims.w, dims.h) === 1280 && dims.bytes < 2 * 1024 * 1024, "page saved as a JPEG with long side 1280: " + JSON.stringify(dims));
  const orig = await fetch(base + "/api/recipes/file?id=lemon-drizzle-cake&name=original.pdf");
  check(orig.status === 200 && (await orig.text()).startsWith("%PDF"), "original PDF kept with the recipe");
  check((await fetch(base + "/api/recipes/file?id=lemon-drizzle-cake&name=thumb.jpg")).status === 200, "thumbnail saved");

  // 2. Two recipes on one page: choose which to import.
  await upload("dips-two-recipes.pdf");
  check(await page.isVisible("#rec-pick-row") && (await page.locator("#rec-pick option").count()) === 2, "two recipes offered");
  check((await page.inputValue("#rec-title")) === "Guacamole", "first recipe shown first");
  await page.selectOption("#rec-pick", "1");
  check((await page.inputValue("#rec-title")) === "Tomato Salsa" && (await page.inputValue("#rec-servings")) === "4", "second recipe selected");
  check((await lines("#rec-ingredients")).length === 2 && (await lines("#rec-steps")).length === 2, "second recipe's own ingredients and steps");
  await shot("11-recipes-import-two");
  await save();
  check((await library()).recipes.some((r) => r.id === "tomato-salsa" && r.has_card), "second recipe saved");

  // 3. A scan: no text, pages only.
  await upload("scan.pdf");
  check((await page.textContent("#rec-note")).startsWith("No recipe text found"), "scan: explains there is no text");
  check((await page.inputValue("#rec-title")) === "scan" && (await lines("#rec-ingredients")).length === 0, "scan: title from the file name, empty draft");
  await page.fill("#rec-title", "Beef stew (scan)");
  await save();
  const scan = (await library()).recipes.find((r) => r.id === "beef-stew-scan");
  check(scan && !scan.has_card && scan.pages === 1, "scan saved with pages only: " + JSON.stringify(scan));

  // 4. A PDF from the SD card inbox: fetched from the device, converted, PDF moved to the recipe.
  await page.click('button[data-inbox="from-sd-card.pdf"]');
  await page.waitForSelector("#rec-edit:not([hidden])", { timeout: 30000 });
  check((await page.inputValue("#rec-title")) === "Lemon Drizzle Cake", "inbox PDF converted");
  await page.fill("#rec-title", "Lemon cake from the SD card");
  await save();
  lib = await library();
  check(lib.inbox.length === 0 && await page.isHidden("#rec-inbox-card"), "inbox is empty after conversion");
  check((await fetch(base + "/api/recipes/file?id=lemon-cake-from-the-sd-card&name=original.pdf")).status === 200, "inbox PDF kept as original.pdf");
  check(lib.recipes.length === before + 4, "four recipes added: " + lib.recipes.length);
  await page.waitForSelector('#rec-list tr[data-recipe="tomato-salsa"]');
  await shot("12-recipes-library");

  // 6. From a website: the device fetches the page and reads its schema.org Recipe.
  await page.fill("#rec-url", "https://recipes.example.com/simple");
  await page.click("#rec-url-go");
  await page.waitForSelector("#rec-url-msg.ok", { timeout: 15000 });
  check((await page.textContent("#rec-url-msg")).includes("Classic Banana Bread") && (await page.textContent("#rec-url-msg")).includes("7 ingredients, 3 steps, with its photo"), "URL import: " + (await page.textContent("#rec-url-msg")));
  check((await page.textContent("#rec-url-msg")).endsWith("Tap Edit to fix anything.") && (await page.locator('#rec-list tr.new[data-recipe="classic-banana-bread"] button[data-edit]').count()) === 1, "imported recipe shown with Edit next to it");
  const bread = (await library()).recipes.find((r) => r.id === "classic-banana-bread");
  check(bread && bread.has_card && bread.pages === 0 && bread.has_photo, "URL recipe saved: steps only, with photo");
  await page.fill("#rec-url", "https://recipes.example.com/no-recipe");
  await page.click("#rec-url-go");
  await page.waitForSelector("#rec-url-msg.err", { timeout: 15000 });
  check((await page.textContent("#rec-url-msg")).includes("no recipe data"), "page without a recipe explained");
  await page.fill("#rec-url", "https://192.168.1.1/admin");
  await page.click("#rec-url-go");
  await page.waitForSelector("#rec-url-msg.err");
  check((await page.textContent("#rec-url-msg")).includes("https://"), "local address refused");
  await shot("13-recipes-url-import");

  // 7. Edit a saved recipe: same id, text replaced, source/photo/timers kept.
  const getRecipe = async (id) => (await page.request.get(base + "/api/recipes/get?id=" + id)).json();
  await page.click('#rec-list button[data-edit="classic-banana-bread"]');
  await page.waitForSelector("#rec-edit:not([hidden])");
  check((await page.textContent("#rec-edit-title")) === "Edit recipe" && (await page.inputValue("#rec-title")) === "Classic Banana Bread", "edit form filled from the saved recipe");
  const stepsBefore = (await page.inputValue("#rec-steps")).split("\n");
  await page.fill("#rec-title", "Banana bread (ours)");
  await page.fill("#rec-tags", "baking, family");
  await page.fill("#rec-steps", [...stepsBefore, "Cool before slicing."].join("\n"));
  await shot("14-recipes-edit");
  await page.click("#rec-save");
  await page.waitForSelector("#rec-edit", { state: "hidden" });
  const after = await getRecipe("classic-banana-bread");
  check(after.title === "Banana bread (ours)" && after.steps.length === stepsBefore.length + 1 && after.tags.join() === "baking,family", "edit saved under the same id");
  check(after.source.type === "url", "source kept");
  const again = (await library()).recipes.find((r) => r.id === "classic-banana-bread");
  check(again && again.title === "Banana bread (ours)" && again.has_photo, "library shows the new title; photo kept");
  const pan = await getRecipe("pancakes");
  if (pan.title) {
    await page.click('#rec-list button[data-edit="pancakes"]');
    await page.waitForSelector("#rec-edit:not([hidden])");
    await page.fill("#rec-tags", "breakfast");
    await page.click("#rec-save");
    await page.waitForSelector("#rec-edit", { state: "hidden" });
    const pan2 = await getRecipe("pancakes");
    check(pan2.steps[1].timer_s === 1800 && pan2.created === pan.created && pan2.tags.join() === "breakfast", "unchanged step keeps its timer");
  } else check(false, "seeded pancakes recipe missing");
  // An empty title is refused in the form.
  await page.click('#rec-list button[data-edit="pancakes"]');
  await page.waitForSelector("#rec-edit:not([hidden])");
  await page.fill("#rec-title", " ");
  await page.click("#rec-save");
  check((await page.textContent("#rec-save-msg")) === "Give the recipe a title.", "empty title refused");
  await page.click("#rec-cancel");

  // 5. Not a PDF.
  await page.setInputFiles("#rec-file", { name: "notes.pdf", mimeType: "application/pdf", buffer: Buffer.from("this is not a pdf") });
  await page.waitForSelector("#rec-msg.err", { timeout: 30000 });
  check((await page.textContent("#rec-msg")).startsWith("Could not read this PDF"), "broken PDF refused with a message");
} finally {
  await browser.close();
  proc.kill();
}
console.log(`recipe import e2e: ${passes} passed, ${failures} failed`);
process.exit(failures ? 1 : 0);
