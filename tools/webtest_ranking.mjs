// Browser test of the shared ranking client (WebDriver BiDi / Firefox).
// usage: node tools/webtest_ranking.mjs <bidi ws url> <base url> <out dir>
// Puts a score in the offline queue, opens the game, checks the queue is flushed to the API
// and takes a screenshot of the RANKING screen.
import fs from "node:fs";

const [, , wsUrl, base, outDir] = process.argv;
const ws = new WebSocket(wsUrl);
let nextId = 1;
const pending = new Map();
ws.onmessage = (ev) => {
  const msg = JSON.parse(ev.data);
  if (msg.id && pending.has(msg.id)) {
    const { resolve, reject } = pending.get(msg.id);
    pending.delete(msg.id);
    msg.type === "error" ? reject(new Error(msg.error + ": " + msg.message)) : resolve(msg.result);
  }
};
const send = (method, params = {}) => new Promise((resolve, reject) => {
  const id = nextId++;
  pending.set(id, { resolve, reject });
  ws.send(JSON.stringify({ id, method, params }));
});
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
await new Promise((r) => (ws.onopen = r));
await send("session.new", { capabilities: {} });
const { contexts } = await send("browsingContext.getTree", {});
const context = contexts[0].context;
await send("browsingContext.setViewport", { context, viewport: { width: 1280, height: 720 } });
const evaluate = async (expression) =>
  (await send("script.evaluate", { expression, target: { context }, awaitPromise: true })).result.value;
const key = (value) => send("input.performActions", { context, actions: [{ type: "key", id: "kb",
  actions: [{ type: "keyDown", value }, { type: "pause", duration: 120 }, { type: "keyUp", value }] }] });

await send("browsingContext.navigate", { context, url: base + "/", wait: "complete" });
await evaluate(`localStorage.setItem('petwings_pending_scores', JSON.stringify(['{"name":"QUEUED","potions":4321,"episode":3,"level":16,"cleared":false}'])); 1`);
await send("browsingContext.navigate", { context, url: base + "/?autostart=1", wait: "complete" });
await sleep(4000);
console.log("queue after start:", await evaluate("localStorage.getItem('petwings_pending_scores')"));
console.log("server top 3:", await evaluate("fetch('api/ranking?limit=3').then(r=>r.text())"));
await sleep(7000);  // logo -> title
for (let i = 0; i < 3; i++) { await key(""); await sleep(250); }  // down to RANKING
await key("");
await sleep(2500);
const { data } = await send("browsingContext.captureScreenshot", { context });
fs.writeFileSync(`${outDir}/ranking.png`, Buffer.from(data, "base64"));
console.log("saved ranking.png");
await send("session.end", {});
process.exit(0);
