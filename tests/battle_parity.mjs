import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';

const args = process.argv.slice(2);
const option = name => args[args.indexOf(name) + 1];
assert(args.includes('--reference') && args.includes('--native'));
const { Battle } = await import(pathToFileURL(resolve(option('--reference'), 'server/sim/Battle.js')));
const damageTypes = ['phys', 'arts', 'true', 'elemental'];
const priorities = [null, 'fly', 'lowDef', 'lowestHp', 'highestHp', 'nearest', 'farthest'];
const directions = ['UP', 'RIGHT', 'DOWN', 'LEFT'];
const steps = ['MOVE', 'WAIT', 'DISAPPEAR', 'APPEAR'];
const statuses = ['stun', 'disarm', 'noMove', 'unblockable'];
const pair = p => [p[1], p[0]];

function compare(actual, expected, path) {
  if (typeof actual === 'number' && typeof expected === 'number') {
    assert(Number.isFinite(actual) && Number.isFinite(expected));
    assert(Math.abs(actual - expected) <= 1e-9 * Math.max(1, Math.abs(expected)),
      `${path}: native=${actual}, reference=${expected}`);
  } else if (Array.isArray(expected)) {
    assert.equal(actual.length, expected.length, `${path}.length`);
    expected.forEach((value, index) => compare(actual[index], value, `${path}[${index}]`));
  } else if (expected !== null && typeof expected === 'object') {
    assert.deepEqual(Object.keys(actual).sort(), Object.keys(expected).sort(), path);
    for (const key of Object.keys(expected)) compare(actual[key], expected[key], `${path}.${key}`);
  } else assert.equal(actual, expected, path);
}

function reference(input, attacks) {
  const chess = Object.fromEntries(input.units.map(({ definition: d }) => [d.id, {
    chessId: d.id, stats: d.stats, rangeGrid: d.range,
  }]));
  const enemies = Object.fromEntries(input.spawns.map(s => {
    const d = s.definition;
    return [d.id, { stats: { ...d.stats, blockCnt: d.weight, rangeRadius: d.attack[5] ? d.attack[10] : 0 },
      motion: d.flying ? 'FLY' : 'WALK', applyWay: d.attack[5] ? 'RANGED' : 'MELEE',
      dmgType: damageTypes[d.attack[0]], lifePointReduce: s.life,
      attackAnim: { dur: d.attack[11], hit: d.attack[12] }, attackMoves: d.attack[6] }];
  }));
  return new Battle({
    content: 'none', rect: { r0: 0, r1: 18, c0: 0, c1: 20 }, timeLimit: input.limit,
    stage: { id: 'parity', rows: Array(19).fill('r'.repeat(21)) },
    data: { chess, enemies }, flags: { dpInit: input.dp[0], dpPerSec: input.dp[1], dpMax: input.dp[2] },
    players: [{ playerId: 'one', coords: 'field', units: input.units.map(u => ({
      uid: u.uid, chessId: u.definition.id, row: u.position[1], col: u.position[0], dir: directions[u.dir],
    })) }],
    spawns: input.spawns.map(s => ({ time: s.time, enemyKey: s.definition.id, ownerPlayerId: 'one',
      route: { motion: s.definition.flying ? 'FLY' : 'WALK', start: pair(s.start), end: pair(s.end),
        checkpoints: s.steps.map(st => ({ type: steps[st[0]], pos: [st[2], st[1]], time: st[3] })) } })),
    setup(b) {
      for (const u of b.allyUnits) {
        const a = input.units.find(x => x.uid === u.uid).definition.attack;
        u.profile = { attack: a[5] ? 'ranged' : 'melee', dmgType: a[2] ? 'heal' : damageTypes[a[0]],
          projectile: a[9] === 14 ? 'arrow' : 'none', canHitFly: a[3], blockFly: a[4],
          maxTargets: a[7], priority: priorities[a[8]], heal: a[2] ? { mode: 'single' } : null,
          noAttack: a[1] };
      }
      b.on('attack', ({ attacker, targets }) => {
        for (const target of targets) attacks.push([b.tickCount, attacker.id, target.id]);
      });
    },
  });
}

function snapshot(b) {
  return { tick: b.tickCount, reason: [null, 'cleared', 'timeout', 'forced'].indexOf(b.reason),
    counts: [b.killed, b.leakedCount, b.total, b.unspawned?.length ?? 0],
    players: b.players.map(p => {
      const pp = b._perPlayer[p.playerId];
      return [p.dp, pp.total, pp.killed, pp.leaked.length, pp.deaths,
        pp.leaked.reduce((sum, e) => sum + e.lpr, 0), pp.damageDealt, pp.healingDone];
    }),
    units: b.units.map(u => [u.id, u.x, u.y, u.hp, u.alive, !!u.hidden, u.blockedBy?.id ?? 0,
      u.atkCd, u.deploySeq, u.stats.dmg, u.stats.heal, u.stats.taken, u.stats.attacks, u.stats.kills,
      ...statuses.map(key => u.findBuff(key)?.timeLeft ?? 0)]),
  };
}

let frames = 0;
const coverage = { healed: false, blocked: false, redeployed: false, hidden: false, killed: false };
const statusCoverage = new Set();
for (const mode of ['base', 'statuses']) for (const seed of [0, 1, 2, 3, 4, 5, 6, 42, 0xffffffff]) {
  const result = spawnSync(resolve(option('--native')), [String(seed), mode], {
    encoding: 'utf8', maxBuffer: 64 * 1024 * 1024, timeout: 15000,
  });
  assert.equal(result.error, undefined, String(result.error));
  assert.equal(result.status, 0, result.stderr);
  const native = JSON.parse(result.stdout);
  const attacks = [];
  const b = reference(native.input, attacks);
  for (const frame of native.frames) {
    for (const [tick, target, status, duration, remove] of native.input.effects) {
      if (tick !== b.tickCount) continue;
      const unit = b.units.find(u => u.id === target);
      assert(unit, `missing effect target ${target}`);
      if (remove) b.removeStatus(unit, statuses[status]);
      else b.applyStatus(unit, statuses[status], { duration });
    }
    b.step();
    assert.equal(b.errorCount, 0, JSON.stringify(b.errors));
    compare(frame, snapshot(b), `${mode}, seed ${seed}, tick ${b.tickCount}`);
    for (const status of statuses) if (b.units.some(u => u.alive && u.findBuff(status))) statusCoverage.add(status);
    coverage.healed ||= b._perPlayer.one.healingDone > 0;
    coverage.blocked ||= b.units.some(u => u.blockedBy);
    coverage.redeployed ||= b.allyUnits.some(u => u.deploySeq > native.input.units.length);
    coverage.hidden ||= b.units.some(u => u.hidden);
    coverage.killed ||= b.killed > 0;
    ++frames;
  }
  assert(b.finished);
  assert.deepEqual(native.attacks, attacks, `seed ${seed}: attack timing and targets`);
}
for (const [name, covered] of Object.entries(coverage)) assert(covered, `missing coverage: ${name}`);
assert.equal(statusCoverage.size, statuses.length, 'every status must actually be active');
console.log(`Battle parity: ${frames} frames; base combat, status timers, movement, targeting and attack events matched.`);
