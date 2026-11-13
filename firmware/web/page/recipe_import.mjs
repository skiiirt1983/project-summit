// Turns text extracted from a recipe PDF into draft recipes for the edit screen.
// Pure functions, no DOM: unit-tested in Node (tests/web/test_recipe_import.mjs).
// The user always reviews the draft; the device validates what is saved.

const ING_HEADING = /^(ingredients?|what you need|you will need|for the \w+)\s*:?$/i;
const METHOD_HEADING = /^(method|directions?|instructions?|steps?|preparation|how to make( it)?)\s*:?$/i;
const SERVINGS = /\b(serves|servings|makes|yields?)\s*:?\s*(\d{1,3})\b/i;
const FOOTER = /^(-\s*\d+\s*-|page\s+\d+(\s+of\s+\d+)?|\d+)$/i;
const NUMBERED = /^(?:step\s+)?(\d{1,2})\s*[.):]\s+/i;
const BULLET = /^[•‣◦⁃∙*\-–]\s+/;

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

// lines: the document's text lines in reading order. Returns one draft per recipe
// found (a page can hold several). found=false: no ingredients heading, so no card.
export function parseRecipeText(inputLines, fallbackTitle = "Recipe") {
  const lines = clean(inputLines);
  const heads = [];
  lines.forEach((l, i) => { if (ING_HEADING.test(l)) heads.push(i); });
  if (!heads.length) {
    return [{ title: lines[0] && lines[0].length <= 80 ? lines[0] : fallbackTitle, servings: 0, ingredients: [], steps: [], found: false }];
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
      ingredients: ingLines.filter((l) => !SERVINGS.test(l)).map(parseIngredient),
      steps: parseSteps(stepLines),
      found: true,
    };
  });
}

// pdf.js text items of one page -> lines, top to bottom, left to right.
export function itemsToLines(items) {
  const rows = [];
  for (const it of items) {
    if (!it.str || !it.str.trim()) continue;
    const y = it.transform[5], x = it.transform[4];
    let row = rows.find((r) => Math.abs(r.y - y) < 3);
    if (!row) { row = { y, parts: [] }; rows.push(row); }
    row.parts.push({ x, str: it.str });
  }
  rows.sort((a, b) => b.y - a.y);
  return rows.map((r) => r.parts.sort((a, b) => a.x - b.x).map((p) => p.str).join(" ").replace(/\s+/g, " ").trim());
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
