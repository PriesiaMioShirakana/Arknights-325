// Private DIY copies must join only the owner's rolls, and return to that same pool on every removal path.
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
const { createRng, deriveSeed } = await load('server/sim/rng.js');
const { makeCtx } = await load('server/match/effectsMeta.js');
const { PHASE } = await load('shared/constants.js');
const { buildDeployMap } = await load('server/match/board.js');
const data = Object.fromEntries(['config', 'chess', 'items', 'stages', 'tokens', 'backups'].map(k => [k, JSON.parse(readFileSync(resolve(root, `data/${k}.json`), 'utf8'))]));
const slots = Object.keys(data.backups.diy.slots), kitted = Object.keys(data.backups.units);
let snapshots = 0, diyBuys = 0;
for (const seed of [0, 1, 2, 7, 42, 177, 2147483648, 4294967295]) {
  const gd = new GameData(data, 'mode_multi_normal'), pool = new SharedPool(gd), noop = () => {};
  let nextUid = 0;
  const m = { gd, poolFor: () => pool, nextUid: () => ++nextUid, rngShop: createRng(deriveSeed(seed, 'shop')), phase: PHASE.INFO_CHECK, round: 0, deployMapFor: () => buildDeployMap(null),
    dispatch: noop, dispatchItem: noop, markPrivate: noop, tickerFor: noop, onReadyChanged: noop, log: {}, toast: noop };
  const players = ['p', 'q'].map((id, seat) => new PlayerState(m, { playerId: id, seat, name: id }));
  for (const p of players) { p.recompute = () => p._fillHandFromTemp(); p.grantTokensFor = noop; }
  const commands = [], expected = [];
  const piece = p => p ? [p.uid, p.id, p.poolCopies || 0] : null;
  const view = p => [p.funds, p.shop.level, +p.alive, Object.keys(p.diy).map(id => [id, p.diyStock.cap(id), p.diyStock.left(id)]),
    p.hand.map(piece), p.temp.map(piece), Array.from({ length: 36 }, (_, i) => piece(p.board.get(`${12 - Math.floor(i / 9)},${2 + i % 9}`))),
    p.shop.slots.map(s => s ? [s.id, +!!s.sold] : null), p.offers.map(o => o.slots.map(s => s.id))];
  function act(op, player = 'p', ...a) {
    commands.push([op, player, ...a].join(' '));
    const p = players.find(p => p.playerId === player), ctx = makeCtx(m, p, {}, 'test');
    let ok = true, uid = 0;
    if (op === 'config') {
      if (m.phase !== PHASE.INFO_CHECK || nextUid) ok = false;
      else {
        const count = a[0], picks = Object.fromEntries(Array.from({ length: count }, (_, i) => [a[1 + 4 * i], { charId: a[2 + 4 * i], skillIndex: a[3 + 4 * i], uniEquipId: a[4 + 4 * i] }]));
        assert(p.setDiy(picks, { kitted })); p.initDiyStock(new Set(a.slice(2 + 4 * count)));
      }
    } else if (op === 'begin') { m.phase = PHASE.PREP; ++m.round; for (const q of players) if (q.alive) { q.startRound(m.round); q.checkItemMerges(); q.recompute(); } }
    else if (op === 'end') { for (const q of players) if (q.alive) q.endPrep(); m.phase = PHASE.COMBAT; }
    else if (op === 'settle') { players[0].pendingFunds += 70; if (a[0]) players[1].eliminate(); }
    else if (op === 'grant') uid = p.acquireChess(a[0])?.uid || 0;
    else if (op === 'promote') { ok = ctx.promote(a[0]); uid = ok ? a[0] : 0; }
    else if (op === 'remove') ok = ctx.destroyPiece(a[0]);
    else if (op === 'transform') { const old = p.find(a[0]); ok = !!old && old.piece.kind === 'chess'; if (ok) uid = p.transformChess(old.piece, a[1])?.uid || 0; }
    else if (op === 'board') ok = !!p.move(a[0], { area: 'board', row: a[1], col: a[2] }, 'RIGHT').ok;
    else if (op === 'level') ok = !!p.levelUp().ok;
    else if (op === 'reward') ok = !!p.pickReward(a[0]).ok;
    else ok = !!p[op](...a).ok;
    expected.push([+ok, uid, ...players.map(view)]); return uid;
  }
  // A ranged DIY body proves placement uses the selected character, not the melee slot template.
  const char = 'char_2013_cerber';
  const bonds = data.backups.diy.operators[char].bonds;
  const pick = [slots[0], char, 0, 'none', slots[2], 'char_002_amiya', 0, 'none'];
  // Use the same valid prototype in both tiers; each player still owns independent 8/5 stock.
  const proto = data.backups.diy.prototypes[6].find(c => data.backups.diy.prototypes[5].includes(c));
  const locked = data.backups.diy.locked[6][proto];
  pick.splice(4, 4, slots[2], proto, locked.skillIndex, locked.uniEquipId ?? 'none');
  act('config', 'p', 2, ...pick, 0);
  act('config', 'q', 2, ...pick, seed % 2 ? bonds.length : 0, ...(seed % 2 ? bonds : []));
  act('grant', 'p', slots[1]); // Unfilled slots never consume UIDs or become pieces.
  act('begin');
  const first = act('grant', 'p', slots[0]); act('board', 'p', first, 12, 2); act('promote', 'p', first);
  act('grant', 'q', slots[0]); act('grant', 'q', slots[0]); act('grant', 'q', slots[0]);
  act('sell', 'p', first);
  for (let round = 0; round < 10; ++round) {
    if (round) { act('end'); act('settle', 'p', 0); act('begin'); }
    while (players[0].shop.level < 6 && players[0].funds >= players[0].shop.upgradePrice) { if (!players[0].shop.upgradePrice && players[0].shop.level >= 6) break; act('level'); }
    for (let i = 0; i < 28; ++i) {
      const p = players[0];
      if (i % 7 === 0) act('grant', 'p', slots[(i + round) % 2 ? 0 : 2]);
      else if (i % 7 === 1) { const found = p.hand.find(x => x?.kind === 'chess'); if (found) act('transform', 'p', found.uid, slots[2]); }
      else if (i % 7 === 2) { const found = p.hand.find(x => x?.kind === 'chess'); if (found) act('remove', 'p', found.uid); }
      else if (i % 7 === 3 && p.offers.length) act('reward', 'p', 0);
      else {
        const slot = p.shop.slots.findIndex(s => s && !s.sold && s.kind === 'chess' && p.gd.chess(s.id)?.isDiy);
        if (slot >= 0 && p.hand.some(x => !x)) { act('buy', 'p', slot); ++diyBuys; }
        else act('refresh');
      }
    }
  }
  // Overflow and temporary expiry return copies; elimination releases another player's independent stock.
  for (let i = 0; i < 24; ++i) act('grant', 'p', data.chess[slots[0]].goldenId);
  act('end'); act('settle', 'p', 1); act('begin');
  act('config', 'p', 0, 0); // Configuration is frozen once play starts.
  const run = spawnSync(resolve(opt('--native')), [String(seed)], { input: commands.join('\n') + '\n', encoding: 'utf8', maxBuffer: 32 * 1024 * 1024 });
  assert.equal(run.status, 0, run.stderr);
  const actual = run.stdout.trim().split('\n').map(JSON.parse); assert.equal(actual.length, expected.length);
  for (let i = 0; i < actual.length; ++i) assert.deepEqual(actual[i], expected[i], `seed ${seed}, step ${i}: ${commands[i]}`);
  snapshots += actual.length;
}
assert(diyBuys > 0, 'fixture must exercise DIY shop purchases');
console.log(`DIY stock parity: ${snapshots} snapshots, ${diyBuys} DIY purchase attempts`);
