import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { spawnSync } from 'node:child_process';

const args = process.argv.slice(2), opt = key => args[args.indexOf(key) + 1], root = resolve(opt('--reference'));
const load = path => import(pathToFileURL(resolve(root, path)));
const { GameData } = await load('server/match/gamedata.js');
const { setupMatchWaves, buildNormalWave, buildBossWave, withBounties, bountySpawns } = await load('server/match/waves.js');
const { duckReplace } = await load('server/sim/content/bands/meta.js');
const { createRng } = await load('server/sim/rng.js');
const data = Object.fromEntries(['config', 'factions', 'enemies', 'stages', 'bosses', 'waves', 'bands']
  .map(k => [k, JSON.parse(readFileSync(resolve(root, `data/${k}.json`), 'utf8'))]));
const buff = data.bands.band_ducklord.buffs.find(b => b.key === 'round_start_all_player_change_enemy_2');
const params = { ...buff.bb, ...buff.bbStr };
const run = spawnSync(resolve(opt('--native')), [], { encoding: 'utf8', timeout: 120000, maxBuffer: 64 * 1024 * 1024 });
assert.equal(run.status, 0, run.stderr); assert.equal(run.error, undefined);
const actual = JSON.parse(run.stdout);
const snapshot = spawns => spawns.map(s => [s.enemyKey, s.time, s.routeIndex, s.count, s.interval,
  ({boss: 1, part: 2, bounty: 3, duck: 4}[s.tag] || 0), Math.max(0, ['', 'N', 'E', 'S', 'NF', 'EF', 'SF', 'T', 'TF'].indexOf(s.mods?.slot)),
  +(s.countInTotal !== false), s.mods?.hpMul ?? 1, s.mods?.atkMul ?? 1, s.mods?.defMul ?? 1, s.mods?.resMul ?? 1,
  s.mods?.speedMul ?? 1, s.mods?.supplyHpMul ?? null, s.bounty?.coins ?? 0, s.bounty?.ownerPlayerId ?? '', s.mods?.bountyId ?? '', s.ownerPlayerId ?? '']);
let ctx, setup, mode, seed;
for (const row of actual) {
  const scenario = row[2];
  if (mode !== row[0] || seed !== row[1]) {
    [mode, seed] = row; ctx = { gd: new GameData(data, mode), rng: createRng(seed) };
    setup = setupMatchWaves(ctx.gd, ctx.rng);
  }
  const { gd, rng } = ctx;
  const round = scenario < 13 ? scenario + 1 : scenario < 16 ? gd.bossRound : gd.hiddenRound ?? 0;
  const wave = scenario < 13 ? buildNormalWave(gd, rng, setup, round) : buildBossWave(gd, rng, setup, round,
    { bossId: scenario < 16 ? setup.bossId : setup.hiddenBossId, solo: scenario % 3 === 0 });
  const side = scenario < 13 ? null : [null, 'L', 'R'][scenario % 3];
  const bounty = [{id: 'card', card: {enemyKey: 'enemy_1422_lrsldr', count: 3, coin: 7}}];
  const spawns = scenario === 19 ? [] : scenario < 13 ? withBounties(gd, round, wave, bounty, 'p', {side}) :
    [...wave.spawns, ...bountySpawns(gd, round, wave, bounty, 'p', {side})];
  const count = duckReplace(ctx, spawns, params, 'p', {routes: wave.routes, side}).length;
  const expected = [mode, seed, scenario, count, rng.state(), snapshot(spawns)];
  const second = scenario >= 13 ? duckReplace(ctx, spawns, params, 'q', {routes: wave.routes, side: 'R'}).length : 0;
  expected.push(second, rng.state(), snapshot(spawns));
  assert.deepEqual(row, expected, `${mode}/${seed}/${scenario}`);
}
assert.equal(actual.length, Object.keys(data.config.modes).length * 5 * 20);
console.log(`duck replacements: ${actual.length} wave scenarios with RNG, split actions, boss halves and bounty preservation`);
