// Real PlayerState placement and summon lifecycle, with content hooks disabled but recompute/ranges active.
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
const { makeCtx } = await load('server/match/effectsMeta.js');
const { createRng, deriveSeed } = await load('server/sim/rng.js');
const { PHASE } = await load('shared/constants.js');
const { isDiyModule } = await load('shared/diy.js');
const { pieceDir, buildDeployMap } = await load('server/match/board.js');
const data = Object.fromEntries(['config', 'chess', 'items', 'stages', 'tokens', 'backups'].map(k => [k, JSON.parse(readFileSync(resolve(root, `data/${k}.json`), 'utf8'))]));
const dirs = ['UP', 'RIGHT', 'DOWN', 'LEFT'], gd0 = new GameData(data, 'mode_multi_normal');
const goldItem = Object.keys(data.items).find(id => data.items[id].isGolden && data.items[id].itemType === 'EQUIP');
const tiles = Array.from({ length: 36 }, (_, i) => [12 - Math.floor(i / 9), 2 + i % 9]);
const cases = [];
for (const c of Object.values(data.chess).filter(c => c.visible && !c.isGolden && !c.isDiy && c.tokens.some(t => gd0.token(t)?.placeable))) {
  for (const skill of c.skills.map(s => s.index)) cases.push([c.chessId, skill, '-', 0, '-']);
  if (gd0.standIn(c.chessId)) cases.push([c.chessId, -1, '-', 1, '-']);
}
// DIY has additional placeable devices and module-specific stack capacities absent from preset owners.
const slot = Object.keys(data.backups.diy.slots).find(id => data.backups.diy.slots[id].tier === 6);
const locked = data.backups.diy.locked[6];
const summonChars = new Set(Object.values(data.backups.tokens).filter(t => t.placeable).flatMap(t => Object.keys(t.variants ?? {}).map(k => k.split('@')[0])));
for (const char of summonChars) {
  const unit = data.backups.units[char]; if (!unit) continue;
  const s = data.chess[data.chess[slot].goldenId].status;
  const form = unit.forms[`${s.phase}/${s.level}/${s.skillLevel}/${s.equipLevel}`]; if (!form) continue;
  const picks = locked[char] ? [[locked[char].skillIndex, locked[char].uniEquipId ?? 'none']] :
    form.skills.flatMap(s => ['none', ...(form.modules || []).filter(isDiyModule).map(m => m.uniEquipId)].map(mid => [s.index, mid]));
  for (const [skill, module] of picks) cases.push([slot, skill, module, 0, char]);
}
let snapshots = 0, tokensDeployed = 0;
const coverage = new Set();
for (let scene = 0; scene < cases.length; ++scene) {
  const [ownerId, skill, module, standIn, char] = cases[scene], seed = scene * 7919;
  const gd = new GameData(data, 'mode_multi_normal'), pool = new SharedPool(gd), noop = () => {};
  let uid = 0, layout = buildDeployMap(null);
  const m = { gd, poolFor: () => pool, nextUid: () => ++uid, rngShop: createRng(deriveSeed(seed, 'shop')), round: 0, phase: PHASE.PREP,
    deployMapFor: () => layout, dispatch: noop, dispatchItem: noop, markPrivate: noop, tickerFor: noop, onReadyChanged: noop, log: {}, toast: noop };
  const p = new PlayerState(m, { seat: 0, playerId: 'p', name: 'p' });
  if (char !== '-') {
    assert(p.setDiy({ [ownerId]: { charId: char, skillIndex: skill, uniEquipId: module } }, { kitted: [char] }));
    if (!p.diy[ownerId]) continue;
    p.initDiyStock();
  } else {
    if (skill >= 0 || module !== '-') assert(p.setLoadout({ [ownerId]: { ...(skill >= 0 ? { skill } : {}), ...(module !== '-' ? { module } : {}) } }));
    if (standIn) assert(p.setNotOwned([ownerId]));
  }
  p.deployMap = () => layout;
  // Bond recomputation is tested independently; keep the original summon reconciliation and hand fill.
  p.recompute = () => { p._liftOutOfRange(); p._fillHandFromTemp(); };
  const ctx = makeCtx(m, p, {}, 'test'), commands = [], expected = [];
  const piece = x => x ? [x.uid, x.id, x.poolCopies || 0, dirs.indexOf(pieceDir(x)), (x.items || []).map(piece),
    p.temp.includes(x) ? p.tempDue(x) : null, +!!x.deferMerge, x.ownerUid || 0, x.count || 1] : null;
  const snapshot = (ok, granted) => [ok, granted, p.funds, +p.ready, p.prepsEnded, p.hand.map(piece), p.temp.map(piece),
    tiles.map(([r, c]) => piece(p.board.get(`${r},${c}`))), p.offers.map(o => o.slots.map(s => s.id)),
    m.phase === PHASE.PREP ? 1 : m.phase === PHASE.COMBAT ? 2 : 3, tiles.map(([r, c]) => ({ melee: 1, ranged: 2 }[layout.get(`${r},${c}`)] || 0)), [...p.board.values()].map(x => x.uid)];
  function act(op, ...a) {
    commands.push([op, ...a].join(' ')); let ok = true, granted = 0;
    if (op === 'begin') { m.phase = PHASE.PREP; p.startRound(++m.round); p.checkItemMerges(); p.recompute(); }
    else if (op === 'end') { p.endPrep(); m.phase = PHASE.COMBAT; }
    else if (op === 'layout') {
      layout = a[0] === 0 ? buildDeployMap(null) : a[0] === 2 ? new Map() : new Map(tiles.map(([r, c], i) => [`${r},${c}`, a[0] === 1 || (a[0] === 4 && i % 2) ? 'ranged' : 'melee']));
      p._evictIllegal(); p.recompute();
    } else if (op === 'grant') granted = (data.chess[a[0]] ? p.acquireChess(a[0], { fromPool: !!a[1], toTemp: !!a[2] }) : p.acquireItem(a[0], { toTemp: !!a[2] }))?.uid || 0;
    else if (op === 'promote') { ok = ctx.promote(a[0]); granted = ok ? a[0] : 0; }
    else if (op === 'remove') ok = ctx.destroyPiece(a[0]);
    else if (op === 'board') { ok = !!p.move(a[0], { area: 'board', row: a[1], col: a[2] }, dirs[a[3]]).ok; if (ok && p.find(a[0])?.piece.kind === 'token') { ++tokensDeployed; const moved = p.find(a[0]).piece; if (p.gd.token(moved.id)?.ownerRangeOutside) coverage.add('outside-token'); if (p.gd.token(moved.id)?.ownerRange) coverage.add('inside-token'); } }
    else if (op === 'hand') ok = !!p.move(a[0], { area: 'hand', idx: a[1] }).ok;
    else if (op === 'ready') ok = !!p.setReady(!!a[0]).ok;
    else if (op === 'equip') ok = !!p.equip(a[0], a[1], a[2] || null).ok;
    else ok = !!p[op](...a).ok;
    expected.push(snapshot(+ok, granted)); coverage.add(`${op}:${ok}`); return granted;
  }
  const grant = id => act('grant', id, 1, 0, 0);
  const locateBoard = x => [...p.board].find(([, v]) => v.uid === x);
  function deploy(x, pref = null) {
    const piece = p.find(x)?.piece; if (!piece) return;
    const legal = [pref, ...tiles].filter(Boolean).find(([r, c]) => !p.board.has(`${r},${c}`) && p._legal(piece, r, c));
    if (legal) act('board', x, ...legal, 1);
  }
  function deployTokens() {
    for (const stack of [...p.hand, ...p.temp].filter(x => x?.kind === 'token')) {
      const count = stack.count || 1;
      for (let n = 0; n < count; ++n) { const current = p.find(stack.uid); if (!current || current.area === 'board') break; deploy(stack.uid); }
    }
  }
  act('begin'); act('layout', 4);
  const owner = grant(ownerId); deploy(owner, [10, 5]); deployTokens();
  for (let direction = 0; direction < 4; ++direction) {
    const at = locateBoard(owner); if (at) act('board', owner, ...at[0].split(',').map(Number), direction);
    deployTokens();
  }
  const summon = [...p.board.values()].find(x => x.kind === 'token');
  if (summon) {
    const at = locateBoard(owner); if (at) act('board', summon.uid, ...at[0].split(',').map(Number), 1);
    act('sell', summon.uid); act('destroy', summon.uid); act('equip', summon.uid, owner, 0);
  }
  deploy(owner, [11, 7]); deployTokens();
  act('promote', owner); deployTokens();
  const deployed = [...p.board.values()].find(x => x.kind === 'token');
  if (deployed) { act('hand', deployed.uid, 0); deployTokens(); }
  // Removing the stacks and filling both containers makes reorientation/withdrawal exercise their full-hand guards.
  for (const stack of [...p.hand, ...p.temp].filter(x => x?.kind === 'token')) act('remove', stack.uid);
  for (let i = 0; i < 15; ++i) grant(goldItem);
  const fullToken = [...p.board.values()].find(x => x.kind === 'token');
  if (fullToken) act('hand', fullToken.uid, 0);
  const at = locateBoard(owner); if (at) act('board', owner, ...at[0].split(',').map(Number), 3);
  act('layout', 1); act('layout', 2); act('end'); act('layout', 0); act('begin');
  // Withdrawal clears all summons; re-deployment tops up once, with newly allocated stack UIDs.
  if (p.find(owner)?.area === 'board') act('hand', owner, 0);
  for (const item of [...p.hand].filter(x => x?.kind === 'item').slice(0, 4)) act('destroy', item.uid);
  deploy(owner, [10, 5]); deployTokens();
  act('remove', owner);
  // Merge while one consumed copy is deployed clears its old stacks, then grants the elite's stacks.
  const one = grant(ownerId); deploy(one, [10, 5]); deployTokens(); grant(ownerId); grant(ownerId);
  const merged = [...p.board.values()].find(x => x.kind === 'chess');
  if (merged) { deployTokens(); act('sell', merged.uid); }
  const rng = createRng(seed + 19);
  for (let step = 0; step < 80; ++step) {
    const owned = [...p.hand, ...p.temp, ...p.board.values()].filter(Boolean);
    const current = owned[Math.floor(rng() * owned.length)];
    switch (step % 8) {
      case 0: grant(ownerId); break;
      case 1: if (current) act('board', current.uid, ...tiles[Math.floor(rng() * tiles.length)], Math.floor(rng() * 4)); break;
      case 2: if (current) act('hand', current.uid, Math.floor(rng() * 10)); break;
      case 3: if (current) act('remove', current.uid); break;
      case 4: if (current) act('promote', current.uid); break;
      case 5: act('layout', Math.floor(rng() * 5)); break;
      case 6: if (current) act('sell', current.uid); break;
      case 7: if (step === 39) { act('end'); act('begin'); } else deployTokens(); break;
    }
  }
  const run = spawnSync(resolve(opt('--native')), [seed, ...cases[scene]].map(String), { input: commands.join('\n') + '\n', encoding: 'utf8', maxBuffer: 16 * 1024 * 1024 });
  assert.equal(run.status, 0, `scene ${scene} ${cases[scene]}: ${run.stderr}`);
  const actual = run.stdout.trim().split('\n').map(JSON.parse); assert.equal(actual.length, expected.length);
  for (let i = 0; i < actual.length; ++i) assert.deepEqual(actual[i], expected[i], `scene ${scene} ${cases[scene]}, step ${i}: ${commands[i]}`);
  snapshots += actual.length;
}
assert(tokensDeployed > 100, 'must deploy real summons');
for (const key of ['hand:false', 'board:false', 'promote:true', 'remove:true', 'layout:true', 'outside-token', 'inside-token']) assert(coverage.has(key), key);
console.log(`summons parity: ${cases.length} configurations, ${snapshots} snapshots, ${tokensDeployed} token moves`);
