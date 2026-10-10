// Execute the real map JavaScript against a small DOM substitute, without a
// browser-global `event`. This catches handlers that rely on that global.
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const nodes = new Map();
function node() {
  const classes = new Set();
  return {
    innerText: '', innerHTML: '', children: [],
    classList: { add: x => classes.add(x), remove: x => classes.delete(x), contains: x => classes.has(x) },
    appendChild(child) { this.children.push(child); },
    getContext() { return { clearRect() {}, beginPath() {}, arc() {}, fill() {} }; }
  };
}
const document = {
  getElementById(id) { if (!nodes.has(id)) nodes.set(id, node()); return nodes.get(id); },
  createElement() { return node(); },
  querySelectorAll() { return [...nodes.values()]; }
};
const context = vm.createContext({ document, window: { innerWidth: 1, innerHeight: 1, addEventListener() {} }, requestAnimationFrame() {} });
const html = fs.readFileSync(path.join(__dirname, '../docs/interactive_map.html'), 'utf8');
vm.runInContext(html.match(/<script>([\s\S]*?)<\/script>/)[1], context);
const clicked = node();
const ev = { currentTarget: clicked };
switch (process.argv[2]) {
  case 'drawer':
    for (let i = 0; i <= 12; i++) {
      context.openDrawer(`P${i}`);
      assert.equal(document.getElementById('drawer-pill').innerText, `P${i}`);
      assert.ok(document.getElementById('drawer-content').innerHTML.includes('Milestone Goal'));
    }
    break;
  case 'view':
    context.switchView('pinout', ev);
    assert.ok(clicked.classList.contains('active'));
    assert.ok(document.getElementById('view-pinout').classList.contains('active'));
    break;
  case 'filter':
    context.filterPerip('all', ev);
    assert.ok(clicked.classList.contains('active'));
    break;
  case 'state':
    context.simulateState('BOOT', ev);
    assert.ok(clicked.classList.contains('active-state'));
    assert.equal(document.getElementById('sim-state-text').innerText, 'STATE: BOOT');
    break;
  default: throw Error('Unknown test scenario');
}
