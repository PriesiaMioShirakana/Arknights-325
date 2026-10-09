// 原 MatchSpDraft 接收固定卡片，保留占用、实际 item 奖励发放与超时随机选择。
import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
const args=process.argv.slice(2),opt=k=>args[args.indexOf(k)+1],root=resolve(opt('--reference'));
const {MatchSpDraft}=await import(pathToFileURL(resolve(root,'server/match/match/spDraft.js')));
const {PHASE}=await import(pathToFileURL(resolve(root,'shared/constants.js')));
const {createRng}=await import(pathToFileURL(resolve(root,'server/sim/rng.js')));
const run=spawnSync(resolve(opt('--native')),[],{encoding:'utf8',maxBuffer:8*1024*1024});assert.equal(run.status,0,run.stderr);const actual=JSON.parse(run.stdout);
for(let scene=0;scene<36;scene++){
 const count=scene%3===0?1:scene%3===1?4:8,acquired=[];
 const players=Array.from({length:count},(_,i)=>({playerId:`p${i}`,seat:9-i,alive:true,recompute(){},acquireItem(id){acquired.push([this.playerId,id]);}}));
 const sorted=players.slice().sort((a,b)=>a.seat-b.seat),rng=createRng(scene+31),order=sorted.map(p=>p.playerId);
 if(scene%4!==0)rng.shuffle(order);
 const cards=Array.from({length:scene%6+1},(_,i)=>({idx:i,kind:'item',id:`item_${i%2}`}));
 let now=10000,timer=null;
 const m={phase:PHASE.SP_DRAFT,players:new Map(players.map(p=>[p.playerId,p])),rngDraft:rng,_turnToken:0,sp:{cards,order,idx:0,picks:{},taken:{},untimed:scene%4===0||scene%5===0,turnDeadline:0},
  gd:{timer:k=>k==='spFirst'?30:16},dispatcher:{runKey:()=>false},dispatch(){},markPrivate(){},markPublic(){},cancel(){},later(_ms,fn){fn();},
  setDeadline(s,fn){this.deadline=s?now+s*1000:0;timer=s?{at:this.deadline,fn}:null;},enterPrep(){this.phase=PHASE.PREP;}};
 for(const k of Object.getOwnPropertyNames(MatchSpDraft.prototype))if(k!=='constructor')m[k]=MatchSpDraft.prototype[k];
 const advance=t=>{now=t*1000;if(timer&&now>=timer.at){const fn=timer.fn;timer=null;fn();}};
 const snapshot=()=>[rng.state(),m.sp.idx,m.phase===PHASE.SP_DRAFT?m.deadline/1000:0,+(m.phase!==PHASE.SP_DRAFT),order.slice(),
  sorted.map(p=>[p.playerId,+p.alive,m.sp.picks[p.playerId]??null]),cards.map(c=>m.sp.taken[c.idx]??null)];
 m.startSpTurn();const expected=[snapshot(),[]];
 for(let step=0;step<16;step++){
  let ok=true;const current=m.players.get(m.spTurn())??{playerId:''};
  const eliminate=i=>{const p=players[i];if(!p.alive)return false;p.alive=false;if(m.phase===PHASE.SP_DRAFT)m.startSpTurn();return true;};
  switch(step){
   case 0:ok=!!m.pickCard(players[0],0).ok;break;
   case 1:ok=!!m.pickCard(current,999).ok;break;
   case 2:ok=!!m.pickCard(current,0).ok;break;
   case 3:advance(42);break;
   case 4:ok=eliminate(count-1);break;
   case 5:ok=!!m.pickCard(current,1).ok;break;
   case 6:advance(100);break;
   case 7:ok=!!m.pickCard(current,0).ok;break;
   case 8:advance(116);break;
   case 9:advance(132);break;
   case 10:advance(148);break;
   case 11:ok=eliminate(0);break;
   case 12:advance(1000);break;
   case 13:ok=!!m.pickCard(current,2).ok;break;
   case 14:m.finishSpDraft();break;
   case 15:ok=!!m.pickCard(players[0],0).ok;break;
  }
  expected[1].push([+ok,snapshot()]);
 }
 assert.equal(acquired.length,Object.keys(m.sp.picks).length,'each slot grants its reward once');
 assert.deepEqual(actual[scene],expected,`special draft ${scene}`);
}
console.log('36 special drafts / 576 commands and timeout transitions matched JS');
