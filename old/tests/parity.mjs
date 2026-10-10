// Differential tests invoke the unchanged JS reference; content effects and summons remain outside this slice.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { spawnSync } from 'node:child_process';

const args = process.argv.slice(2);
const reference = resolve(args[args.indexOf('--reference') + 1]);
const native = resolve(args[args.indexOf('--native') + 1]);
const module = path => import(pathToFileURL(resolve(reference, path)).href);
const { createRng, deriveSeed } = await module('server/sim/rng.js');
const { SharedPool } = await module('server/match/pool.js');
const { GameData } = await module('server/match/gamedata.js');
const { PlayerState } = await module('server/match/PlayerState.js');
const { mitigate } = await module('server/sim/damage.js');
const { PHASE } = await module('shared/constants.js');
const { buildDeployMap, boardOrder, pieceDir, positionClass } = await module('server/match/board.js');
const directions = ['UP', 'RIGHT', 'DOWN', 'LEFT'];
const data = Object.fromEntries(['config', 'chess', 'items', 'stages'].map(name =>
  [name, JSON.parse(readFileSync(resolve(reference, 'data', name + '.json'), 'utf8'))]));
const run = (...command) => {
  const result = spawnSync(native, command.map(String), { encoding: 'utf8', maxBuffer: 32 * 1024 * 1024, timeout: 15000 });
  assert.equal(result.error, undefined, String(result.error));
  assert.equal(result.status, 0, result.stderr);
  return JSON.parse(result.stdout);
};
const gd = new GameData(data, 'mode_multi_normal');
const catalog = run('catalog');
assert.equal(catalog.definitions, Object.keys(data.chess).length + Object.keys(data.items).length);
assert.equal(catalog.visibleChess, gd.visibleChess.length);
assert.equal(catalog.modes, Object.keys(data.config.modes).length);
for (const seed of [0, 1, 2, 42, 0x80000000, 0xffffffff]) {
  const got = run('probe', seed);
  const rng = createRng(seed);
  assert.deepEqual(got.rng, Array.from({ length: 256 }, () => rng() * 4294967296), `rng ${seed}`);
  assert.deepEqual(got.derived, ['shop', 'waves', '\u536b\u{1f600}'].map(salt => deriveSeed(seed, salt)));
  assert.deepEqual(got.shuffle, rng.shuffle(Array.from({ length: 16 }, (_, i) => i)));
  const pool = new SharedPool(gd);
  const rolls = Array.from({ length: 120 }, (_, i) => {
    const level = 1 + i % 6;
    const chess = pool.roll(rng, { maxTier: level });
    const item = pool.rollItem(rng, level);
    if (chess) pool.take(chess);
    return [chess, item];
  });
  assert.deepEqual(got.rolls, rolls, `rolls ${seed}`);
  for (let i = 0; i < 160; ++i) {
    const expected = mitigate(i * 19.25, ['phys', 'arts', 'true', 'elemental'][i % 4],
      { def: i * 31 - 100, res: i * 3 - 20 },
      { defIgnorePct: (i % 14 - 2) / 10, defIgnoreFlat: i % 9 * 10,
        resIgnorePct: (i % 14 - 2) / 10, resIgnoreFlat: i % 5 * 7, elementalRes: i * 2 - 10 });
    assert.ok(Math.abs(got.damage[i] - expected) <= Math.max(1, expected) * 1e-14, `damage ${seed}/${i}`);
  }
}

function trace(seed, mode, stage = null, field = 'normal') {
  const gd = new GameData(data, mode);
  const pool = new SharedPool(gd);
  let uid = 0;
  const noop = () => {};
  const match = {
    gd, poolFor: () => pool, nextUid: () => ++uid, rngShop: createRng(deriveSeed(seed, 'shop')),
    phase: PHASE.PREP, round: 0, dispatch: noop, dispatchItem: noop, markPrivate: noop, tickerFor: noop,
    onReadyChanged: noop, log: {}, toast: noop,
    stage: stage ? data.stages[stage] : null,
  };
  const players = [0, 1].map(seat => {
    const player = new PlayerState(match, { seat, playerId: 'p' + seat, name: 'p' + seat });
    // Base rules only: no content modifiers, temporary grants, summons or dynamic terrain.
    // The original _legal/move/merge methods and deployCount/deployCap getters stay active.
    player.recompute = noop;
    if (stage) {
      player.deployMap = () => buildDeployMap(data.stages[stage], { field });
      player.grantTokensFor = noop;
    }
    return player;
  });
  const slot = s => s ? [s.id, s.basePrice ?? s.price, !!s.frozen, !!s.sold] : null;
  const snapshot = ok => ({ ok, players: players.map(p => ({
    id: p.playerId, funds: p.funds, level: p.shop.level, upgrade: p.shop.upgradePrice,
    frozen: p.shop.frozen, ready: p.ready,
    hand: p.hand.map(x => x ? [x.uid, x.id, x.poolCopies] : null),
    ...(stage ? {
      handFacing: p.hand.map(x => x ? directions.indexOf(pieceDir(x)) : null),
      board: boardOrder(p.board).map(({ r, c, piece: x }) => [r, c, x.uid, x.id, x.poolCopies, directions.indexOf(pieceDir(x))]),
    } : {}),
    shop: p.shop.slots.map(slot), offers: p.offers.map(o => o.slots.map(slot)),
  })) });
  const snapshots = [];
  for (let round = 1; round <= 12; ++round) {
    match.round = round;
    match.phase = PHASE.PREP;
    for (const player of players) player.startRound(round);
    snapshots.push(snapshot(true));
    for (let step = 0; step < 24; ++step) {
      const player = players[step % 2];
      let result;
      switch (step % 12) {
      case 4: result = player.levelUp(); break;
      case 5: result = player.refresh(); break;
      case 6: result = player.freeze(); break;
      case 7: result = player.pickReward(0); break;
      case 8: result = player.sell(player.hand.find(Boolean)?.uid ?? 0); break;
      case 9: result = player.setReady(true); break;
      case 11: result = player.setReady(false); break;
      default: result = player.buy(Math.floor(step / 2) % player.shop.slots.length); break;
      }
      snapshots.push(snapshot(!!result.ok));
      if (stage) {
        for (let move = 0; move < 3; ++move) {
          const owned = [...player.hand.filter(Boolean), ...boardOrder(player.board).map(x => x.piece)];
          const selector = round * 7 + step * 3 + move;
          const uid = owned.length ? owned[selector % owned.length].uid : 0;
          const index = selector * 7 % 36;
          const result = move === 2 ? player.move(uid, { area: 'hand', idx: selector % player.hand.length }) :
            player.move(uid, { area: 'board', row: 12 - Math.floor(index / 9), col: 2 + index % 9 }, directions[selector % 4]);
          snapshots.push(snapshot(!!result.ok));
        }
      }
    }
    for (const player of players) player.endPrep();
    match.phase = PHASE.COMBAT;
    snapshots.push(snapshot(true));
  }
  return snapshots;
}
let states = 0;
for (const mode of ['mode_training_1', 'mode_single_funny', 'mode_multi_normal']) {
  for (const seed of [0, 1, 42, 0xffffffff]) {
    const expected = trace(seed, mode);
    const got = run('trace', seed, mode);
    assert.equal(got.length, expected.length);
    for (let i = 0; i < expected.length; ++i)
      assert.deepEqual(got[i], expected[i], `${mode} seed=${seed} snapshot=${i}`);
    states += expected.length;
  }
}
const boards = run('boards');
assert.deepEqual(Object.keys(boards.stages).sort(), Object.keys(data.stages).sort());
for (const [id, stage] of Object.entries(data.stages)) {
  for (const [i, field] of ['normal', 'bossL', 'bossR'].entries()) {
    const map = buildDeployMap(stage, { field });
    const expected = Array.from({ length: 36 }, (_, j) =>
      ({ melee: 1, ranged: 2 }[map.get(`${12 - Math.floor(j / 9)},${2 + j % 9}`)] ?? 0));
    assert.deepEqual(boards.stages[id][i], expected, `board ${id}/${field}`);
  }
}
for (const [id, placement] of Object.entries(boards.placement))
  assert.equal(placement, { all: 0, ranged: 0, melee: 1, high: 2 }[positionClass(data.chess[id])], `placement ${id}`);
let placementStates = 0;
let deployedStates = 0;
for (const [stage, seed] of [['act1autochess_m02', 0], ['act2autochess_m01', 42], ['act2autochess_m04', 0xffffffff]]) {
  for (const [i, field] of ['normal', 'bossL', 'bossR'].entries()) {
    const expected = trace(seed, 'mode_multi_normal', stage, field);
    const got = run('placement', seed, 'mode_multi_normal', stage, i);
    assert.equal(got.length, expected.length);
    for (let j = 0; j < expected.length; ++j) {
      assert.deepEqual(got[j], expected[j], `placement ${stage}/${field} seed=${seed} snapshot=${j}`);
      if (expected[j].players.some(p => p.board.length)) deployedStates++;
    }
    placementStates += expected.length;
  }
}
assert.ok(deployedStates > 1000, 'placement traces must actually deploy pieces');
console.log(`JS parity passed: 6 RNG/pool/damage probes, ${states} economy and ${placementStates} placement snapshots, 39 static field maps.`);
