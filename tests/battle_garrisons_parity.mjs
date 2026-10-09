import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { spawnSync } from 'node:child_process';

const args = process.argv.slice(2), opt = key => args[args.indexOf(key) + 1], root = resolve(opt('--reference'));
const load = path => import(pathToFileURL(resolve(root, path)));
const { Battle } = await load('server/sim/Battle.js');
const { SkillRuntime } = await load('server/sim/skills.js');
const { install } = await load('server/sim/content/garrisons/battle.js');
const { setGameData } = await load('server/sim/content/support/index.js');
const data = Object.fromEntries(['garrisons', 'bonds'].map(k => [k, JSON.parse(readFileSync(resolve(root, `data/${k}.json`), 'utf8'))]));
setGameData(data);
const ids = Object.keys(data.garrisons).filter(id => data.garrisons[id].eventType === 'IN_BATTLE').sort();
const selected = args.includes('--garrison') ? [opt('--garrison')] : ids;
const scenes = args.includes('--scene') ? [+opt('--scene')] : args.includes('--garrison') && opt('--garrison') === '@mixed' ? Array.from({length: 12}, (_, i) => i) : [0, 1, 2, 3];
const runs = selected.flatMap(gid => [1, 42].flatMap(seed => scenes.map(scene => ({gid, seed, scene}))));
if (!args.includes('--garrison')) runs.push(...[1, 42, 4294967295].flatMap(seed => Array.from({length: 12}, (_, scene) => ({gid: '@mixed', seed, scene}))));
const mixed = ['garrison_145_a', 'garrison_144_a', 'garrison_160_a', 'garrison_159_a', 'garrison_72_a', 'garrison_95_a', 'garrison_148_a', 'garrison_108_a', 'garrison_106_a', 'garrison_90_a', 'garrison_42_a', 'garrison_22_a', 'garrison_24_a', 'garrison_40_a', 'garrison_125_a', 'garrison_28_a', 'garrison_05_a', 'garrison_115_a', 'garrison_101_a', 'garrison_18_a', 'garrison_01_a', 'garrison_126_a', 'garrison_118_b'];
const run = spawnSync(resolve(opt('--native')), [], {input: runs.map(r => `${r.gid} ${r.scene} ${r.seed}`).join('\n'), encoding: 'utf8', maxBuffer: 256 * 1024 * 1024, timeout: 150000});
assert.equal(run.error?.message, undefined); assert.equal(run.status, 0, run.stderr);
const actual = run.stdout.trim().split('\n'); run.stdout = '';
assert.equal(actual.length, runs.length);
const bonds = Object.keys(data.bonds), positions = [[10, 5], [10, 6], [10, 7], [9, 5], [11, 5], [10, 15], [10, 4]];
const stats = {maxHp: 1000, atk: 100, def: 50, res: 20, aspd: 100, bat: 1, blockCnt: 0, respawnTime: 1, cost: 0};
const rangeGrid = Array.from({length: 7}, (_, r) => Array.from({length: 7}, (_, c) => [r - 3, c - 3])).flat();

function equal(a, b, path) {
  if (Array.isArray(b)) { assert.equal(a?.length, b.length, path); b.forEach((v, i) => equal(a[i], v, `${path}.${i}`)); }
  else if (typeof b === 'number') assert(Number.isFinite(a) && Math.abs(a - b) <= 1e-9 * Math.max(1, Math.abs(b)), `${path}: C++ ${a}, JS ${b}`);
  else assert.equal(a, b, path);
}

let snapshots = 0;
for (const [index, {gid, seed, scene}] of runs.entries()) {
  const expected = JSON.parse(actual[index]); actual[index] = null;
  const names = positions.map((_, i) => `a${i}`);
  const chess = Object.fromEntries(names.map((id, i) => [id, {chessId: id, baseId: id, tier: i + 1, position: 'MELEE', bonds: scene === 5 && (i === 1 || i === 2) ? ['maniShip'] : bonds,
    garrisonIds: [0, 1, 4, 5].includes(i) ? gid === '@mixed' ? mixed : [gid, gid] : i === 6 ? ['garrison_59_b'] : [], stats, rangeGrid,
    ...((scene === 2 || scene >= 4) && i === 0 ? {profession: 'SPECIAL', subProfessionId: 'dollkeeper'} : {})}]));
  const enemies = Object.fromEntries(Array.from({length: 24}, (_, i) => [`e${i}`, {stats: {...stats, maxHp: 1000000, atk: 10, def: 80, res: 40, moveSpeed: 0}, tags: [i % 2 ? 'drone' : 'seamonster'], applyWay: 'NONE'}]));
  const route = {start: [10, 7], end: [10, 0]};
  const b = new Battle({content: 'none', seed, autoFinish: false, timeLimit: 200, flags: {layerGainsEnabled: scene !== 3},
    rect: {r0: 0, r1: 18, c0: 0, c1: 20}, stage: {id: 'garrisons', rows: Array(19).fill('r'.repeat(21))}, data: {...data, chess, enemies},
    players: [0, 1].map(player => ({playerId: player ? 'q' : 'p', coords: 'field', side: player ? 'R' : 'L', colOffset: player ? 8 : 0,
      bonds: Object.fromEntries(bonds.map((id, i) => [id, {active: (scene !== 1 || i % 3 !== 0) && (scene !== 5 || i >= 8), layers: (scene === 2 || scene >= 4) ? 300 : scene === 1 ? 37 : 0, count: 3, tier: 1}])),
      units: names.flatMap((id, i) => (i === 5) === !!player ? [{uid: i + 1, ...(i === 4 ? {kind: 'token', tokenId: id, def: chess[id]} : {chessId: id}),
        row: scene === 9 && i === 5 ? 9 : positions[i][0], col: scene === 9 && i === 5 ? 7 : positions[i][1],
        ...(scene === 6 && i === 0 ? {carryState: {down: true}} : {}), dir: scene === 1 && i < 2 ? 'UP' : 'RIGHT'}] : [])})),
    routes: [route], spawns: Object.keys(enemies).map(enemyKey => ({time: 0, enemyKey, ownerPlayerId: 'p', route})),
    setup(b) {
      b.getPlayer('p').mirror = scene === 2;
      for (const u of b.allyUnits) {
        u.rangeGrid = rangeGrid;
        u.profile = {...u.profile, noAttack: true, dmgType: 'phys', attack: 'melee', maxTargets: 1, dollDuration: 1};
        u.skill = new SkillRuntime(b, u, {skillType: 'MANUAL'}, {kind: scene === 11 ? 'passive' : 'ammo', spType: 'time', spCost: 10, ammo: 3, trigger: 'NEVER'});
      }
      if (scene === 6) b.allyUnits[1].deferDeploy = true;
      b.on('enemySpawn', ({enemy}) => { enemy.profile = {noAttack: true}; });
      install(b);
    }});
  const snapshot = () => [b.allyUnits.map(u => [u.hp, +u.alive, u.s.maxHp, u.s.atk, u.s.def, u.s.aspd, u.s.redeployMul,
    u.s.spRecovery, u.s.hpRegen, u.skill?.spTotal ?? 0, +!!u.skill?.active, u.skill?.ammoLeft ?? 0, +!!u.trait.doll, u.stats.dmg,
    u.buffs.filter(v => v.key.startsWith('gar:')).map(v => [v.key, Number.isFinite(v.timeLeft) ? v.timeLeft : null])]),
    b.players.map(p => bonds.map(id => p.bonds[id].layers)), b.units.filter(u => u.side === 'enemy').map(e => e.hp), b.rng.state()];
  let frame = 0;
  const check = () => { equal(expected[frame], snapshot(), `${gid}/${seed}/${scene}/${frame}`); ++frame; ++snapshots; };
  b.start(); b.step(); check();
  const units = b.allyUnits;
  for (let tick = 0; tick < (scene === 4 ? 3630 : 360); ++tick) {
    const enemy = b.enemies.find(e => e.alive);
    if (tick < 360 && tick % 10 === 0) for (const u of units) { u.skill.end(); u.skill.setSpTotal(10); u.skill.activate(); }
    if (enemy && tick < 360) {
      if (tick % 3 === 0) for (const u of units) b.forceAttack(u, [enemy]);
      if (scene === 10 && tick % 10 === 1) b.addBuff(enemy, {key: 'ward', duration: 0.2, status: 'freeze'});
      if (tick % 10 === 1) { b.applyStatus(enemy, 'freeze', {duration: 2}); b.applyStatus(enemy, 'freeze', {duration: 2}); b.applyStatus(enemy, 'stun', {duration: 2}); }
      if (tick % 10 === 2) b.applyStatus(enemy, 'freeze', {duration: 2, reenter: true});
      if (scene !== 10 && tick % 10 === 3) { b.applyStatus(enemy, 'bind', {duration: 1}); b.applyStatus(enemy, 'sluggish', {duration: 1}); }
      if (scene === 10 && tick % 10 === 3) b.addBuff(enemy, {key: 'sluggish', duration: 1});
      if (tick % 10 === 4) b.dealDamage(units[0], enemy, {amount: 170, type: 'arts', tags: ['dot']});
      if (tick % 13 === 12) b.loseHp(enemy, 10000000, {source: units[0]});
    }
    if (tick < 360 && tick % 40 === 6) b.applyStatus(units[1], 'sleep', {duration: 0.3});
    if (tick < 360 && tick % 50 === 7) b.loseHp(units[1], 10000000, {source: units[0]});
    if (tick < 360 && tick % 50 === 8) b.redeploy(units[1], {free: true});
    if (scene === 7 && tick === 0) b.addBuff(units[0], {key: 'school', duration: 100, mods: {physDealtMul: 2, artsDealtMul: 0.2}});
    if (scene === 8 && tick === 0) for (const id of [2, 3, 6]) b.relocate(units[id-1], 6, 5 + id);
    if (scene === 8 && tick === 150) { b.relocate(units[1], 10, 6); b.relocate(units[2], 10, 7); b.relocate(units[5], 10, 4); }
    if (tick === 90) b.emit('dollSwitch', {unit: units[0]});
    if (tick === 130) b.retreat(units[0]);
    if (tick === 132) b.redeploy(units[0], {free: true});
    if (tick === 60 || tick === 200) { b.addLayers('p', 'yanShip', 51); b.addLayers('q', 'sargonShip', 100); }
    if (tick === 180) { b.getPlayer('p').bonds.sargonShip.layers = 600; b.addLayers('p', 'sargonShip', 1); }
    if (tick === 210) { b.getPlayer('p').bonds.sargonShip.layers = 0; b.addLayers('p', 'yanShip', 1); }
    if (scene !== 4 && tick === 230) for (const id of bonds) b.getPlayer('p').bonds[id].layers = 0;
    if (tick === 231) b.addLayers('p', 'yanShip', 1);
    if ((tick < 360 && tick % 10 === 0) || (tick >= 360 && tick % 60 === 0)) check();
    b.step();
    if ((tick < 360 && tick % 10 === 0) || (tick >= 360 && tick % 60 === 0)) check();
  }
  assert.equal(frame, expected.length);
  assert.equal(b.errorCount, 0, JSON.stringify(b.errors));
}
console.log(`${selected.length} battle garrisons, ${runs.length} scenarios / ${snapshots} snapshots matched JS`);
