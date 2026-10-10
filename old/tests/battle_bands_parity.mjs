import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { spawnSync } from 'node:child_process';

const args = process.argv.slice(2), opt = key => args[args.indexOf(key) + 1], root = resolve(opt('--reference'));
const load = path => import(pathToFileURL(resolve(root, path)));
const { Battle } = await load('server/sim/Battle.js');
const { SkillRuntime } = await load('server/sim/skills.js');
const { install, hasBattlePart } = await load('server/sim/content/bands/battle.js');
const { setGameData } = await load('server/sim/content/support/index.js');
const data = Object.fromEntries(['bands', 'bonds', 'items'].map(k => [k, JSON.parse(readFileSync(resolve(root, `data/${k}.json`), 'utf8'))]));
setGameData(data);
const bands = Object.keys(data.bands).filter(id => hasBattlePart(id) && id !== 'band_amedic');
assert.equal(bands.length, 10);
const runs = bands.flatMap(band => [1, 42, 4294967295].flatMap(seed => Array.from({length: 8}, (_, scene) => ({band, seed, scene}))));
const run = spawnSync(resolve(opt('--native')), [], {input: runs.map(r => `${r.band} ${r.scene} ${r.seed}`).join('\n'),
  encoding: 'utf8', maxBuffer: 128 * 1024 * 1024, timeout: 120000});
assert.equal(run.error, undefined); assert.equal(run.status, 0, run.stderr);
const actual = run.stdout.trim().split('\n').map(JSON.parse);
assert.equal(actual.length, runs.length);
const stats = {maxHp: 1000, atk: 100, def: 80, res: 30, aspd: 100, bat: 1, moveSpeed: 0, blockCnt: 0, respawnTime: 1, cost: 0};
const positions = [[5, 5], [6, 5], [5, 4], [5, 6], [4, 5]];
const names = ['a', 'a_b', 'b2', 'b3', 'b4'];
function equal(a, b, path) {
  if (Array.isArray(b)) { assert.equal(a?.length, b.length, path); b.forEach((v, i) => equal(a[i], v, `${path}.${i}`)); }
  else if (typeof b === 'number') assert(Number.isFinite(a) && Math.abs(a - b) <= 1e-9 * Math.max(1, Math.abs(b)), `${path}: C++ ${a}, JS ${b}`);
  else assert.equal(a, b, path);
}
let snapshots = 0;
for (const [index, {band, seed, scene}] of runs.entries()) {
  const chess = Object.fromEntries(names.map((id, i) => [id, {chessId: id, baseId: i < 2 ? 'a' : 'b', tier: i + 1,
    isGolden: i === 1 || i === 3, position: scene % 3 ? 'MELEE' : 'RANGED', bonds: ['egirShip'], stats, rangeGrid: [[0, 0], [0, 1]]}]));
  const bonds = {egirShip: {active: true, layers: 0, tier: 1}};
  for (let i = 1; i <= scene; ++i) bonds[`bond${i}`] = {active: true, layers: 0, tier: 1};
  const b = new Battle({content: 'none', seed, autoFinish: false, timeLimit: 60,
    flags: {layerGainsEnabled: scene !== 7}, rect: {r0: 0, r1: 18, c0: 0, c1: 20}, stage: {id: 'bands', rows: Array(19).fill('r'.repeat(21))},
    data: {chess, enemies: {enemy: {stats: {...stats, maxHp: 1000000, def: scene % 2 ? 0 : 200, res: scene % 2 ? 90 : 0}, applyWay: 'NONE'}}},
    players: [{playerId: 'p', coords: 'field', bandId: band, bonds,
      units: names.map((id, i) => ({uid: i + 1, ...(i === 4 ? {kind: 'token', tokenId: id, def: chess[id]} : {chessId: id}),
        row: positions[i][0], col: positions[i][1], dir: ['UP', 'RIGHT', 'DOWN', 'LEFT'][scene % 4]}))},
      {playerId: 'q', coords: 'field', units: [{uid: 6, chessId: 'a', row: 5, col: 15}]}],
    spawns: [{time: 0, enemyKey: 'enemy', ownerPlayerId: 'p', route: {start: [9, 10], end: [9, 0]}}],
    setup(b) {
      b.getPlayer('p').mirror = !!(scene % 2);
      for (const [i, u] of b.allyUnits.entries()) {
        u.profile = {noAttack: true, dmgType: 'phys', attack: 'melee', maxTargets: 1};
        u.skill = new SkillRuntime(b, u, {skillType: 'MANUAL'}, {kind: 'duration', spCost: 10, initSp: 0, duration: 0.1, trigger: 'NEVER'});
        if (scene === 6 && i === 3) u.deferDeploy = true;
      }
      b.on('enemySpawn', ({enemy}) => { enemy.profile = {noAttack: true}; });
      install(b);
    }
  });
  // Qalaisa stores its logical stack count in JS buff.data.n; C++ uses the typed buff stack counter.
  const snapshot = () => [b.units.map(u => [u.hp, +u.alive, u.s.maxHp, u.s.atk, u.s.redeployMul, u.skill?.spTotal ?? 0,
    +!!u.skill?.active, Number.isFinite(u.respawnAt) ? u.respawnAt : null,
    u.buffs.filter(x => x.key.startsWith('band:')).map(x => [x.key, x.data?.n ?? x.stacks, x.shieldHits || 0])]),
    b.getPlayer('p').bonds.egirShip.layers, b.result().perPlayer.p.deaths, b.rng.state()];
  let frame = 0;
  const check = () => { equal(actual[index][frame], snapshot(), `${band}/${seed}/${scene}/${frame}`); ++frame; ++snapshots; };
  b.start(); b.step(); check();
  for (let tick = 0; tick < 80; ++tick) {
    const units = b.allyUnits, enemy = b.enemies[0];
    if (tick % 4 === 0) for (const u of units.slice(0, 5)) b.forceAttack(u, [enemy]);
    if (tick % 9 === 1) b.loseHp(units[0], 100000);
    if (tick % 9 === 2) b.redeploy(units[0], {free: true});
    if (tick % 11 === 3) b.loseHp(units[1], 100000);
    if (tick % 11 === 4) b.redeploy(units[1], {free: true});
    if (tick % 5 === 2) for (const u of units.slice(0, 4)) if (u.alive) { u.skill.gainSp(10); u.skill.activate(); }
    if (tick % 7 === 3) for (const u of units.slice(0, 4)) u.skill.stop();
    if (tick % 4 === 1) for (const u of units) b.dealDamage(enemy, u, {amount: 55, type: 'true'});
    if (tick % 3 === 0) b.dealDamage(units[0], enemy, {amount: 150, type: tick % 2 ? 'phys' : 'arts'});
    check(); b.step(); check();
  }
  assert.equal(frame, actual[index].length);
  assert.equal(b.errorCount, 0, JSON.stringify(b.errors));
}
console.log(`battle strategies: ${bands.length} behaviours, ${runs.length} scenarios, ${snapshots} state snapshots`);
