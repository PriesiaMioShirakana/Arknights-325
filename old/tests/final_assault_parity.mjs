// 执行原最终攻势控制方法；只替换传输、计时调度与战场执行，数值规则和结算仍由原代码运行。
import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
const args=process.argv.slice(2),opt=k=>args[args.indexOf(k)+1],root=resolve(opt('--reference'));
const {bossPoolShareOf,GameData}=await import(pathToFileURL(resolve(root,'server/match/gamedata.js')));
const {bossPoolHp,hiddenEligible,pairPlayers}=await import(pathToFileURL(resolve(root,'server/match/finalAssault.js')));
const {MatchBoss}=await import(pathToFileURL(resolve(root,'server/match/match/bossRounds.js')));
const run=spawnSync(resolve(opt('--native')),[],{encoding:'utf8',maxBuffer:8*1024*1024});assert.equal(run.status,0,run.stderr);const actual=JSON.parse(run.stdout);
function equal(a,b,path){
 if(Array.isArray(b)){assert.equal(a.length,b.length,path);b.forEach((v,i)=>equal(a[i],v,`${path}.${i}`));}
 else if(typeof b==='number')assert(Number.isFinite(a)&&Math.abs(a-b)<=1e-10*Math.max(1,Math.abs(b)),`${path}: native ${a}, JS ${b}`);
 else assert.equal(a,b,path);
}
for(let scene=0;scene<96;scene++){
 const global={solo:.25,coop:1.5,aliveFull:4,perPlayer:scene%3!==0,aliveScaling:scene%4===0};
 const mode={solo:scene%5?undefined:.75,coop:scene%7?undefined:-1,aliveFull:scene%8?undefined:7.8,perPlayer:scene%9?undefined:false};
 const solo=scene%6===0,experimental=scene%2===0,count=scene%24,share=bossPoolShareOf(mode,global,solo,scene%11?count:undefined);
 const clock=new GameData({config:{combatTimeScale:scene%3?2:1.5,bossOvertimeDrainPerSec:scene%4?1:.5}},'test');
 const gd={boss:()=>({bloodPoint:{NORMAL:1000.5}}),difficulty:'NORMAL',isSolo:solo,bossPoolShare:()=>share,bossHpMul:()=>1.1,
  hiddenRound:scene%5?15:null,hiddenCore:{single:350,multi:1200,minTeamLpExclusive:1,difficulties:scene%7?['NORMAL']:[]}};
 equal(actual[0][scene],[share,bossPoolHp(gd,'test',count,{experimental}),+hiddenEligible(gd,{layerSum:scene%2?1200:2401,teamLp:scene%3?2:1,aliveCount:count,experimental}),
  [0,240,300,301.999,302,304,999.5].map(t=>clock.bossOvertimeDue(t))],`numeric/${scene}`);
}
for(let scene=0;scene<48;scene++){
 const count=[1,2,3,4,8][scene%5],order=Array.from({length:count},(_,i)=>({playerId:`p${i}`,seat:10-i,lp:11+3*i,alive:true,pendingFunds:0,
  name:`p${i}`,stats:{bossDamage:0,dmgDealt:0,kills:0,fundsGained:0,healing:0,perfectRounds:0},dirty(){},battleInput(){return {};},
  bounties:[{card:{coin:999,payout:'perfect'},roundsLeft:1},{card:{coin:7},roundsLeft:2}]}));
 const sorted=order.slice().sort((a,b)=>a.seat-b.seat),clock=new GameData({config:{}},'test');
 const gd={isSolo:scene%7===0,difficulty:'NORMAL',hiddenRound:scene%3?15:null,hiddenCore:{single:350,multi:1200,minTeamLpExclusive:1,difficulties:scene%11?['NORMAL']:[]},
  boss:id=>({bloodPoint:{NORMAL:(id==='final'?1000:100)/(scene%7!==0&&count>4?count/4:1)}}),bossPoolShare:()=>1,enemy:()=>null,wave:()=>null,
  enemyScale:()=>({}),bossLevelTime:()=>120,combatTimeScale:2,bossOvertimeAfterReal:150,bossOvertimeDue:t=>clock.bossOvertimeDue(t)};
 let hits=[],steps=new Map(),finishResult=null;
 const m={gd,isSolo:gd.isSolo,capacityExperiment:true,order,players:new Map(order.map(p=>[p.playerId,p])),alivePlayers:()=>order,
  bossId:'final',hiddenBossId:'hidden',hiddenLayerSum:count*(scene%4?1300:300),round:14,seed:123,stageId:'test',gameSpeed:2,clientCombat:true,
  bossWaves:pairPlayers(order).map(g=>({players:g.map(p=>p.playerId),wave:{spawns:[],routes:[],actions:[],templateId:'test'}})),
  sched:{now:()=>0,instant:true},dispatch(){},_sanitizeSpawns:x=>x,_beginDamage(){},_flushDamage(){},markPublic(){},
  tickerFor(_kind,_names,e){hits.push([e.playerId,Number(e.param)]);steps.set(e.playerId,[.2,.5,.8].indexOf(Number(e.param))+1);},
  _freezeDamage(){},_stopClientCombat(){},_collectSimErrors(){},_charDamageTickers(){},scaled:t=>t,later(_t,fn){fn();},tickerText(){},
  startRound(){this.wantHidden=true;},finish(r){finishResult=r;}};
 for(const k of Object.getOwnPropertyNames(MatchBoss.prototype))if(k!=='constructor')m[k]=MatchBoss.prototype[k];
 // 禁止原方法启动宿主计时器或网络；保留共享池 onHit、LP 分配、悬赏与最终结果计算。
 m._startFinalClient=()=>{m._finalEnding=null;m.fields.forEach(f=>f.cc=true);};m._endFinal=reason=>{if(!m._finalEnding)m._finalEnding=reason;};
 m._bossLazyPublic=()=>false;m._stopClientCombat=()=>{};m._charDamageTickers=()=>{};
 const canHidden=()=>!m.isHidden&&m._finalEnding==='cleared'&&hiddenEligible(gd,{layerSum:m.hiddenLayerSum,teamLp:m.teamLp,aliveCount:count,experimental:true});
 const snap=()=>{const h=hits;hits=[];return [m._finalEnding==='cleared'?1:m._finalEnding?2:0,+!!m.isHidden,m.teamLp,m.bossPool.hp,m.overtimeApplied,+canHidden(),
  sorted.map(p=>[p.playerId,p.lp,m.bossPool.byPlayer.get(p.playerId)||0,p.stats.bossDamage,steps.get(p.playerId)||0]),h];};
 const damage=n=>{m._checkFinalEnd();if(!m._finalEnding)m.bossPool.damage('p0',n);m._checkFinalEnd();};
 const loss=n=>{m._checkFinalEnd();if(!m._finalEnding)m._teamLpLoss(n);m._checkFinalEnd();};
 const advance=t=>{m._checkFinalEnd();if(!m._finalEnding)m._applyOvertime(t);m._checkFinalEnd();};
 const ledger=()=>order.map(p=>[p.lp,+p.alive,p.pendingFunds,p.stats.fundsGained,p.stats.kills,p.stats.dmgDealt,p.stats.bossDamage,p.stats.healing,p.stats.perfectRounds,p.bounties.map(b=>b.roundsLeft)]);
 const result={perPlayer:Object.fromEntries(order.map((p,i)=>[p.playerId,{coins:3.9+i,killed:2+i,damageDealt:100+i,healingDone:50,perfect:true}]))};
 m.startFinalAssault(false);
 const expected=[pairPlayers(order).map(g=>[g[0].playerId,g[1]?.playerId??'',g.length,+(gd.isSolo||g.length===1)]),[snap()]];
 for(let step=0;step<10;step++){
  switch(step){case 0:damage(199.25);break;case 1:damage(.75);break;case 2:damage(301);break;case 3:loss(scene%6?1.25:10000);break;
   case 4:damage(300);break;case 5:advance(301.999);break;case 6:advance(302);break;case 7:advance(304);break;case 8:advance(304);break;case 9:damage(199);loss(10000);break;}
  expected[1].push(snap());
 }
 m._finishFinal(false,()=>result);expected.push(ledger());const hidden=[];
 if(m.wantHidden){
  hits=[];steps=new Map();m.isHidden=true;m.round=15;m.startFinalAssault(true);hidden.push(snap());damage(30);hidden.push(snap());
  if(scene%2)loss(10000);else damage(70);hidden.push(snap());m._finishFinal(true,()=>result);
 }
 expected.push(hidden,ledger(),[+finishResult.victory,+!!m.hiddenReached,+!!finishResult.hiddenCleared]);
 equal(actual[1][scene],expected,`fight/${scene}`);
}
console.log('96 boss rule configurations / 48 final-assault and hidden-core lifecycle scenarios matched JS');
