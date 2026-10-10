// Real strategy/item handlers. Exclusive garrison and bond meta handlers are tested separately.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { spawnSync } from 'node:child_process';

const args = process.argv.slice(2), opt = key => args[args.indexOf(key) + 1], root = resolve(opt('--reference'));
const load = path => import(pathToFileURL(resolve(root, path)));
const { GameData } = await load('server/match/gamedata.js');
const { PlayerState } = await load('server/match/PlayerState.js');
const { MatchSpDraft } = await load('server/match/match/spDraft.js');
const { SharedPool } = await load('server/match/pool.js');
const { makeCtx, MetaRegistry, EffectDispatcher } = await load('server/match/effectsMeta.js');
const { registerBuiltins } = await load('server/match/builtinMeta.js');
const { registerMeta } = await load('server/sim/content/items/meta.js');
const { registerMeta: registerBands } = await load('server/sim/content/bands/meta.js');
const { setGameData } = await load('server/sim/content/support/index.js');
const { computeBonds } = await load('server/match/bondsMeta.js');
const { buildDeployMap } = await load('server/match/board.js');
const { createRng, deriveSeed } = await load('server/sim/rng.js');
const { PHASE } = await load('shared/constants.js');
const data = Object.fromEntries(['config', 'chess', 'items', 'choices', 'stages', 'tokens', 'backups', 'effects', 'enemies', 'bonds', 'bands']
  .map(k => [k, JSON.parse(readFileSync(resolve(root, `data/${k}.json`), 'utf8'))]));
setGameData(data);
const coverage = new Set();
let snapshots = 0;

const strategies = Object.keys(data.bands).filter(id => id !== 'band_lisa');
// Three independent streams; representative economy modes (including inactive bonds).
const runs = [1, 42, 4294967295].flatMap(seed => strategies.map(variant => ({ seed, mode: 'mode_multi_normal', variant })))
  .concat(['mode_single_normal', 'mode_multi_funny'].flatMap(mode => strategies.map(variant => ({ seed: 42, mode, variant }))));
const group = args.includes('--group') ? opt('--group') : null;
const selected = runs.filter(r => !group || (group.startsWith('mode_') ? r.mode === group : r.mode === 'mode_multi_normal' && String(r.seed) === group));
for (const { seed, mode, variant } of selected) {
  const gd = new GameData(data, mode), pool = new SharedPool(gd), noop = () => {};
  let uid = 0;
  const m = { gd, poolFor: () => pool, nextUid: () => ++uid, rngMeta: createRng(seed), rngShop: createRng(deriveSeed(seed, 'shop')),
    phase: PHASE.INFO_CHECK, round: 0, markPrivate: noop, tickerFor: noop, toast: noop, log: {}, onReadyChanged: noop,
    addBounty: MatchSpDraft.prototype.addBounty, rollPool: MatchSpDraft.prototype.rollPool, rollItemId: MatchSpDraft.prototype.rollItemId };
  m.players = new Map(['p', 'q', 'r'].map((id, seat) => [id, new PlayerState(m, { playerId: id, seat, name: id })]));
  m.alivePlayers = () => [...m.players.values()].filter(p => p.alive);
  const registry = new MetaRegistry(); registerBuiltins(registry, data); registerMeta(registry); registerBands(registry);
  m.registry = registry; m.dispatcher = new EffectDispatcher(m, registry);
  m.dispatch = (...a) => m.dispatcher.dispatch(...a); m.dispatchItem = (...a) => m.dispatcher.dispatchItem(...a);
  for (const p of m.players.values()) {
    p.recompute = () => { p._fillHandFromTemp(); p.bonds = computeBonds(p.gd, p); };
    p.grantTokensFor = noop; p.deployMap = () => buildDeployMap(null);
  }
  const p = m.players.get('p'); p.bandId = variant;
  assert(p.setDiy({ chess_char_5_diy1_a: { charId: 'char_2013_cerber', skillIndex: 0, uniEquipId: 'none' } }, { kitted: ['char_2013_cerber'] }));
  p.initDiyStock(new Set());
  const piece = (p, x) => x ? [x.uid, x.id, x.poolCopies || 0, (x.items || []).map(x => piece(p, x)),
    p.temp.includes(x) ? p.tempDue(x) : null] : null;
  const view = p => [+p.alive, +p.ready, p.funds, p.pendingFunds, p.deployCap, p.shop.level,
    p.hand.map(x => piece(p, x)), p.temp.map(x => piece(p, x)),
    Array.from({ length: 36 }, (_, i) => piece(p, p.board.get(`${12 - Math.floor(i / 9)},${2 + i % 9}`))),
    p.shop.slots.map(x => x ? [x.id, p.priceOf(x), +!!x.sold] : null), p.offers.map(x => x.slots.map(s => s.id)),
    Object.fromEntries(Object.entries(p.layers).filter(([, n]) => n > 0)),
    p.effects.filter(e => ['effect:builtin_round_coin', 'effect:builtin_gift'].includes(e.key))
      .map(e => [+e.key.endsWith('gift'), Number(e.id.split(':')[1]), e.params.count || 0, e.params.toPlayerId || '', e.params.chessId || '', e.params.bonds || []]),
    ['gold', 'fundsGained', 'refreshes', 'buys', 'sells', 'merges', 'itemMerges', 'itemsEquipped'].map(k => p.stats[k]),
    ['refreshes', 'buys', 'sells', 'spent', 'gainedChess', 'arts'].map(k => p.round[k]),
    p.bounties.map(b => [b.id, b.card.enemyKey, b.card.count, b.card.coin, +(b.card.payout === 'perfect'), b.roundsLeft]), p.shop.upgradePrice, p.shop.freeRefreshes, p.shop.slots.map(x => +!!x?.frozen)];
  const hooks = () => {
    for (const p of m.alivePlayers()) m.dispatch(p, 'onRoundStart', { round: m.round });
    for (const p of m.players.values()) if (!p.alive) m.dispatcher.dispatchEliminated(p, 'onRoundStart', { round: m.round });
  };
  const prep = () => {
    m.phase = PHASE.PREP;
    for (const p of m.alivePlayers()) { p.ready = false; p.checkItemMerges(); p.recompute(); }
  };
  const commands = [], expected = [], battleResults = new Map();
  const act = (op, player = 'p', ...a) => {
    commands.push([op, player, ...a].join(' '));
    const p = m.players.get(player); let ok = true, grant = 0;
    if (op === 'begin' || op === 'round_start') {
      m.phase = PHASE.ROUND_START; ++m.round;
      for (const p of m.alivePlayers()) p.startRound(m.round);
      if (op === 'begin') { hooks(); prep(); }
    } else if (op === 'hooks') hooks();
    else if (op === 'battle_result') {
      ok = !!p?.alive && m.phase === PHASE.COMBAT && battleResults.get(player) !== m.round;
      if (ok) { m.dispatch(p, 'onBattleResult', {}); battleResults.set(player, m.round); }
    }
    else if (op === 'prep') prep();
    else if (op === 'end') { for (const p of m.alivePlayers()) m.dispatch(p, 'onPrepEnd', {}); for (const p of m.alivePlayers()) p.endPrep(); m.phase = PHASE.COMBAT; }
    else if (op === 'settle') { for (const p of m.alivePlayers()) if (a[0] & (1 << p.seat)) p.eliminate(m.round); }
    else if (op === 'grant') {
      if (!p?.alive) ok = false;
      else grant = (p.gd.chess(a[0]) ? p.acquireChess(a[0], { fromPool: !!a[1], toTemp: !!a[2] }) : p.acquireItem(a[0], { toTemp: !!a[2] }))?.uid || 0;
    } else if (op === 'free') p.shop.freeRefreshes += a[0];
    else if (op === 'funds') p.addFunds(a[0]);
    else if (op === 'layers') p.addLayers(a[0], a[1]);
    else if (op === 'remove' || op === 'promote') {
      const ctx = makeCtx(m, p, {}, 'test'); ok = op === 'remove' ? ctx.destroyPiece(a[0]) : ctx.promote(a[0]);
    } else if (op === 'equip') ok = !!p?.equip(a[0], a[1], a[2] || null).ok;
    else if (op === 'board') ok = !!p.move(a[0], { area: 'board', row: a[1], col: a[2] }, 'RIGHT').ok;
    else if (op === 'ready') ok = !!p.setReady(!!a[0]).ok;
    else if (op === 'reward') ok = !!p.pickReward(a[0]).ok;
    else if (op === 'level') ok = !!p.levelUp().ok;
    else ok = !!p[op](...a).ok;
    assert.equal(m.dispatcher.errors, 0, JSON.stringify([...m.dispatcher.errorsByKey]));
    coverage.add(`${op}:${ok}`);
    expected.push([+ok, grant, m.rngMeta.state(), m.phase === PHASE.PREP ? 1 : m.phase === PHASE.COMBAT ? 2 : 3,
      ...[...m.players.values()].map(view)]);
    return grant;
  };
  const grant = (id, player = 'p', pool = 1, temp = 0) => act('grant', player, id, pool, temp);
  const advance = () => { act('end'); act('settle', 'p', 0); act('begin'); };
  const fill = (player = 'p') => {
    const p = m.players.get(player);
    while ([...p.hand, ...p.temp].some(x => !x)) assert(grant('chess_item_1_01_e_b', player));
  };
  act('begin');
  // Distinct tiers and duplicate tiers exercise random layer recipients; leave the board through R8.
  const boardIds = ['chess_char_1_01_a', 'chess_char_1_03_a', 'chess_char_2_01_a'];
  for (let i = 0; i < boardIds.length; ++i) {
    const uid = grant(boardIds[i]);
    const rec = gd.chess(boardIds[i]);
    act('board', 'p', uid, rec.position === 'RANGED' ? 11 : 12, 2 + i);
  }
  const clearBench = () => {
    for (;;) {
      const item = [...p.hand, ...p.temp].find(Boolean);
      if (!item) break;
      act('remove', 'p', item.uid);
    }
  };
  for (let round = 1; round <= 16; ++round) {
    assert.equal(m.round, round);
    act('funds', 'p', 150);
    if (round === 1) fill(); // Spending gifts may fail; once-only gifts must retry on the next payment.
    act('level');
    for (let i = 0; i < 4; ++i) act('refresh');
    clearBench();
    act('free', 'p', 3);
    for (let i = 0; i < 7; ++i) {
      act('refresh');
      for (let slot = 0; slot < p.shop.slots.length; ++slot) {
        if (p.shop.slots[slot] && !p.shop.slots[slot].sold) act('buy', 'p', slot);
      }
      // Sold normal chess may be exchanged; elites must still return their sale price.
      for (const piece of [...p.hand].filter(x => x?.kind === 'chess')) if (p.find(piece.uid)) act('sell', 'p', piece.uid);
      clearBench();
    }
    while (p.offers.length) act('reward', 'p', 0);
    clearBench();
    const elite = grant('chess_char_1_02_b'); act('sell', 'p', elite);
    act('buy', 'p', 999); act('sell', 'p', 999999);

    if (round < 16) advance();
  }
  act('end'); act('settle', 'p', 1); act('begin'); // Eliminated owners receive no strategy income/gifts.
  const run = spawnSync(resolve(opt('--native')), [String(seed), mode, variant], {
    input: commands.join('\n') + '\n', encoding: 'utf8', timeout: 60000, maxBuffer: 128 * 1024 * 1024,
  });
  assert.equal(run.error, undefined);
  assert.equal(run.status, 0, `signal ${run.signal}: ${run.stderr}; last output: ${run.stdout.slice(-1000)}`);
  const actual = run.stdout.trim().split('\n').map(JSON.parse); assert.equal(actual.length, expected.length);
  for (let i = 0; i < actual.length; ++i) assert.deepEqual(actual[i], expected[i], `${mode}/${variant}, seed ${seed}, step ${i}: ${commands[i]}`);
  snapshots += actual.length;
  coverage.add(variant);
}
for (const id of strategies) assert(coverage.has(id));
console.log(`preparation strategies: ${strategies.length} records, ${selected.length} scenarios, ${snapshots} snapshots; income, price, spend, refresh, sale and round hooks`);
