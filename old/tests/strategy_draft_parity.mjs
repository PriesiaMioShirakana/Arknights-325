// 直接运行原 MatchPhases 的选秀方法，使用显式时钟执行一次到期回调。
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const args=process.argv.slice(2),opt=k=>args[args.indexOf(k)+1],root=resolve(opt('--reference'));
const {MatchPhases}=await import(pathToFileURL(resolve(root,'server/match/match/phases.js')));
const {PHASE}=await import(pathToFileURL(resolve(root,'shared/constants.js')));
const {createRng}=await import(pathToFileURL(resolve(root,'server/sim/rng.js')));
const run=spawnSync(resolve(opt('--native')),[],{encoding:'utf8',maxBuffer:8*1024*1024});assert.equal(run.status,0,run.stderr);const actual=JSON.parse(run.stdout);
const snapshot=m=>{const d=m.draft,complete=m.phase!==PHASE.BAND_DRAFT;return [complete?d.order.length:d.idx,complete?0:d.turnDeadline/1000,+complete,d.order.slice(),
 m.order.map(p=>[p.playerId,d.picks[p.playerId]??'',d.focus.get(p.playerId)??'',p.lp??0,d.skipsLeft[p.playerId]])];};
for(let scene=0;scene<30;scene++){
 const count=scene%3===0?1:scene%3===1?4:8,players=Array.from({length:count},(_,i)=>({playerId:`p${i}`,seat:9-i})),order=players.slice().sort((a,b)=>a.seat-b.seat),rng=createRng(scene+1);
 const ids=['band_bldsk','a','b','c'],hp=[40,20,30,50];let now=10000;
 const timers=[];const m={phase:PHASE.INFO_CHECK,isSolo:scene%4===0,soloUntimed:scene%5===0||scene%4===0,order,players:new Map(players.map(p=>[p.playerId,p])),rngDraft:rng,_turnToken:0,
  gd:{bandDraft:{skipsPerPlayer:scene%2+1,timeoutBandId:'band_bldsk'},bandAllowed:id=>ids.includes(id),bandIds:()=>ids,startLp:id=>hp[ids.indexOf(id)]},
  sched:{now:()=>now},scaled:t=>t,markPublic(){},markPrivate(){},setDeadline(){},cancel(h){if(h)h.cancelled=true;},later(ms,fn){const h={at:now+ms,fn};timers.push(h);return h;}};
 for(const k of Object.getOwnPropertyNames(MatchPhases.prototype))if(k!=='constructor')m[k]=MatchPhases.prototype[k];
 m.enterBattleCheck=()=>{m.phase=PHASE.BATTLE_CHECK;};m.enterBandDraft();
 const flush=()=>{for(let i=0;i<timers.length;i++){const t=timers[i];if(!t.cancelled&&!t.done&&t.at<=now){t.done=true;t.fn();}}};
 const expected=[rng.state(),snapshot(m),[]];
 for(let step=0;step<16;step++){
  let ok=true;const current=m.players.get(m.draftTurn())??{playerId:''};
  switch(step){
   case 0:for(const p of players)m.bandFocus(p,'b');break;
   case 1:ok=!!m.skipBand(current).ok;break;
   case 2:ok=!!m.pickBand(players[0],'a').ok;break;
   case 3:ok=!!m.pickBand(current,'band_bldsk').ok;break;
   case 4:ok=!!m.pickBand(current,'b').ok;break;
   case 5:{const p=players[count-1];ok=!m.draft.picks[p.playerId];if(ok)m._applyBand(p,m.defaultBand(p.playerId));break;}
   case 6:now=100000;flush();break;
   case 7:ok=!!m.bandFocus(current,'invalid').ok;break;
   case 8:ok=!!m.bandFocus(current,null).ok;break;
   case 9:ok=!!m.skipBand(current).ok;break;
   case 10:now=130000;flush();break;
   case 11:ok=!!m.pickBand(current,'band_bldsk').ok;break;
   case 12:now=160000;flush();break;
   case 13:now=1000000;flush();break;
   case 14:m.finishBandDraft(false);break;
   case 15:ok=!!m.pickBand(players[0],'a').ok;break;
  }
  flush();expected[2].push([+ok,snapshot(m)]);
 }
 assert.deepEqual(actual[scene],expected,`scene ${scene}`);
}
console.log('30 strategy drafts / 480 commands and deadline transitions matched JS');
