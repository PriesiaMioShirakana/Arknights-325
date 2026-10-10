// 整局真实阶段方法，内容限已迁移的机变/道具；这里在首次战斗前停止。
import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import {readFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';

const args = process.argv.slice(2), option = key => args[args.indexOf(key) + 1], root = resolve(option('--reference'));
const load = file => import(pathToFileURL(resolve(root, file)));
const [{Match}, {VirtualScheduler}, {MetaRegistry}, {registerBuiltins}, {registerMeta}, {setGameData}] = await Promise.all([
  load('server/match/Match.js'), load('server/match/scheduler.js'), load('server/match/effectsMeta.js'),
  load('server/match/builtinMeta.js'), load('server/sim/content/items/meta.js'), load('server/sim/content/support/index.js'),
]);
const {registerMeta: registerGarrisons} = await load('server/sim/content/garrisons/meta.js');
const {registerMeta: registerBands} = await load('server/sim/content/bands/meta.js');
const data = Object.fromEntries(['config', 'chess', 'items', 'bonds', 'stages', 'waves', 'enemies', 'bosses', 'bands',
  'factions', 'choices', 'tokens', 'backups', 'effects', 'garrisons'].map(key =>
  [key, JSON.parse(readFileSync(resolve(root, `data/${key}.json`), 'utf8'))]));
setGameData(data);
let snapshots = 0;
for (const mode of Object.keys(data.config.modes).sort()) for (const seed of [0, 42, 4294967295]) {
  const solo = data.config.modes[mode].type === 'SINGLE' || mode.startsWith('mode_single_');
  const seats = Array.from({length: solo ? 1 : 3}, (_, seat) => ({seat, playerId: `p${seat}`, name: `p${seat}`, isBot: false, connected: true}));
  const registry = new MetaRegistry(); registerBuiltins(registry, data); registerMeta(registry); registerBands(registry); registerGarrisons(registry);
  const scheduler = new VirtualScheduler();
  const match = new Match({mode: solo ? 'solo' : 'coop', modeId: mode, seed, data, seats, scheduler,
    registry, clientCombat: false, botRehearsal: 0, send: () => true, broadcast: () => {}, onEnd: () => {}});
  const expected = [];
  const snapshot = () => {
    const started = !['INFO_CHECK', 'BAND_DRAFT', 'LOBBY'].includes(match.phase);
    expected.push([match.phase, match.round, match.order.map(p => [p.playerId, p.funds, p.shop.level, +p.ready,
      started ? p.lp : null, match.draft?.picks[p.playerId] ?? '',
      p.shop.slots.map(s => s ? [s.id, s.basePrice, +!!s.sold, +!!s.frozen] : null),
      p.hand.map(x => x ? [x.uid, x.id, x.poolCopies || 0] : null),
      p.bounties.map(b => [b.id, b.card.enemyKey, b.card.count, b.card.coin, +(b.card.payout === 'perfect'), b.roundsLeft])]),
    match.phase === 'BAND_DRAFT' ? match.draft.order : [], match.phase === 'BAND_DRAFT' ? match.draftTurn() ?? '' : '',
    match.sp?.cards.map(c => c.id) ?? [], match.sp?.cards.map((_, i) => match.sp.taken[i] ?? null) ?? [], match.spTurn() ?? '']);
  };
  match.start(); snapshot();
  for (const p of match.order) p.infoReady = true;
  match.maybeEndInfo(); scheduler.advance(0); snapshot();
  while (match.phase === 'BAND_DRAFT') {
    const id = match.draftTurn(); assert(match.pickBand(match.players.get(id), match.timeoutBand(id)).ok);
    scheduler.advance(0); snapshot();
  }
  assert.equal(match.phase, 'BATTLE_CHECK'); scheduler.advance(match.gd.timer('battleCheck') * 1000); snapshot();
  assert.equal(match.phase, 'ROUND_START'); scheduler.advance(2000); snapshot();
  while (match.phase === 'SP_DRAFT') {
    const card = match.sp.cards.findIndex((_, i) => match.sp.taken[i] == null);
    assert(match.pickCard(match.players.get(match.spTurn()), card).ok);
    scheduler.advance(0); snapshot();
  }
  assert.equal(match.phase, 'PREP'); assert(match.players.get('p0').freeze().ok); snapshot();
  assert.equal(match.errorCount, 0); assert.equal(scheduler.errors.length, 0);
  const run = spawnSync(resolve(option('--native')), [mode, String(seed)], {encoding: 'utf8', maxBuffer: 4 * 1024 * 1024});
  assert.equal(run.status, 0, `${mode}/${seed}: ${run.stderr}`);
  const actual = run.stdout.trim().split('\n').map(line => JSON.parse(line));
  assert.deepEqual(actual, expected, `${mode}/${seed}`);
  snapshots += actual.length;
  match.dispose(); scheduler.dispose();
}
console.log(`27 local match openings / ${snapshots} phase and inventory snapshots matched JS`);
