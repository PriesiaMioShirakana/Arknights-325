// Exercise the original PlayerState inventory methods. Content hooks and summons are separate migration slices.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { spawnSync } from 'node:child_process';

const args = process.argv.slice(2);
const root = resolve(args[args.indexOf('--reference') + 1]);
const native = resolve(args[args.indexOf('--native') + 1]);
const load = path => import(pathToFileURL(resolve(root, path)).href);
const { GameData } = await load('server/match/gamedata.js');
const { SharedPool } = await load('server/match/pool.js');
const { makeCtx, MetaRegistry } = await load('server/match/effectsMeta.js');
const { registerBuiltins } = await load('server/match/builtinMeta.js');
const { PlayerState } = await load('server/match/PlayerState.js');
const { createRng, deriveSeed } = await load('server/sim/rng.js');
const { PHASE } = await load('shared/constants.js');
const { pieceDir, buildDeployMap } = await load('server/match/board.js');
const data = Object.fromEntries(['config', 'chess', 'items', 'stages'].map(key => [key, JSON.parse(readFileSync(resolve(root, 'data', key + '.json'), 'utf8'))]));
const directions = ['UP', 'RIGHT', 'DOWN', 'LEFT'];
const chess = Object.keys(data.chess).sort().filter(id => data.chess[id].visible && !data.chess[id].isGolden && !data.chess[id].isDiy && data.chess[id].position === 'MELEE');
const items = Object.keys(data.items).sort().filter(id => data.items[id].itemType === 'EQUIP' && !data.items[id].isGolden && data.items[id].mergeable && !String(data.items[id].kind).startsWith('consume_on_equip'));
const gold = id => data.items[id].upgradeChessId || data.items[id].goldenId;
let snapshots = 0;
const coverage = new Set();

for (const seed of [0, 1, 2, 42, 177, 65535, 2147483648, 4294967295]) {
  const gd = new GameData(data, 'mode_multi_normal'), pool = new SharedPool(gd);
  let uid = 0;
  const noop = () => {};
  const match = { gd, poolFor: () => pool, nextUid: () => ++uid, rngShop: createRng(deriveSeed(seed, 'shop')),
    phase: PHASE.PREP, round: 0, dispatch: noop, dispatchItem: noop, markPrivate: noop, tickerFor: noop,
    onReadyChanged: noop, log: {}, toast: noop };
  const player = new PlayerState(match, { seat: 0, playerId: 'p', name: 'p' });
  player.recompute = () => player._fillHandFromTemp();
  player.grantTokensFor = noop;
  let layout = buildDeployMap(null);
  player.deployMap = () => layout;
  const registry = new MetaRegistry(); registerBuiltins(registry, data);
  match.dispatch = (ps, hook, ev) => {
    if (hook !== 'onBuy') return;
    for (const ref of ps.effects.slice()) {
      const handler = registry.get(ref.key);
      if (handler?.onBuy) handler.onBuy(makeCtx(match, ps, { kind: 'effect', ref, key: ref.key }, hook, ev), ev);
    }
  };
  const ctx = makeCtx(match, player, {}, "test");
  const commands = [], expected = [];
  const piece = p => p ? [p.uid, p.id, p.poolCopies || 0, directions.indexOf(pieceDir(p)), (p.items || []).map(piece),
    player.temp.includes(p) ? player.tempDue(p) : null, +!!p.deferMerge] : null;
  const snapshot = (ok, granted) => [ok, granted, player.funds, +player.ready, player.prepsEnded,
    player.hand.map(piece), player.temp.map(piece), Array.from({ length: 36 }, (_, i) => piece(player.board.get(`${12 - Math.floor(i / 9)},${2 + i % 9}`))),
    player.offers.map(o => o.slots.map(s => s.id)), match.phase === PHASE.PREP ? 1 : match.phase === PHASE.COMBAT ? 2 : 3,
    Array.from({ length: 36 }, (_, i) => ({ melee: 1, ranged: 2 }[layout.get(`${12 - Math.floor(i / 9)},${2 + i % 9}`)] || 0)),
    player.shop.freeRefreshes,
    ['gold', 'fundsGained', 'refreshes', 'buys', 'sells', 'merges', 'itemMerges', 'itemsEquipped'].map(k => player.stats[k]),
    ['refreshes', 'buys', 'sells', 'spent', 'gainedChess', 'arts'].map(k => player.round[k]),
    player.effects.map(e => [+e.key.endsWith('golden_item'), e.counter, e.id]), player.pendingFunds, player.deployCap];
  const act = (op, ...a) => {
    commands.push([op, ...a].join(' '));
    let result = { ok: true }, granted = 0;
    if (op === 'begin') { match.phase = PHASE.PREP; player.startRound(++match.round); player.checkItemMerges(); player.recompute(); }
    else if (op === 'round_start') { match.phase = PHASE.ROUND_START; player.startRound(++match.round); }
    else if (op === 'prep') { match.phase = PHASE.PREP; player.ready = false; player.checkItemMerges(); player.recompute(); }
    else if (op === 'deadline') { if (!player.ready) { player.resolveTemp(); player.ready = true; } }
    else if (op === 'layout') {
      layout = a[0] === 0 ? buildDeployMap(null) : a[0] === 2 ? new Map() : new Map(Array.from({ length: 36 }, (_, i) => [`${12 - Math.floor(i / 9)},${2 + i % 9}`, a[0] === 1 ? 'ranged' : a[0] === 2 ? 'none' : 'melee']));
      player._evictIllegal(); player.recompute();
    }
    else if (op === 'end') { player.endPrep(); match.phase = PHASE.COMBAT; }
    else if (op === 'funds') player.addFunds(a[0]);
    else if (op === 'pending') ctx.addPendingFunds(a[0]);
    else if (op === 'free') ctx.grantFreeRefresh(a[0]);
    else if (op === 'cap') ctx.setDeployCapAtLeast(a[0]);
    else if (op === 'keep') player.bandId = a[0] ? gd.leftoverKeptBands[0] : null;
    else if (op === 'bonus') {
      if (a[1]) player.effects.push({ id: a[2], key: a[0] ? 'effect:builtin_next_buy_golden_item' : 'effect:builtin_next_buy_elite', counter: a[1], battle: false });
    }
    else if (op === 'offer') { if (a[0]) player.pushItemOffer(a.slice(1)); else player.pushRewardOffer('effect', { ids: a.slice(1) }); }
    else if (op === 'grant') {
      const p = data.chess[a[0]] ? player.acquireChess(a[0], { fromPool: !!a[1], toTemp: !!a[2] }) :
        player.acquireItem(a[0], { toTemp: !!a[2], deferMerge: !!a[3] });
      granted = p?.uid || 0;
      if (!p) coverage.add('overflow');
    }
    else if (op === 'promote' || op === 'upgrade') {
      result = { ok: op === 'promote' ? ctx.promote(a[0]) : ctx.upgradeItem(a[0]) };
      granted = result.ok ? a[0] : 0;
    }
    else if (op === 'remove') result = { ok: ctx.destroyPiece(a[0]) };
    else if (op === 'attach') { result = { ok: ctx.equipDirect(a[0], a[1]) }; granted = result.ok && player.find(a[0]) ? a[0] : 0; }
    else if (op === 'transform') {
      const old = player.find(a[0]);
      if (!old || old.piece.kind !== 'chess' || !gd.chess(a[1])) result = { ok: false };
      else granted = player.transformChess(old.piece, a[1])?.uid || 0;
    }
    else if (op === 'equip') result = player.equip(a[0], a[1], a[2] || null);
    else if (op === 'board') result = player.move(a[0], { area: 'board', row: a[1], col: a[2] }, directions[a[3]]);
    else if (op === 'hand') result = player.move(a[0], { area: 'hand', idx: a[1] });
    else if (op === 'ready') result = player.setReady(!!a[0]);
    else if (op === 'reward') result = player.pickReward(a[0]);
    else result = player[op](a[0]);
    coverage.add(op + ':' + !!result.ok);
    if (result.error) coverage.add(result.error);
    expected.push(snapshot(+!!result.ok, granted));
    return granted;
  };
  act('begin');
  // Rewards stack across rounds; only actual non-golden shop purchases consume an upgrade.
  act('funds', 80); act('free', 3); act('free', 2); act('bonus', 0, 2, 'elite-one'); act('bonus', 0, 1, 'elite-two');
  act('bonus', 1, 2, 'item-one'); act('cap', 11); act('cap', 9);
  act('offer', 0, 'missing', chess[0], chess[0], ...chess.slice(1, 9)); act('reward', 0);
  act('offer', 1, chess[0], 'missing', ...items.slice(0, 8)); act('reward', 0);
  act('offer', 1, 'missing', chess[0]);
  // Valid off-pool operators can be explicitly offered; unconfigured DIY templates remain invalid.
  for (const id of Object.keys(data.chess).sort().filter(id => !data.chess[id].isDiy && !data.chess[id].isGolden && !pool.has(id)).slice(0, 3)) { act('offer', 0, id); act('reward', 0); }
  for (let k = 0; k < 5; ++k) {
    act('refresh');
    for (let slot = 0; slot < player.shop.slots.length; ++slot) act('buy', slot);
    for (const p of [...player.hand].filter(Boolean)) act(p.kind === 'chess' ? 'sell' : 'destroy', p.uid);
  }
  act('keep', 1); act('pending', 7); act('end'); act('begin'); act('keep', 0);
  act('funds', -9999); act('refresh'); act('free', 1); act('refresh'); act('funds', 100);
  // Explicit replacement, equipped-lock and returned-equipment merge cases.
  const owner = act('grant', chess[0], 1, 0, 0);
  act('board', owner, 12, 2, 3);
  const eq1 = act('grant', gold(items[0]), 0, 0, 0); act('equip', eq1, owner, 0);
  const eq2 = act('grant', gold(items[1]), 0, 0, 0); act('equip', eq2, owner, 0);
  const eq3 = act('grant', gold(items[2]), 0, 0, 0);
  act('equip', eq3, owner, 987654); act('equip', eq3, owner, eq2);
  act('destroy', eq1); act('hand', eq1, 0); act('sell', eq1);
  for (let i = 0; i < 15; ++i) act('grant', gold(items[3]), 0, 0, 0);
  act('sell', owner); // No free room for its two equipped items: must refuse without mutation.
  act('ready', 1); act('grant', chess[1], 1, 0, 0);
  act('destroy', player.temp.find(Boolean).uid); act('destroy', player.temp.find(Boolean).uid);
  act('sell', owner); // Exactly two free temporary slots allow the sale.
  act('end'); act('begin');
  while (player.hand.some(Boolean)) act('destroy', player.hand.find(Boolean).uid);
  const lateOwner = act('grant', chess[0], 1, 0, 0);
  const normal = act('grant', items[0], 0, 0, 0); act('equip', normal, lateOwner, 0);
  for (let i = 0; i < 9; ++i) act('grant', gold(items[3]), 0, 0, 0);
  act('ready', 1);
  act('grant', items[0], 0, 0, 1); // Deferred twin cannot strip this round's equipped item.
  act('end'); act('begin'); // Next preparation merges twins, then pulls temp into the free hand slots.
  act('promote', lateOwner); act('promote', lateOwner);
  act('transform', lateOwner, chess[1]);
  act('ready', 1); act('grant', chess[2], 1, 0, 0); act('ready', 0); act('end');
  act('grant', chess[3], 1, 0, 0); // Combat grant survives through the following preparation.
  act('begin');

  act('end'); act('round_start'); act('buy', 0); act('grant', items[0], 0, 0, 1);
  act('layout', seed % 4); act('prep'); act('deadline'); act('grant', items[0], 0, 0, 1);
  act('end'); act('round_start'); act('layout', 0); act('prep');
  const rng = createRng(seed);
  const choose = a => a.length ? a[Math.floor(rng() * a.length)] : null;
  for (let round = 0; round < 8; ++round) {
    for (let step = 0; step < 120; ++step) {
      const held = [...player.hand, ...player.temp].filter(Boolean), owned = [...held, ...player.board.values()];
      const operator = choose(owned.filter(p => p.kind === 'chess'));
      const item = choose(held.filter(p => p.kind === 'item'));
      const target = choose(owned);
      switch (step % 23) {
        case 0: case 1: case 2: case 3: act('grant', choose(chess.slice(0, 5)), step % 2, step % 3 === 0 ? 1 : 0, 0); break;
        case 4: case 5: act('grant', choose(items.slice(0, 5)), 0, 0, +(step % 5 === 0)); break;
        case 6: act('equip', item?.uid || 0, operator?.uid || 0, choose(operator?.items || [])?.uid || 0); break;
        case 7: act('board', operator?.uid || 0, 12 - step % 4, 2 + step % 9, step % 4); break;
        case 8: act('hand', target?.uid || 0, step % 10); break;
        case 9: act('sell', operator?.uid || 0); break;
        case 10: act('destroy', item?.uid || 0); break;
        case 11: act('ready', 1); break;
        case 12: act('buy', step % 4); break;
        case 13: act('ready', 0); break;
        case 14: act('reward', step % 3); break;
        case 15: act('grant', gold(choose(items.slice(0, 5))), 0, 0, 0); break;
        case 16: act('promote', operator?.uid || 0); break;
        case 17: act('upgrade', choose([item, ...(operator?.items || [])].filter(Boolean))?.uid || 0); break;
        case 18: act('transform', operator?.uid || 0, choose(chess.slice(0, 5))); break;
        case 19: act('remove', choose([target, ...(operator?.items || [])].filter(Boolean))?.uid || 0); break;
        case 20: act('attach', item?.uid || 0, operator?.uid || 0); break;
        case 21: act('layout', Math.floor(step / 23) % 4); break;
        case 22: act('layout', 0); break;
      }
    }
    act('end'); act('grant', choose(chess.slice(0, 5)), 1, 0, 0); act('begin');
  }
  const run = spawnSync(native, [String(seed)], { input: commands.join('\n') + '\n', encoding: 'utf8', timeout: 30000, maxBuffer: 32 * 1024 * 1024 });
  assert.equal(run.error, undefined); assert.equal(run.status, 0, run.stderr);
  const actual = run.stdout.trim().split('\n').map(JSON.parse);
  assert.equal(actual.length, expected.length);
  for (let i = 0; i < actual.length; ++i) assert.deepEqual(actual[i], expected[i], `inventory seed=${seed} step=${i}: ${commands[i]}`);
  snapshots += actual.length;
}
for (const key of ['overflow', 'HAND_FULL', 'TEMP_NOT_EMPTY', 'equip:true', 'equip:false', 'sell:true', 'sell:false', 'board:true', 'destroy:false', 'promote:true', 'upgrade:true', 'transform:true', 'remove:true', 'attach:true'])
  assert.ok(coverage.has(key), `missing inventory coverage: ${key}`);
console.log(`Inventory JS parity passed: ${snapshots} snapshots, equipment replacement/locks, merges, full-storage rejection, ready deadlines and combat grants.`);
