// Compare session selections against the real protocol/PlayerState, including composed DIY bodies and token supply.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { spawnSync } from 'node:child_process';
const args = process.argv.slice(2), opt = key => args[args.indexOf(key) + 1], root = resolve(opt('--reference'));
const load = path => import(pathToFileURL(resolve(root, path)));
const { GameData } = await load('server/match/gamedata.js');
const { PlayerState } = await load('server/match/PlayerState.js');
const { DataSource } = await load('server/sim/simdata.js');
const { checkLoadout, loadoutOptions, checkNotOwned, checkDiyPicks } = await load('shared/protocol.js');
const { statusKey } = await load('shared/standIn.js');
const { positionClass } = await load('server/match/board.js');
const { attackRangeGrid } = await load('shared/loadoutRecord.js');
const data = Object.fromEntries(['config', 'chess', 'items', 'stages', 'tokens', 'backups'].map(k => [k, JSON.parse(readFileSync(resolve(root, `data/${k}.json`), 'utf8'))]));
const gd = new GameData(data, 'mode_multi_normal'), ds = new DataSource(data);
const get = id => data.chess[id] || null, allChars = Object.keys(data.backups.units), slots = Object.keys(data.backups.diy.slots);
const commands = [], expected = [], labels = [];
let ps, bot = false, kitted = allChars;
const err = r => r.error === 'BAD_MSG' ? 1 : r.error ? 2 : 0;
const snapshot = (error = 0, dropped = 0) => [error, dropped,
  Object.entries(ps.loadout).map(([id, e]) => [id, e.skill, e.module ?? '']), ps.standIns,
  Object.entries(ps.diy).map(([id, e]) => [id, e.charId, e.skillIndex, e.uniEquipId ?? 'none'])];
function record(command, value, label = command) { commands.push(command); expected.push(value); labels.push(label); }
function reset(isBot = false) {
  bot = isBot;
  ps = new PlayerState({ gd, log: {}, markPrivate() {} }, { playerId: 'p', seat: 0, name: 'p', isBot });
  record(`reset ${+isBot}`, snapshot());
}
function kits(ids) { kitted = ids; commands.push(`kits ${ids.length} ${ids.map(JSON.stringify).join(' ')}`); }
function loadout(entries) {
  const r = checkLoadout(entries, get), error = bot ? 3 : err(r);
  if (!error) assert(ps.setLoadout(entries));
  record(`loadout ${Object.keys(entries).length} ${Object.entries(entries).map(([id, e]) => `${JSON.stringify(id)} ${e.skill ?? -99} ${JSON.stringify(e.module ?? '-')}`).join(' ')}`, snapshot(error));
}
function own(ids) {
  const r = checkNotOwned(ids, get), error = bot ? 3 : err(r);
  if (!error) assert(ps.setNotOwned(ids));
  record(`own ${ids.length} ${ids.map(JSON.stringify).join(' ')}`, snapshot(error, error ? 0 : r.dropped));
}
function diy(picks) {
  const r = checkDiyPicks(picks, { data, kitted }), error = bot ? 3 : err(r);
  if (!error) assert(ps.setDiy(picks, { kitted }));
  record(`diy ${Object.keys(picks).length} ${Object.entries(picks).map(([id, p]) => `${JSON.stringify(id)} ${JSON.stringify(p?.charId ?? '-')} ${p?.skillIndex ?? -99} ${JSON.stringify(p?.uniEquipId ?? '-')}`).join(' ')}`, snapshot(error, error ? 0 : r.dropped));
}
function query(id) {
  const raw = ps.gd.chess(id), standIn = ps.fieldsStandIn(raw), lo = ps.loadoutFor(raw), pick = ps.diyPickOf(id);
  const d = standIn ? ds.getStandIn(id) : pick ? ds.getDiy(id, pick) : ds.getChess(id, lo);
  const grants = standIn || (raw.isDiy && !pick) ? [] : ps.gd.placeableTokens(id, lo);
  const tokens = grants.map(({ tokenId, count }) => [tokenId, count, +!!ps.gd.token(tokenId).ownerRange, +!!ps.gd.token(tokenId).ownerRangeOutside]);
  record(`query ${id}`, [d.charId ?? '', lo.skillIndex ?? -1, lo.moduleId ?? '', +standIn, +!!pick, raw.bonds, tokens, d.position, d.stats.atk, d.stats.maxHp,
    { all: 0, ranged: 0, melee: 1, high: 2 }[positionClass(ps.fieldRecord(raw))], attackRangeGrid(d.raw) ?? []]);
}
reset(); kits(allChars);
const visible = Object.values(data.chess).filter(c => !c.isGolden && c.visible && !c.isHidden && !c.isDiy);
for (const c of visible) {
  const o = loadoutOptions(c, get(c.goldenId));
  for (const skill of o.skills) for (const module of o.modules) {
    loadout({ [c.chessId]: { skill, module } }); query(c.chessId); query(c.goldenId);
  }
  own([c.chessId, c.chessId, c.goldenId, 'gone']); query(c.chessId); query(c.goldenId); own([]);
  loadout({ [c.chessId]: { skill: 9 } });
  loadout({ [c.chessId]: { module: 'unknown' } });
}
const id = visible[0].chessId;
for (const entries of [{ [id]: {} }, { [id]: { skill: -1 } }, { [id]: { module: '' } }, { 'bad id': { skill: 0 } }, { [data.chess[id].goldenId]: { skill: 0 } }, { [slots[0]]: { skill: 0 } }]) loadout(entries);
own(['bad id']); own(Array(161).fill(id)); loadout(Object.fromEntries(Array.from({ length: 161 }, (_, i) => [`unknown_${i}`, { skill: 0 }])));
let diySelections = 0;
// Enumerate legal form choices independently from the native generator, including roster-excluded modules.
for (const slot of [slots[0], slots[2]]) {
  const tier = data.backups.diy.slots[slot].tier, normal = data.chess[slot], golden = data.chess[normal.goldenId];
  const prototypes = data.backups.diy.prototypes[tier];
  for (const char of new Set([...prototypes, ...data.backups.diy.ownedPool])) {
    const unit = data.backups.units[char], nf = unit?.forms[statusKey(normal.status)], gf = unit?.forms[statusKey(golden.status)];
    if (!nf || !gf) continue;
    const choices = prototypes.includes(char) ? [{ charId: char }] : nf.skills.filter(s => gf.skills.some(t => t.index === s.index)).flatMap(s => [null, ...gf.modules.map(m => m.uniEquipId)].map(mid => ({ charId: char, skillIndex: s.index, uniEquipId: mid })));
    for (const pick of choices) { diy({ [slot]: pick }); query(slot); query(normal.goldenId); ++diySelections; }
  }
}
const prototype = data.backups.diy.prototypes[6].find(c => data.backups.diy.prototypes[5].includes(c));
diy(Object.fromEntries([...slots].reverse().map(s => [s, { charId: prototype }])));
for (const slot of slots) query(slot);
const ownedChar = data.backups.diy.ownedPool[0];
diy(Object.fromEntries([...slots].reverse().map(s => [s, { charId: ownedChar, skillIndex: 0 }])));
diy({ [slots[0]]: { charId: ownedChar }, [slots[1]]: { charId: 'unknown' }, unknown: { charId: prototype } });
diy({ [slots[0]]: { charId: prototype, skillIndex: 9 }, [slots[1]]: null });
diy({ [slots[0]]: { charId: prototype, uniEquipId: 'none' } });
for (const picks of [{ [slots[0]]: { charId: 'bad id' } }, { [slots[0]]: { charId: ownedChar, skillIndex: 10 } }, Object.fromEntries(Array.from({ length: 9 }, (_, i) => [`unknown_${i}`, null]))]) diy(picks);
kits([]); diy({ [slots[0]]: { charId: prototype } }); kits(allChars);
reset(true); loadout({ [id]: { skill: 0 } }); own([id]); diy({ [slots[0]]: { charId: prototype } }); query(id);
const run = spawnSync(resolve(opt('--native')), [], { input: commands.join('\n') + '\n', encoding: 'utf8', maxBuffer: 64 * 1024 * 1024 });
assert.equal(run.status, 0, run.stderr);
const actual = run.stdout.trim().split('\n').map(JSON.parse); assert.equal(actual.length, expected.length);
for (let i = 0; i < expected.length; ++i) assert.deepEqual(actual[i], expected[i], `${i}: ${labels[i]}`);
console.log(`roster parity: ${actual.length} snapshots, ${visible.length} operators and ${diySelections} DIY choices`);
