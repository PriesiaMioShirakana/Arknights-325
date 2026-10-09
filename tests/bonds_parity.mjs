import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { spawnSync } from 'node:child_process';

const args = process.argv.slice(2), root = resolve(args[args.indexOf('--reference') + 1]);
const load = path => import(pathToFileURL(resolve(root, path)).href);
const { GameData } = await load('server/match/gamedata.js');
const { computeBonds, offBondCounts, bondList, thresholdsOf, activatedLayers, bondsWithGains } = await load('server/match/bondsMeta.js');
const { layerGainRoom } = await load('shared/constants.js');
const data = Object.fromEntries(['bonds', 'chess', 'items', 'config'].map(key => [key, JSON.parse(readFileSync(resolve(root, 'data', key + '.json'), 'utf8'))]));
const run = spawnSync(resolve(args[args.indexOf('--native') + 1]), [], { encoding: 'utf8', timeout: 15000, maxBuffer: 48 * 1024 * 1024 });
assert.equal(run.error, undefined); assert.equal(run.status, 0, run.stderr);
const actual = JSON.parse(run.stdout);
const roster = Object.keys(data.chess).sort(), items = Object.keys(data.items).sort();
const giver = items.find(id => data.items[id].canGiveBond);
const targets = items.filter(id => data.items[id].giveBondId && !data.items[id].canGiveBond);
const harmony = roster.find(id => data.chess[id].bonds?.includes('maniShip'));
const layerValues = [-1, 0, 1.5, 998.5, 999, 1003, NaN, Infinity], gainValues = [-1, 0, 0.25, 3.75, 1000, Infinity, NaN];
assert.deepEqual(actual.room, layerValues.flatMap(before => gainValues.map(gain => layerGainRoom(before, gain))));
let index = 0;
const coverage = new Set();
for (const mode of Object.keys(data.config.modes).sort()) {
  const gd = new GameData(data, mode);
  assert.deepEqual(actual.rules, gd.bondIds.map(id => {
    const b = gd.bond(id);
    return [id, b.countMode === 'BOARD_ALL_CHESS' || b.thresholdTemplate === 'count_threshold_upward_golden' ? 2 : b.countMode === 'BOARD_AND_DECK' ? 1 : 0,
      +!!b.isCore, +(b.thresholdTemplate === 'count_threshold_downward'), thresholdsOf(b), Number.isFinite(b.maxCount) ? b.maxCount : null];
  }));
  for (let scene = 0; scene < 120; ++scene) {
    const build = i => ({ kind: 'chess', id: i === 0 && scene % 3 === 0 ? harmony : roster[(i * 17 + scene * 11) % roster.length],
      items: (i % 4 === 0 ? [giver, targets[(i + scene) % targets.length]] : i % 4 === 1 ? [giver] : []).map(id => ({ id })) });
    const ps = { board: new Map(Array.from({ length: scene % 37 }, (_, i) => [String(i), build(i)])),
      hand: Array.from({ length: 10 }, (_, i) => build(i + 36)), temp: [build(0), build(1)],
      layers: Object.fromEntries(gd.bondIds.map((id, i) => [id, layerValues[(i + scene) % layerValues.length]])),
      bondCountBonus: Object.fromEntries(gd.bondIds.map((id, i) => [id, (scene + i) % 7 - 3])) };
    const bonds = computeBonds(gd, ps), off = offBondCounts(gd, ps);
    const reached = bondsWithGains(bonds, Object.fromEntries(gd.bondIds.map((id, i) => [id, gainValues[(i * 3 + scene) % gainValues.length]])));
    const state = (id, b, disabled = false) => [id, b.count, +!!b.active, b.tier || 0, b.layers, +!!b.harmony, +disabled];
    const enabled = Object.entries(bonds).map(([id, b]) => state(id, b));
    const disabled = Object.entries(off || {}).map(([id, b]) => state(id, b, true));
    const view = bondList(gd, bonds, { full: true, off }).map(b => state(b.bondId, b, !!b.off));
    assert.deepEqual(actual.scenes[index++], [enabled, disabled, view, Object.entries(reached).map(([id, b]) => state(id, b)), activatedLayers(bonds), activatedLayers(reached), bondList(gd, reached, { full: true, off }).map(b => state(b.bondId, b, !!b.off)), enabled, disabled], `${mode}/${scene}`);
    for (const [id, b] of Object.entries(bonds)) {
      if (b.harmony) coverage.add('harmony');
      if (b.active) coverage.add(id);
      if (b.layers >= 999) coverage.add('cap');
    }
    if (disabled.length) coverage.add('off');
  }
}
for (const name of ['harmony', 'off', 'cap', 'deputShip', 'soloShip', 'suntShip', 'visiShip', 'miraShip', 'investShip']) assert.ok(coverage.has(name), `bond coverage ${name}`);
assert.equal(index, actual.scenes.length);
console.log(`Bond JS parity passed: ${index} real-roster scenes across 9 modes, equipment membership, tiers, disabled views and layer gain caps.`);
