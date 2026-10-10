import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { spawnSync } from 'node:child_process';

const args = process.argv.slice(2), opt = key => args[args.indexOf(key) + 1], root = resolve(opt('--reference'));
const load = path => import(pathToFileURL(resolve(root, path)));
const { Battle } = await load('server/sim/Battle.js');
const { SkillRuntime } = await load('server/sim/skills.js');
const { install } = await load('server/sim/content/items/battle.js');
const { setGameData } = await load('server/sim/content/support/index.js');
const templates = new Set(['attr_common_global_buff', 'act1autochess_equip_acarm056_global_buff', 'act1autochess_equip_acarm050_global_buff', 'magic_penetrate_global_buff', 'act1autochess_equip_acarm054_global_buff']);
// This suite isolates the five stat templates; event procs and equipment sets require their own full-record suites.
const items = JSON.parse(readFileSync(resolve(root, 'data/items.json'), 'utf8'));
for (const record of Object.values(items)) record.buffs = record.buffs.filter(b => templates.has(b.bbStr?.key));
setGameData({items});
const ids = Object.keys(items).filter(id => items[id].buffs.length).sort(); assert.equal(ids.length, 46);
const cases = [[], ...ids.flatMap((id, i) => [[id], [id, id], [id, ids[(i + 1) % ids.length]], [id, ids[(i + 19) % ids.length]]])];
const run = spawnSync(resolve(opt('--native')), [], {input: cases.map(ids => `${ids.length} ${ids.join(' ')}`).join('\n'), encoding: 'utf8', timeout: 30000});
assert.equal(run.error, undefined); assert.equal(run.status, 0, run.stderr);
const actual = run.stdout.trim().split('\n').map(JSON.parse); assert.equal(actual.length, cases.length);
function equal(a, b, path) {
  if (Array.isArray(b)) { assert.equal(a?.length, b.length, path); b.forEach((v, i) => equal(a[i], v, `${path}.${i}`)); }
  else assert(Number.isFinite(a) && Math.abs(a - b) <= 1e-9 * Math.max(1, Math.abs(b)), `${path}: C++ ${a}, JS ${b}`);
}
for (const [index, equipped] of cases.entries()) {
  const stats = {maxHp: 1000, atk: 100, def: 200, res: 20, aspd: 100, bat: 1, blockCnt: 0, respawnTime: 70, moveSpeed: 0, cost: 0};
  const b = new Battle({content: 'none', autoFinish: false, rect: {r0: 0, r1: 18, c0: 0, c1: 20}, stage: {rows: Array(19).fill('r'.repeat(21))},
    data: {chess: {op: {chessId: 'op', stats}}}, players: [{playerId: 'p', coords: 'field', units: [{uid: 1, chessId: 'op', row: 10, col: 5, items: equipped}]}],
    setup(b) {
      const u = b.allyUnits[0]; u.profile.noAttack = true;
      u.skill = new SkillRuntime(b, u, {skillType: 'MANUAL'}, {kind: 'duration', spCost: 100, initSp: 3, duration: 1, trigger: 'NEVER'});
      b.addBuff(u, {key: 'fixture', mods: {atkPct: .25, hpPct: .15}, persist: true, allowDead: true}); install(b);
    }});
  const u = b.allyUnits[0], snapshot = () => [u.s.maxHp, u.s.atk, u.s.def, u.s.res, u.s.aspd, u.s.spRecovery, u.s.redeployMul, u.s.resIgnorePct, u.s.taunt, u.hp, u.skill.spTotal, +u.alive];
  const expected = [snapshot()]; b.start(); expected.push(snapshot()); for (let tick = 0; tick < 30; ++tick) b.step(); expected.push(snapshot());
  b.loseHp(u, 100000); expected.push(snapshot()); b.redeploy(u, {free: true}); expected.push(snapshot()); for (let tick = 0; tick < 30; ++tick) b.step(); expected.push(snapshot());
  equal(actual[index], expected, `${index}/${equipped.join('+')}`); assert.equal(b.errorCount, 0, JSON.stringify(b.errors));
}
console.log(`equipment stat templates: 46 items, ${cases.length} loadouts, ${cases.length * 6} state snapshots`);
