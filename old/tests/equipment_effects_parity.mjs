import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { spawnSync } from 'node:child_process';

const args = process.argv.slice(2), opt = key => args[args.indexOf(key) + 1], root = resolve(opt('--reference'));
const load = path => import(pathToFileURL(resolve(root, path)));
const { Battle } = await load('server/sim/Battle.js');
const { SkillRuntime } = await load('server/sim/skills.js');
const { install: itemsInstall, lendItemEffects, itemGrants, hasBattleEffect } = await load('server/sim/content/items/battle.js');
const { install: bandsInstall } = await load('server/sim/content/bands/battle.js');
const { setGameData } = await load('server/sim/content/support/index.js');
const data = Object.fromEntries(['items', 'bonds', 'bands'].map(k => [k, JSON.parse(readFileSync(resolve(root, `data/${k}.json`), 'utf8'))])); setGameData(data);
const lending = args.includes('--lending');
const catalog = Object.values(data.items).filter(hasBattleEffect).map(x => x.id).sort();
assert.equal(catalog.length, 82);
const periodic = ['1_05', '3_01', '3_08', '6_04'];
const signatures = ['5_09', '6_01', '6_02', '6_03', '6_06', '6_07', '6_10', '6_11'];
const isSignature = item => signatures.some(id => item.includes(`_${id}_e_`));
const hammers = ['2_03', '3_09', '3_10', '4_09', '6_05'];
const isHammer = item => hammers.some(id => item.includes(`_${id}_e_`));
const selected = args.includes('--hammers') ? hammers : [...signatures, ...periodic, '3_02', '3_04', '3_05', '3_06', '3_11', '4_03', '4_04', '4_05', '4_06', '4_07', '4_08', '4_10', '4_11', '4_12', '5_01', '5_02', '5_03'];
const ids = lending ? catalog.filter(id => !args.includes('--quality') || id.endsWith('_' + opt('--quality'))) : selected.flatMap(id => ['a', 'b'].map(suffix => `chess_item_${id}_e_${suffix}`)); ids.forEach(id => assert(data.items[id]));
const cases = ids.flatMap(item => (lending ? [1, 42] : [1, 42, 4294967295]).flatMap(seed => Array.from({length: lending ? 12 : isHammer(item) ? 24 : isSignature(item) ? 16 : periodic.some(id => item.includes(`_${id}_e_`)) ? 12 : 6}, (_, scene) => ({item, seed, scene}))));
const run = spawnSync(resolve(opt('--native')), [], {input: '@catalog\n' + cases.map(r => `${r.item} ${r.scene + (lending ? 100 : 0)} ${r.seed}`).join('\n'), encoding: 'utf8', maxBuffer: 256 * 1024 * 1024, timeout: 120000});
assert.equal(run.error?.message, undefined); assert.equal(run.status, 0, run.stderr.slice(0, 2000));
const actual = run.stdout.trim().split('\n'); run.stdout = '';
assert.deepEqual(JSON.parse(actual.shift()), catalog); assert.equal(actual.length, cases.length);
function equal(a, b, path) {
  if (Array.isArray(b)) { assert.equal(a?.length, b.length, path); b.forEach((v, i) => equal(a[i], v, `${path}.${i}`)); }
  else if (typeof b === 'number') assert(Number.isFinite(a) && Math.abs(a - b) <= 1e-9 * Math.max(1, Math.abs(b)), `${path}: C++ ${a}, JS ${b}`);
  else assert.equal(a, b, path);
}
let snapshots = 0;
for (const [index, {item, seed, scene}] of cases.entries()) {
  const expected = JSON.parse(actual[index]); actual[index] = null;
  let receipt = 0;
  const signature = isSignature(item), hammer = isHammer(item), steam = item.startsWith('chess_item_6_05'), bond = hammer ? 'victoriaShip' : data.items[item].requiresBondId || 'egirShip';
  const hammerItems = ['4_09', '3_09', '3_10', '2_03'].map(id => `chess_item_${id}_e_a`);
  const partner = data.items[item].buffs.map(b => b.bbStr?.equip_chess_id).find(Boolean);
  const stats = {maxHp: 3000, atk: 100, def: 80, res: 20, aspd: 100, bat: 1, blockCnt: 1, respawnTime: 70, moveSpeed: 0, cost: 0};
  const op = {chessId: 'op', stats, position: scene === 22 ? 'RANGED' : 'MELEE', bonds: [scene === 8 ? 'other' : scene === 9 || scene === 10 ? 'maniShip' : bond], rangeGrid: (scene >= 12 ? [4, 3, 0, 1, 2, 5] : [0, 1, 2, 3, 4, 5]).map(col => [0, col])};
  const positions = [[5, 5], [5, scene === 0 ? 4 : 6], [6, 5], [6, 6]];
  const units = positions.map(([row, col], i) => ({uid: i + 1, ...(i === 2 && (scene === 3 || scene === 7) ? {kind: 'token', tokenId: scene >= 6 ? 'enemy_ally' : 'op', def: op} : {chessId: scene >= 6 && i >= 2 ? 'enemy_ally' : 'op'}), row, col,
    dir: scene === 11 ? 'UP' : scene === 5 ? 'LEFT' : 'RIGHT', items: i === 0 ? [item, ...(partner && (scene === 7 || scene >= 11) ? [partner + '_a'] : [])] : i === 1 && scene === 6 ? [item] : []}));
  if (lending && scene === 2) units[1].items = [item];
  if (lending && scene === 5) units[2].items = [item];
  if (lending && scene === 4) units[1].items = ['chess_item_6_05_e_a'];
  if (hammer && scene >= 16) {
    if (!steam) units[0].items.push('chess_item_6_05_e_a');
    if (steam) units[scene % 2 ? 1 : 0].items.push(hammerItems[Math.floor((scene - 16) / 2)]);
    if (scene === 23) units[0].items.push(item);
  }
  const b = new Battle({content: 'none', seed, autoFinish: false, timeLimit: 60, rect: {r0: 0, r1: 18, c0: 0, c1: 20}, stage: {rows: Array(19).fill('r'.repeat(21))},
    data: {chess: {op, enemy_ally: {...op, chessId: 'enemy_ally'}}, enemies: {enemy: {stats: {...stats, maxHp: 300000, blockCnt: 0, moveSpeed: scene === 15 ? 2 : 0}, applyWay: 'NONE'}}},
    players: [{playerId: 'p', coords: 'field', units: units.slice(0, 3), contentInfo: {roundStats: {gainedChess: scene % 5}}, bonds: scene === 9 || scene === 10 ? {[bond]: {active: true, layers: 0}, maniShip: {active: scene === 9, layers: 0}} : {}, bandId: scene === 5 ? 'band_ermengard' : null}, {playerId: 'q', coords: 'field', units: units.slice(3)}],
    spawns: [5, 9].map(col => ({time: 0, enemyKey: 'enemy', ownerPlayerId: 'p', route: {start: [5, col], end: [5, 0]}})),
    setup(b) {
      for (const [i, u] of b.allyUnits.entries()) { u.rangeGrid = op.rangeGrid; u.profile = {noAttack: true, dmgType: scene === 4 ? 'heal' : scene === 3 || (hammer && scene >= 16) ? 'arts' : 'phys', maxTargets: 2, ...(scene === 4 ? {heal: {mode: 'single'}} : {})}; if (scene === 1 && i === 2) u.deferDeploy = true; if ((signature || lending) && i > 0) u.skill = new SkillRuntime(b, u, {skillType: 'MANUAL'}, {kind: lending && scene % 2 === 0 ? 'ammo' : 'duration', spCost: 100, duration: .2, ammo: 4, trigger: 'NEVER'}); }
      const u = b.allyUnits[0]; u.skill = new SkillRuntime(b, u, {skillType: 'MANUAL'}, {kind: scene % 2 ? 'duration' : 'ammo', spCost: 100, initSp: scene === 14 ? 100 : 1, activateOnDeploy: scene === 14, duration: .2, ammo: 4, trigger: 'NEVER'});
      b.on('enemySpawn', ({enemy}) => { enemy.profile = {noAttack: true}; }); itemsInstall(b); bandsInstall(b);
    }});
  const snapshot = () => [...b.units.map(u => [u.hp, u.s.maxHp, u.s.atk, u.s.aspd, u.s.moveSpeed, u.x, u.y, u.skill?.spTotal ?? 0, +!!u.skill?.active, u.skill?.ammoLeft ?? 0, +u.alive,
    ...['stealth', 'stun', 'cold', 'freeze', 'levitate', 'silence', 'tremble'].map(k => +!!u.s.flags[k]), u.findBuff('palsy')?.stacks ?? 0, u.elem.burn, u.blocking.length, u.buffs.reduce((n, x) => n + (x.shieldHits || 0), 0), Number.isFinite(u.respawnAt) ? u.respawnAt : null]), b.units.flatMap(u => itemGrants(b, u).filter(g => g.lent).map(g => [u.id, g.id, g.until])), receipt, b.rng.state()];
  let frame = 0; const check = () => { equal(expected[frame], snapshot(), `${item}/${seed}/${scene}/${frame}`); ++frame; ++snapshots; };
  b.start(); b.step(); check(); const [u, a, c] = b.allyUnits, [e, f] = b.enemies;
  for (let tick = 0; tick < 900; ++tick) {
    if (lending) {
      if ([0, 30, 120, 300].includes(tick)) receipt = lendItemEffects(b, u, a, {maxTier: scene % 2 ? 6 : 5, duration: 2.5});
      if (scene === 5 && tick === 45) receipt = lendItemEffects(b, c, a, {maxTier: 6, duration: 2.5});
      if (tick % 30 === 0 && a.alive) { a.skill.gainSp(100); a.skill.activate(); }
      if (tick % 5 === 0) b.forceAttack(a, [e, f]);
      if (tick % 30 === 1) b.dealDamage(e, a, {amount: 300, type: 'arts'});
      if (tick % 90 === 4) b.loseHp(a, 100000);
      if (tick % 90 === 5) b.redeploy(a, {free: true});
      if (tick % 60 === 20) b.applyStatus(a, 'stealth', {duration: 1, source: a});
      if (scene === 6 && tick === 70) b.retreat(a);
      if (scene === 6 && tick === 100) b.redeploy(a, {free: true});
    }
    if (tick % 30 === 0 && u.alive) { u.skill.gainSp(100); u.skill.activate(); }
    if (tick % 5 === 0 && (scene < 6 || tick < 30 || signature || hammer)) b.forceAttack(u, scene === 4 ? [a, c] : [e, f]);
    if (scene < 12 && tick % 30 === 1) { b.dealDamage(e, u, {amount: scene >= 6 ? 700 : 140, type: 'phys'}); b.dealDamage(f, u, {amount: scene >= 6 ? 600 : 130, type: 'arts'}); }
    if (scene >= 6 && scene < 12 && tick % 17 === 6) {
      b.dealDamage(c, e, {amount: 50, type: 'true'}); b.dealDamage(b.allyUnits[3], f, {amount: 50, type: 'true'});
      b.dealDamage(e, u, {amount: 450, type: 'true', tags: ['item']});
    }
    if (scene >= 6 && tick % 90 === 8) b.heal(a, u, 10000);
    if (tick % 15 === 2) { b.heal(u, a, 55); b.heal(u, u, 20, {self: true}); b.heal(u, u, 20, {regen: true}); }
    if (tick % 60 === 3) for (const status of ['stun', 'cold', 'cold', 'levitate']) b.applyStatus(u, status, {duration: .4, source: e});
    if (scene !== 2 && scene < 12 && tick % 90 === 4) b.loseHp(u, 100000);
    if (scene !== 2 && scene < 12 && tick % 90 === 5) b.redeploy(u, {free: true});
    if (hammer && scene >= 16) {
      if (tick % 30 === 6) b.dealDamage(u, f, {amount: 600, type: 'arts'});
      if (tick % 180 === 4) b.loseHp(u, 100000);
      if (tick % 180 === 5) b.redeploy(u, {free: true});
      if (tick % 180 === 30) b.retreat(a);
      if (tick % 180 === 60) b.redeploy(a, {free: true});
      if (scene === 21 && tick % 150 === 50) { b.retreat(u); b.redeploy(u, {free: true}); }
    }
    if (signature && tick % 180 === 20) b.applyStatus(u, 'stealth', {duration: 2, source: u});
    if (signature && tick % 60 === 10) { b.applyStatus(e, 'cold', {duration: 1.5, source: u}); b.applyStatus(f, scene % 2 ? 'cold' : 'freeze', {duration: 1.5, source: u}); }
    if (signature && tick === 15) b.loseHp(u, 1000);
    b.step(); if (tick % 10 === 0 || tick % 90 === 4 || tick % 90 === 5) check();
  }
  assert.equal(frame, expected.length); assert.equal(b.errorCount, 0, JSON.stringify(b.errors));
}
console.log(`equipment events: ${lending ? 'lending ' + ids.length + ' records' : args.includes('--hammers') ? '5 hammer behaviours, 10 records' : '30 behaviours, 58 records'}, ${cases.length} scenarios, ${snapshots} state snapshots`);
