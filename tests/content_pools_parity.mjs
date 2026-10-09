// MatchSpDraft is the oracle; this fixture does not reimplement either weighted selection or stock filtering.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { spawnSync } from 'node:child_process';
const args = process.argv.slice(2), opt = key => args[args.indexOf(key) + 1], root = resolve(opt('--reference'));
const load = path => import(pathToFileURL(resolve(root, path)));
const { GameData } = await load('server/match/gamedata.js');
const { PlayerState } = await load('server/match/PlayerState.js');
const { SharedPool } = await load('server/match/pool.js');
const { MatchSpDraft } = await load('server/match/match/spDraft.js');
const { makeCtx } = await load('server/match/effectsMeta.js');
const { createRng } = await load('server/sim/rng.js');
const { PHASE } = await load('shared/constants.js');
const data = Object.fromEntries(['config', 'chess', 'items', 'choices', 'stages', 'tokens', 'backups'].map(k => [k, JSON.parse(readFileSync(resolve(root, `data/${k}.json`), 'utf8'))]));
Object.assign(data.choices.pools, {
  zero_item: { kind: 'equip', weighted: [['chess_item_1_01_e_a', 0], ['chess_item_1_02_e_a', -2]] },
  zero_chess: { kind: 'chess', weighted: [['chess_char_1_01_a', 0], ['chess_char_1_02_a', -2]] },
  invalid_item: { kind: 'equip', weighted: [['missing', 1]] },
  invalid_chess: { kind: 'chess', weighted: [['missing', 1]] },
  diy_list: { kind: 'chess', items: ['chess_char_5_diy1_a'] },
  diy_bond: { kind: 'chess', bond: 'emptyShip' },
  duplicate_tiers: { kind: 'equip', tiers: [2, 1, 2, 9] },
});
let snapshots = 0, diyDraws = 0;
for (const seed of [0, 1, 2, 42, 65535, 2147483648, 4294967295]) {
  const gd = new GameData(data, 'mode_multi_normal'), pool = new SharedPool(gd), noop = () => {};
  let uid = 0;
  const m = { gd, poolFor: () => pool, nextUid: () => ++uid, rngMeta: createRng(seed), rngShop: createRng(1),
    phase: PHASE.INFO_CHECK, round: 0, markPrivate: noop, dispatch: noop, dispatchItem: noop, tickerFor: noop, toast: noop, log: {},
    rollItemId: MatchSpDraft.prototype.rollItemId, rollPool: MatchSpDraft.prototype.rollPool };
  m.players = new Map(['p', 'q'].map((id, seat) => [id, new PlayerState(m, { playerId: id, seat, name: id })]));
  for (const p of m.players.values()) { p.recompute = () => p._fillHandFromTemp(); p.grantTokensFor = noop; }
  const p = m.players.get('p');
  assert(p.setDiy({ chess_char_5_diy1_a: { charId: 'char_2013_cerber', skillIndex: 0, uniEquipId: 'none' } }, { kitted: ['char_2013_cerber'] })); p.initDiyStock(new Set());
  assert(p.diy.chess_char_5_diy1_a, 'fixture must contain the intended DIY slot');
  const commands = [], expected = [];
  const act = (op, ...a) => {
    commands.push([op, ...a].join(' ')); let result = null;
    if (op === 'pool') {
      const p = m.players.get(a[0]);
      result = m.rollPool(a[1], { shopLevel: a[2], player: p, extra: p.diyStockEntries(), chessOf: id => p.gd.chess(id) });
    } else if (op === 'item') {
      const id = m.rollItemId({ pool: a[0], tier: a[1] === -1 ? null : a[1], maxTier: a[2], shopLevel: a[3] });
      if (id) result = { kind: 'item', id };
    } else if (op === 'grant') m.players.get(a[0]).acquireChess(a[1]);
    else if (op === 'clear') {
      const p = m.players.get(a[0]), ctx = makeCtx(m, p, {}, 'test');
      for (const piece of [...p.hand, ...p.temp].filter(Boolean)) ctx.destroyPiece(piece.uid);
    }
    if (result?.id === 'chess_char_5_diy1_a') diyDraws++;
    expected.push([m.rngMeta.state(), result ? [+ (result.kind === 'item'), result.id, +!!result.golden] : null]);
  };
  for (const level of [0, 1, 3, 5, 6, 8]) for (const id of [...Object.keys(data.choices.pools), 'missing']) {
    for (let i = 0; i < 5; ++i) act('pool', 'p', id, level);
    act('item', id, -1, 6, level);
  }
  for (const tier of [-1, 0, 1, 3, 6, 7]) for (const max of [0, 1, 4, 6, 9]) act('item', 'missing', tier, max, 6);
  // Exhaust explicit-list stock, including DIY below its shop unlock, then return it and draw again.
  for (const id of ['chess_char_5_diy1_a', 'chess_char_3_05_a', 'chess_char_2_07_a', 'chess_char_1_04_a']) {
    const copies = p.poolOf(gd.baseIdOf(id)).left(gd.baseIdOf(id));
    for (let i = 0; i < copies; i += 3) act('grant', 'p', gd.goldenIdOf(id));
    for (let i = 0; i < 20; ++i) { act('pool', 'p', 'diy_list', 1); act('pool', 'p', 'pool_chess_glady', 1); }
  }
  act('pool', 'q', 'diy_list', 1); act('clear', 'p');
  for (let i = 0; i < 120; ++i) act('pool', 'p', i % 2 ? 'diy_list' : 'pool_chess_shop_5_reward', 1);
  const run = spawnSync(resolve(opt('--native')), [String(seed)], { input: commands.join('\n') + '\n', encoding: 'utf8', maxBuffer: 16 * 1024 * 1024 });
  assert.equal(run.status, 0, run.stderr);
  const actual = run.stdout.trim().split('\n').map(JSON.parse); assert.equal(actual.length, expected.length);
  for (let i = 0; i < actual.length; ++i) assert.deepEqual(actual[i], expected[i], `seed ${seed}, step ${i}: ${commands[i]}`);
  snapshots += actual.length;
}
assert(diyDraws > 100);
console.log(`content pools: ${snapshots} snapshots, ${diyDraws} private-stock draws`);
