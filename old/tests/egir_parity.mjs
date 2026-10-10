import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { spawnSync } from 'node:child_process';

const args = process.argv.slice(2), opt = key => args[args.indexOf(key) + 1], root = resolve(opt('--reference'));
const load = path => import(pathToFileURL(resolve(root, path)));
const { Battle } = await load('server/sim/Battle.js');
const { SkillRuntime } = await load('server/sim/skills.js');
const { install } = await load('server/sim/content/bonds/core.js');
const { install: installAddon } = await load('server/sim/content/bonds/addon/battle.js');
const { install: installItems } = await load('server/sim/content/items/battle.js');
const { install: installBands } = await load('server/sim/content/bands/battle.js');
const { setGameData } = await load('server/sim/content/support/index.js');
const data = Object.fromEntries(['bands', 'bonds', 'items'].map(k => [k, JSON.parse(readFileSync(resolve(root, `data/${k}.json`), 'utf8'))]));
setGameData(data);
const runs = [1, 42, 4294967295].flatMap(seed => Array.from({length: 24}, (_, scene) => ({seed, scene})));
const run = spawnSync(resolve(opt('--native')), [], {input: runs.map(r => `${r.scene} ${r.seed}`).join('\n'), encoding: 'utf8', maxBuffer: 32 * 1024 * 1024, timeout: 60000});
assert.equal(run.error?.message, undefined); assert.equal(run.status, 0, run.stderr);
const actual = run.stdout.trim().split('\n'); assert.equal(actual.length, runs.length);
const positions = [[10, 2], [10, 3], [10, 4], [10, 5], [10, 6], [11, 3], [10, 7], [10, 8], [10, 9]];
const cycle = [[10, 2], [10, 3], [11, 3], [11, 2]], directions = ['RIGHT', 'UP', 'LEFT', 'DOWN'];

function equal(a, b, path) {
  if (Array.isArray(b)) { assert.equal(a?.length, b.length, path); b.forEach((v, i) => equal(a[i], v, `${path}.${i}`)); }
  else if (typeof b === 'number') assert(Number.isFinite(a) && Math.abs(a - b) <= 1e-9 * Math.max(1, Math.abs(b)), `${path}: C++ ${a}, JS ${b}`);
  else assert.equal(a, b, path);
}

let snapshots = 0;
for (const [index, {seed, scene}] of runs.entries()) {
  const expected = JSON.parse(actual[index]), mirror = scene === 3;
  const names = positions.map((_, i) => `a${i}`);
  const chess = Object.fromEntries(names.map((id, i) => [id, {chessId: id, baseId: id, tier: 1 + i % 6, position: 'MELEE',
    bonds: scene === 17 && i === 1 ? [] : [(scene === 13 && i >= 6) || (scene === 16 && (i === 1 || i === 2)) ? 'maniShip' : 'egirShip'],
    stats: {maxHp: scene === 1 || scene === 2 ? 7000 : scene === 20 ? 5000 : 1000, atk: 100 + i * 50,
      def: scene === 4 ? 5000 : scene === 20 ? 0 : 650, res: 0, aspd: 100, bat: 1, moveSpeed: 0, blockCnt: 1 + i % 2, respawnTime: 1, cost: 0}, rangeGrid: [[0, 0], [0, 1]],
    ...(scene === 11 ? {profession: 'SPECIAL', subProfessionId: 'dollkeeper'} : {})}]));
  const b = new Battle({content: 'none', seed, autoFinish: false, timeLimit: 30, flags: {layerGainsEnabled: scene !== 15},
    rect: {r0: 0, r1: 18, c0: 0, c1: 20}, stage: {id: 'egir', rows: Array(19).fill('r'.repeat(21))}, data: {...data, chess},
    players: [0, 1].map(player => ({playerId: player ? 'q' : 'p', coords: 'field', side: player ? 'R' : 'L', colOffset: player ? 8 : 0, mirror,
      bandId: scene === 9 ? 'band_ermengard' : null,
      bonds: {egirShip: {active: !(player === 1 && (scene === 12 || scene === 13)) && !(player === 0 && scene === 23),
        layers: scene === 2 ? 60 : 0, count: scene === 22 ? 5 : 3, tier: scene === 22 ? 0 : scene === 14 && player === 0 ? 1 : 2},
        ...([13, 16].includes(scene) ? {maniShip: {active: true, tier: 1, layers: 0}} : {}),
        ...(scene === 10 ? {indomShip: {active: true, tier: 2, count: 3, layers: 200}} : {})},
      units: names.flatMap((id, i) => (i >= 6) === !!player ? (() => {
        const [row, col] = scene === 18 && i < 4 ? cycle[i] : scene === 18 && i === 5 ? [11, 6] : positions[i];
        return [{uid: i + 1, ...(scene === 5 && i === 2 ? {kind: 'token', tokenId: id, def: chess[id], ownerUid: 1} : {chessId: id}),
          row, col: mirror ? 20 - col : col, dir: scene === 18 && i < 4 ? directions[i] : i === 5 ? 'DOWN' : mirror || scene === 19 ? 'LEFT' : 'RIGHT',
          items: scene === 8 ? ['chess_item_4_12_e_a'] : [],
          ...(scene === 7 && (i === 1 || i === 6) ? {carryState: {hpPct: 0.4, sp: 37, down: true}} : {})}];
      })() : [])})),
    setup(b) {
      for (const player of b.players) player.mirror = mirror;
      for (const [i, u] of b.allyUnits.entries()) {
        u.profile = {...u.profile, noAttack: true, dmgType: 'phys', attack: 'melee', maxTargets: 1, dollDuration: 1};
        u.skill = new SkillRuntime(b, u, {skillType: 'MANUAL'}, {kind: 'duration', spCost: 100, initSp: 25, duration: 1, trigger: 'NEVER'});
        b.addBuff(u, {key: 'initial', mods: {atkPct: 0.5}, persist: true, allowDead: true});
        if (scene === 21) b.addBuff(u, {key: 'mitigation', mods: {dmgTakenMul: 0.1}, shield: 10000, persist: true, allowDead: true});
        if (scene === 6 && i === 2) u.deferDeploy = true;
      }
      install(b); installAddon(b); installItems(b); installBands(b);
    }
  });
  const snapshot = () => [b.allyUnits.map(u => [u.hp, +u.alive, u.s.maxHp, u.s.atk, u.s.def, u.s.blockCnt,
    u.skill.spTotal, u.x, u.y, +!!u.trait.doll, +!!u.trait.dollSwitching, Number.isFinite(u.respawnAt) ? u.respawnAt : null]),
    ['p', 'q'].map(id => b.getPlayer(id).bonds.egirShip.layers), ['p', 'q'].map(id => b.result().perPlayer[id].deaths), b.rng.state()];
  let frame = 0;
  const check = () => { equal(expected[frame], snapshot(), `${seed}/${scene}/${frame}`); ++frame; ++snapshots; };
  b.start(); check();
  const u = b.allyUnits;
  for (let tick = 0; tick < 90; ++tick) {
    if (tick === 2) for (const a of u) b.addBuff(a, {key: 'later', mods: {atkPct: 1}, persist: true, allowDead: true});
    if (tick === 5) { b.addLayers('p', 'egirShip', 60); b.addLayers('q', 'egirShip', 60); }
    if (tick === 15) b.retreat(u[0]);
    if (tick === 16) b.redeploy(u[0], {free: true});
    if (tick % 9 === 3) b.loseHp(u[Math.floor(tick / 9) % 9], 100000);
    if (tick % 20 === 7) for (const a of u) b.redeploy(a, {free: true});
    if (tick % 30 === 11) for (const a of u) b.heal(a, a, 10000);
    if (tick % 3 === 0) check();
    b.step();
    if (tick % 3 === 0) check();
  }
  assert.equal(frame, expected.length);
  assert.equal(b.errorCount, 0, JSON.stringify(b.errors));
}
console.log(`${runs.length} Egir scenarios / ${snapshots} snapshots matched JS`);
