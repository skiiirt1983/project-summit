// Turns text extracted from a recipe PDF into draft recipes for the edit screen.
// Pure functions, no DOM: unit-tested in Node (tests/web/test_recipe_import.mjs).
// The user always reviews the draft; the device validates what is saved.

// A heading line: the keyword, optionally followed by a colon, a parenthesis, or "serves 4" / "for 4 servings" (printed
// web pages and cookbooks write "Ingredients (serves 4)", "INGREDIENTS - 4 servings", "Ingredients:"). The line must
// still be only the heading: a sentence that starts with the word is not one.
const HEAD_TAIL = String.raw`(?:\s*[:\-\u2013\u2014]?\s*(?:\([^)]{0,40}\)|(?:serves|servings?|makes|yields?|for)\s*:?\s*\d{1,3}(?:\s*servings?)?|\d{1,3}\s*servings?))?\s*:?`;
const ING_HEADING = new RegExp(
  "^(?:ingredients?(?:\\s+list)?|what you(?:['\\u2019]ll|\\s+will)?\\s+need|you(?:['\\u2019]ll|\\s+will)\\s+need)" + HEAD_TAIL + "$", "i");
// "For the Muffins:", "Optional Icing:": a sub-heading inside the ingredients (not a new recipe, and not an ingredient).
const SUB_HEADING = /^[^\d:]{2,40}:$/;
const METHOD_HEADING = new RegExp(
  "^(?:method|directions?|instructions?|cooking instructions?|steps?|preparation(?:\\s+steps?)?|how to make(?: it)?)" + HEAD_TAIL + "$", "i");
const SERVINGS = /\b(serves|servings|makes|yields?)\s*:?\s*(\d{1,3})\b/i;
const FOOTER = /^(-\s*\d+\s*-|page\s+\d+(\s+of\s+\d+)?|\d+)$/i;
const NUMBERED = /^(?:step\s+)?(\d{1,2})\s*[.):]\s+/i;
const BULLET = /^[•‣◦⁃∙*\-–\uF0B7\uF0A7\uF076\uF0D8\u25AA\u25CF\u2022]\s*/;  // incl. the private-use bullets Word exports

const VULGAR = { "½": 0.5, "¼": 0.25, "¾": 0.75, "⅓": 1 / 3, "⅔": 2 / 3, "⅛": 0.125 };
const UNITS = {
  cup: "cup", cups: "cup", tbsp: "tbsp", tablespoon: "tbsp", tablespoons: "tbsp", tsp: "tsp", teaspoon: "tsp",
  teaspoons: "tsp", g: "g", gram: "g", grams: "g", kg: "kg", ml: "ml", l: "l", litre: "l", litres: "l", liter: "l",
  liters: "l", oz: "oz", ounce: "oz", ounces: "oz", lb: "lb", lbs: "lb", pound: "lb", pounds: "lb", pinch: "pinch",
  clove: "clove", cloves: "clove", can: "can", cans: "can", tin: "tin", tins: "tin", slice: "slice", slices: "slice",
  stick: "stick", sticks: "stick", bunch: "bunch",
};

function number(text) {
  // "1 1/2", "3/4", "1½", "½", "2.5", "2-3" (lower bound). Returns [value, rest] or null.
  let m = text.match(/^(\d+)\s+(\d+)\/(\d+)\s*/);
  if (m && +m[3] > 0) return [+m[1] + +m[2] / +m[3], text.slice(m[0].length)];
  m = text.match(/^(\d+)\/(\d+)\s*/);
  if (m && +m[2] > 0) return [+m[1] / +m[2], text.slice(m[0].length)];
  m = text.match(/^(\d+)?\s*([½¼¾⅓⅔⅛])\s*/);
  if (m) return [(m[1] ? +m[1] : 0) + VULGAR[m[2]], text.slice(m[0].length)];
  m = text.match(/^(\d+(?:\.\d+)?)(?:\s*[-–]\s*\d+(?:\.\d+)?)?\s*/);
  if (m) return [+m[1], text.slice(m[0].length)];
  return null;
}

// "1 1/2 cups flour, sifted" -> {qty: 1.5, unit: "cup", item: "flour", note: "sifted"}
export function parseIngredient(line) {
  let text = line.trim().replace(BULLET, "").trim();
  const out = { qty: null, unit: "", item: "", note: "" };
  const n = number(text);
  if (n && n[0] > 0) {
    out.qty = Math.round(n[0] * 1000) / 1000;
    text = n[1];
    const u = text.match(/^([A-Za-z]+)\.?(?=\s|$)\s*/);
    if (u && UNITS[u[1].toLowerCase()]) {
      out.unit = UNITS[u[1].toLowerCase()];
      text = text.slice(u[0].length).replace(/^of\s+/i, "");
    }
  }
  const comma = text.indexOf(",");
  out.item = (comma < 0 ? text : text.slice(0, comma)).trim();
  out.note = comma < 0 ? "" : text.slice(comma + 1).trim();
  if (!out.item) { out.item = line.trim(); out.qty = null; out.unit = ""; out.note = ""; }
  return out;
}

// Method lines -> steps. Numbered lines start a step; without numbering, a line
// ending a sentence ends the step (wrapped lines are joined).
export function parseSteps(lines) {
  const numbered = lines.some((l) => NUMBERED.test(l));
  const steps = [];
  let current = "";
  for (const raw of lines) {
    const line = raw.replace(BULLET, "").trim();
    if (!line) continue;
    if (numbered) {
      if (NUMBERED.test(line)) { if (current) steps.push(current); current = line.replace(NUMBERED, ""); }
      else current = current ? current + " " + line : line;
    } else {
      current = current ? current + " " + line : line;
      if (/[.!?]$/.test(line)) { steps.push(current); current = ""; }
    }
  }
  if (current) steps.push(current);
  return steps.map((text) => ({ text }));
}

function clean(lines) {
  return lines.map((l) => l.replace(/\s+/g, " ").trim()).filter((l) => l && !FOOTER.test(l));
}

// "Ingredients 2 slices bread, butter" -> "Ingredients" and "2 slices bread, butter". Only when what follows starts with a
// quantity: "Ingredients are cheap" is a sentence, not a heading.
function splitInlineHeadings(lines) {
  const out = [];
  for (const l of lines) {
    if (ING_HEADING.test(l)) { out.push(l); continue; }  // already a whole heading ("Ingredients - 4 servings")
    const m = l.match(/^(ingredients?)\s*[:\-\u2013\u2014]?\s+((?:\d|[\u00BD\u00BC\u00BE\u2153\u2154\u215B]).*)$/i);
    if (m) out.push(m[1], m[2]);
    else out.push(l);
  }
  return out;
}

// "Prep Time Cook Time Total Time Servings" followed by "10 minutes 20 minutes 30 minutes 18 muffins" (the table printed web
// recipes start with) -> "Prep Time: 10 minutes", ..., "Servings: 18 muffins". Left alone when the values do not line up.
const TABLE_LABEL = /prep(?:aration)? time|cook(?:ing)? time|total time|ready in|active time|servings?|yields?|makes|serves/gi;
function readTimeTables(lines) {
  const out = [];
  for (let i = 0; i < lines.length; i++) {
    const labels = lines[i].match(TABLE_LABEL);
    if (labels && labels.length >= 2 && lines[i].replace(TABLE_LABEL, "").replace(/[\s|:]+/g, "") === "" && i + 1 < lines.length) {
      const values = lines[i + 1].match(/\d+(?:[./]\d+)?(?:\s*-\s*\d+)?\s+[A-Za-z]+/g);
      if (values && values.length === labels.length) {
        labels.forEach((l, k) => out.push(l.replace(/^./, (c) => c.toUpperCase()) + ": " + values[k]));
        i++;
        continue;
      }
    }
    out.push(lines[i]);
  }
  return out;
}

// lines: the document's text lines in reading order. Returns one draft per recipe
// found (a page can hold several). found=false: no ingredients heading, so no card.
export function parseRecipeText(inputLines, fallbackTitle = "Recipe") {
  const lines = splitInlineHeadings(readTimeTables(clean(inputLines)));
  const heads = [];
  lines.forEach((l, i) => { if (ING_HEADING.test(l)) heads.push(i); });
  if (!heads.length) {
    const title = lines[0] && lines[0].length <= 80 ? lines[0] : fallbackTitle;
    if (lines.length >= 3) {
      // There is text, only no ingredients heading was recognised: keep all of it (in the steps box) so nothing is lost
      // and the person can move the ingredient lines over.
      const rest = title === lines[0] ? lines.slice(1) : lines;
      return [{ title, servings: 0, ingredients: [], steps: rest.map((text) => ({ text })), found: false, hasText: true }];
    }
    return [{ title, servings: 0, ingredients: [], steps: [], found: false }];
  }
  // Where each recipe starts: its title is the line before the ingredients heading,
  // skipping a "Serves 4" line; the first recipe starts at the top.
  const starts = heads.map((h, k) => {
    if (k === 0) return 0;
    let t = h - 1;
    if (t > heads[k - 1] && SERVINGS.test(lines[t])) t -= 1;
    return Math.max(t, heads[k - 1] + 1);
  });
  return heads.map((h, k) => {
    const end = k + 1 < heads.length ? starts[k + 1] : lines.length;
    const intro = lines.slice(starts[k], h);
    const body = lines.slice(h + 1, end);
    let m = body.findIndex((l) => METHOD_HEADING.test(l));
    let ingLines, stepLines;
    if (m >= 0) { ingLines = body.slice(0, m); stepLines = body.slice(m + 1); }
    else {
      m = body.findIndex((l) => NUMBERED.test(l) && !number(l.replace(NUMBERED, "")));  // "1. Whisk..." not "1. 2 eggs"
      ingLines = m >= 0 ? body.slice(0, m) : body;
      stepLines = m >= 0 ? body.slice(m) : [];
    }
    const servingsLine = intro.concat(ingLines.slice(0, 2)).map((l) => l.match(SERVINGS)).find(Boolean);
    const title = intro.find((l) => !SERVINGS.test(l)) || fallbackTitle;
    return {
      title: title.slice(0, 120),
      servings: servingsLine ? +servingsLine[2] : 0,
      ingredients: ingLines.filter((l) => !SERVINGS.test(l) && !SUB_HEADING.test(l.replace(BULLET, '').trim())).map(parseIngredient),
      steps: parseSteps(stepLines),
      found: true,
    };
  });
}

// pdf.js text items of one page -> lines, top to bottom, left to right. A page laid out in two or more columns (a recipe
// with Ingredients on the left and Method on the right) is read column by column instead of line by line across them.
const GAP_PT = 24;           // a horizontal gap this wide inside a row starts a new segment
const COLUMN_APART_PT = 100; // column starts at least this far apart are columns; closer ones are table cells
const SPACE_EM = 0.15;  // a gap wider than this (in font sizes) is a space; pieces of one word touch (kerning, "lem"+"on")
const TABLE_HEAD = /^(?:prep(?:aration)?(?: time)?|cook(?:ing)?(?: time)?|total(?: time)?|ready in|active time|servings?|yields?|makes|serves)$/i;
export function itemsToLines(items) {
  const rows = [];
  for (const it of items) {
    if (!it.str || !it.str.trim()) continue;
    const y = it.transform[5], x = it.transform[4];
    const size = Math.hypot(it.transform[0], it.transform[1]) || 10;
    const w = it.width > 0 ? it.width : it.str.length * size * 0.5;
    let row = rows.find((r) => Math.abs(r.y - y) < 3);
    if (!row) { row = { y, parts: [] }; rows.push(row); }
    row.parts.push({ x, w, size, str: it.str });
  }
  rows.sort((a, b) => b.y - a.y);
  for (const r of rows) {
    r.parts.sort((a, b) => a.x - b.x);
    r.segs = [];
    for (const p of r.parts) {
      const last = r.segs[r.segs.length - 1];
      const gap = last ? p.x - last.end : 0;
      if (last && gap <= GAP_PT) {
        const space = gap > SPACE_EM * p.size && !/\s$/.test(last.text) && !/^\s/.test(p.str);
        last.text += (space ? " " : "") + p.str;
        last.end = p.x + p.w;
      } else r.segs.push({ x: p.x, end: p.x + p.w, text: p.str });
    }
  }
  // "Prep Time | Cook Time | Total Time | Servings" over "10 minutes | 20 minutes | 30 minutes | 18 muffins" (printed web
  // recipes): turn each pair into "Servings: 18 muffins" so the servings are found.
  for (let i = 0; i + 1 < rows.length; i++) {
    const h = rows[i], v = rows[i + 1];
    if (h.segs.length >= 2 && h.segs.length === v.segs.length && h.segs.every((g) => TABLE_HEAD.test(g.text.trim()))) {
      h.segs = [{ x: h.segs[0].x, end: 0, text: h.segs.map((g, k) => g.text.trim() + ": " + v.segs[k].text.trim()).join("  |  ") }];
      rows.splice(i + 1, 1);
    }
  }
  const clean = (t) => t.replace(/\s+/g, " ").trim();
  const cols = columnStarts(rows);
  if (cols.length < 2) return rows.flatMap((r) => r.segs.map((g) => g.text).join(" ").split("  |  ").map(clean));
  const colOf = (x) => { let c = 0; for (let i = 0; i < cols.length; i++) if (x >= cols[i] - 10) c = i; return c; };
  const perCol = cols.map(() => []);
  for (const r of rows) {
    const byCol = new Map();
    for (const g of r.segs) { const c = colOf(g.x); byCol.set(c, (byCol.get(c) ? byCol.get(c) + " " : "") + g.text); }
    for (const [c, t] of byCol) perCol[c].push(...t.split("  |  ").map(clean));
  }
  return perCol.flat().filter(Boolean);
}

// The x positions where columns start, or [] when the page is a single column. A column is a start position shared by at
// least 3 segments, and columns must be COLUMN_APART_PT apart (a quantity next to its ingredient is a table, not columns).
function columnStarts(rows) {
  const multi = rows.filter((r) => r.segs.length >= 2);
  if (multi.length < 2) return [];
  const xs = rows.flatMap((r) => r.segs.map((g) => g.x)).sort((a, b) => a - b);
  const clusters = [];
  for (const x of xs) {
    const c = clusters[clusters.length - 1];
    if (c && x - c.last <= 10) { c.count++; c.last = x; } else clusters.push({ x, last: x, count: 1 });
  }
  const starts = [];
  for (const c of clusters.filter((k) => k.count >= 3)) if (!starts.length || c.x - starts[starts.length - 1] >= COLUMN_APART_PT) starts.push(c.x);
  return starts.length >= 2 ? starts : [];
}

// Edit-screen text areas -> the recipe object the device accepts (recipe.json format 1).
export function buildRecipe({ title, servings, tags, ingredientsText, stepsText, source }) {
  const lines = (t) => t.split("\n").map((l) => l.trim()).filter(Boolean);
  const recipe = {
    format: 1, title: title.trim(), ingredients: lines(ingredientsText).map(parseIngredient),
    steps: lines(stepsText).map((text) => ({ text })), source,
    tags: tags.split(",").map((t) => t.trim().toLowerCase()).filter(Boolean),
    created: new Date().toISOString().slice(0, 19),
  };
  const n = parseFloat(servings);
  if (n > 0) recipe.servings = n;
  return recipe;
}

// Editing a saved recipe: the form replaces title, servings, tags, ingredients and steps.
// Everything else (source, created, fields this page does not know) is kept, and a step
// whose text is unchanged keeps its timer.
export function applyEdit(original, form) {
  const edited = buildRecipe({ ...form, source: original.source });
  const timers = new Map((original.steps || []).filter((s) => s.timer_s).map((s) => [s.text.trim(), s.timer_s]));
  const out = { ...original, title: edited.title, ingredients: edited.ingredients, tags: edited.tags,
    steps: edited.steps.map((s) => (timers.has(s.text) ? { text: s.text, timer_s: timers.get(s.text) } : s)) };
  if (edited.servings) out.servings = edited.servings; else delete out.servings;
  return out;
}

export function ingredientToLine(g) {
  const frac = (q) => {
    const whole = Math.floor(q), rest = q - whole;
    const table = [[0.125, "1/8"], [0.25, "1/4"], [1 / 3, "1/3"], [0.5, "1/2"], [2 / 3, "2/3"], [0.75, "3/4"]];
    const hit = table.find(([v]) => Math.abs(rest - v) < 0.02);
    if (rest < 0.02) return String(whole);
    return hit ? (whole ? whole + " " : "") + hit[1] : String(q);
  };
  return [g.qty === null ? "" : frac(g.qty), g.unit, g.item].filter(Boolean).join(" ") + (g.note ? ", " + g.note : "");
}
