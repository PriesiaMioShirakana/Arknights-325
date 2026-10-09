// Real preparation traits plus item hooks; bond milestone handlers are a separate integration suite.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { spawnSync } from 'node:child_process';

const args = process.argv.slice(2), opt = key => args[args.indexOf(key) + 1], root = resolve(opt('--reference'));
const load = path => import(pathToFileURL(resolve(root, path)));
const { GameData } = await load('server/match/gamedata.js');
const { PlayerState } = await load('server/match/PlayerState.js');
const { MatchCombat } = await load('server/match/match/combat.js');
const { MatchSpDraft } = await load('server/match/match/spDraft.js');
const { SharedPool } = await load('server/match/pool.js');
const { makeCtx, MetaRegistry, EffectDispatcher } = await load('server/match/effectsMeta.js');
const { registerBuiltins } = await load('server/match/builtinMeta.js');
const { registerMeta } = await load('server/sim/content/items/meta.js');
const { registerMeta: registerGarrisons } = await load('server/sim/content/garrisons/meta.js');
const { registerMeta: registerBands } = await load('server/sim/content/bands/meta.js');
const { registerMeta: registerCoreBonds } = await load('server/sim/content/bonds/core.js');
const { registerMeta: registerAddonBonds } = await load('server/sim/content/bonds/addon/meta.js');
const { setGameData } = await load('server/sim/content/support/index.js');
const { computeBonds } = await load('server/match/bondsMeta.js');
const { buildDeployMap } = await load('server/match/board.js');
const { createRng, deriveSeed } = await load('server/sim/rng.js');
const { PHASE } = await load('shared/constants.js');
const data = Object.fromEntries(['config', 'chess', 'items', 'choices', 'stages', 'tokens', 'backups', 'effects', 'enemies', 'bonds', 'bands', 'garrisons']
  .map(k => [k, JSON.parse(readFileSync(resolve(root, `data/${k}.json`), 'utf8'))]));
setGameData(data);
const coverage = new Set();
let snapshots = 0;

const modes = ['mode_multi_normal', 'mode_single_normal'];
const cases = ['victoriaShip', 'visiShip', 'miraShip', 'deputShip'];
const runs = [1, 42, 4294967295].flatMap(seed => modes.flatMap(mode => cases.map(bond => ({ seed, mode, bond }))));
for (const {seed, mode, bond} of runs) {
  const bonds = true, lisa = false, variant = 'bonds';
  const gd = new GameData(data, mode), pool = new SharedPool(gd), noop = () => {};
  let uid = 0;
  const m = { gd, poolFor: () => pool, nextUid: () => ++uid, rngMeta: createRng(seed), rngShop: createRng(deriveSeed(seed, 'shop')),
    phase: PHASE.INFO_CHECK, round: 0, markPrivate: noop, tickerFor: noop, toast: noop, log: {}, onReadyChanged: noop,
    addBounty: MatchSpDraft.prototype.addBounty, rollPool: MatchSpDraft.prototype.rollPool, rollItemId: MatchSpDraft.prototype.rollItemId };
  m.players = new Map(['p', 'q', 'r'].map((id, seat) => [id, new PlayerState(m, { playerId: id, seat, name: id })]));
  m.alivePlayers = () => [...m.players.values()].filter(p => p.alive);
  const registry = new MetaRegistry(); registerBuiltins(registry, data); registerMeta(registry); registerBands(registry);
  if (bonds) { registerCoreBonds(registry); registerAddonBonds(registry); }
  m.registry = registry; m.dispatcher = new EffectDispatcher(m, registry);
  m.dispatch = (...a) => m.dispatcher.dispatch(...a); m.dispatchItem = (...a) => m.dispatcher.dispatchItem(...a);
  for (const p of m.players.values()) {
    p.recompute = () => { p._fillHandFromTemp(); p.bonds = computeBonds(p.gd, p); };
    p.grantTokensFor = noop; p.deployMap = () => buildDeployMap(null);
  }
  const p = m.players.get('p'); if (lisa) p.bandId = 'band_lisa';
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
    for (const p of m.alivePlayers()) { p.ready = false; p.checkItemMerges(); p.recompute(); if (bonds) { m.dispatch(p, 'onPrepStart', {}); p.recompute(); } }
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
    else if (op === 'cap') p.deployCapMin = a[0];
    else if (op === 'funds') p.addFunds(a[0]);
    else if (op === 'sync') { const gains = {}; for (let i = 0; i < a.length; i += 2) gains[a[i]] = a[i + 1]; MatchCombat.prototype._applyBattleLayerGains.call(m, p, gains); p.recompute(); }
    else if (op === 'hand') ok = !!p.move(a[0], {area: 'hand', idx: 0}, 'RIGHT').ok;
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
  act('begin'); act('cap', 'p', 12); act('funds', 'p', 1000);
  const onBoard = uid => {
    for (let row = 12; row >= 9; --row) for (let col = 2; col <= 10; ++col) {
      if (!p.find(uid) || p.find(uid).area === 'board') return;
      if (!p.board.has(`${row},${col}`)) act('board', 'p', uid, row, col);
    }
  };
  const activate = target => {
    const candidates = Object.entries(data.chess).filter(([id, c]) => c.visible && !c.isGolden && !c.isDiy && c.bonds.includes(target));
    for (const [id] of candidates) {
      if (p.bonds[target]?.active) break;
      if (p.allChess().some(c => gd.baseIdOf(c.id) === id)) continue;
      onBoard(grant(id));
    }
    assert(p.bonds[target]?.active, target);
  };
  // Earn while inactive, activate by a move (no catch-up), then trigger gain/merge/start hooks.
  act('layers', 'p', bond, bond === 'victoriaShip' ? 100 : 79);
  activate(bond);
  const gift = grant('chess_item_1_01_e_a');
  grant('chess_item_1_01_e_a');
  act('layers', 'p', bond, 1);
  act('layers', 'p', bond, 69);
  act('layers', 'p', bond, 1);
  // A refresh with a remaining free charge does not roll the next-free reward.
  act('free', 'p', 2); act('refresh'); act('refresh'); act('refresh');
  for (let i = 0; i < p.shop.slots.length; ++i) if (p.shop.slots[i]) act('buy', 'p', i);
  // Deactivate, gain layers, then sell the last member; the handler observes the source's cached activation.
  const members = [...p.board.values()].filter(c => gd.chess(c.id)?.bonds.includes(bond));
  if (members.length) act('hand', 'p', members[0].uid);
  act('layers', 'p', bond, 100);
  if (members.length && p.find(members[0].uid)) onBoard(members[0].uid);
  if (members.length && p.find(members[0].uid)) act('sell', 'p', members[0].uid);
  activate(bond);
  // Prep end adds layers and defers any newly due hammers; next prep merges them.
  activate('deputShip');
  for (const target of ['victoriaShip', 'visiShip', 'miraShip']) {
    const step = target === 'victoriaShip' ? 25 : target === 'visiShip' ? 10 : 100;
    const current = p.layers[target] || 0;
    act('layers', 'p', target, step - 1 - current % step);
  }
  act('end');
  act('sync', 'p', bond, 15); act('sync', 'p', bond, 15); act('sync', 'p', bond, 3);
  act('sync', 'p', bond, 10000); act('sync', 'p', bond, 10000);
  act('settle', 'p', 0); act('begin'); act('funds', 'p', 100); act('refresh');
  act('end'); act('settle', 'p', 0); act('round_start'); act('hooks'); act('prep');
  const run = spawnSync(resolve(opt('--native')), [String(seed), mode, variant], {
    input: commands.join('\n') + '\n', encoding: 'utf8', timeout: 60000, maxBuffer: 128 * 1024 * 1024,
  });
  assert.equal(run.error, undefined);
  assert.equal(run.status, 0, `${bond}/${mode}/${seed}: ${run.stderr}; ${run.stdout.slice(-500)}`);
  const actual = run.stdout.trim().split('\n').map(JSON.parse); assert.equal(actual.length, expected.length);
  for (let i = 0; i < actual.length; ++i) assert.deepEqual(actual[i], expected[i], `${bond}/${mode}, seed ${seed}, step ${i}: ${commands[i]}`);
  snapshots += actual.length; coverage.add(bond);
}
for (const bond of cases) assert(coverage.has(bond));
console.log(`preparation bonds: ${runs.length} scenarios, ${snapshots} snapshots`);
