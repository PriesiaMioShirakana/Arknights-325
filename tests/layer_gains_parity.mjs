import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const args = process.argv.slice(2), option = key => args[args.indexOf(key) + 1], root = resolve(option('--reference'));
const load = path => import(pathToFileURL(resolve(root, path)).href);
const { Battle } = await load('server/sim/Battle.js');
const { MatchCombat } = await load('server/match/match/combat.js');
const run = spawnSync(resolve(option('--native')), [], { encoding: 'utf8', timeout: 15000, maxBuffer: 8 * 1024 * 1024 });
assert.equal(run.error, undefined); assert.equal(run.status, 0, run.stderr);
const actual = JSON.parse(run.stdout), initial = [0, 998.5, 999, 1200];
for (let scene = 0; scene < 32; ++scene) {
  const hooked = !!(scene % 2), kind = Math.floor(scene / 2) % 4;
  const b = new Battle({ content: 'none', autoFinish: false, timeLimit: 100, rect: { r0: 0, r1: 18, c0: 0, c1: 20 }, kind: kind === 1 || kind === 3 ? 'boss' : 'normal',
    flags: kind === 2 ? { layerGainsEnabled: false } : kind === 3 ? { layerGainsEnabled: true } : {},
    data: { chess: { ally: { chessId: 'ally', stats: { maxHp: 100, blockCnt: 0 }, rangeGrid: [[0, 0]] } } },
    players: [{ playerId: 'p', coords: 'field', units: [{ uid: 1, chessId: 'ally', row: 3, col: 5 }], bonds: { known: { layers: initial[Math.floor(scene / 8)], count: 3, active: true, tier: 1 } } }],
    setup(battle) {
      for (const unit of battle.allyUnits) unit.profile = { noAttack: true };
      if (!hooked) return;
      battle.on('layerGain', event => {
        if (event.reason === 'double') event.n *= 2;
        if (event.reason === 'cancel') event.n = 0;
        if (event.reason === 'invalid') event.n = NaN;
        if (event.reason === 'nested') { battle.addLayers('p', 'other', 1); event.n += 1; }
        if (event.reason === 'tile') battle.addCoins('p', event.tile ? event.tile[1] + 10 * event.tile[0] : 1);
      });
      battle.on('battleEnd', () => battle.addLayers('p', 'known', 2));
    } });
  b.start();
  const expected = [];
  const add = (bond, amount, reason = '', source = null, tile = undefined) => {
    const result = b.addLayers('p', bond, amount, reason, { source, tile });
    const stats = b.result().perPlayer.p;
    expected.push([result, b.getPlayer('p').bonds.known.layers, stats.coins, Object.entries(stats.layerGains), b.drainEvents().filter(e => e[0] === 'layer').map(e => e.slice(1))]);
  };
  add('known', 0.75); add('absent', 1.5); add('known', 10000, 'double'); add('known', 1);
  b.getPlayer('p').bonds.known.layers = 998.5; add('known', 1, 'double'); add('absent', 0.5);
  add('known', Infinity); add('known', NaN); add('known', -1);
  b.getPlayer('p').bonds.known.layers = 0; add('known', 1, 'cancel'); add('known', 1, 'invalid'); add('known', 1, 'nested');
  const source = b.allyUnits[0]; add('known', 1, 'tile', source); add('known', 1, 'tile', source, [7, 15]);
  b.loseHp(source, 10000); add('known', 1, 'tile', source); b.step(); add('known', 1, 'tile', source);
  b.forceEnd(); add('known', 1);
  assert.equal(b.errorCount, 0, JSON.stringify(b.errors));
  assert.deepEqual(actual.battles[scene], expected, `battle layers scene=${scene}`);
}

const reports = [0, 1.5, 1.9, 1, 3.5, 999, 1002, 1002, 1005];
const player = { alive: true, left: false, layers: {}, battleLayerGains: {}, dirty() {} };
let changes = [], index = 0;
const match = { gd: { bond: id => ['known', 'other'].includes(id) }, dispatch(ps, hook, event) {
  assert.equal(hook, 'onLayers'); changes.push([event.bondId, event.from, event.to]);
  assert.equal(MatchCombat.prototype._applyBattleLayerGains.call(match, ps, { [event.bondId]: ps.battleLayerGains[event.bondId] }), false, 'reentrant duplicate');
} };
for (let round = 1; round <= 3; ++round) {
  player.battleLayerGains = {}; player.layers = { known: round === 2 ? 998.5 : 0, other: 0 };
  for (let i = 0; i < reports.length; ++i) {
    changes = [];
    const changed = MatchCombat.prototype._applyBattleLayerGains.call(match, player, { known: reports[i], other: reports[(i + 3) % reports.length], unknown: 100 });
    assert.deepEqual(actual.ledger[index++], [+changed, ['known', 'other'].map(id => [id, player.layers[id], player.battleLayerGains[id] || 0]), changes], `ledger ${round}/${i}`);
  }
}
assert.equal(index, actual.ledger.length);
console.log('Layer gains JS parity passed: 32 battle scenes and 27 cumulative ledger reports, caps, hooks, source tiles, final hooks and reentrant deduplication.');
