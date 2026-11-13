// Recipes tab: library list, PDF import (converted in the browser with pdf.js), inbox.
import { applyEdit, buildRecipe, ingredientToLine, itemsToLines, parseRecipeText } from "/recipe_import.mjs";

const { $, el, api, msg, loaders } = window.summit;
const PAGE_LONG_SIDE = 1280;   // book-mode pages are sized for the device screen
const THUMB_LONG_SIDE = 320;
const MAX_PAGES = 200;

let pdfjs = null;
async function pdfLib() {
  if (!pdfjs) {
    pdfjs = await import("/vendor/pdfjs/pdf.min.mjs");
    pdfjs.GlobalWorkerOptions.workerSrc = "/vendor/pdfjs/pdf.worker.min.mjs";
  }
  return pdfjs;
}

async function renderPage(page, longSide, quality) {
  const base = page.getViewport({ scale: 1 });
  const scale = longSide / Math.max(base.width, base.height);
  const viewport = page.getViewport({ scale });
  const canvas = document.createElement("canvas");
  canvas.width = Math.round(viewport.width); canvas.height = Math.round(viewport.height);
  const ctx = canvas.getContext("2d");
  ctx.fillStyle = "#fff"; ctx.fillRect(0, 0, canvas.width, canvas.height);
  await page.render({ canvasContext: ctx, viewport }).promise;
  return new Promise((resolve) => canvas.toBlob(resolve, "image/jpeg", quality));
}

// PDF bytes -> page JPEGs, a thumbnail, and draft recipes parsed from the text.
async function convert(bytes, name, progress) {
  const lib = await pdfLib();
  // pdf.js takes ownership of the bytes it is given (they move to its worker): hand it a
  // copy so the original is still there to upload as original.pdf.
  const doc = await lib.getDocument({ data: new Uint8Array(bytes).slice() }).promise;
  if (doc.numPages > MAX_PAGES) throw new Error("This PDF has more than " + MAX_PAGES + " pages");
  const pages = [], lines = [];
  let thumb = null;
  for (let n = 1; n <= doc.numPages; n++) {
    progress("Converting page " + n + " of " + doc.numPages + "…");
    const page = await doc.getPage(n);
    pages.push(await renderPage(page, PAGE_LONG_SIDE, 0.8));
    if (n === 1) thumb = await renderPage(page, THUMB_LONG_SIDE, 0.7);
    lines.push(...itemsToLines((await page.getTextContent()).items));
  }
  const fallback = name.replace(/\.pdf$/i, "").replace(/[_-]+/g, " ").trim() || "Recipe";
  return { pages, thumb, drafts: parseRecipeText(lines, fallback) };
}

let job = null;  // the import being edited

function fillDraft(i) {
  const d = job.drafts[i];
  $("rec-title").value = d.title;
  $("rec-servings").value = d.servings || "";
  $("rec-ingredients").value = d.ingredients.map(ingredientToLine).join("\n");
  $("rec-steps").value = d.steps.map((s) => s.text).join("\n");
}

// Edit a recipe that is already on the device (text only; its pages and photo stay as they are).
async function startEdit(id) {
  msg("rec-msg", "");
  let recipe;
  try { recipe = await api("/api/recipes/get?id=" + encodeURIComponent(id)); }
  catch (err) { msg("rec-msg", "Could not open this recipe: " + err.message, "err"); return; }
  job = { edit: { id, original: recipe }, pages: [] };
  $("rec-edit-title").textContent = "Edit recipe";
  $("rec-note").textContent = "Change the text and save. Pages and the photo are not changed. A step keeps its timer unless you change that step.";
  $("rec-pick-row").hidden = true;
  $("rec-pages").replaceChildren();
  $("rec-title").value = recipe.title || "";
  $("rec-servings").value = recipe.servings || "";
  $("rec-tags").value = (recipe.tags || []).join(", ");
  $("rec-ingredients").value = (recipe.ingredients || []).map(ingredientToLine).join("\n");
  $("rec-steps").value = (recipe.steps || []).map((s) => s.text).join("\n");
  $("rec-save").textContent = "Save changes";
  msg("rec-save-msg", "");
  $("rec-edit").hidden = false;
  $("rec-edit").scrollIntoView({ block: "nearest" });
}

async function saveEdit() {
  const recipe = applyEdit(job.edit.original, {
    title: $("rec-title").value, servings: $("rec-servings").value, tags: $("rec-tags").value,
    ingredientsText: $("rec-ingredients").value, stepsText: $("rec-steps").value });
  if (!recipe.title) { msg("rec-save-msg", "Give the recipe a title.", "err"); return; }
  $("rec-save").disabled = true;
  try {
    await api("/api/recipes/save", { method: "POST", json: { id: job.edit.id, recipe } });
    msg("rec-msg", "Saved changes to \u201C" + recipe.title + "\u201D.", "ok");
    $("rec-edit").hidden = true; job = null;
    await loaders.recipes();
  } catch (err) { msg("rec-save-msg", "Could not save: " + err.message, "err"); }
  $("rec-save").disabled = false;
}

async function startImport(bytes, name, fromInbox) {
  $("rec-edit").hidden = true;
  try {
    const result = await convert(bytes, name, (t) => msg("rec-msg", t));
    job = { ...result, name, bytes, fromInbox };
  } catch (err) { msg("rec-msg", "Could not read this PDF: " + err.message, "err"); return; }
  msg("rec-msg", "Converted " + job.pages.length + (job.pages.length === 1 ? " page." : " pages."), "ok");
  const found = job.drafts[0].found;
  $("rec-note").textContent = found
    ? "Found the recipe text. Check it and fix anything that looks wrong; the pages are saved too."
    : "No recipe text found (a scan or photo?). It will be saved with its pages only. You can type the ingredients and steps here to get step-by-step mode.";
  const pick = $("rec-pick"); pick.replaceChildren();
  job.drafts.forEach((d, i) => { const o = el("option", d.title); o.value = String(i); pick.append(o); });
  $("rec-pick-row").hidden = job.drafts.length < 2;
  const strip = $("rec-pages"); strip.replaceChildren();
  job.pages.slice(0, 8).forEach((b) => { const img = el("img"); img.src = URL.createObjectURL(b); img.alt = "page"; strip.append(img); });
  $("rec-tags").value = "";
  $("rec-edit-title").textContent = "Check the recipe";
  $("rec-save").textContent = "Save to device";
  fillDraft(0);
  msg("rec-save-msg", "");
  $("rec-edit").hidden = false;
  $("rec-edit").scrollIntoView({ block: "nearest" });
}

async function putFile(id, name, blob) {
  const r = await fetch("/api/recipes/file?id=" + encodeURIComponent(id) + "&name=" + encodeURIComponent(name), {
    method: "POST", headers: { "Content-Type": "application/octet-stream", "X-Summit-Upload": "1" }, body: blob });
  if (!r.ok) { let b = null; try { b = await r.json(); } catch (e) {} throw new Error((b && b.error) || ("HTTP " + r.status)); }
}

async function save() {
  if (job.edit) return saveEdit();
  const recipe = buildRecipe({
    title: $("rec-title").value, servings: $("rec-servings").value, tags: $("rec-tags").value,
    ingredientsText: $("rec-ingredients").value, stepsText: $("rec-steps").value, source: { type: "pdf", name: job.name } });
  if (!recipe.title) { msg("rec-save-msg", "Give the recipe a title.", "err"); return; }
  msg("rec-save-msg", "");
  const bar = $("rec-progress"); bar.hidden = false; bar.value = 0; $("rec-save").disabled = true;
  try {
    const { id } = await api("/api/recipes/save", { method: "POST", json: { recipe } });
    const total = job.pages.length + 2;
    for (let i = 0; i < job.pages.length; i++) {
      await putFile(id, "pages/" + String(i + 1).padStart(3, "0") + ".jpg", job.pages[i]);
      bar.value = Math.round(100 * (i + 1) / total);
    }
    if (job.thumb) await putFile(id, "thumb.jpg", job.thumb);
    bar.value = Math.round(100 * (job.pages.length + 1) / total);
    // Keep the original PDF with the recipe so it can be converted again later.
    if (job.fromInbox) await api("/api/recipes/inbox/done", { method: "POST", json: { name: job.name, id } });
    else await putFile(id, "original.pdf", new Blob([job.bytes], { type: "application/pdf" }));
    msg("rec-msg", "Saved “" + recipe.title + "” to the device.", "ok");
    $("rec-edit").hidden = true; $("rec-file").value = ""; job = null;
    await loaders.recipes();
  } catch (err) { msg("rec-save-msg", "Could not save: " + err.message, "err"); }
  bar.hidden = true; $("rec-save").disabled = false;
}

loaders.recipes = async () => {
  let data;
  try { data = await api("/api/recipes"); }
  catch (err) { $("rec-nocard").hidden = false; $("rec-main").hidden = true; return; }
  $("rec-nocard").hidden = true; $("rec-main").hidden = false;
  const inbox = $("rec-inbox"); inbox.replaceChildren();
  for (const name of data.inbox) {
    const tr = el("tr"), action = el("td");
    const btn = el("button", "Convert", "btn"); btn.dataset.inbox = name;
    btn.addEventListener("click", async () => {
      msg("rec-msg", "Fetching " + name + " from the device…");
      try { const r = await fetch("/api/recipes/inbox?name=" + encodeURIComponent(name));
        if (!r.ok) throw new Error("HTTP " + r.status);
        await startImport(await r.arrayBuffer(), name, true); }
      catch (err) { msg("rec-msg", "Could not fetch " + name + ": " + err.message, "err"); }
    });
    action.append(btn); tr.append(el("td", name), action); inbox.append(tr);
  }
  $("rec-inbox-card").hidden = data.inbox.length === 0;
  const list = $("rec-list"); list.replaceChildren();
  for (const r of data.recipes) {
    const tr = el("tr"); tr.dataset.recipe = r.id;
    const pic = el("td");
    if (r.pages > 0) { const img = el("img"); img.alt = ""; img.src = "/api/recipes/file?id=" + encodeURIComponent(r.id) + "&name=thumb.jpg"; img.onerror = () => img.remove(); pic.append(img); }
    const kinds = [r.has_card ? "steps" : "", r.pages ? r.pages + (r.pages === 1 ? " page" : " pages") : ""].filter(Boolean).join(" + ");
    tr.append(pic, el("td", r.title), el("td", kinds, "muted"), el("td", r.tags.map((t) => "#" + t).join(" "), "muted"));
    const edit = el("button", "Edit", "btn"); edit.dataset.edit = r.id;
    edit.addEventListener("click", () => startEdit(r.id));
    const act = el("td"); act.append(edit); tr.append(act);
    list.append(tr);
  }
  $("rec-none").hidden = data.recipes.length > 0;
};

$("rec-file").addEventListener("change", async () => {
  const file = $("rec-file").files[0]; if (!file) return;
  if (file.size > 32 * 1024 * 1024) { msg("rec-msg", "This PDF is larger than 32 MB.", "err"); return; }
  await startImport(await file.arrayBuffer(), file.name, false);
});
$("rec-url-form").addEventListener("submit", async (e) => {
  e.preventDefault();
  $("rec-url-go").disabled = true; msg("rec-url-msg", "The device is fetching the page…");
  try {
    const r = await api("/api/recipes/import-url", { method: "POST", json: { url: $("rec-url").value.trim() } });
    msg("rec-url-msg", "Imported \u201C" + r.title + "\u201D: " + r.ingredients + " ingredients, " + r.steps + " steps" + (r.photo ? ", with its photo." : "."), "ok");
    $("rec-url").value = ""; await loaders.recipes();
  } catch (err) { msg("rec-url-msg", err.message, "err"); }
  $("rec-url-go").disabled = false;
});
$("rec-pick").addEventListener("change", () => fillDraft(+$("rec-pick").value));
$("rec-save").addEventListener("click", save);
$("rec-cancel").addEventListener("click", () => { $("rec-edit").hidden = true; $("rec-file").value = ""; job = null; msg("rec-msg", ""); });
window.summit.recipesReady = true;
