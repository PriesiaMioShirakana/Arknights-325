import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { spawnSync } from 'node:child_process';

const args = process.argv.slice(2), opt = key => args[args.indexOf(key) + 1], root = resolve(opt('--reference'));
const load = path => import(pathToFileURL(resolve(root, path)));
const { Battle } = await load('server/sim/Battle.js');
const { setupUnitKit } = await load('server/sim/content/index.js');
const { SkillRuntime } = await load('server/sim/skills.js');
const { install } = await load('server/sim/content/bonds/core.js');
const { install: installItems, itemGrants } = await load('server/sim/content/items/battle.js');
const { install: installBands } = await load('server/sim/content/bands/battle.js');
const { setGameData } = await load('server/sim/content/support/index.js');
const data = Object.fromEntries(['bands', 'bonds', 'items', 'tokens'].map(k => [k, JSON.parse(readFileSync(resolve(root, `data/${k}.json`), 'utf8'))]));
setGameData(data);
const ids = ['yanShip', 'sargonShip', 'victoriaShip', 'kjeragShip', 'lateranoShip', 'egirShip', 'siracusaShip', 'kazimierzShip'];
const bonds = args.includes('--bond') ? [opt('--bond')] : [...ids, '@all'];
const runs = bonds.flatMap(bond => [1, 42, 4294967295].flatMap(seed => Array.from({length: 12}, (_, scene) => ({bond, seed, scene}))));
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
  const selected = bond === '@all' ? ids : [bond], offset = 0, share = [4, 7, 9].includes(scene);
  const names = positions.map((_, i) => `a${i}`);
  const chess = Object.fromEntries(names.map((id, i) => [id, {chessId: id, baseId: id, tier: 1,
    isGolden: i === 1 || i === 5, position: i === 2 ? 'RANGED' : 'MELEE',
    bonds: (i < 2 || i === 5) ? selected : ['maniShip'],
    stats: {...stats, blockCnt: scene >= 3 && i === 0 ? 2 : 0}, rangeGrid: [[0, 0], [0, 1]],
    ...(scene === 8 && i === 0 ? {profession: 'SPECIAL', subProfessionId: 'dollkeeper'} : {})}]));
  const makeBonds = player => Object.fromEntries(selected.map(id => [id, {active: scene !== 0, layers: layers[scene % layers.length] + (player ? 70 : 0), tier: scene === 2 ? 0 : scene === 1 ? 1 : 2, count: scene === 2 ? 6 : 3}]));
  const routes = [0, 1, 2].map(i => ({start: [(i === 1 ? 10.4 : 10) + offset, i === 2 ? 14 : i === 1 ? 5.6 : 5.4], end: [(i === 1 ? 10.4 : 10) + offset, 0]}));
  const items = i => scene === 0 ? [] : i === 0 ? ['chess_item_3_04_e_a', 'chess_item_6_07_e_b'] : i === 1 ? ['chess_item_2_03_e_a', 'chess_item_3_01_e_a'] : [];
  const rows = Array.from({length: 19}, () => Array(21).fill('r'));
  const b = new Battle({content: 'none', seed, autoFinish: false, timeLimit: 90, kind: 'normal',
    flags: {layerGainsEnabled: true}, rect: {r0: 0, r1: 18, c0: 0, c1: 20}, stage: {id: 'bonds', rows: rows.map(row => row.join(''))},
    data: {...data, chess, enemies: {enemy: {stats: {...stats, maxHp: 1000000}, applyWay: 'NONE'}}},
    players: [0, 1].map(player => ({playerId: player ? 'q' : 'p', coords: 'field', side: player ? 'R' : 'L', colOffset: scene === 13 || (player && scene !== 12) ? 8 : 0,
      bandId: share ? 'band_narant' : null, bonds: {...makeBonds(player), ...([6, 7].includes(scene) ? {maniShip: {active: true, tier: 1, layers: 0}} : {})},
      units: names.flatMap((id, i) => (i === 5) === !!player ? [{uid: i + 1, ...(i === 4 ? {kind: 'token', tokenId: id, def: chess[id], ownerUid: 2} : {chessId: id}),
        items: items(i), row: positions[i][0] + offset, col: positions[i][1], dir: ['UP', 'RIGHT', 'DOWN', 'LEFT'][scene % 4]}] : [])})),
    routes, spawns: routes.map((route, i) => ({time: 0, enemyKey: 'enemy', ownerPlayerId: i === 2 ? 'q' : 'p', route})),
    setup(b) {
      const setupUnit = b._setupUnit.bind(b);
      b._setupUnit = (u, kit = null) => setupUnit(u, u.defId === 'enemy_9012_acloon' ? setupUnitKit(b, u, 'full') : kit);
      for (const [i, u] of b.allyUnits.entries()) {
        u.rangeGrid = chess[names[i]].rangeGrid;
        u.profile = {...u.profile, noAttack: true, dmgType: 'phys', attack: 'melee', maxTargets: 1, dollDuration: 1};
        u.skill = new SkillRuntime(b, u, {skillType: 'MANUAL'}, scene === 10 ? null : {kind: scene === 11 ? 'passive' : scene % 2 ? 'duration' : 'ammo', spCost: 100, initSp: 0, duration: 0.2, ammo: 4, trigger: 'NEVER'});
        if (scene === 7 && i === 1) u.deferDeploy = true;
      }
      b.on('enemySpawn', ({enemy}) => { enemy.profile = {noAttack: true}; });
      install(b); installItems(b);
      if (scene === 9) { b.getPlayer('p').bandId = 'band_ermengard'; installBands(b); }
    }
  });
  const snapshot = () => [b.units.map(u => [u.hp, +u.alive, u.s.maxHp, u.s.atk, u.s.def, u.s.aspd,
    u.s.redeployMul, u.s.defIgnorePct, u.s.resIgnorePct, u.s.dmgDealtMul, u.s.physTakenMul, u.s.artsTakenMul, u.s.dmgTakenMul,
    u.skill?.spTotal ?? 0, u.skill && !u.skill.noSkill && u.skill.kind !== 'passive' ? u.skill.spCost : 0, +!!u.skill?.active, u.x, u.y, +!!u.mem.indomFreeDeploy, +!!u.trait.doll, +!!u.trait.dollSwitching,
    Number.isFinite(u.respawnAt) ? u.respawnAt : null, u.skill?.ammoLeft ?? 0, u.skill?.ammoMax ?? 0,
    u.buffs.filter(buff => buff.key === 'bond:sargon').map(buff => buff.timeLeft), itemGrants(b, u).filter(g => g.lent).map(g => [g.id, g.until])]), ids.map(id => b.getPlayer('p').bonds[id]?.layers ?? 0), b.result().perPlayer.p.deaths, b.rng.state()];
  let frame = 0;
  const check = () => { equal(expected[frame], snapshot(), `${bond}/${seed}/${scene}/${frame}`); ++frame; ++snapshots; };
  b.start(); b.step(); check();
  const u = b.allyUnits.slice(0, 6), e = b.enemies;
  for (let tick = 0; tick < 2100; ++tick) {
    if (tick % 9 === 0 && tick < 300) for (const a of u) if (a.alive) { a.skill?.gainSp(100); a.skill?.activate(); }
    if (tick % 40 === 10) for (const a of u) a.skill?.end();
    if (tick % 3 === 0) { b.forceAttack(u[0], [e[0], e[1]]); b.forceAttack(u[1], [e[0], e[1]]); }
    if (tick === 45 || tick === 75) b.applyStatus(e[0], 'cold', {duration: 1.5});
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
    if ((tick % 30 === 0 || (bond === '@all' && scene === 6 && tick < 300))) check();
    b.step();
    if ((tick % 30 === 0 || (bond === '@all' && scene === 6 && tick < 300))) check();
  }
  assert.equal(frame, expected.length);
  assert.equal(b.errorCount, 0, JSON.stringify(b.errors));
}
console.log(`${runs.length} core bond scenarios / ${snapshots} snapshots matched JS`);
