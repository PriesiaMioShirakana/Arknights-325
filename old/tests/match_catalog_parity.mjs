// 真实 config/bands/bosses 表，不在测试中重写已解析的规则。
import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import {readFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
const args=process.argv.slice(2),opt=k=>args[args.indexOf(k)+1],root=resolve(opt('--reference'));
const {GameData}=await import(pathToFileURL(resolve(root,'server/match/gamedata.js')));
const {bossPoolHp,hiddenEligible}=await import(pathToFileURL(resolve(root,'server/match/finalAssault.js')));
const data=Object.fromEntries(['config','bands','bosses'].map(k=>[k,JSON.parse(readFileSync(resolve(root,`data/${k}.json`),'utf8'))]));
const run=spawnSync(resolve(opt('--native')),[],{encoding:'utf8',maxBuffer:8*1024*1024});assert.equal(run.status,0,run.stderr);const actual=JSON.parse(run.stdout);
for(const row of actual){
 const gd=new GameData(data,row[0]);
 const expected=[gd.modeId,+gd.isSolo,gd.difficulty,gd.lastRound,gd.bossRound,gd.hiddenRound??0,gd.spRounds(),
  Array.from({length:16},(_,i)=>[gd.prepTime(i+1),gd.combatTimeLimit(i+1),gd.bossLevelTime(i+1)]),
  gd.bandIds().map(id=>[id,gd.startLp(id),gd.band(id).effectId??'',gd.band(id).bondIds??[]]),
  [gd.timer('infoCheck'),gd.timer('battleCheck'),gd.timer('bandTurn'),gd.bandDraft.skipsPerPlayer,gd.bandDraft.timeoutBandId,gd.timer('spFirst'),gd.timer('spTurn'),gd.lpCapPerRound,
   gd.bans(gd.difficulty).core,gd.bans(gd.difficulty).addon,gd.unite.maxHelpers,gd.dp.dpInit,gd.dp.dpPerSec,gd.dp.dpMax],
  [...Object.keys(data.bosses).sort(),'unknown'].map(id=>[id,bossPoolHp(gd,id),Array.from({length:22},(_,n)=>[bossPoolHp(gd,id,n),bossPoolHp(gd,id,n,{experimental:true})])]),
  Array.from({length:8},(_,i)=>+hiddenEligible(gd,{layerSum:i<4?1200:2401,teamLp:i%3?2:1,aliveCount:i%2?8:4,experimental:i%2!==0}))];
 assert.deepEqual(row,expected,gd.modeId);
}
assert.equal(actual.length,Object.keys(data.config.modes).length);
console.log('All 9 match mode configurations, strategy options, round timers and boss scaling matched JS');
