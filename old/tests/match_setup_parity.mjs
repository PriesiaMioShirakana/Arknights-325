import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import {readFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';

const args = process.argv.slice(2), option = key => args[args.indexOf(key) + 1];
const root = resolve(option('--reference'));
const load = async file => import(pathToFileURL(resolve(root, file)));
const [{GameData}, {createRng, deriveSeed}, {setupMatchWaves}, {drawDisabledBonds}] = await Promise.all([
  load('server/match/gamedata.js'), load('server/sim/rng.js'), load('server/match/waves.js'), load('server/match/pool.js'),
]);
const data = Object.fromEntries(['config', 'chess', 'items', 'bonds', 'stages', 'waves', 'enemies', 'bosses', 'bands', 'factions'].map(
  key => [key, JSON.parse(readFileSync(resolve(root, `data/${key}.json`), 'utf8'))],
));
const run = spawnSync(resolve(option('--native')), [], {encoding: 'utf8', maxBuffer: 8 * 1024 * 1024});
assert.equal(run.status, 0, run.stderr);
const rows = JSON.parse(run.stdout);
for (const row of rows) {
  const [mode, seed] = row, gd = new GameData(data, mode), rng = createRng(deriveSeed(seed, 'setup'));
  const setup = setupMatchWaves(gd, rng), bans = drawDisabledBonds(gd, rng);
  assert.deepEqual(row, [mode, seed, rng.state(), setup.stageId ?? '', setup.bossId ?? '', setup.hiddenBossId ?? '',
    bans.drawn, [...bans.staticOff, ...bans.drawn].sort(), bans.banned.sort()], `${mode}/${seed}`);
}
assert.equal(rows.length, 45);
console.log('45 match setups matched JS: map, bosses, disabled bonds, stock bans and RNG');
