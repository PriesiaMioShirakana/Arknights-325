// Use the upstream plan parser AND event handlers; compare immediate mutations separately from fixed-step updates.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { spawnSync } from 'node:child_process';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const args = process.argv.slice(2), opt = k => args[args.indexOf(k) + 1], root = resolve(opt('--reference'));
const load = p => import(pathToFileURL(resolve(root, p)));
const { Battle } = await load('server/sim/Battle.js');
const { battlePlanOf, install } = await load('server/sim/content/choices.js');
const { setGameData } = await load('server/sim/content/support/index.js');
const effects = JSON.parse(readFileSync(resolve(root, 'data/effects.json'), 'utf8'));
setGameData({ effects });
const ref = (id, i = 0, scene = 0) => ({ id: `choice:${id}#${i}`, key: `choice:${id}`, data: { effectId: id },
  params: scene === 1 ? { prepOk: false } : scene === 2 ? { prepOk: true } : {} });
const choices = JSON.parse(readFileSync(resolve(root, 'data/choices.json'), 'utf8'));
const cardIds = new Set(['bounty', 'tactic'].flatMap(f => choices.cards[f].map(c => c.effectId)));
const plans = Object.keys(effects).filter(id => cardIds.has(id) || ['ENEMY_GAIN', 'BUFF_GAIN'].includes(effects[id].effectType)).sort().map(id => battlePlanOf(ref(id))).filter(Boolean);
const scenes = plans.flatMap(p => Array.from({ length: 7 }, (_, scene) => ({ scene, ids: [p.effectId] })));
for (let scene = 0; scene < 7; scene++) scenes.push({ scene, ids: plans.flatMap(p => [p.effectId, p.effectId]) });
const run = spawnSync(resolve(opt('--native')), [], { input: scenes.map(s => [s.scene, s.ids.length, ...s.ids].join(' ')).join('\n'),
  encoding: 'utf8', timeout: 120000, maxBuffer: 256 * 1024 * 1024 });
assert.equal(run.status, 0, run.stderr);
const actual = JSON.parse(run.stdout);
const attrs = ['atkFlat', 'atkPct', 'atkFinal', 'defFlat', 'defPct', 'hpFlat', 'hpPct', 'resFlat', 'aspd', 'batPct', 'blockCnt', 'rangeExtend',
  'defIgnoreFlat', 'defIgnorePct', 'resIgnoreFlat', 'resIgnorePct', 'physDodge', 'artsDodge', 'spRecoveryFlat', 'extraTargets', 'taunt',
  'hpRegen', 'hpRegenRatio', 'spCostFlat', 'moveFlat', 'massFlat', 'blockRadiusScale', 'atkMul', 'defMul', 'hpMul', 'resMul', 'moveMul',
  'dmgDealtMul', 'dmgTakenMul', 'physTakenMul', 'artsTakenMul', 'trueTakenMul', 'elementTakenMul', 'elementalTakenMul', 'healDealtMul',
  'healTakenMul', 'spRecoveryMul', 'redeployMul', 'atkScaleMul', 'physDealtMul', 'artsDealtMul'];
const mods = xs => Object.fromEntries(xs.map(([key, value]) => { assert(attrs[key], key); return [attrs[key], value]; }));
assert.equal(actual.rules.length, plans.length);
actual.rules.forEach(([id, gate, count, heal, op, flawless, enemies], i) => {
  const p = plans[i]; assert.equal(id, p.effectId);
  assert.equal(['always', 'benchAtLeast', 'benchAtMost', 'sameRow'][gate], p.gate.kind);
  assert.equal(count, p.gate.count ?? 0); assert.equal(heal, p.heal);
  assert.deepEqual(mods(op), p.opMods ?? {}); assert.deepEqual(mods(flawless), p.flawless ?? {});
  assert.deepEqual(enemies.map(([rank, changes]) => ({ rank: [null, 'NORMAL', 'ELITE', 'BOSS'][rank], mods: mods(changes) })), p.enemy);
});
function equal(a, b, path) {
  if (Array.isArray(b)) { assert.equal(a?.length, b.length, path); b.forEach((v, i) => equal(a[i], v, `${path}.${i}`)); }
  else if (b === null) assert.equal(a, null, path);
  else assert(Number.isFinite(a) && Math.abs(a - b) <= 1e-9 * Math.max(1, Math.abs(b)), `${path}: C++ ${a}, JS ${b}`);
}
const stats = { maxHp: 1000, atk: 100, def: 80, res: 30, aspd: 100, bat: 1, moveSpeed: 0, blockCnt: 0, respawnTime: 1, cost: 0 };
let snapshots = 0;
for (const [index, { scene, ids }] of scenes.entries()) {
  const b = new Battle({ content: 'none', autoFinish: false, timeLimit: 60,
    rect: { r0: 0, r1: 18, c0: 0, c1: 20 }, stage: { id: 'choice', rows: Array(19).fill('r'.repeat(21)) },
    data: { chess: { ally: { chessId: 'ally', stats, rangeGrid: [] } }, enemies: Object.fromEntries(['NORMAL', 'ELITE', 'BOSS'].map(rank => [rank, { rank, stats, applyWay: 'NONE' }])) },
    players: [{ playerId: 'one', coords: 'field', playerEffects: ids.map((id, i) => ref(id, i, scene)),
      contentInfo: scene === 5 ? {} : { handUnits: scene === 6 ? 0 : 10 },
      units: Array.from({ length: 4 }, (_, i) => ({ uid: i + 1, ...(i === 3 ? { kind: 'token', tokenId: 'token', def: { stats, rangeGrid: [] } } : { chessId: 'ally' }),
        col: 3 + i, row: scene === 3 ? 4 + i : 4, carryState: { down: scene === 4 && i === 2 } })) },
      { playerId: 'two', coords: 'field', units: [{ uid: 5, chessId: 'ally', col: 15, row: 4 }] }],
    spawns: ['one', 'two'].flatMap(owner => ['NORMAL', 'ELITE', 'BOSS'].map((rank, i) => ({ time: i === 2 ? 0.6 : 0,
      enemyKey: rank, ownerPlayerId: owner, route: { start: [10, 10], end: [10, 0] } }))),
    setup(b) { for (const u of b.allyUnits) u.profile = { noAttack: true }; install(b); }
  });
  const snapshot = () => b.units.map(u => { const s = u.s; return [u.hp, +u.alive, u.s.maxHp, u.s.atk, u.s.def, u.s.res, u.s.moveSpeed, u.s.aspd,
    u.s.defIgnorePct, u.s.resIgnoreFlat, u.s.redeployMul, Number.isFinite(u.respawnAt) ? u.respawnAt : null, u.buffs.filter(x => x.key.startsWith('choice:')).length]; });
  b.start(); let frame = 0;
  const check = () => { equal(actual.scenes[index][frame], snapshot(), `${ids.join('+')} scene ${scene} frame ${frame}`); frame++; snapshots++; };
  check();
  for (let tick = 0; tick < 100; tick++) {
    for (const target of [b.units[0], b.units[3], b.units[4]]) {
      if ([0, 3, 8].includes(tick)) b.dealDamage(null, target, { amount: 110, type: 'true' });
      if (tick === 1) b.loseHp(target, 110);
      if (tick === 2) b.dealDamage(null, target, { amount: 10, type: 'element', element: 'neural' });
      if (tick === 3) b.applyStatus(target, 'noHeal', { duration: 0.1 });
      if (tick === 4) b.dealDamage(null, target, { amount: 110, type: 'true' });
      if (tick === 5) b.dealDamage(null, target, { amount: 110, type: 'elemental' });
      if (tick === 10 || tick === 31) b.heal(null, target, 10000, { self: true });
      if (tick === 12) b.dealDamage(null, target, { amount: 10000, type: 'true' });
    }
    if (tick === 60) b.retreat(b.units[0]);
    if (tick === 63) b.redeploy(b.units[0], { free: true });
    check(); b.step(); check();
  }
  assert.equal(b.errorCount, 0, JSON.stringify(b.errors));
}
console.log(`${plans.length} choice battle plans / ${scenes.length} scenarios / ${snapshots} snapshots matched JS`);
