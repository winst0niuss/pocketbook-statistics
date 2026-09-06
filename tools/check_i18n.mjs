#!/usr/bin/env node
/* Checks the 29 catalogs in qt/qml/i18n against English and against the four
 * places a language has to be listed.
 *
 * None of this is visible on the device until the wrong screen is opened in the
 * wrong language: a key a catalog is missing falls back to English silently, a
 * placeholder that was renamed in a translation renders as "{version}" in the
 * middle of a sentence, and a plural array one form too short shows the wrong
 * word for every count that needs the last form. The catalogs are also the one
 * part of the app nothing else compiles, checks or runs.
 *
 * Keys a catalog has never had are the one exception: 27 of them were left
 * behind when later screens were written, and those are recorded per language
 * in i18n/untranslated.json. A gap in that file is a known debt; a gap that is
 * not is a new key someone forgot, and fails the build. Translate one and the
 * file has to lose its row, or the check says so — the debt can only shrink.
 * `node tools/check_i18n.mjs --update-baseline` rewrites it.
 *
 * Run from `make test`; needs node and nothing else.
 */

import { readFileSync, readdirSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const qml = join(root, "qt", "qml");
const i18n = join(qml, "i18n");

const problems = [];
const fail = (where, msg) => problems.push(`${where}: ${msg}`);

/* ---- loading a catalog -------------------------------------------------- */

/* The catalogs are QML JS libraries: `.pragma library`, then plain top-level
 * declarations. Dropping the pragma leaves ordinary script that Function can
 * evaluate; nothing in them touches QML. */
function loadCatalog(file) {
    const src = readFileSync(join(i18n, file), "utf8").replace(/^\s*\.pragma\s+library\s*$/m, "");
    const factory = new Function(
        `${src}\n; return { strings: typeof strings === "undefined" ? undefined : strings,` +
            ` plural: typeof plural === "undefined" ? undefined : plural };`);
    return factory();
}

/* A JS object literal keeps the last of two identical keys, so a duplicated
 * entry is a translation that silently does not apply. */
function duplicateKeys(file) {
    const src = readFileSync(join(i18n, file), "utf8");
    const seen = new Set();
    const dupes = [];
    for (const m of src.matchAll(/^\s+"([^"]+)"\s*:/gm)) {
        if (seen.has(m[1]))
            dupes.push(m[1]);
        seen.add(m[1]);
    }
    return dupes;
}

const placeholders = (s) =>
    new Set([...String(s).matchAll(/\{(\w+)\}/g)].map((m) => m[1]));

/* ---- what the call sites actually pass ---------------------------------- */

/* Tr.t("key", { d: …, month: … }) — the names in that object are what a
 * translation may use, over and above the ones English happens to use. It is
 * how "{month} {d}" and Russian's "{d} {monthGen}" are both correct. */
function callSitePlaceholders() {
    const provided = new Map();
    for (const file of readdirSync(qml).filter((f) => f.endsWith(".qml"))) {
        const src = readFileSync(join(qml, file), "utf8");
        /* Tr.t(…) at a call site, and the bare t(…) inside Tr.qml itself —
         * fmtDayMonth passes the month in two forms and only one of them is in
         * the English string. */
        for (const m of src.matchAll(/(?<![\w.])(?:Tr\.)?t\(\s*"([^"]+)"\s*,\s*\{/g)) {
            const key = m[1];
            /* Scan to the matching brace; the argument objects here are one
             * level deep, but nested calls with braces do appear. */
            let depth = 0;
            let end = m.index + m[0].length - 1;
            for (let i = end; i < src.length; i++) {
                if (src[i] === "{") depth++;
                else if (src[i] === "}" && --depth === 0) { end = i; break; }
            }
            const body = src.slice(m.index + m[0].length, end);
            const names = provided.get(key) ?? new Set();
            let depth2 = 0;
            for (const part of body.split(/([{}])/)) {
                if (part === "{") { depth2++; continue; }
                if (part === "}") { depth2--; continue; }
                if (depth2 !== 0) continue;
                for (const n of part.matchAll(/(?:^|,)\s*(\w+)\s*:/g))
                    names.add(n[1]);
            }
            provided.set(key, names);
        }
    }
    return provided;
}

/* ---- the four lists a language has to appear in ------------------------- */

function listedLanguages() {
    const tr = readFileSync(join(qml, "Tr.qml"), "utf8");
    const imports = new Set(
        [...tr.matchAll(/^import\s+"i18n\/(\w+)\.js"\s+as\s+(\w+)$/gm)].map((m) => m[2]));
    const importedFile = new Map(
        [...tr.matchAll(/^import\s+"i18n\/(\w+)\.js"\s+as\s+(\w+)$/gm)].map((m) => [m[2], m[1]]));
    const table = new Map(
        [...tr.matchAll(/^\s+"(\w\w)":\s*(Lang\w+)/gm)].map((m) => [m[1], m[2]]));

    const qrc = new Set(
        [...readFileSync(join(qml, "pocketbook-statistics.qrc"), "utf8")
            .matchAll(/<file>i18n\/(\w+)\.js<\/file>/g)].map((m) => m[1]));

    const installer = new Set(
        [...readFileSync(join(root, "qt", "src", "installer.cpp"), "utf8")
            .matchAll(/\{"(\w\w)",\s*"/g)].map((m) => m[1]));

    return { imports, importedFile, table, qrc, installer };
}

/* ---- the checks --------------------------------------------------------- */

const files = readdirSync(i18n).filter((f) => f.endsWith(".js")).sort();
if (!files.includes("en.js")) {
    console.error("check_i18n: no en.js — there is nothing to check against");
    process.exit(1);
}

const baselinePath = join(i18n, "untranslated.json");
const updateBaseline = process.argv.includes("--update-baseline");
let baseline = {};
try {
    baseline = JSON.parse(readFileSync(baselinePath, "utf8"));
} catch (e) {
    if (!updateBaseline)
        fail("untranslated.json", `cannot be read: ${e.message}`);
}
const gaps = {}; /* what is actually missing right now, per catalog */

const english = loadCatalog("en.js");
const provided = callSitePlaceholders();

for (const file of files) {
    let cat;
    try {
        cat = loadCatalog(file);
    } catch (e) {
        fail(file, `does not parse: ${e.message}`);
        continue;
    }
    if (!cat.strings || typeof cat.strings !== "object") {
        fail(file, "exports no `strings` object");
        continue;
    }
    if (typeof cat.plural !== "function") {
        fail(file, "exports no `plural(n)` function");
        continue;
    }
    for (const key of duplicateKeys(file))
        fail(file, `"${key}" appears twice — the second entry wins and the first is dead`);

    const keys = new Set(Object.keys(cat.strings));
    const known = new Set(baseline[file.replace(/\.js$/, "")] ?? []);
    for (const key of Object.keys(english.strings)) {
        if (keys.has(key))
            continue;
        (gaps[file.replace(/\.js$/, "")] ??= []).push(key);
        if (!known.has(key))
            fail(file, `missing "${key}" — the screen falls back to English.` +
                ` Translate it, or record it in i18n/untranslated.json`);
    }
    for (const key of keys)
        if (!(key in english.strings))
            fail(file, `"${key}" is in no other catalog — a stale key nothing reads`);

    for (const [key, want] of Object.entries(english.strings)) {
        const got = cat.strings[key];
        if (got === undefined)
            continue;
        if (Array.isArray(want) !== Array.isArray(got)) {
            fail(file, `"${key}" is ${Array.isArray(got) ? "an array" : "a string"},` +
                ` English has ${Array.isArray(want) ? "an array" : "a string"}`);
            continue;
        }
        /* Fixed-length lists: a month short and the calendar draws the wrong
         * name; the plural arrays are checked against plural() below. */
        if (Array.isArray(want) && !key.startsWith("plural.") && want.length !== got.length)
            fail(file, `"${key}" has ${got.length} entries, English has ${want.length}`);

        const allowed = new Set([...placeholders(Array.isArray(want) ? want.join(" ") : want),
                                 ...(provided.get(key) ?? [])]);
        const strings = Array.isArray(got) ? got : [got];
        const used = new Set();
        for (const s of strings)
            for (const p of placeholders(s))
                used.add(p);
        for (const p of used)
            if (!allowed.has(p))
                fail(file, `"${key}" uses {${p}}, which nothing passes in`);
        /* A dropped placeholder loses a number or a date out of the sentence.
         * Substituting one of the call site's other names is fine — that is
         * how a language picks the genitive month over the nominative. */
        const wanted = placeholders(Array.isArray(want) ? want.join(" ") : want);
        const substituted = [...used].some((p) => !wanted.has(p));
        for (const p of wanted)
            if (!used.has(p) && !substituted)
                fail(file, `"${key}" drops {${p}}`);
    }

    /* Every index plural() can return must exist in every form array, or the
     * count picks a form that is not there and Tr.plural() clamps it to the
     * last one — the wrong word, silently. */
    let maxIndex = 0;
    for (let n = 0; n <= 1000; n++) {
        const idx = cat.plural(n);
        if (!Number.isInteger(idx) || idx < 0) {
            fail(file, `plural(${n}) returned ${idx}, which is not a form index`);
            break;
        }
        maxIndex = Math.max(maxIndex, idx);
    }
    for (const [key, forms] of Object.entries(cat.strings)) {
        if (!key.startsWith("plural.") || !Array.isArray(forms))
            continue;
        if (forms.length <= maxIndex)
            fail(file, `"${key}" has ${forms.length} forms, plural() returns up to ${maxIndex}`);
    }
}

/* Cross-file: a catalog that exists but is not imported, listed in the qrc and
 * mapped to a device language is a file that never reaches a reader. */
const { imports, importedFile, table, qrc, installer } = listedLanguages();
const importedFiles = new Set([...importedFile.values()].map((f) => `${f}.js`));

for (const file of files) {
    const lang = file.replace(/\.js$/, "");
    if (!importedFiles.has(file))
        fail("Tr.qml", `does not import i18n/${file}`);
    if (!qrc.has(lang))
        fail("pocketbook-statistics.qrc", `does not list i18n/${file} — it would not be in the binary`);
}
for (const lang of qrc)
    if (!files.includes(`${lang}.js`))
        fail("pocketbook-statistics.qrc", `lists i18n/${lang}.js, which does not exist`);
for (const [code, name] of table) {
    if (!imports.has(name))
        fail("Tr.qml", `"${code}" maps to ${name}, which is not imported`);
    if (!installer.has(code))
        fail("installer.cpp", `no launcher label for "${code}" — the tile falls back to English`);
}
for (const code of installer)
    if (!table.has(code))
        fail("Tr.qml", `installer.cpp has a launcher label for "${code}" but no catalog is mapped to it`);
for (const name of imports)
    if (![...table.values()].includes(name))
        fail("Tr.qml", `${name} is imported but mapped to no language`);

/* A row that is no longer missing has been translated since — the file has to
 * lose it, or it would go on excusing a key that comes back. */
for (const [lang, keys] of Object.entries(baseline)) {
    if (!files.includes(`${lang}.js`)) {
        fail("untranslated.json", `lists ${lang}, which has no catalog`);
        continue;
    }
    for (const key of keys) {
        if (!(gaps[lang] ?? []).includes(key))
            fail("untranslated.json",
                `${lang} no longer misses "${key}" — remove the row`);
    }
}

if (updateBaseline) {
    const sorted = {};
    for (const lang of Object.keys(gaps).sort())
        sorted[lang] = gaps[lang].sort();
    writeFileSync(baselinePath, JSON.stringify(sorted, null, 2) + "\n");
    const total = Object.values(sorted).reduce((n, k) => n + k.length, 0);
    console.log(`i18n: baseline rewritten — ${total} untranslated keys in` +
                ` ${Object.keys(sorted).length} catalogs`);
    process.exit(0);
}

/* ---- verdict ------------------------------------------------------------ */

if (problems.length) {
    for (const p of problems)
        console.error(`i18n: ${p}`);
    console.error(`i18n: ${problems.length} problem${problems.length === 1 ? "" : "s"}`);
    process.exit(1);
}
const debt = Object.values(gaps).reduce((n, k) => n + k.length, 0);
console.log(`i18n: ${files.length} catalogs, ${Object.keys(english.strings).length} keys,` +
            ` 0 problems` + (debt ? `, ${debt} known untranslated` : ""));
