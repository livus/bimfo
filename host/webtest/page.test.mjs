// End-to-end test of the configuration page (main/www/index.html) against the
// mock device (host/mock_server.py), driven through jsdom.
//   cd host/webtest && npm install && node page.test.mjs
import { spawn } from "node:child_process";
import { JSDOM } from "jsdom";
import path from "node:path";
import { fileURLToPath } from "node:url";

const here = path.dirname(fileURLToPath(import.meta.url));
const PORT = 8765;
const BASE = `http://127.0.0.1:${PORT}`;
const python = process.env.PYTHON || (process.platform === "win32" ? "py" : "python3");

let failures = 0;
function check(cond, what) {
  if (cond) console.log("  ok   " + what);
  else { failures++; console.log("  FAIL " + what); }
}
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
async function until(fn, what, ms = 8000) {
  const t0 = Date.now();
  while (Date.now() - t0 < ms) {
    try { if (fn()) return true; } catch (e) { /* not yet */ }
    await sleep(50);
  }
  check(false, "timeout waiting for " + what);
  return false;
}

const server = spawn(python, [path.join(here, "..", "mock_server.py"), "--port", String(PORT)], { stdio: "pipe" });
server.stdout.on("data", () => {});
server.stderr.on("data", (d) => process.stderr.write(d));
try {
  for (let i = 0; i < 100; i++) {
    try { await fetch(BASE + "/api/status"); break; } catch (e) { await sleep(100); }
  }

  const errors = [];
  const dom = await JSDOM.fromURL(BASE + "/", {
    runScripts: "dangerously",
    pretendToBeVisual: true,
    beforeParse(window) {
      // Node's fetch behind the page's fetch (jsdom has none).
      window.fetch = async (url, init = {}) => {
        const { signal, ...rest } = init;
        const r = await fetch(new URL(url, BASE), rest);
        const text = await r.text();
        return { ok: r.ok, status: r.status, text: async () => text };
      };
      window.alert = (m) => errors.push("alert: " + m);
      window.confirm = () => true;
      window.addEventListener("error", (e) => errors.push(String(e.message)));
    },
  });
  const w = dom.window, d = w.document, $ = (id) => d.getElementById(id);
  const click = (e) => e.dispatchEvent(new w.MouseEvent("click", { bubbles: true }));
  const type = (e, v) => { e.value = v; e.dispatchEvent(new w.Event("input", { bubbles: true })); };

  console.log("load");
  await until(() => $("ssid").value === "Zuhause", "config loaded");
  check($("stops").children.length === 1, "existing stop listed");
  check($("stops").textContent.includes("U3 (H), U3 (R)"), "stop line summary");
  check($("save").disabled, "save disabled while unchanged");
  await until(() => $("preview").querySelectorAll(".badge").length > 0, "preview filled");
  await until(() => $("netchip").textContent.startsWith("Online"), "status chip");

  console.log("add a Wiener Linien stop with selected lines");
  click($("add-stop"));
  check(!$("editor").classList.contains("hidden"), "editor opens");
  $("search").value = "karlspl";
  click($("search-btn"));
  await until(() => $("results").querySelectorAll("button").length > 0, "WL search results");
  const res = [...$("results").querySelectorAll("button")];
  check(res[0].textContent === "Karlsplatz", "prefix match ranked first (" + res[0].textContent + ")");
  click(res[0]);
  await until(() => $("lines").querySelectorAll("input[type=checkbox]").length === 13, "13 line choices");
  const boxes = [...$("lines").querySelectorAll("input[type=checkbox]")];
  boxes[0].checked = true;   // U3 ...
  boxes[4].checked = true;   // tram 5
  type($("walk"), "5");
  type($("stop-name"), "Karlsplatz <b>X</b>");
  click($("stop-ok"));
  check($("stops").children.length === 2, "second stop added");
  check($("stops").querySelector("b") === null, "names are rendered as text, not HTML");
  check(!$("save").disabled, "save enabled after a change");

  console.log("add an OEBB stop with filters");
  click($("add-stop"));
  click($("provider").querySelector("[data-kind=oebb]"));
  $("search").value = "Westbahnhof";
  click($("search-btn"));
  await until(() => $("results").querySelectorAll("button").length === 1, "OEBB search results");
  click($("results").querySelector("button"));
  await until(() => !$("oebb-opts").classList.contains("hidden"), "OEBB options");
  $("trains-only").checked = true;
  $("oebb-lines").value = "S 50, REX 51 ,";
  click($("stop-ok"));
  check($("stops").children.length === 3, "third stop added");

  console.log("updates");
  check(!$("auto-update").checked && !$("auto-update").disabled, "automatic updates offered, off by default");
  await until(() => $("update-info").textContent === "Noch nicht geprüft", "update info before the first check");
  $("auto-update").checked = true;
  $("auto-update").dispatchEvent(new w.Event("input", { bubbles: true }));
  click($("update-check"));
  await until(() => $("update-info").textContent.startsWith("Prüfe"), "check runs");
  await until(() => $("update-info").textContent.startsWith("Aktuell (neueste Version 1.0.0)"), "check result shown", 10000);
  check(!$("update-check").disabled, "check button enabled again");

  console.log("display settings and save");
  click($("theme").querySelector("[data-theme=oebb]"));
  type($("brightness"), "55");
  type($("hostname"), "Küche-Monitor");
  check($("host-preview").textContent === "kche-monitor", "hostname sanitised live");
  type($("hostname"), "kueche");
  click($("save"));
  await until(() => $("save").disabled && $("overlay").style.display === "flex", "saved, restart overlay (hostname change)");
  const cfg = await (await fetch(BASE + "/api/config")).json();
  check(cfg.theme === "oebb" && cfg.brightness === 55 && cfg.hostname === "kueche", "display settings saved");
  check(cfg.stops.length === 3, "three stops saved");
  check(cfg.auto_update === true, "automatic updates switched on");
  const wl = cfg.stops[1], oe = cfg.stops[2];
  check(wl.kind === "wl" && wl.id > 0 && wl.walk === 5, "WL stop id/walk");
  check(wl.sel.length === 2 && wl.sel.every((s) => s.rbl > 0 && (s.dir === "H" || s.dir === "R")), "WL selection with platform ids");
  check(oe.kind === "oebb" && oe.id === 1291501 && oe.trains_only === true, "OEBB stop saved");
  check(JSON.stringify(oe.sel) === '[{"line":"S 50"},{"line":"REX 51"}]', "OEBB line filter parsed");
  check(!("pass" in cfg.wifi), "password not sent when untouched");

  console.log("CSRF header");
  const csrf = await fetch(BASE + "/api/config", { method: "POST", body: "{}" });
  check(csrf.status === 403, "POST without X-Bimfo rejected");

  check(errors.length === 0, "no script errors" + (errors.length ? ": " + errors.join("; ") : ""));
  dom.window.close();
} finally {
  server.kill();
}
console.log(failures ? `${failures} FAILED` : "all page checks passed");
process.exit(failures ? 1 : 0);
