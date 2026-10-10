import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { spawnSync } from 'node:child_process';

const args = process.argv.slice(2), opt = key => args[args.indexOf(key) + 1], root = resolve(opt('--reference'));
const load = path => import(pathToFileURL(resolve(root, path)));
const { Battle } = await load('server/sim/Battle.js');
const { install } = await load('server/sim/content/bonds/core.js');
const { setGameData } = await load('server/sim/content/support/index.js');
const data = {bonds: JSON.parse(readFileSync(resolve(root, 'data/bonds.json'), 'utf8'))}; setGameData(data);
const runs = [1, 42, 4294967295].flatMap(seed => Array.from({length: 24}, (_, scene) => ({seed, scene})));
const run = spawnSync(resolve(opt('--native')), [], {input: runs.map(r => `${r.scene} ${r.seed}`).join('\n'), encoding: 'utf8', maxBuffer: 64 * 1024 * 1024, timeout: 90000});
assert.equal(run.error?.message, undefined); assert.equal(run.status, 0, run.stderr);
const actual = run.stdout.trim().split('\n'); assert.equal(actual.length, runs.length);

function equal(a, b, path) {
  if (Array.isArray(b)) { assert.equal(a?.length, b.length, path); b.forEach((v, i) => equal(a[i], v, `${path}.${i}`)); }
  else if (typeof b === 'number') assert(Number.isFinite(a) && Math.abs(a - b) <= 1e-9 * Math.max(1, Math.abs(b)), `${path}: C++ ${a}, JS ${b}`);
  else assert.equal(a, b, path);
}

let snapshots = 0;
for (const [index, {seed, scene}] of runs.entries()) {
  const expected = JSON.parse(actual[index]);
  const stats = {maxHp: 1000000, atk: 100, def: 0, res: 0, aspd: 100, bat: 1, moveSpeed: 0, blockCnt: 0, respawnTime: 1, cost: 0};
  const chess = Object.fromEntries(Array.from({length: 4}, (_, i) => [`a${i}`, {chessId: `a${i}`, baseId: `a${i}`, tier: 1,
    position: 'MELEE', bonds: [scene === 18 && i % 2 ? 'maniShip' : 'siracusaShip'], stats, rangeGrid: [[0, 0], [0, 1]]}]));
  const routes = Array.from({length: 3}, (_, i) => ({start: [10, 7 + i * 0.2], end: [10, 0]}));
  const b = new Battle({content: 'none', seed, autoFinish: false, timeLimit: 90, flags: {layerGainsEnabled: true},
    rect: {r0: 0, r1: 18, c0: 0, c1: 20}, stage: {id: 'siracusa', rows: Array(19).fill('r'.repeat(21))},
    data: {...data, chess, enemies: {enemy: {stats: {...stats, maxHp: scene === 20 ? 8000 : 1000000000}, applyWay: 'NONE'}}},
    players: [0, 1].map(player => ({playerId: player ? 'q' : 'p', coords: 'field',
      bonds: {siracusaShip: {active: scene !== 0, layers: scene === 23 ? 30 : 0, count: scene === 2 ? 6 : 3,
        tier: scene === 2 ? 0 : scene === 1 || scene === 22 ? 1 : 2}, ...(scene === 18 ? {maniShip: {active: true, tier: 1}} : {})},
      units: [0, 1].map(i => ({uid: player * 2 + i + 1, chessId: `a${player * 2 + i}`, row: 10 + player, col: 5 + i, dir: 'RIGHT'}))})),
    routes, spawns: routes.map(route => ({time: 0, enemyKey: 'enemy', ownerPlayerId: 'p', route})),
    setup(b) {
      for (const u of b.allyUnits) u.profile = {...u.profile, noAttack: true, dmgType: 'phys', attack: 'melee', maxTargets: 1, splashRadius: scene === 7 ? 2 : 0};
      b.on('enemySpawn', ({enemy}) => { enemy.profile = {noAttack: true}; });
      install(b);
    }
  });
  const snapshot = () => [b.units.map(u => [u.hp, +u.alive, u.s.aspd, +!!u.s.flags.stealth,
    u.buffs.find(buff => buff.key === 'fear')?.timeLeft ?? 0, u.x, u.y, Number.isFinite(u.mem.siraStealthEnd) ? u.mem.siraStealthEnd : null,
    u.buffs.filter(buff => buff.key === 'bond:siracusa' || buff.key === 'bond:siracusa:stealth').map(buff => [buff.key, buff.timeLeft])]), b.rng.state()];
  let frame = 0;
  const check = () => { equal(expected[frame], snapshot(), `${seed}/${scene}/${frame}`); ++frame; ++snapshots; };
  b.start(); b.step();
  if (scene === 19) for (const e of b.enemies) b.applyStatus(e, 'resist', {duration: 90, value: 0.5});
  check();
  const u = b.allyUnits, e = b.enemies;
  for (let tick = 0; tick < 2100; ++tick) {
    if ((scene === 3 && tick === 60) || (scene === 4 && tick === 1200)) for (const a of u) b.removeBuff(a, 'bond:siracusa:stealth');
    if (scene === 5 && tick === 300) b.loseHp(u[0], 10000000);
    if (scene === 5 && tick === 360) b.redeploy(u[0], {free: true});
    if ((scene === 6 || scene === 22) && tick === 1500) for (const a of u) b.applyStatus(a, 'stealth', {duration: 10});
    if (scene === 21 && tick === 600) for (const a of u) b.applyStatus(a, 'stun', {duration: 40});
    if (scene === 23 && tick === 60) { b.addLayers('p', 'siracusaShip', 70); b.addLayers('q', 'siracusaShip', 100); }
    if (tick % 3 === 0) {
      for (const a of u) if (a.alive) {
        if (scene === 7) { b.forceAttack(a, [e[0]]); continue; }
        if (scene === 17) { b.dealDamage(a, e[0], {type: 'element', element: 'burn', amount: 0.01}); continue; }
        const tags = ['dot', 'periodic', 'addition', 'item', 'hpLoss', 'bond:fixture'];
        b.dealDamage(a, e[a.id % 3], {amount: 3, type: scene === 15 ? 'elemental' : 'arts', sourceless: scene === 14,
          tags: scene >= 8 && scene <= 13 ? [tags[scene - 8]] : [], isAttack: tick % 2 === 0, isSplash: scene === 16});
      }
    }
    if (tick % 30 === 0) check();
    b.step();
    if (tick % 30 === 0) check();
  }
  assert.equal(frame, expected.length);
  assert.equal(b.errorCount, 0, JSON.stringify(b.errors));
}
console.log(`${runs.length} Siracusa scenarios / ${snapshots} snapshots matched JS`);
