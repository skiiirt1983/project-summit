// Node unit tests for firmware/web/page/recipe_import.mjs (no browser needed).
import assert from "node:assert/strict";
import { applyEdit, buildRecipe, ingredientToLine, itemsToLines, parseIngredient, parseRecipeText, parseSteps } from "../../firmware/web/page/recipe_import.mjs";

let n = 0;
const eq = (a, b, what) => { n++; assert.deepEqual(a, b, what); };

// ---- ingredients
eq(parseIngredient("1 1/2 cups flour, sifted"), { qty: 1.5, unit: "cup", item: "flour", note: "sifted" });
eq(parseIngredient("• 2 eggs"), { qty: 2, unit: "", item: "eggs", note: "" });
eq(parseIngredient("250 ml milk"), { qty: 250, unit: "ml", item: "milk", note: "" });
eq(parseIngredient("3/4 cup sugar"), { qty: 0.75, unit: "cup", item: "sugar", note: "" });
eq(parseIngredient("1½ tbsp. olive oil"), { qty: 1.5, unit: "tbsp", item: "olive oil", note: "" });
eq(parseIngredient("½ tsp salt"), { qty: 0.5, unit: "tsp", item: "salt", note: "" });
eq(parseIngredient("2-3 cloves of garlic, crushed"), { qty: 2, unit: "clove", item: "garlic", note: "crushed" });
eq(parseIngredient("Salt and pepper, to taste"), { qty: null, unit: "", item: "Salt and pepper", note: "to taste" });
eq(parseIngredient("2 Tablespoons butter"), { qty: 2, unit: "tbsp", item: "butter", note: "" });
eq(parseIngredient("6 apples, peeled and sliced"), { qty: 6, unit: "", item: "apples", note: "peeled and sliced" });
eq(parseIngredient("2 large onions"), { qty: 2, unit: "", item: "large onions", note: "" });
eq(ingredientToLine({ qty: 1.5, unit: "cup", item: "flour", note: "sifted" }), "1 1/2 cup flour, sifted");
eq(ingredientToLine({ qty: null, unit: "", item: "salt", note: "" }), "salt");
eq(parseIngredient(ingredientToLine({ qty: 0.75, unit: "cup", item: "sugar", note: "" })).qty, 0.75);

// ---- steps
eq(parseSteps(["1. Heat the oven to 180 degrees and", "line a loaf tin.", "2) Beat butter and sugar.", "Step 3: Bake 45 minutes."]),
  [{ text: "Heat the oven to 180 degrees and line a loaf tin." }, { text: "Beat butter and sugar." }, { text: "Bake 45 minutes." }]);
eq(parseSteps(["Whisk everything", "together until smooth.", "Rest for 30 minutes."]),
  [{ text: "Whisk everything together until smooth." }, { text: "Rest for 30 minutes." }]);

// ---- a whole recipe
const cake = parseRecipeText(["Lemon Drizzle Cake", "Serves 8", "Ingredients", "225 g butter, softened", "225 g caster sugar",
  "4 eggs", "1 lemon, zested", "Method", "1. Heat the oven to 180 degrees.", "2. Beat the butter and sugar, then add the eggs.",
  "3. Bake for 45-50 minutes.", "- 1 -"]);
eq(cake.length, 1);
eq([cake[0].title, cake[0].servings, cake[0].found], ["Lemon Drizzle Cake", 8, true]);
eq(cake[0].ingredients.length, 4);
eq(cake[0].ingredients[0], { qty: 225, unit: "g", item: "butter", note: "softened" });
eq(cake[0].steps.map((s) => s.text), ["Heat the oven to 180 degrees.", "Beat the butter and sugar, then add the eggs.", "Bake for 45-50 minutes."]);

// ---- two recipes on one page
const two = parseRecipeText(["Guacamole", "Ingredients:", "2 avocados", "1 lime", "Directions", "Mash the avocados.", "Add lime juice.",
  "Salsa", "Makes 4", "INGREDIENTS", "4 tomatoes", "1 onion", "Instructions", "Chop everything.", "Mix and chill for 1 hour."]);
eq(two.map((r) => r.title), ["Guacamole", "Salsa"]);
eq(two[0].steps.length, 2);
eq(two[0].ingredients.length, 2);
eq([two[1].servings, two[1].ingredients.length, two[1].steps.length], [4, 2, 2]);

// ---- no method heading: numbered steps start the method; numbered ingredients do not
const nohead = parseRecipeText(["Toast", "Ingredients", "2 slices bread", "1. Toast the bread.", "2. Butter it."]);
eq([nohead[0].ingredients.length, nohead[0].steps.length], [1, 2]);

// ---- scanned / no text
eq(parseRecipeText([], "scan0001")[0], { title: "scan0001", servings: 0, ingredients: [], steps: [], found: false });
eq(parseRecipeText(["Grandma's notes", "a pinch of this and that"], "x")[0].found, false);

// ---- pdf.js text items -> lines
eq(itemsToLines([
  { str: "Serves 8", transform: [1, 0, 0, 1, 50, 700] }, { str: "Cake", transform: [1, 0, 0, 1, 120, 750.4] },
  { str: "Lemon", transform: [1, 0, 0, 1, 50, 750] }, { str: " ", transform: [1, 0, 0, 1, 0, 0] }]), ["Lemon Cake", "Serves 8"]);

// ---- edit screen -> recipe.json
const built = buildRecipe({ title: " Lemon Cake ", servings: "8", tags: "Dessert, baking,", ingredientsText: "225 g butter\n\n4 eggs",
  stepsText: "Mix.\nBake 45 minutes.", source: { type: "pdf", name: "cake.pdf" } });
eq([built.format, built.title, built.servings, built.tags], [1, "Lemon Cake", 8, ["dessert", "baking"]]);
eq(built.ingredients[0], { qty: 225, unit: "g", item: "butter", note: "" });
eq(built.steps.length, 2);
eq("servings" in buildRecipe({ title: "x", servings: "", tags: "", ingredientsText: "", stepsText: "", source: { type: "manual" } }), false);

// Editing a saved recipe keeps what the form does not show.
const saved = { format: 1, title: "Pancakes", servings: 4, created: "2026-11-12T09:00:00", source: { type: "url", url: "https://example.com/p" },
  photo: "photo.jpg", tags: ["breakfast"], ingredients: [{ qty: 2, unit: "", item: "eggs", note: "" }],
  steps: [{ text: "Whisk." }, { text: "Rest the batter for 30 minutes.", timer_s: 1800 }, { text: "Fry.", timer_s: 120 }] };
const edited = applyEdit(saved, { title: "Thin pancakes", servings: "", tags: "Breakfast, quick",
  ingredientsText: "3 eggs\n250 ml milk", stepsText: "Whisk well.\nRest the batter for 30 minutes.\nFry until golden." });
eq([edited.title, edited.created, edited.source, edited.photo, edited.format], ["Thin pancakes", saved.created, saved.source, "photo.jpg", 1]);
eq("servings" in edited, false);
eq(edited.tags, ["breakfast", "quick"]);
eq(edited.ingredients.map((g) => g.item), ["eggs", "milk"]);
eq(edited.steps, [{ text: "Whisk well." }, { text: "Rest the batter for 30 minutes.", timer_s: 1800 }, { text: "Fry until golden." }]);
eq(saved.steps.length === 3 && saved.title === "Pancakes" && saved.servings === 4, true);  // the original is not changed

console.log(`recipe import parser: ${n} checks, 0 failures`);
