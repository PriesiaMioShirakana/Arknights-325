import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { spawnSync } from 'node:child_process';

const args = process.argv.slice(2), opt = key => args[args.indexOf(key) + 1], root = resolve(opt('--reference'));
const load = path => import(pathToFileURL(resolve(root, path)));
const { Battle } = await load('server/sim/Battle.js');
const { SkillRuntime } = await load('server/sim/skills.js');
const { setGameData } = await load('server/sim/content/support/index.js');
const data = Object.fromEntries(['tokens', 'stages', 'bands', 'items', 'bonds'].map(k => [k, JSON.parse(readFileSync(resolve(root, `data/${k}.json`), 'utf8'))]));
setGameData(data);
const stats = {maxHp: 10000, atk: 100, def: 0, res: 0, aspd: 100, bat: 1, moveSpeed: 0, blockCnt: 0, respawnTime: 70, cost: 0};
const chess = {target: {chessId: 'target', stats, rangeGrid: [[0, 0], [0, 1]]}};
const maps = Object.keys(data.stages).flatMap(stage => [0, 1, 2, 3].flatMap(mode => [0, 1, 2, 3].flatMap(elites => [0, 1, 2, 3].map(occupied => ({stage, mode, elites, occupied})))));
const lines = [...maps.map(r => `map ${r.stage} ${r.mode} ${r.elites} ${r.occupied}`), ...Array.from({length: 16}, (_, scene) => `heal ${scene}`)];
const run = spawnSync(resolve(opt('--native')), [], {input: lines.join('\n'), encoding: 'utf8', timeout: 120000, maxBuffer: 64 * 1024 * 1024});
assert.equal(run.error, undefined); assert.equal(run.status, 0, run.stderr);
const actual = run.stdout.trim().split('\n').map(JSON.parse); assert.equal(actual.length, lines.length);
const snapshot = (b, tokens = false) => b.units.filter(u => !tokens || u.kind === 'token').map(u => [u.defId, b.players.findIndex(p => p.playerId === u.ownerId), u.x, u.y,
  ['UP', 'RIGHT', 'DOWN', 'LEFT'].indexOf(u.dir), u.hp, u.s.atk, u.skill?.spTotal ?? 0, +!!u.skill?.active, u.stats.attacks, u.stats.heal, +u.alive, +!!u.removed]);
function equal(a, b, path) {
  if (Array.isArray(b)) { assert.equal(a?.length, b.length, path); b.forEach((v, i) => equal(a[i], v, `${path}.${i}`)); }
  else if (typeof b === 'number') assert(Number.isFinite(a) && Math.abs(a - b) <= 1e-9 * Math.max(1, Math.abs(b)), `${path}: C++ ${a}, JS ${b}`);
  else assert.equal(a, b, path);
}
for (const [index, {stage, mode, elites, occupied}] of maps.entries()) {
  const boss = mode >= 2, multi = !!(mode % 2);
  const players = Array.from({length: multi ? 2 : 1}, (_, owner) => ({playerId: owner ? 'q' : 'p', side: owner ? 'R' : 'L', colOffset: owner && !boss ? 8 : 0,
    contentInfo: {matchBands: ['band_amedic']}, units: Array.from({length: 3}, (_, i) => ({uid: 1 + owner * 3 + i, chessId: `op${owner}${i}`, abs: true,
      row: boss ? 3 : 10, col: i === 0 && occupied ? (owner ? (boss ? 18 : 10) : 2) : (owner ? 15 - i : 5 + i), dir: 'RIGHT',
      ...(i === 0 && occupied === 3 ? {carryState: {down: true}} : {})}))}));
  const defs = Object.fromEntries(players.flatMap((p, owner) => p.units.map((u, i) => [u.chessId, {...chess.target, chessId: u.chessId, isGolden: i < (owner ? 3 - elites : elites)}])));
  const b = new Battle({content: 'full', kind: boss ? 'boss' : 'normal', autoFinish: false,
    stage: {...data.stages[stage], devices: []}, rect: boss ? {r0: 0, r1: 5, c0: 0, c1: 20} : {r0: 9, r1: 12, c0: 0, c1: multi ? 20 : 10},
    data: {...data, chess: defs}, players, setup(b) {
      for (const [i, u] of b.allyUnits.entries()) { u.profile.noAttack = true; if (i % 3 === 0 && occupied === 2) u.deferDeploy = true; }
    }});
  b.start(); equal(actual[index], snapshot(b, true), lines[index]);
  assert.equal(b.errorCount, 0, JSON.stringify(b.errors));
}
for (let scene = 0; scene < 16; ++scene) {
  const positions = [[10, 3], [10, 4], [11, 3], [9, 3]];
  const units = positions.map(([row, col], i) => ({uid: i + 1, chessId: 'target', row, col: col + (scene >= 8 ? 3 : 0)}));
  const b = new Battle({content: 'full', autoFinish: false, timeLimit: 60, stage: {rows: Array(19).fill('r'.repeat(21))}, rect: {r0: 0, r1: 18, c0: 0, c1: 20},
    data: {...data, chess}, players: [{playerId: 'p', coords: 'field', units: units.slice(0, 3)}, {playerId: 'q', coords: 'field', units: units.slice(3)}],
    setup(b) { for (const u of b.allyUnits) {
      u.profile.noAttack = true;
      u.skill = new SkillRuntime(b, u, {skillType: 'MANUAL'}, {kind: 'duration', spCost: 100000, duration: 1, trigger: 'NEVER'});
    }}});
  b.start(); const targets = b.allyUnits.slice();
  const medic = b.spawnToken('p', scene % 2 ? 'char_613_acmedc' : 'char_605_cmedic', 10, 2);
  equal(actual[maps.length + scene][0], snapshot(b), `heal/${scene}/0`);
  for (let tick = 0; tick < 600; ++tick) {
    if (tick % 40 === 0) targets.forEach((u, i) => b.loseHp(u, Math.max(0, u.hp - (i === 0 ? 4900 : i === 1 ? 5000 : 500))));
    if (tick === 2 && scene < 8) medic.skill.gainSp(100);
    if (tick === 10) b.loseHp(targets[0], 100000);
    if (tick === 11) b.redeploy(targets[0], {free: true});
    if (tick === 20) b.retreat(targets[1], {dying: scene % 4 >= 2});
    if (tick === 21) b.redeploy(targets[1], {free: true});
    if (tick === 30) b.heal(medic, targets[0], 100000);
    if (tick === 31) b.heal(medic, targets[0], 100);
    if (tick === 32) b.heal(medic, targets[0], 100, {regen: true});
    if (tick === 40 && scene % 8 >= 4) b.applyStatus(targets[2], 'noHeal', {duration: 3});
    if (tick === 50 && scene % 8 >= 4) b.applyStatus(targets[3], 'isolated', {duration: 3});
    if (tick === 160 && scene % 8 >= 4) b.applyStatus(targets[2], 'healFree', {duration: 3});
    if (tick === 110) medic.skill.stop();
    if (tick === 111) b.loseHp(targets[0], 100000);
    if (tick === 112) b.redeploy(targets[0], {free: true});
    if (tick === 113) b.retreat(targets[1], {dying: scene % 4 >= 2});
    if (tick === 114) b.redeploy(targets[1], {free: true});
    if (tick === 115) b.loseHp(targets[3], 100000);
    if (tick === 116) b.redeploy(targets[3], {free: true});
    if (tick === 120) medic.skill.stop();
    if (tick === 121) medic.skill.gainSp(100);
    if (tick % 17 === 0 && scene < 8) b.forceAttack(medic);
    b.step(); equal(actual[maps.length + scene][tick + 1], snapshot(b), `heal/${scene}/${tick + 1}`);
  }
  assert.equal(b.errorCount, 0, JSON.stringify(b.errors));
}
console.log(`map medics: ${maps.length} placements and 16 healing/skill/talent scenarios, ${maps.length + 16 * 601} state snapshots`);
