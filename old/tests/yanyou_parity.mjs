import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { spawnSync } from 'node:child_process';

const args = process.argv.slice(2), opt = key => args[args.indexOf(key) + 1], root = resolve(opt('--reference'));
const load = path => import(pathToFileURL(resolve(root, path)));
const { Battle } = await load('server/sim/Battle.js');
const { setGameData } = await load('server/sim/content/support/index.js');
const data = Object.fromEntries(['tokens', 'stages', 'bonds', 'items', 'bands'].map(k => [k, JSON.parse(readFileSync(resolve(root, `data/${k}.json`), 'utf8'))]));
setGameData(data);
const maps = Object.keys(data.stages).flatMap(stage => [0, 1, 2, 3].flatMap(mode => [0, 1, 2, 3].flatMap(tier => [0, 1].map(occupied => ({stage, mode, tier, occupied})))));
const fights = [1, 42, 4294967295].flatMap(seed => Array.from({length: 24}, (_, scene) => ({seed, scene})));
const lines = [...maps.map(r => `map ${r.stage} ${r.mode} ${r.tier} ${r.occupied}`), ...fights.map(r => `fight ${r.scene} ${r.seed}`)];
const run = spawnSync(resolve(opt('--native')), [], {input: lines.join('\n'), encoding: 'utf8', timeout: 120000, maxBuffer: 128 * 1024 * 1024});
assert.equal(run.error?.message, undefined); assert.equal(run.status, 0, run.stderr);
const actual = run.stdout.trim().split('\n'); assert.equal(actual.length, lines.length);
const snapshot = b => [b.units.map(u => [u.defId, u.x, u.y, u.hp, u.s.maxHp, u.s.atk, u.s.dmgTakenMul, +u.alive, +!!u.removed,
  u.skill?.spTotal ?? 0, +!!u.skill?.active, u.skill?.timeLeft ?? 0, u.stats.attacks, u.stats.dmg,
  u.elem?.burn ?? 0, u.s.elementalTakenMul, u.s.res]), b.rng.state()];

function equal(a, b, path) {
  if (Array.isArray(b)) { assert.equal(a?.length, b.length, path); b.forEach((v, i) => equal(a[i], v, `${path}.${i}`)); }
  else if (typeof b === 'number') assert(Number.isFinite(a) && Math.abs(a - b) <= 1e-8 * Math.max(1, Math.abs(b)), `${path}: C++ ${a}, JS ${b}`);
  else assert.equal(a, b, path);
}

function input(mode, tier, scene, map) {
  const boss = mode >= 2, multi = !!(mode % 2), chess = {};
  const players = Array.from({length: multi ? 2 : 1}, (_, player) => ({playerId: player ? 'q' : 'p', coords: 'field',
    side: player ? 'R' : 'L', colOffset: player && !boss ? 8 : 0,
    bonds: {yanShip: {active: tier > 0, layers: map ? 30 : 0, count: !map && scene === 3 ? 9 : 3, tier: !map && scene === 3 ? 0 : tier}, maniShip: {active: true, layers: 0, count: 1, tier: 1}},
    ...(map && scene ? {contentInfo: {matchBands: ['band_amedic']}} : {}),
    units: Array.from({length: 3}, (_, i) => {
      const id = `a${player * 3 + i}`;
      chess[id] = {chessId: id, baseId: id, tier: 1, position: 'MELEE', bonds: [i === 2 || (!map && scene === 4) ? 'maniShip' : 'yanShip'],
        stats: {maxHp: 1000 + i * 100, atk: 100 + i * 50, def: 0, res: 0, aspd: 100, bat: 1, blockCnt: 0, respawnTime: 70, cost: 0}, rangeGrid: [[0, 0], [0, 1]]};
      return {uid: player * 3 + i + 1, chessId: id, row: boss ? 3 : 10, col: player ? (boss ? 15 - i : 13 + i) : 5 + i,
        dir: boss && player ? 'LEFT' : 'RIGHT', ...(!map && scene === 5 && i < 2 ? {carryState: {down: true}} : {})};
    })}));
  return {players, chess, setup(b) {
    for (const [i, u] of b.allyUnits.entries()) {
      u.profile.noAttack = true;
      b.addBuff(u, {key: 'fixture', mods: {atkPct: 0.5}, persist: true, allowDead: true});
      if (((map && scene === 1) || (!map && scene === 6)) && i % 3 === 0) u.deferDeploy = true;
    }
    b.on('enemySpawn', ({enemy}) => { enemy.profile = {noAttack: true}; });
  }};
}

let snapshots = 0;
for (const [index, {stage, mode, tier, occupied}] of maps.entries()) {
  const fixture = input(mode, tier, occupied, true);
  const b = new Battle({content: 'full', kind: mode >= 2 ? 'boss' : 'normal', autoFinish: false, timeLimit: 100, seed: 1, flags: {layerGainsEnabled: true},
    stage: {...data.stages[stage], devices: []}, rect: mode >= 2 ? {r0: 0, r1: 5, c0: 0, c1: 20} : {r0: 9, r1: 12, c0: 0, c1: mode % 2 ? 20 : 10},
    players: fixture.players, data: {...data, chess: fixture.chess}, setup: fixture.setup});
  b.start(); equal(JSON.parse(actual[index]), snapshot(b), lines[index]); ++snapshots;
  assert.equal(b.errorCount, 0, JSON.stringify(b.errors));
}
for (const [index, {seed, scene}] of fights.entries()) {
  const expected = JSON.parse(actual[maps.length + index]);
  const fixture = input(scene === 8 || scene === 9 ? 1 : 0, scene === 0 ? 1 : scene === 1 || scene === 20 ? 2 : 3, scene, false);
  const routes = Array.from({length: 3}, (_, i) => ({start: [9 + i, i === 0 ? 14 : 13], end: [9 + i, 0]}));
  const b = new Battle({content: 'full', autoFinish: false, timeLimit: 100, seed, flags: {layerGainsEnabled: true},
    stage: {id: 'yanyou', rows: Array(19).fill('r'.repeat(21))}, rect: {r0: 0, r1: 18, c0: 0, c1: 20},
    players: fixture.players, setup: fixture.setup, data: {...data, chess: fixture.chess, enemies: {enemy: {applyWay: 'NONE',
      stats: {maxHp: scene === 16 ? 4000 : 1000000, atk: 30, def: 100, res: 20, moveSpeed: scene === 7 || scene === 8 ? 0.4 : 0, bat: 1, aspd: 100, motion: scene === 18 ? 'FLY' : 'WALK'},
      ...(scene === 19 ? {hitArea: {w: 2, h: 2}} : {})}}},
    routes, spawns: routes.map(route => ({time: scene === 20 ? 40 : 0, enemyKey: 'enemy', ownerPlayerId: 'p', route}))});
  let frame = 0;
  const check = () => { equal(expected[frame], snapshot(b), `fight/${seed}/${scene}/${frame}`); ++frame; ++snapshots; };
  b.start(); check();
  const tokens = b.allyUnits.filter(u => u.defId === 'enemy_9012_acloon');
  for (let tick = 0; tick < 2400; ++tick) {
    const enemy = b.enemies.find(u => u.alive);
    if (tick === 60) { b.addLayers('p', 'yanShip', 40); if (b.players.length > 1) b.addLayers('q', 'yanShip', 80); }
    if (tick % 100 === 10 && enemy) for (const u of tokens) { b.dealDamage(enemy, u, {type: 'element', element: 'burn', amount: 300}); b.dealDamage(enemy, u, {amount: 500, type: 'phys'}); }
    if (scene === 10 && tick === 510) for (const u of tokens) b.applyStatus(u, 'silence', {duration: 5});
    if (scene === 11 && tick === 510) for (const u of tokens) b.applyStatus(u, 'stun', {duration: 4});
    if (scene === 12 && tick === 600 && enemy) b.loseHp(enemy, 100000000);
    if (scene === 13 && tick === 600 && enemy) b.applyStatus(enemy, 'stealth', {duration: 6});
    if (scene === 14 && tick === 200) for (const u of tokens) b.addBuff(u, {key: 'attack', duration: 5, mods: {atkPct: 0.7}});
    if (scene === 15 && tick === 200) for (const u of tokens) b.addBuff(u, {key: 'range', duration: 2, mods: {rangeExtend: 1}});
    if (scene === 17 && tick === 650 && tokens.length) b.loseHp(tokens[0], 100000000);
    if (scene === 21 && (tick === 300 || tick === 600) && enemy) b.applyStatus(b.units.find(u => u.id === enemy.id + 2), 'taunt', {duration: 15, value: 10});
    if (scene === 22 && tick === 450) for (const u of tokens) b.applyStatus(u, 'noMove', {duration: 8});
    if (scene === 23 && (tick === 2 || tick === 200)) for (const u of tokens) { u.skill.gainSp(15); u.skill.activate(); }
    if (tick % 15 === 0) check();
    b.step();
    if (tick % 15 === 0) check();
  }
  assert.equal(frame, expected.length);
  assert.equal(b.errorCount, 0, JSON.stringify(b.errors));
}
console.log(`${maps.length} Yanyou placements and ${fights.length} battle scenarios / ${snapshots} snapshots matched JS`);
