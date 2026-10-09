import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { spawnSync } from 'node:child_process';

const args = process.argv.slice(2), opt = key => args[args.indexOf(key) + 1], root = resolve(opt('--reference'));
const load = path => import(pathToFileURL(resolve(root, path)));
const { Battle } = await load('server/sim/Battle.js');
const { SkillRuntime } = await load('server/sim/skills.js');
const { install, ID } = await load('server/sim/content/bonds/addon/battle.js');
const { install: installBands } = await load('server/sim/content/bands/battle.js');
const { setGameData } = await load('server/sim/content/support/index.js');
const data = Object.fromEntries(['bands', 'bonds', 'items'].map(k => [k, JSON.parse(readFileSync(resolve(root, `data/${k}.json`), 'utf8'))]));
setGameData(data);
const ids = Object.values(ID);
const bonds = args.includes('--bond') ? [opt('--bond')] : [...ids, '@all'];
const runs = bonds.flatMap(bond => [1, 42, 4294967295].flatMap(seed => Array.from({length: bond === 'raidShip' ? 20 : 16}, (_, scene) => ({bond, seed, scene}))));
const run = spawnSync(resolve(opt('--native')), [], {input: runs.map(r => `${r.bond} ${r.scene} ${r.seed}`).join('\n'),
  encoding: 'utf8', maxBuffer: 256 * 1024 * 1024, timeout: 150000});
assert.equal(run.error?.message, undefined); assert.equal(run.status, 0, run.stderr?.slice(0, 2000));
const actual = run.stdout.trim().split('\n'); run.stdout = '';
assert.equal(actual.length, runs.length);
const stats = {maxHp: 1000, atk: 100, def: 80, res: 30, aspd: 100, bat: 1, moveSpeed: 0, blockCnt: 0, respawnTime: 1, cost: 0};
const positions = [[10, 5], [11, 5], [11, 6], [10, 6], [10, 4], [10, 15]];
const layers = [0, 0, 0, 39, 49, 205, 250, 0, 30, 70, 300, 90, 1, 40, 50, 0];
function equal(a, b, path) {
  if (Array.isArray(b)) { assert.equal(a?.length, b.length, path); b.forEach((v, i) => equal(a[i], v, `${path}.${i}`)); }
  else if (typeof b === 'number') assert(Number.isFinite(a) && Math.abs(a - b) <= 1e-9 * Math.max(1, Math.abs(b)), `${path}: C++ ${a}, JS ${b}`);
  else assert.equal(a, b, path);
}
let snapshots = 0;
for (const [index, {bond, seed, scene}] of runs.entries()) {
  const expected = JSON.parse(actual[index]); actual[index] = null;
  const selected = bond === '@all' ? ids : [bond], offset = scene === 12 ? -7 : 0;
  const names = positions.map((_, i) => `a${i}`);
  const chess = Object.fromEntries(names.map((id, i) => [id, {chessId: id, baseId: id, tier: 1,
    isGolden: i === 1 || i === 5, position: i === 2 || (scene === 18 && i === 0) ? 'RANGED' : 'MELEE',
    bonds: scene !== 15 && (i < 2 || i === 5) ? selected : ['maniShip'],
    stats: {...stats, blockCnt: (scene === 14 || scene >= 16) && i === 0 ? 2 : 0}, rangeGrid: [[0, 0], [0, 1]],
    ...(scene === 8 && i === 0 ? {profession: 'SPECIAL', subProfessionId: 'dollkeeper'} : {})}]));
  const makeBonds = player => Object.fromEntries(selected.map(id => [id, {active: scene !== 0, layers: layers[scene % layers.length] + (player ? 70 : 0), tier: scene === 1 ? 1 : 2, count: 3}]));
  const routes = [0, 1, 2].map(i => ({start: [(i === 1 ? 12 : 10) + offset, i === 2 ? 14 : 9], end: [(i === 1 ? 12 : 10) + offset, 0]}));
  const rows = Array.from({length: 19}, () => Array(21).fill('r'));
  if (scene >= 16) { rows[10][9] = scene === 16 ? 'b' : scene === 17 ? 'R' : 'h'; if (scene === 17) rows[10][8] = 'R'; }
  const b = new Battle({content: 'none', seed, autoFinish: false, timeLimit: 60, kind: scene === 12 ? 'boss' : 'normal',
    flags: {layerGainsEnabled: true}, rect: {r0: 0, r1: 18, c0: 0, c1: 20}, stage: {id: 'bonds', rows: rows.map(row => row.join(''))},
    data: {chess, enemies: {enemy: {stats: {...stats, maxHp: 1000000}, applyWay: 'NONE'}}},
    players: [0, 1].map(player => ({playerId: player ? 'q' : 'p', coords: 'field', side: player ? 'R' : 'L', colOffset: scene === 13 || (player && scene !== 12) ? 8 : 0,
      bandId: scene === 9 && !player ? 'band_ermengard' : null, bonds: makeBonds(player),
      units: names.flatMap((id, i) => (i === 5) === !!player ? [{uid: i + 1, ...(i === 4 ? {kind: 'token', tokenId: id, def: chess[id], ownerUid: 2} : {chessId: id}),
        row: positions[i][0] + offset, col: positions[i][1], dir: ['UP', 'RIGHT', 'DOWN', 'LEFT'][scene % 4]}] : [])})),
    routes, spawns: routes.map((route, i) => ({time: 0, enemyKey: 'enemy', ownerPlayerId: i === 2 ? 'q' : 'p', route})),
    setup(b) {
      for (const [i, u] of b.allyUnits.entries()) {
        u.rangeGrid = chess[names[i]].rangeGrid;
        u.profile = {...u.profile, noAttack: true, dmgType: 'phys', attack: 'melee', maxTargets: 1, dollDuration: 1};
        u.skill = new SkillRuntime(b, u, {skillType: 'MANUAL'}, scene === 10 ? null : {kind: scene === 11 ? 'passive' : 'duration', spCost: 100, initSp: 0, duration: 0.2, trigger: 'NEVER'});
        if (scene === 7 && i === 1) u.deferDeploy = true;
      }
      b.on('enemySpawn', ({enemy}) => { enemy.profile = {noAttack: true}; });
      install(b); installBands(b);
    }
  });
  const snapshot = () => [b.units.map(u => [u.hp, +u.alive, u.s.maxHp, u.s.atk, u.s.def, u.s.aspd,
    u.s.redeployMul, u.s.defIgnorePct, u.s.resIgnorePct, u.s.dmgDealtMul, u.s.physTakenMul, u.s.artsTakenMul, u.s.dmgTakenMul,
    u.skill?.spTotal ?? 0, u.skill && !u.skill.noSkill && u.skill.kind !== 'passive' ? u.skill.spCost : 0, +!!u.skill?.active, u.x, u.y, +!!u.mem.indomFreeDeploy, +!!u.trait.doll, +!!u.trait.dollSwitching,
    Number.isFinite(u.respawnAt) ? u.respawnAt : null]), ids.map(id => b.getPlayer('p').bonds[id]?.layers ?? 0), b.result().perPlayer.p.deaths, b.rng.state()];
  let frame = 0;
  const check = () => { equal(expected[frame], snapshot(), `${bond}/${seed}/${scene}/${frame}`); ++frame; ++snapshots; };
  b.start(); b.step(); check();
  const u = b.allyUnits, e = b.enemies;
  for (let tick = 0; tick < 360; ++tick) {
    if (tick % 30 === 0) for (const a of u) if (a.alive) { a.skill?.gainSp(100); a.skill?.activate(); }
    if (tick % 40 === 10) for (const a of u) a.skill?.end();
    if (tick % 17 === 0) { b.dealDamage(u[0], e[0], {amount: 170, type: 'arts'}); b.dealDamage(u[1], e[0], {amount: 130, type: 'arts'}); }
    if (tick === 30) b.dealDamage(u[5], e[0], {amount: 140, type: 'arts'});
    if (tick % 31 === 3) for (const a of u) b.dealDamage(e[0], a, {amount: 230, type: tick % 2 ? 'phys' : 'arts'});
    if (tick % 45 === 5) b.loseHp(u[0], 100000);
    if (tick % 45 === 6) b.redeploy(u[0], {free: true});
    if (tick === 80) b.retreat(u[1]);
    if (tick === 90) { b.redeploy(u[1], {free: true}); u[0].skill.setSpTotal(100); }
    if (tick === 120) for (const id of ids) { b.addLayers('p', id, 25); b.addLayers('p', id, 30); }
    if (tick === 150) b.loseHp(e[0], 550000);
    if (tick === 180) b.relocate(u[2], 11 + offset, 7);
    if (tick === 220) b.relocate(u[2], 11 + offset, 6);
    if (tick === 55) { b.dealDamage(e[0], u[0], {type: 'element', element: 'burn', amount: 50}); b.dealDamage(e[0], u[2], {amount: 50, type: 'elemental'}); }
    if (tick % 50 === 11) b.dealDamage(null, u[0], {amount: 20, type: 'true'});
    if (tick % 50 === 12) b.dealDamage(e[0], u[0], {amount: 20, type: 'true'});
    if (tick % 90 === 15) for (const a of u) b.heal(a, a, 10000);
    if (scene === 8 && tick === 210) b.emit('dollSwitch', {unit: u[0]});
    if (tick % 10 === 0) check();
    b.step();
    if (tick % 10 === 0) check();
  }
  assert.equal(frame, expected.length);
  assert.equal(b.errorCount, 0, JSON.stringify(b.errors));
}
console.log(`${runs.length} addon bond scenarios / ${snapshots} snapshots matched JS`);
