// Execute the real choice registry, dispatcher and Match.applyCard, including its team target order.
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
const { makeCtx, MetaRegistry, EffectDispatcher } = await load('server/match/effectsMeta.js');
const { registerBuiltins } = await load('server/match/builtinMeta.js');
const { applyCard } = await load('server/match/choices.js');
const { setGameData } = await load('server/sim/content/support/index.js');
const { registerMeta } = await load('server/sim/content/choices.js');
const { createRng, deriveSeed } = await load('server/sim/rng.js');
const { PHASE } = await load('shared/constants.js');
const data = Object.fromEntries(['config', 'chess', 'items', 'choices', 'stages', 'tokens', 'backups', 'effects', 'enemies', 'bonds'].map(k => [k, JSON.parse(readFileSync(resolve(root, `data/${k}.json`), 'utf8'))]));
setGameData(data);
const cards = [];
for (const [id, e] of Object.entries(data.effects)) if (['ENEMY_GAIN', 'BUFF_GAIN'].includes(e.effectType)) {
  const kind = e.effectType === 'ENEMY_GAIN' ? 'bounty' : 'tactic';
  const card = data.choices.cards[kind].find(c => c.effectId === id);
  cards.push({ kind, id, name: card?.name ?? e.name, desc: card?.desc ?? e.desc, team: !!card?.team });
}
for (const [id, item] of Object.entries(data.items)) if (item.itemType === 'EQUIP' && !item.isGolden && !item.hideInShop && !item.shopExcluded) cards.push({ kind: 'item', id });
cards.sort((a, b) => a.id.localeCompare(b.id, 'en'));
let snapshots = 0, teamPicks = 0;
for (const seed of [1, 42, 4294967295]) {
  const gd = new GameData(data, 'mode_multi_normal'), pool = new SharedPool(gd), noop = () => {};
  let uid = 0;
  const m = { gd, poolFor: () => pool, nextUid: () => ++uid, rngMeta: createRng(seed), rngShop: createRng(deriveSeed(seed, 'shop')),
    phase: PHASE.INFO_CHECK, round: 0, markPrivate: noop, tickerFor: noop, toast: noop, log: {}, onReadyChanged: noop,
    rollItemId: MatchSpDraft.prototype.rollItemId, rollPool: MatchSpDraft.prototype.rollPool, addBounty: MatchSpDraft.prototype.addBounty };
  m.players = new Map(['p', 'q', 'r'].map((id, seat) => [id, new PlayerState(m, { playerId: id, seat, name: id })]));
  m.alivePlayers = () => [...m.players.values()].filter(p => p.alive);
  const registry = new MetaRegistry(); registerBuiltins(registry, data); registerMeta(registry);
  m.registry = registry; m.dispatcher = new EffectDispatcher(m, registry);
  m.dispatch = (...a) => m.dispatcher.dispatch(...a); m.dispatchItem = (...a) => m.dispatcher.dispatchItem(...a);
  for (const p of m.players.values()) { p.recompute = () => p._fillHandFromTemp(); p.grantTokensFor = noop; }
  const p = m.players.get('p');
  assert(p.setDiy({ chess_char_5_diy1_a: { charId: 'char_2013_cerber', skillIndex: 0, uniEquipId: 'none' } }, { kitted: ['char_2013_cerber'] })); p.initDiyStock(new Set());
  m.phase = PHASE.PREP; m.round = 1; for (const p of m.players.values()) p.startRound(1);
  for (const p of m.players.values()) p.endPrep(); m.players.get('r').eliminate(1);
  m.round = 2; for (const p of m.alivePlayers()) p.startRound(2);
  const piece = (p, x) => x ? [x.uid, x.id, x.poolCopies || 0, p.temp.includes(x) ? p.tempDue(x) : null] : null;
  const view = p => [p.funds, p.shop.freeRefreshes, p.counters['choices:refSeq'] || 0,
    p.hand.map(x => piece(p, x)), p.temp.map(x => piece(p, x)), Object.fromEntries(Object.entries(p.layers).filter(([, n]) => n > 0)),
    p.effects.filter(e => e.key.startsWith('effect:builtin_next_buy')).map(e => [e.id, +e.key.endsWith('golden_item'), e.counter]),
    p.effects.filter(e => e.battle !== false).map(e => [e.id, e.data.effectId, e.data.round, typeof e.params?.prepOk === 'boolean' ? +e.params.prepOk : null, e.params?.bench ?? null]),
    Object.fromEntries(Object.entries(p.deviceOverrides).map(([k, v]) => [k, +v])),
    p.bounties.map(b => [b.id, b.card.enemyKey, b.card.count, b.card.coin, +(b.card.payout === 'perfect'), b.roundsLeft])];
  const commands = [], expected = [];
  const act = (op, player = 'p', ...a) => {
    commands.push([op, player, ...a].join(' '));
    const p = m.players.get(player); let ok = true;
    if (op === 'pick') {
      const card = cards.find(c => c.id === a[1]);
      if (!p || !p.alive || !card || ['bounty', 'item', 'tactic'][a[0]] !== card.kind) ok = false;
      else { applyCard(m, p, { ...card, team: !!a[2] }); if (a[2]) teamPicks++; }
    } else if (op === 'clear') {
      const ctx = makeCtx(m, p, {}, 'test'); for (const piece of [...p.hand, ...p.temp].filter(Boolean)) ctx.destroyPiece(piece.uid);
    } else if (op === 'grant') { if (gd.chess(a[0])) p.acquireChess(a[0]); else p.acquireItem(a[0]); }
    else if (op === 'layers') p.addLayers(a[0], a[1]);
    else if (op === 'prep_end') {
      for (const p of m.alivePlayers()) if (!p.ready) { p.resolveTemp(); p.ready = true; }
      for (const p of m.alivePlayers()) m.dispatch(p, 'onPrepEnd', {});
    } else if (op === 'unready') ok = !!p.setReady(false).ok;
    else ok = !!p[op](...a).ok;
    assert.equal(m.dispatcher.errors, 0, JSON.stringify([...m.dispatcher.errorsByKey]));
    expected.push([+ok, m.rngMeta.state(), ...[...m.players.values()].map(view)]);
  };
  const pick = (card, player = 'p') => act('pick', player, ['bounty', 'item', 'tactic'].indexOf(card.kind), card.id, +card.team);
  for (const bond of gd.bondIds) act('layers', 'p', bond, 998);
  for (let i = 0; i < cards.length; ++i) {
    const card = cards[i]; pick(card, i % 3 ? 'p' : 'q');
    if (i % 2 === 0) pick(card); // Repeated cards stack; team cards have one independent grant per recipient.
    if (i % 7 === 0) { act('clear', 'p'); act('clear', 'q'); }
    if (i % 13 === 0) { act('refresh'); act('buy', 'p', 0); }
  }
  act('pick', 'missing', 2, 'missing', 0); act('pick', 'r', 2, cards.find(c => c.kind === 'tactic').id, 1);
  act('pick', 'p', 2, 'missing', 0);
  act('prep_end'); act('clear', 'p'); act('unready'); act('prep_end');
  act('grant', 'p', 'chess_char_1_01_a'); act('grant', 'p', 'chess_item_1_01_e_a'); act('prep_end');
  const run = spawnSync(resolve(opt('--native')), [String(seed)], { input: commands.join('\n') + '\n', encoding: 'utf8', maxBuffer: 128 * 1024 * 1024 });
  assert.equal(run.status, 0, run.stderr);
  const actual = run.stdout.trim().split('\n').map(JSON.parse); assert.equal(actual.length, expected.length);
  for (let i = 0; i < actual.length; ++i) assert.deepEqual(actual[i], expected[i], `seed ${seed}, step ${i}: ${commands[i]}`);
  snapshots += actual.length;
}
assert(teamPicks > 20);
console.log(`choice rewards: ${cards.length} real cards, ${snapshots} snapshots, ${teamPicks} team picks`);
