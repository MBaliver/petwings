// Browser smoke test for the web build via WebDriver BiDi (Firefox).
// usage: node tools/webtest.mjs <bidi ws url> <page url> <out dir> [width height]
// Takes screenshots while the game runs and presses Enter to go from the title into the game.
import fs from "node:fs";

const [, , wsUrl, pageUrl, outDir, w = "1280", h = "720"] = process.argv;
const ws = new WebSocket(wsUrl);
let nextId = 1;
const pending = new Map();
ws.onmessage = (ev) => {
  const msg = JSON.parse(ev.data);
  if (msg.id && pending.has(msg.id)) {
    const { resolve, reject } = pending.get(msg.id);
    pending.delete(msg.id);
    msg.type === "error" ? reject(new Error(msg.error + ": " + msg.message)) : resolve(msg.result);
  } else if (msg.method === "log.entryAdded") {
    console.log("[console]", msg.params.level, msg.params.text);
  }
};
const send = (method, params = {}) =>
  new Promise((resolve, reject) => {
    const id = nextId++;
    pending.set(id, { resolve, reject });
    ws.send(JSON.stringify({ id, method, params }));
  });
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

await new Promise((r) => (ws.onopen = r));
await send("session.new", { capabilities: {} });
await send("session.subscribe", { events: ["log.entryAdded"] });
const { contexts } = await send("browsingContext.getTree", {});
const context = contexts[0].context;
await send("browsingContext.setViewport", { context, viewport: { width: +w, height: +h } });
await send("browsingContext.navigate", { context, url: pageUrl, wait: "complete" });

async function shot(name) {
  const { data } = await send("browsingContext.captureScreenshot", { context });
  fs.writeFileSync(`${outDir}/${name}.png`, Buffer.from(data, "base64"));
  console.log("saved", name);
}
async function key(value) {
  await send("input.performActions", {
    context,
    actions: [{ type: "key", id: "kb", actions: [{ type: "keyDown", value }, { type: "pause", duration: 120 }, { type: "keyUp", value }] }],
  });
}
const ENTER = "", DOWN = "";

await send("script.evaluate", { expression: "window.__keys=[]; window.addEventListener('keydown', e => window.__keys.push(e.key + ':' + (document.activeElement && document.activeElement.id)), true); 1", target: { context }, awaitPromise: false });
await sleep(4000);
await shot("1_logo");
await sleep(7000);
await shot("2_title");
for (let i = 0; i < 4; i++) { await key(DOWN); await sleep(300); }
await shot("3_title_options_selected");
console.log("keys seen:", (await send("script.evaluate", { expression: "JSON.stringify(window.__keys)", target: { context }, awaitPromise: false })).result.value);
await key(ENTER);
await sleep(2500);
await shot("4_options");
await key(""); // Escape -> back to title
await sleep(2500);
await key(ENTER);  // EPISODE 1 (menu resets to the first item)
await sleep(6000);
await shot("5_game");
const r = await send("script.evaluate", {
  expression: "JSON.stringify({w: innerWidth, h: innerHeight, canvas: [Module.canvas.width, Module.canvas.height]})",
  target: { context }, awaitPromise: false,
});
console.log("page:", r.result.value);
await send("session.end", {});
process.exit(0);
