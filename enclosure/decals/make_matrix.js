// Writes matrix.tex: display decals as amber LED dot matrices in the layout of the
// original Wiener Linien stop displays, on the glow of the backlit panel and split
// into three modules, like the firmware's Wiener Linien theme. Each departure row
// has the line number (double size), a wheelchair sign for low-floor trams, the
// destination on two lines of the 5x7 font, and the minutes (double size) or a
// block for "arriving". One macro per station: \ledmatrixA{x}{y}, \ledmatrixB ...
// Font: tools/fonts/adafruit_glcdfont.c (BSD, Adafruit). Run: node make_matrix.js
const fs = require("fs");
const path = require("path");

// The display's lit area is 32.35 x 17.39 mm; 145 x 78 dots keep that ratio.
const W = 145, H = 78;
const AREA_W = 32.35;
const PITCH = AREA_W / W;

// One platform each, trams only. A display shows only the next tram of each line, so
// each line appears once; unused rows stay dark. min: null = arriving (the block).
// A is the reference photo (Wiedner Hauptstrasse, before line 1 was rerouted); the
// others are real departures from the Wiener Linien realtime API on 27 Oct 2026. Where
// a destination is split over two lines, the line break replaces the comma, as on the photo.
const VARIANTS = [
    { id: "A", name: "Paulanergasse", note: "as in the reference photo", rows: [
        { line: "1", ulf: true, dest: ["Stefan-Fadinger", "Platz"], min: null },
        { line: "62", ulf: false, dest: ["Lainz", "Wolkersbergenstr"], min: "5" },
    ] },
    { id: "B", name: "Schottentor U", note: "the Jonas-Reindl, lines 37 to 42", rows: [
        { line: "38", ulf: true, dest: ["Grinzing", ""], min: null },
        { line: "40", ulf: true, dest: ["Herbeckstraße", ""], min: "4" },
        { line: "37", ulf: true, dest: ["Hohe Warte", ""], min: "5" },
    ] },
    { id: "C", name: "Oper, Karlsplatz U", note: "Ring, towards Burgtheater", rows: [
        { line: "2", ulf: true, dest: ["Dornbach", ""], min: "1" },
        { line: "D", ulf: true, dest: ["Börse", ""], min: "2" },
        { line: "1", ulf: true, dest: ["Prater", "Hauptallee"], min: "3" },
    ] },
    { id: "D", name: "Rathausplatz, Burgtheater", note: "Ring, towards Oper", rows: [
        { line: "71", ulf: true, dest: ["Kaiserebersdorf", "Zinnergasse"], min: "2" },
        { line: "1", ulf: true, dest: ["Reumannplatz U", ""], min: "3" },
        { line: "D", ulf: true, dest: ["Absberggasse", ""], min: "5" },
    ] },
    { id: "E", name: "Zentralfriedhof 2. Tor", note: "at the main gate", rows: [
        { line: "11", ulf: true, dest: ["Otto-Probst-", "Platz"], min: "2" },
        { line: "71", ulf: true, dest: ["Schottenring U", ""], min: "4" },
    ] },
];

const src = fs.readFileSync(path.join(__dirname, "..", "..", "tools", "fonts", "adafruit_glcdfont.c"), "utf8");
const font = [...src.slice(src.indexOf("{")).matchAll(/0x([0-9A-Fa-f]{2})/g)].map(m => parseInt(m[1], 16));
// the table is CP437; the few non-ASCII letters Vienna's stop names need
const CP437 = { "ä": 0x84, "ö": 0x94, "ü": 0x81, "Ä": 0x8E, "Ö": 0x99, "Ü": 0x9A, "ß": 0xE1 };

// wheelchair sign, 8 x 8: head, back, seat and leg, and the big wheel round the seat
const WHEELCHAIR = [
    "..##....",
    "..##....",
    "...#....",
    "...####.",
    ".#.#....",
    "#..####.",
    "#.....#.",
    ".####..#",
];

const NUM_RIGHT = 24, WHEEL_X = 27, DEST_X = 36, MIN_RIGHT = W - 2;
const width = (s, k) => [...s].length * 6 * k - k;

function render(rows) {
    const lit = new Set();
    const set = (x, y) => { if (x >= 0 && x < W && y >= 0 && y < H) lit.add(y * W + x); };
    function glyph(ch, x, y, k) {   // k = 1 or 2 (each font pixel as k x k dots)
        const c = CP437[ch] !== undefined ? CP437[ch] : ch.charCodeAt(0);
        if (c > 0xFF) throw new Error(`no glyph for "${ch}"`);
        for (let col = 0; col < 5; col++) {
            const bits = font[c * 5 + col];
            for (let row = 0; row < 8; row++)   // row 7 holds descenders (g, p, y)
                if (bits & (1 << row))
                    for (let i = 0; i < k; i++) for (let j = 0; j < k; j++) set(x + col * k + i, y + row * k + j);
        }
    }
    const text = (s, x, y, k = 1) => [...s].forEach((ch, i) => glyph(ch, x + i * 6 * k, y, k));
    const bitmap = (b, x, y) => b.forEach((r, j) => [...r].forEach((c, i) => { if (c === "#") set(x + i, y + j); }));

    if (rows.length > 3) throw new Error("at most three rows fit");
    rows.forEach((r, n) => {
        const y0 = 4 + n * 26;                                   // each row 16 dots tall, 26 apart
        const minW = r.min === null ? 6 : width(r.min, 2);
        for (const d of r.dest)
            if (DEST_X + width(d, 1) > MIN_RIGHT - minW - 2) throw new Error(`"${d}" runs into the minutes`);
        text(r.line, NUM_RIGHT - width(r.line, 2), y0 + 1, 2);
        if (r.ulf) bitmap(WHEELCHAIR, WHEEL_X, y0 + 8);
        const one = !r.dest[1];                                  // a one-line destination sits mid-row
        text(r.dest[0], DEST_X, one ? y0 + 4 : y0);
        if (!one) text(r.dest[1], DEST_X, y0 + 9);
        if (r.min === null) {                                    // arriving: a block, as on the real display
            for (let i = 0; i < 6; i++) for (let j = 0; j < 6; j++) set(MIN_RIGHT - 6 + i, y0 + 5 + j);
        } else {
            text(r.min, MIN_RIGHT - width(r.min, 2), y0 + 1, 2);
        }
    });
    return lit;
}

// TikZ: \ledmatrixX{x}{y} draws the lit area with its top left at (x, y) mm, y running down
const r = PITCH * 0.36;
let out = `% generated by make_matrix.js; ${W} x ${H} dots, pitch ${PITCH.toFixed(4)} mm\n`;
let preview = "";
for (const v of VARIANTS) {
    const lit = render(v.rows);
    out += `% ${v.id}: ${v.name} (${v.note})\n`;
    out += `\\newcommand{\\ledmatrix${v.id}}[2]{\\begin{scope}[shift={(#1,#2)}]\n`;
    out += `  \\fill[ledbg] (0,0) rectangle (${AREA_W},${(H * PITCH).toFixed(3)});\n`;
    // unlit dots: one dashed line per row, zero-length dashes with round caps
    out += `  \\draw[ledoff, line width=${(2 * r).toFixed(4)}mm, line cap=round, dash pattern=on 0mm off ${PITCH.toFixed(5)}mm]\n`;
    for (let j = 0; j < H; j++)
        out += `    (${(PITCH / 2).toFixed(4)},${((j + 0.5) * PITCH).toFixed(4)}) -- (${(AREA_W - PITCH / 4).toFixed(4)},${((j + 0.5) * PITCH).toFixed(4)})\n`;
    out += `  ;\n`;
    const pts = [...lit].map(i => `(${((i % W) * PITCH + PITCH / 2).toFixed(3)},${(Math.floor(i / W) * PITCH + PITCH / 2).toFixed(3)})`);
    for (let i = 0; i < pts.length; i += 40)
        out += `  \\fill[ledon] ${pts.slice(i, i + 40).map(p => p + ` circle (${r.toFixed(4)})`).join(" ")};\n`;
    // seams between the three modules, one dot column wide, over the dots as on the real display
    for (const c of [Math.round(W / 3), Math.round(2 * W / 3)])
        out += `  \\fill[ledseam] (${(c * PITCH).toFixed(4)},0) rectangle (${((c + 1) * PITCH).toFixed(4)},${(H * PITCH).toFixed(3)});\n`;
    out += `\\end{scope}}\n`;
    out += `\\newcommand{\\stationname${v.id}}{${v.name.replace(/ß/g, "\\ss{}").replace(/ö/g, '\\"o').replace(/ä/g, '\\"a').replace(/ü/g, '\\"u')}}\n`;
    out += `\\newcommand{\\stationnote${v.id}}{${v.note}}\n`;
    out += `\\newcommand{\\stationlines${v.id}}{${v.rows.map(x => x.line).join(", ")}}\n`;

    preview += `${v.id}: ${v.name}\n`;
    for (let y = 0; y < H; y++) { for (let x = 0; x < W; x++) preview += lit.has(y * W + x) ? "#" : "."; preview += "\n"; }
    console.log(`${v.id} ${v.name}: ${lit.size} lit dots`);
}
out += `\\let\\ledmatrix\\ledmatrixA\n`;
fs.writeFileSync(path.join(__dirname, "matrix.tex"), out);
fs.writeFileSync(path.join(__dirname, "matrix_preview.txt"), preview);
