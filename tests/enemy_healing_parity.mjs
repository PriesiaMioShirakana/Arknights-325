import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const args = process.argv.slice(2), opt = k => args[args.indexOf(k) + 1], root = resolve(opt('--reference'));
const load = p => import(pathToFileURL(resolve(root, p)));
const { Battle } = await load('server/sim/Battle.js');
const { SharedBossPool } = await load('server/match/finalAssault.js');
const run = spawnSync(resolve(opt('--native')), [], { encoding: 'utf8', maxBuffer: 8 * 1024 * 1024 });
assert.equal(run.status, 0, run.stderr); const actual = JSON.parse(run.stdout);
for (let scene = 0; scene < 12; scene++) {
  const pool = new SharedBossPool(1000);
  const b = new Battle({ content: 'none', autoFinish: false, timeLimit: 60, kind: scene === 10 ? 'boss' : 'solo', sharedBoss: scene === 10 ? pool : null,
    rect: { r0: 0, r1: 18, c0: 0, c1: 20 }, stage: { id: 'heal', rows: Array(19).fill('r'.repeat(21)) },
    data: { chess: { ally: { chessId: 'ally', stats: { maxHp: 1000, blockCnt: 0 } } },
      enemies: Object.fromEntries(Array.from({ length: 4 }, (_, i) => [String(i), { rank: i === 3 ? 'BOSS' : 'NORMAL',
        stats: { maxHp: i === 1 ? 2000 : 1000, atk: i === 0 ? 100 : 0, bat: 0.5, moveSpeed: scene === 3 && i === 0 ? 2 : 0,
          dmgType: i === 0 ? 'heal' : 'none', rangeRadius: scene === 1 || scene === 2 ? 0 : 3 }, applyWay: 'RANGED',
        attackAnim: { dur: 1.2, hit: 0.9 }, hitArea: scene === 2 && i === 2 ? { w: 3, h: 1 } : null }])) },
    players: [{ playerId: 'one', coords: 'field', units: [{ uid: 1, chessId: 'ally', row: 1, col: 1 }] }],
    spawns: Array.from({ length: 4 }, (_, i) => ({ time: 0, enemyKey: String(i), ownerPlayerId: 'one', tag: scene === 10 && i === 3 ? 'boss' : undefined,
      route: { start: [i === 3 ? 11 : 10, 10 + (i === 1 ? 0.9 : i === 2 ? 2 : 0)], end: [i === 3 ? 11 : 10, 0] } })),
    setup(b) { b.allyUnits[0].profile = { noAttack: true }; } });
  b.step(); const [healer, first, second, boss] = b.enemies;
  b.loseHp(healer, 900);
  if (scene !== 11) { b.loseHp(first, 1200); b.loseHp(second, 600); b.loseHp(boss, 700, { source: b.allyUnits[0] }); }
  if (scene === 4) b.applyStatus(boss, 'noHeal', { duration: 3 });
  if (scene === 5) { b.applyStatus(boss, 'sleep', { duration: 3 }); b.applyStatus(boss, 'stealth', { duration: 3 }); }
  if (scene === 6) b.applyStatus(healer, 'disarm', { duration: 2 });
  if (scene === 7) b.applyStatus(healer, 'fear', { duration: 2 });
  if (scene === 8) b.applyStatus(healer, 'palsy', { duration: 3 });
  if (scene === 9) b.addBuff(healer, { key: 'healing', mods: { healingDealtMul: 1.5, aspd: 100 } });
  for (let tick = 0; tick < 180; tick++) {
    if (tick === 30) b.applyStatus(healer, 'stun', { duration: 0.3 });
    if (tick === 40) b.applyStatus(first, 'healFree', { duration: 1 });
    if (tick === 80 && scene !== 11) b.loseHp(second, 200);
    b.step();
    const expected = [healer.x, healer.atkCd, healer.stats.attacks, healer.stats.heal, ...b.enemies.map(e => e.hp)];
    expected.forEach((v, i) => assert(Number.isFinite(actual[scene][tick][i]) && Math.abs(actual[scene][tick][i] - v) <= 1e-9 * Math.max(1, Math.abs(v)),
      `scene ${scene} tick ${tick}.${i}: C++ ${actual[scene][tick][i]}, JS ${v}`));
  }
  assert.equal(b.errorCount, 0, JSON.stringify(b.errors));
}
console.log('12 enemy healer scenarios / 2160 frames matched JS');
