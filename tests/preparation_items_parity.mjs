// Real consume-on-equip handlers, including the content overrides and persistent EffectRefs.
// Operator/bond callbacks and summons belong to separate migration slices.
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
const { setGameData } = await load('server/sim/content/support/index.js');
const { computeBonds } = await load('server/match/bondsMeta.js');
const { buildDeployMap } = await load('server/match/board.js');
const { createRng, deriveSeed } = await load('server/sim/rng.js');
const { PHASE } = await load('shared/constants.js');
const data = Object.fromEntries(['config', 'chess', 'items', 'choices', 'stages', 'tokens', 'backups', 'effects', 'enemies', 'bonds']
  .map(k => [k, JSON.parse(readFileSync(resolve(root, `data/${k}.json`), 'utf8'))]));
setGameData(data);
const consumables = Object.keys(data.items).filter(id => String(data.items[id].kind).startsWith('consume_on_equip')).sort();
assert.equal(consumables.length, 24);
const coverage = new Set();
let snapshots = 0;

const artsOnly = args.includes('--arts-only');
const hooksOnly = args.includes('--hooks-only');
const runs = [1, 42, 4294967295].flatMap(seed => artsOnly
  ? [...Object.keys(data.config.modes).map(mode => ({ seed, mode, variant: 'normal' })),
    ...['fallback', 'empty'].map(variant => ({ seed, mode: 'mode_multi_normal', variant }))]
  : hooksOnly ? Object.keys(data.config.modes).map(mode => ({ seed, mode, variant: 'normal' }))
    : [{ seed, mode: 'mode_multi_normal', variant: 'normal' }]);
for (const { seed, mode, variant } of runs) {
  const bounties = variant === 'empty' ? [] : data.choices.cards.bounty.filter(c => variant !== 'fallback' ||
    (c.payout !== 'perfect' && !c.effectId.startsWith('enemyeffect_b_')));
  const fixture = { ...data, choices: { ...data.choices, cards: { ...data.choices.cards, bounty: bounties } } };
  const gd = new GameData(fixture, mode), pool = new SharedPool(gd), noop = () => {};
  let uid = 0;
  const m = { gd, poolFor: () => pool, nextUid: () => ++uid, rngMeta: createRng(seed), rngShop: createRng(deriveSeed(seed, 'shop')),
    phase: PHASE.INFO_CHECK, round: 0, markPrivate: noop, tickerFor: noop, toast: noop, log: {}, onReadyChanged: noop,
    addBounty: MatchSpDraft.prototype.addBounty };
  m.players = new Map(['p', 'q', 'r'].map((id, seat) => [id, new PlayerState(m, { playerId: id, seat, name: id })]));
  m.alivePlayers = () => [...m.players.values()].filter(p => p.alive);
  const registry = new MetaRegistry(); registerBuiltins(registry, data); registerMeta(registry);
  m.registry = registry; m.dispatcher = new EffectDispatcher(m, registry);
  m.dispatch = (...a) => m.dispatcher.dispatch(...a); m.dispatchItem = (...a) => m.dispatcher.dispatchItem(...a);
  for (const p of m.players.values()) {
    p.recompute = () => { p._fillHandFromTemp(); p.bonds = computeBonds(p.gd, p); };
    p.grantTokensFor = noop; p.deployMap = () => buildDeployMap(null);
  }
  const p = m.players.get('p'), q = m.players.get('q');
  assert(p.setDiy({ chess_char_5_diy1_a: { charId: 'char_2013_cerber', skillIndex: 0, uniEquipId: 'none' } }, { kitted: ['char_2013_cerber'] }));
  p.initDiyStock(new Set());
  const piece = (p, x) => x ? [x.uid, x.id, x.poolCopies || 0, (x.items || []).map(x => piece(p, x)),
    p.temp.includes(x) ? p.tempDue(x) : null] : null;
  const view = p => [+p.alive, +p.ready, p.funds, p.pendingFunds, p.deployCap, p.shop.level,
    p.hand.map(x => piece(p, x)), p.temp.map(x => piece(p, x)),
    Array.from({ length: 36 }, (_, i) => piece(p, p.board.get(`${12 - Math.floor(i / 9)},${2 + i % 9}`))),
    p.shop.slots.map(x => x ? [x.id, x.basePrice, +!!x.sold] : null), p.offers.map(x => x.slots.map(s => s.id)),
    Object.fromEntries(Object.entries(p.layers).filter(([, n]) => n > 0)),
    p.effects.filter(e => ['effect:builtin_round_coin', 'effect:builtin_gift'].includes(e.key))
      .map(e => [+e.key.endsWith('gift'), Number(e.id.split(':')[1]), e.params.count || 0, e.params.toPlayerId || '', e.params.chessId || '', e.params.bonds || []]),
    ['gold', 'fundsGained', 'refreshes', 'buys', 'sells', 'merges', 'itemMerges', 'itemsEquipped'].map(k => p.stats[k]),
    ['refreshes', 'buys', 'sells', 'spent', 'gainedChess', 'arts'].map(k => p.round[k]),
    p.bounties.map(b => [b.id, b.card.enemyKey, b.card.count, b.card.coin, +(b.card.payout === 'perfect'), b.roundsLeft])];
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
    else if (op === 'end') { for (const p of m.alivePlayers()) p.endPrep(); m.phase = PHASE.COMBAT; }
    else if (op === 'settle') { for (const p of m.alivePlayers()) if (a[0] & (1 << p.seat)) p.eliminate(m.round); }
    else if (op === 'grant') {
      if (!p?.alive) ok = false;
      else grant = (p.gd.chess(a[0]) ? p.acquireChess(a[0], { fromPool: !!a[1], toTemp: !!a[2] }) : p.acquireItem(a[0], { toTemp: !!a[2] }))?.uid || 0;
    } else if (op === 'funds') p.addFunds(a[0]);
    else if (op === 'layers') p.addLayers(a[0], a[1]);
    else if (op === 'remove' || op === 'promote') {
      const ctx = makeCtx(m, p, {}, 'test'); ok = op === 'remove' ? ctx.destroyPiece(a[0]) : ctx.promote(a[0]);
    } else if (op === 'equip') ok = !!p?.equip(a[0], a[1], a[2] || null).ok;
    else if (op === 'art') {
      // The C++ command boundary is atomic. JS increments gainedChess/UID on a failed full-inventory grant;
      // normalize only these rejected-command bookkeeping changes, then compare every state as usual.
      const oldUid = uid, oldRound = p && { ...p.round };
      ok = !!p?.useArt(a[0], a[1], a[2], ['UP', 'RIGHT', 'DOWN', 'LEFT'][a[3]] || 'invalid').ok;
      if (!ok && p) { uid = oldUid; p.round = oldRound; }
    }
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
  const clear = (player = 'p') => {
    const p = m.players.get(player);
    for (;;) {
      const piece = [...p.board.values(), ...p.hand, ...p.temp].find(Boolean);
      if (!piece) break;
      act('remove', player, piece.uid);
    }
  };
  const equip = (id, holder, replace = 0, player = 'p') => {
    const item = grant(id, player); act('equip', player, item, holder, replace); return item;
  };
  const advance = () => { act('end'); act('settle', 'p', 0); act('begin'); };
  const fill = (player = 'p') => {
    const p = m.players.get(player);
    while ([...p.hand, ...p.temp].some(x => !x)) assert(grant('chess_item_1_01_e_b', player));
  };
  act('begin');
  if (artsOnly) {
    // The fixed board fixture allows every direction; the spell itself need not land on a deployable tile.
    const ranged = Object.keys(data.chess).find(id => data.chess[id].visible && !data.chess[id].isGolden &&
      !data.chess[id].isDiy && data.chess[id].position === 'RANGED' && data.chess[id].tier === 1);
    assert(ranged);
    const cast = (id, row = 11, col = 4, facing = 1, player = 'p') => {
      const item = grant(id, player); act('art', player, item, row, col, facing); return item;
    };
    const canvas = 'chess_item_6_02_m', training = 'chess_item_6_03_m', pass = 'chess_item_6_01_m';
    for (const [direction, row, col] of [[0, 10, 4], [1, 11, 3], [2, 12, 4], [3, 11, 5]]) {
      clear(); const holder = grant(ranged); act('board', 'p', holder, 11, 4);
      assert.equal(p.board.size, 1);
      equip('chess_item_1_01_e_a', holder); equip('chess_item_1_02_e_a', holder);
      cast(canvas, row, col, direction); coverage.add(`art-direction:${direction}`); advance();
    }
    clear(); let holder = grant(gd.goldenIdOf(ranged)); act('board', 'p', holder, 11, 4);
    equip('chess_item_1_01_e_b', holder); equip('chess_item_1_02_e_b', holder);
    cast(canvas); cast(canvas); const capped = cast(canvas); assert(p.find(capped)); coverage.add('art-limit');
    advance(); act('art', 'p', capped, 11, 4, 1);
    clear(); advance();
    holder = grant(ranged); grant(ranged); act('board', 'p', holder, 11, 4);
    equip('chess_item_1_01_e_a', holder); cast(canvas); coverage.add('art-merge-holder');
    clear(); advance();
    holder = grant(ranged); act('board', 'p', holder, 11, 4); equip('chess_item_5_06_e_a', holder);
    cast(canvas); advance(); clear();
    holder = grant(gd.goldenIdOf(ranged)); act('board', 'p', holder, 11, 4);
    const full = grant(canvas); fill(); act('art', 'p', full, 11, 4, 1); assert(p.find(full));
    coverage.add('art-full-rollback'); clear(); advance();
    holder = grant(ranged); act('board', 'p', holder, 11, 4);
    while (p.hand.some(x => !x)) grant('chess_item_1_01_e_b');
    const temporary = grant(canvas); assert(p.temp.some(x => x?.uid === temporary));
    act('art', 'p', temporary, 11, 4, 1); clear(); advance();
    // Empty target, malformed direction/tile, ready and closed phase do not consume an Art or its quota.
    const invalid = grant(canvas);
    act('art', 'p', invalid, 8, 4, 1); act('art', 'p', invalid, 11, 4, 9);
    act('art', 'p', invalid, 11, 4, 1); act('art', 'missing', invalid, 11, 4, 1);
    act('ready', 'p', 1); act('art', 'p', invalid, 11, 4, 1); act('ready', 'p', 0);
    act('end'); act('art', 'p', invalid, 11, 4, 1); act('settle', 'p', 0); act('begin'); clear();
    for (let round = 0; round < 4; ++round) {
      cast(training, 9, 9); cast(pass, 12, 10);
      // Using the passing Art must neither grant destroy income nor send a copy to the next player.
      assert(!q.hand.some(x => x?.id === pass)); advance(); clear();
    }
    coverage.add(`art-bounty:${mode}:${variant}`);
    let passing = grant(pass); act('destroy', 'p', passing);
    passing = q.hand.find(x => x?.id === pass).uid; act('destroy', 'q', passing);
    const r = m.players.get('r'); passing = r.hand.find(x => x?.id === pass).uid;
    act('destroy', 'r', passing); coverage.add('art-pass-cycle'); clear();
    fill('q'); passing = grant(pass); act('destroy', 'p', passing);
    assert(!q.temp.some(x => x?.id === pass)); clear('q');
    act('end'); act('settle', 'p', 2); act('begin');
    passing = grant(pass); act('destroy', 'p', passing);
    assert(r.hand.some(x => x?.id === pass)); coverage.add('art-pass-eliminated');
    act('end'); act('settle', 'p', 4); act('begin');
    passing = grant(pass); act('destroy', 'p', passing); coverage.add('art-pass-alone');
  } else if (hooksOnly) {
    const sellable = 'chess_char_1_02_b';
    const cauldron = 'chess_item_6_03_e_a', knife = 'chess_item_3_04_e_a';
    for (const quality of ['a', 'b']) {
      clear();
      const holder = grant('chess_char_1_03_b');
      equip(`chess_item_6_03_e_${quality}`, holder); equip(`chess_item_3_04_e_${quality}`, holder);
      const before = p.funds;
      for (let n = 0; n < 5; ++n) { const extra = grant(sellable); act('sell', 'p', extra); }
      assert.equal(p.funds - before, 6 + gd.sellPrice(sellable) * 5); coverage.add(`cauldron-cap:${quality}`);
      advance();
      const start = p.funds; grant(sellable); assert.equal(p.funds - start, 2); coverage.add('cauldron-reset');
      clear(); advance();
      const carrier = grant('chess_char_1_01_b');
      const pack = equip(`chess_item_5_07_e_${quality}`, carrier);
      for (let n = 0; n < 4; ++n) { const extra = grant(sellable); act('sell', 'p', extra); }
      act('sell', 'p', carrier); // Its own sale returns the pack before the dispatch, so does not count itself.
      const replacement = grant('chess_char_1_01_b'); act('equip', 'p', pack, replacement, 0);
      for (let n = 0; n < 13; ++n) { const extra = grant(sellable); act('sell', 'p', extra); }
      coverage.add(`pack-counter:${quality}`);
    }
    clear(); advance();
    let holder = grant('chess_char_1_03_b'); act('board', 'p', holder, 12, 2);
    equip(cauldron, holder);
    const withoutKnife = p.funds; const extra = grant(sellable); assert.equal(p.funds, withoutKnife); act('remove', 'p', extra);
    equip(knife, holder); const beforeCopy = p.funds;
    const art = grant('chess_item_6_02_m'); act('art', 'p', art, 12, 2, 1);
    assert.equal(p.funds - beforeCopy, 2); coverage.add('cauldron-art');
    // Losing the old cauldron to a merge creates a new UID and a fresh quota.
    const merged = p.hand.find(x => x?.id === 'chess_item_6_03_e_b').uid;
    const mergedKnife = p.hand.find(x => x?.id === 'chess_item_3_04_e_b').uid;
    act('equip', 'p', merged, holder, 0); act('equip', 'p', mergedKnife, holder, 0);
    const rearmed = p.funds; grant(sellable); assert.equal(p.funds - rearmed, 2); coverage.add('cauldron-merge');
    clear(); advance();
    holder = grant('chess_char_6_11_b'); act('board', 'p', holder, 12, 2);
    equip(cauldron, holder); equip(knife, holder);
    for (const [id, col] of [['chess_char_1_03_b', 3], ['chess_char_2_04_b', 4]]) {
      const other = grant(id); act('board', 'p', other, 12, col);
    }
    const harmony = p.funds; grant(sellable);
    assert.equal(p.funds - harmony, p.bonds.maniShip?.active && p.bonds.yanShip?.active ? 2 : 0);
    coverage.add('cauldron-harmony');
    clear(); advance();
    holder = grant('chess_char_1_03_b'); equip(cauldron, holder); equip(knife, holder);
    act('funds', 'p', 100); act('buy', 'p', 0); act('buy', 'p', 1);
    const voucherHolder = grant('chess_char_1_01_b'); equip('chess_item_2_02_e_b', voucherHolder);
    coverage.add('cauldron-buy-voucher'); clear(); advance();
    // A failed grant while all containers are full does not fire onGain.
    holder = grant('chess_char_1_03_b'); act('board', 'p', holder, 12, 2);
    equip(cauldron, holder); equip(knife, holder); fill();
    const full = p.funds; assert.equal(grant(sellable), 0); assert.equal(p.funds, full);
    coverage.add('cauldron-full'); clear(); advance();
    // Higher-tier normal transformation returns the cell, uses no old tile and dispatches just once per old item pair.
    for (const quality of ['a', 'b']) for (const tier of [1, 6]) {
      clear();
      const id = `chess_char_${tier}_01_${quality}`;
      holder = grant(id); act('board', 'p', holder, 12, 2);
      equip(`chess_item_5_08_e_${quality}`, holder); equip('chess_item_1_01_e_b', holder);
      act('battle_result'); // Preparing is rejected without consuming the callback.
      act('end'); act('battle_result'); act('battle_result');
      assert(!p.find(holder)); assert.equal(p.board.size, 0);
      assert([...p.hand, ...p.temp].some(x => x?.id === `chess_item_5_08_e_${quality}`));
      act('settle', 'p', 0); act('begin'); coverage.add(`transform:${tier}:${quality}`);
    }
    clear();
    const old1 = grant('chess_char_1_01_b'), old2 = grant('chess_char_1_02_b');
    equip('chess_item_5_08_e_a', old1); equip('chess_item_5_08_e_b', old2);
    act('board', 'p', old2, 12, 3); act('board', 'p', old1, 12, 2);
    act('end'); fill(); act('battle_result'); act('settle', 'p', 0); act('begin');
    coverage.add('transform-full-order');
  } else {
  for (const id of consumables) {
    for (const variant of [0, 1, 2]) {
      clear();
      const holder = grant(variant === 2 ? 'chess_char_1_01_b' : 'chess_char_1_01_a');
      if (variant) act('board', 'p', holder, 12, 2);
      let replacement = 0;
      if (variant) {
        equip('chess_item_1_01_e_b', holder);
        replacement = equip('chess_item_1_02_e_b', holder);
      }
      const item = grant(id);
      act('equip', 'p', item, holder, 999999); // Invalid replacement even while a slot is free.
      act('equip', 'p', item, holder, variant === 2 ? replacement : 0);
      coverage.add(id);
      if (id === 'chess_item_5_06_e_a' && variant !== 2) advance();
      if (p.offers.length) act('reward', 'p', 0);
    }
  }
  // Level-limited draws, no duplicate offers, layers capped at 999, and consumed gear occupies its slot until resolution.
  clear(); act('funds', 'p', 200);
  for (let i = 0; i < 5; ++i) act('level');
  for (const bond of gd.bondIds) act('layers', 'p', bond, 998);
  for (const id of consumables) {
    clear();
    const holder = grant('chess_char_6_01_a');
    const item = grant(id); fill();
    act('equip', 'p', item, holder, 0);
  }
  clear();
  // Equipped bond grants disappear before a full-slot consumable observes its holder.
  const canGive = Object.keys(data.items).find(id => data.items[id].canGiveBond && data.items[id].isGolden);
  const give = Object.keys(data.items).find(id => data.items[id].giveBondId && !data.items[id].canGiveBond && data.items[id].isGolden);
  assert(canGive && give);
  let holder = grant('chess_char_1_01_a');
  equip(canGive, holder); const off = equip(give, holder);
  equip('chess_item_1_04_e_b', holder, off);
  // Mimic: two normal copies merge with the third; empty stock must not fall back to a different operator.
  clear(); holder = grant('chess_char_6_01_a'); grant('chess_char_6_01_a');
  equip('chess_item_5_05_e_a', holder);
  clear(); clear('q'); holder = grant('chess_char_6_01_a'); grant('chess_char_6_01_a');
  while (pool.left('chess_char_6_01_a') > 0) assert(grant('chess_char_6_01_b', 'q'));
  const before = m.rngMeta.state(); equip('chess_item_5_05_e_b', holder); assert.equal(m.rngMeta.state(), before);
  clear(); clear('q');
  // A normal projection stops being a scheduled promotion when detached/sold; promoting twice still consumes it.
  holder = grant('chess_char_1_01_a'); equip('chess_item_5_06_e_a', holder); act('sell', 'p', holder); advance();
  clear(); holder = grant('chess_char_1_01_a'); equip('chess_item_5_06_e_a', holder); act('promote', 'p', holder); advance();
  clear();
  // Promotion order is board reading order, not deployment order, when only one pool copy is available.
  const left = grant('chess_char_6_01_a'), right = grant('chess_char_6_01_a');
  act('board', 'p', right, 12, 3); act('board', 'p', left, 12, 2);
  equip('chess_item_5_06_e_a', left); equip('chess_item_5_06_e_a', right);
  while (pool.left('chess_char_6_01_a') > 1) assert(grant('chess_char_6_01_a', 'q'));
  advance(); clear(); clear('q');
  // Ready/phase/unknown-target failures leave the consumable and random stream untouched.
  holder = grant('chess_char_1_01_a'); const coin = grant('chess_item_3_12_e_a');
  act('ready', 'p', 1); act('equip', 'p', coin, holder, 0); act('ready', 'p', 0);
  act('equip', 'p', coin, coin, 0); act('equip', 'missing', coin, holder, 0);
  act('end'); act('equip', 'p', coin, holder, 0); act('settle', 'p', 0); act('begin'); clear();
  // A selected DIY operator may be sacrificed but never sent to another player's private roster.
  holder = grant('chess_char_5_diy1_a'); equip('chess_item_5_04_e_a', holder);
  clear(); advance(); clear('q'); clear('r');
  // The beacon chooses the player with the greatest actual bond count; a full receiver retries next round.
  const mate = grant('chess_char_1_01_a', 'q'); act('board', 'q', mate, 12, 2);
  holder = grant('chess_char_1_01_b'); equip('chess_item_5_04_e_b', holder);
  const gift = p.effects.find(e => e.key === 'effect:builtin_gift'); assert.equal(gift.params.toPlayerId, 'q');
  act('end'); act('settle', 'p', 0); fill('q'); act('begin');
  assert(p.effects.includes(gift)); coverage.add('gift-full-retry');
  clear('q'); advance(); assert(!p.effects.includes(gift));
  // A scarce-pool gift also retries; a receiver eliminated meanwhile is replaced.
  holder = grant('chess_char_6_01_b'); equip('chess_item_5_04_e_a', holder);
  const scarce = p.effects.find(e => e.key === 'effect:builtin_gift'); assert(scarce);
  while (pool.left('chess_char_6_01_a') > 0) assert(grant('chess_char_6_01_b', 'q'));
  advance(); assert(p.effects.includes(scarce)); coverage.add('gift-stock-retry');
  clear('q');
  const receiverMask = 1 << m.players.get(scarce.params.toPlayerId).seat;
  act('end'); act('settle', 'p', receiverMask); act('begin'); assert(!p.effects.includes(scarce));
  coverage.add('gift-retarget');
  // Gifts survive sender elimination, but recurring income does not pay eliminated senders.
  holder = grant('chess_char_1_01_b'); equip('chess_item_5_04_e_a', holder);
  const last = p.effects.find(e => e.key === 'effect:builtin_gift'); assert(last);
  act('end'); act('settle', 'p', 1); act('begin'); assert(!p.effects.includes(last));
  coverage.add('gift-eliminated-sender'); act('equip', 'p', 0, 0, 0);
  const survivor = m.alivePlayers()[0]; assert.equal(m.alivePlayers().length, 1);
  clear(survivor.playerId);
  holder = grant('chess_char_1_01_b', survivor.playerId);
  equip('chess_item_5_04_e_a', holder, 0, survivor.playerId);
  assert(!survivor.effects.some(e => e.key === 'effect:builtin_gift')); coverage.add('gift-no-teammate');
  }

  const run = spawnSync(resolve(opt('--native')), [String(seed), mode, variant], {
    input: commands.join('\n') + '\n', encoding: 'utf8', timeout: 60000, maxBuffer: 128 * 1024 * 1024,
  });
  assert.equal(run.error, undefined);
  assert.equal(run.status, 0, `signal ${run.signal}: ${run.stderr}; last output: ${run.stdout.slice(-1000)}`);
  const actual = run.stdout.trim().split('\n').map(JSON.parse); assert.equal(actual.length, expected.length);
  for (let i = 0; i < actual.length; ++i) assert.deepEqual(actual[i], expected[i], `${mode}/${variant}, seed ${seed}, step ${i}: ${commands[i]}`);
  snapshots += actual.length;
}
for (const key of artsOnly ? ['art:true', 'art:false', 'art-direction:0', 'art-direction:1', 'art-direction:2', 'art-direction:3',
  'art-limit', 'art-merge-holder', 'art-full-rollback', 'art-pass-cycle', 'art-pass-eliminated', 'art-pass-alone'] :
  hooksOnly ? ['cauldron-cap:a', 'cauldron-cap:b', 'cauldron-reset', 'cauldron-art', 'cauldron-merge', 'cauldron-harmony',
    'cauldron-buy-voucher', 'cauldron-full', 'pack-counter:a', 'pack-counter:b', 'transform:1:a', 'transform:1:b',
    'transform:6:a', 'transform:6:b', 'transform-full-order', 'battle_result:false', 'battle_result:true'] :
  [...consumables, 'equip:true', 'equip:false', 'gift-full-retry', 'gift-stock-retry', 'gift-retarget', 'gift-eliminated-sender', 'gift-no-teammate']) {
  assert(coverage.has(key), `missing ${key}`);
}
console.log(artsOnly ? `preparation arts: 3 spells, ${runs.length} scenarios, ${snapshots} snapshots, bounty modes/fallback, copy and passing covered` :
  hooksOnly ? `preparation item hooks: 6 equipment records, ${runs.length} scenarios, ${snapshots} snapshots, sale/gain/result covered` :
  `preparation items: ${consumables.length} consumed records, ${snapshots} snapshots, full/stock retry and elimination covered`);
