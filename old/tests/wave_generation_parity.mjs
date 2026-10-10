// 原版比赛层独立产生选择和队列；种子主流状态也必须相同。
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const args=process.argv.slice(2),opt=k=>args[args.indexOf(k)+1],root=resolve(opt('--reference'));
const { GameData }=await import(pathToFileURL(resolve(root,'server/match/gamedata.js')));
const { setupMatchWaves,buildNormalWave,buildBossWave,withBounties,bountySpawns,buildUniteWave }=await import(pathToFileURL(resolve(root,'server/match/waves.js')));
const { createRng }=await import(pathToFileURL(resolve(root,'server/sim/rng.js')));
const data=Object.fromEntries(['config','factions','enemies','stages','bosses','waves'].map(k=>[k,JSON.parse(readFileSync(resolve(root,`data/${k}.json`),'utf8'))]));
const run=spawnSync(resolve(opt('--native')),[],{encoding:'utf8',maxBuffer:32*1024*1024});assert.equal(run.status,0,run.stderr);const actual=JSON.parse(run.stdout);
const type=k=>['SPECIAL','FLY','TIMES','ELEMENT','DOT','INVISIBLE','REFLECTION'].indexOf(k);
const slot=k=>Math.max(0,['','N','E','S','NF','EF','SF','T','TF'].indexOf(k));
const pick=p=>p?[p.round,type(p.type),p.key,p.normal??'',p.elite??'',+p.fly,+p.firstHalf]:null;
const plan=p=>[p.templateId??'',Number.isFinite(p.timeLimit)?p.timeLimit:null,pick(p.pick),p.spawns.map(s=>[s.time,s.enemyKey,s.routeIndex,s.count,s.interval,
 s.mods.hpMul??1,s.mods.atkMul??1,s.mods.speedMul??1,s.mods.supplyHpMul??null,slot(s.mods.slot),({boss:1,part:2,bounty:3}[s.tag]??0),s.actionIndex??-1,+(s.countInTotal!==false),
 +(s.preview.gate==='upper'),+s.preview.fly,+s.preview.elite,+!!s.preview.boss,s.preview.start,+(s.tag==='bounty'),s.mods.bountyId??'',s.ownerPlayerId??'',s.sourcePlayerId??'',s.bounty?.coins??0,s.bounty?.ownerPlayerId??'',s.mods.defMul??1,s.mods.resMul??1]),(p.actions??[]).map(a=>[a.index,a.key??'',a.tplKey,slot(a.slot),a.time,a.count,a.tplCount,a.window,a.routeIndex,+a.valid,+a.server])];
function equal(a,b,path){
 if(Array.isArray(b)){assert.equal(a.length,b.length,path);b.forEach((v,i)=>equal(a[i],v,`${path}.${i}`));}
 else if(typeof b==='number')assert(Number.isFinite(a)&&Math.abs(a-b)<=1e-9*Math.max(1,Math.abs(b)),`${path}: native ${a}, JS ${b}`);
 else assert.equal(a,b,path);
}
for(const row of actual){
 const [mode,seed]=row,gd=new GameData(data,mode),rng=createRng(seed),s=setupMatchWaves(gd,rng);
 const bounties=[['zero','enemy_1422_lrsldr',0,7],['air','enemy_1005_yokai',3,11],['many','enemy_1427_lrnazg',25,9],['perfect','enemy_1042_frostd',2,100,'perfect'],['missing','no_such_enemy',1,10]].map(([id,enemyKey,count,coin,payout])=>({id,card:{enemyKey,count,coin,payout}}));
 const keys=['enemy_1422_lrsldr','enemy_1005_yokai','enemy_1427_lrnazg','enemy_1042_frostd','enemy_1000_gopro_2','enemy_1041_lazerd','enemy_1425_lrcmra','enemy_1040_bombd'];
 const leaks=Array.from({length:48},(_,i)=>({enemyKey:keys[i%8],sourcePlayerId:String(i%3),mods:i%2?{hpMul:2,atkMul:1.2,defMul:1.1,resMul:0.8,speedMul:1.3,supplyHpMul:1.6,slot:'S',bountyId:i%5===0?'leaked':''}:null,
  isToken:i%7===0,tag:i%5===0?'bounty':undefined,bounty:i%5===0?{coins:17,ownerPlayerId:i%10===0?'away':undefined}:undefined}));leaks.push({enemyKey:'no_such_enemy'});
 const expected=[mode,seed,rng.state(),s.stageId??'',s.bossId??'',s.hiddenBossId??'',s.factions.map(type),s.typeSlots.map(type),s.picks.map(pick),
  Array.from({length:15},(_,i)=>plan(buildNormalWave(gd,rng,s,i+1))),Array.from({length:4},(_,i)=>plan(buildBossWave(gd,rng,s,i<2?gd.bossRound:gd.hiddenRound??0,
   {bossId:i<2?s.bossId:s.hiddenBossId,solo:i%2===0}))),
  Array.from({length:18},(_,i)=>{const boss=i<9||(i>=12&&i<15),round=i<3?1:i<6?10:boss?gd.bossRound:gd.hiddenRound??0;
   const base=i<6?buildNormalWave(gd,rng,s,round):buildBossWave(gd,rng,s,round,{bossId:boss?s.bossId:s.hiddenBossId,solo:false});
   const options={side:[null,'L','R'][i%3]};
   return plan({...base,spawns:i<12?withBounties(gd,round,base,bounties,'one',options):[...base.spawns,...bountySpawns(gd,round,base,bounties,'one',options)]});}),
  [plan(buildUniteWave(gd,leaks,1)),plan(buildUniteWave(gd,leaks,2))]];
 equal(row,expected,`${mode}/${seed}`);
}
assert.equal(actual.length,Object.keys(data.config.modes).length*5);
console.log(`${actual.length} deterministic match setups / ${actual.length*39} complete wave plans matched JS`);
