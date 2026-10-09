// 使用原 Match 的结算／救援方法；网络、提示文字和层数内容钩子在本测试中关闭。
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const args=process.argv.slice(2),opt=k=>args[args.indexOf(k)+1],root=resolve(opt('--reference'));
const {MatchSettle}=await import(pathToFileURL(resolve(root,'server/match/match/settle.js')));
const {MatchExtensions}=await import(pathToFileURL(resolve(root,'server/match/match/extensions.js')));
const {uniteSurvivors}=await import(pathToFileURL(resolve(root,'server/match/unite.js')));
const {PHASE}=await import(pathToFileURL(resolve(root,'shared/constants.js')));
const run=spawnSync(resolve(opt('--native')),[],{encoding:'utf8',maxBuffer:8*1024*1024});assert.equal(run.status,0,run.stderr);const actual=JSON.parse(run.stdout);
const snapshot=m=>[+m.revivalWindowOpen(),m.order.map(p=>[p.lp,+p.alive,+p.pendingDeath,+p.revived,p.eliminatedRound??null,p.pendingFunds,
 p.stats.lpLost,p.stats.leaks,p.stats.kills,p.stats.perfectRounds,p.stats.fundsGained,p.stats.dmgDealt,p.stats.healing,p.bounties.map(b=>b.roundsLeft)])];
for(let scene=0;scene<36;scene++){
 let now=100000;const order=[],normal=new Map();
 for(let i=0;i<4;i++){
  order.push({playerId:String(i),lp:i<2?2:i===2&&scene%5===0?10:30,alive:true,isBot:i===2&&scene%8===0,left:i===3&&scene%11===0,revived:i===0&&scene%9===0,pendingDeath:false,pendingFunds:0,
   bounties:[{card:{coin:5,payout:'perfect'},roundsLeft:1},{card:{coin:17},roundsLeft:2}],stats:{lpLost:0,leaks:0,kills:0,perfectRounds:0,fundsGained:0,dmgDealt:0,healing:0},
   dirty(){},recompute(){},eliminate(r){this.alive=false;this.pendingDeath=false;this.eliminatedRound=r;this.pendingFunds=0;this.bounties=[];}});
  normal.set(String(i),{killed:3+i,damageDealt:100+i,healingDone:10+i,perfect:i>=2&&!(scene%9===0&&i===2),coins:1.9+i,
   leaked:Array.from({length:i===0?12:i===1?5:0},()=>({counted:true})),synthetic:scene%10===0&&i===2});
 }
 const relay=scene%4===0,ran=scene%6!==0;
 const plan={helpers:relay?[order[2]]:[order[2],order[3]],leakers:[order[0],order[1]],notReentered:new Map([['0',1]]),...(relay?{uniteRound:1}:{})};
 const second={...plan,helpers:[order[3]],uniteRound:2};
 const first={reason:'timeout',synthetic:scene%7===0,perPlayer:{}},last={reason:'cleared',synthetic:scene%7===0,perPlayer:{}};
 for(let i=2;i<4;i++)(i===3&&relay?last:first).perPlayer[String(i)]={coins:5.8+i,killed:2,damageDealt:30,healingDone:4,leaked:[]};
 first.perPlayer['2'].leaked=Array.from({length:4},(_,i)=>({sourcePlayerId:i<3?'0':'1',counted:true}));
 if(relay)last.perPlayer['3'].leaked=Array.from({length:4},(_,i)=>({sourcePlayerId:i===0?'0':'1',counted:true}));
 const m={phase:ran?PHASE.UNITE:PHASE.COMBAT,round:1,disposed:false,ended:false,order,players:new Map(order.map(p=>[p.playerId,p])),lastResults:normal,
  gd:{lpCapPerRound:10,config:{broadcasts:[]}},teamLp:scene%13===0?100:null,revivalEnabled:scene%12!==0,battlePrefix:'case',
  sched:{now:()=>now},alivePlayers:()=>order.filter(p=>p.alive),fields:[{kind:'unite',live:false,players:['2','3']}],watchers:new Map(),unitePlan:plan,
  _freezeDamage(){},_stopClientCombat(){},_applyBattleLayerGains(){},dispatch(){},toast(){},tickerText(){},markPublic(){},
  setDeadline(seconds){this.deadline=now+seconds*1000;}};
 for(const proto of [MatchSettle.prototype,MatchExtensions.prototype])for(const k of Object.getOwnPropertyNames(proto))if(k!=='constructor')m[k]=proto[k];
 // 内容层不在此场景内；保留真正的结算和救援规则。
 m._charDamageTickers=()=>{};m._freezeDamage=()=>{};m._applyBattleLayerGains=()=>{};
 if(ran&&relay){
  const rounds=[{plan,result:first,survivors:uniteSurvivors(plan,first),view:{helpers:['2']}},{plan:second,result:last,survivors:uniteSurvivors(second,last),view:{helpers:['3']}}];
  const eligible=new Set();for(const stage of rounds){m.unitePlan=stage.plan;for(const id of m._revivalHelpers(stage.plan,stage.result))eligible.add(id);}
  m._uniteRelay={rounds,eligible};
 }
 const selected=ran?(relay?second:plan):null,result=ran?(relay?last:first):null;
 m.settle(selected,result);
 const expected=[order.map(p=>p.stats.lpLost),m.uniteResultView?.through??null,snapshot(m)];
 const revive=(donor,target,round,at)=>{now=at;return +!!m.revive(order[donor],{matchId:'case',round,playerId:String(target)}).ok;};
 const now2=scene%3===0?115000:100200;
 expected.push([revive(2,0,2,100100),revive(2,0,1,now2),revive(2,0,1,now2),revive(3,1,1,now2)]);
 expected.push(snapshot(m));now=115000;m._finalizePendingDeaths();expected.push(snapshot(m));
 const before=JSON.stringify(snapshot(m));m.settle(selected,result);assert.equal(JSON.stringify(snapshot(m)),before,'settle twice');
 expected.push(order.map(p=>p.pendingFunds));
 assert.deepEqual(actual[scene],expected,`scene ${scene}`);
}
console.log('36 round settlement / pending-funds / rescue-window scenarios matched JS');
