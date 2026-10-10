// 真实机变数据与原 generateDraft；同时对照随机状态、卡位、奖励字段及显示文本。
import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import {readFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
const args=process.argv.slice(2),opt=k=>args[args.indexOf(k)+1],root=resolve(opt('--reference'));
const {GameData}=await import(pathToFileURL(resolve(root,'server/match/gamedata.js')));
const {generateDraft}=await import(pathToFileURL(resolve(root,'server/match/choices.js')));
const {createRng}=await import(pathToFileURL(resolve(root,'server/sim/rng.js')));
const data=Object.fromEntries(['config','choices','items','enemies','effects'].map(k=>[k,JSON.parse(readFileSync(resolve(root,`data/${k}.json`),'utf8'))]));
const run=spawnSync(resolve(opt('--native')),[],{encoding:'utf8',maxBuffer:128*1024*1024});assert.equal(run.status,0,run.stderr);const actual=JSON.parse(run.stdout);
const modes=new Map(Object.keys(data.config.modes).map(id=>[id,new GameData(data,id)])),seen=new Set();
for(const row of actual){
 const [mode,seed,round]=row,gd=modes.get(mode),rng=createRng(seed);
 const draft=generateDraft(gd,rng,round,{stageId:seed%3===0?null:seed%3===1?'act2autochess_m01':'act2autochess_m02',
  bondAvailable:seed%4===0?null:seed%4===1?()=>false:b=>['yanShip','swiftShip'].includes(b),playerCount:seed%7===0?20:8,experimental:seed%3!==0});
 if(draft)seen.add(draft.family);
 const expected=[mode,seed,round,rng.state(),draft?[
  ['bounty','supply','shop','tactic'].indexOf(draft.family),draft.name,draft.desc,draft.eventId??'',
  draft.cards.map(c=>[['bounty','item','tactic'].indexOf(c.kind),c.id,c.name,c.desc,c.descRaw??'',c.tier??null,c.enemyKey??'',c.count??1,c.coin??0,c.rounds??1,+(c.payout==='perfect'),+!!c.team,c.tacticKind??''])]:null];
 assert.deepEqual(row,expected,`${mode}/${seed}/${round}`);
}
assert.equal(actual.length,9*60*7);assert.equal(seen.size,4);
console.log(`${actual.length} real-data drafts across all four families matched JS, including RNG state and card text`);
