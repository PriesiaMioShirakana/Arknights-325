// 直接调用原版联防规划，比较选人两阶段排序、接力残留重建和继承数据。
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const args=process.argv.slice(2),opt=k=>args[args.indexOf(k)+1],root=resolve(opt('--reference'));
const {helperStats,helperOrder,planUnite,planUniteRelay,uniteSurvivors,uniteBattleOpts}=await import(pathToFileURL(resolve(root,'server/match/unite.js')));
const run=spawnSync(resolve(opt('--native')),[],{encoding:'utf8',maxBuffer:8*1024*1024});assert.equal(run.status,0,run.stderr);const actual=JSON.parse(run.stdout);
const mods=m=>m?[m.hpMul??1,m.bountyId??'',m.bountyCoins??0]:null;
const plan=p=>p?[p.helpers.map(p=>p.playerId),p.leakers.map(p=>p.playerId),[...p.notReentered],p.uniteRound??0,(p.relayCandidates??[]).map(p=>p.playerId),
 p.leaked.map(l=>[l.enemyKey,l.sourcePlayerId,mods(l.mods),+(l.tag==='bounty'),l.bounty?.coins??0,l.bounty?.ownerPlayerId??''])]:null;
for(let scene=0;scene<48;scene++){
 const results=new Map(),players=[];
 const cards=[{id:'kill',card:{enemyKey:'enemy_a',coin:17}},{id:'perfect',card:{enemyKey:'enemy_b',coin:30,payout:'perfect'}}];
 for(let i=0;i<(scene<4?4:10);i++){
  const r={playerId:String(i),perfect:i!==0&&i!==3,leaked:[],unitsEnd:scene%6?Array.from({length:5},(_,j)=>({uid:j+1,hpPct:j===0?0:0.7,sp:j*3,alive:(i+j+scene)%4!==0})):[],synthetic:scene%5===0&&i===7};
  if(i===0||i===3)for(let j=0;j<6;j++)r.leaked.push({enemyKey:j===4?'missing':j===1||j===2?'enemy_b':'enemy_a',mods:{hpMul:1.5,bountyId:j<2?'kill':j===2?'perfect':'',...(j===5?{bountyCoins:12}:{})},sourcePlayerId:'older',tag:'bounty',counted:j!==3});
  const ps={playerId:String(i),seat:9-i,alive:scene%7!==0||i<3,left:scene%9===0&&i===8,deployCount:3+(i+scene)%3,
   board:new Map(Array.from({length:4},(_,j)=>[j,{uid:j+1,kind:'chess'}])),bonds:{a:{active:(i+scene)%2===0,layers:(i*13+scene)%1000},b:{active:false,layers:50}},
   layers:{a:i===2?995:0},pendingLayerGains:{a:20.7},bounties:cards};players.push(ps);
  if(!(scene%11===0&&i===5))results.set(String(i),r);
 }
 const gd={unite:{maxHelpers:scene%4===0?1:2,templates:{}},factions:{templateSlots:{}},wave(){return null;},enemy:k=>['enemy_a','enemy_b'].includes(k)?{}:null};
 const m={isSolo:scene===0,capacityExperiment:true,_normalAliveCount:scene%13===0?7:10,gd,alivePlayers:()=>players.filter(p=>p.alive),players:new Map(players.map(p=>[p.playerId,p])),lastResults:results};
 const metrics=players.map(p=>{const s=helperStats(m,p,results);return [s.units,+s.active,s.layers,s.standing];});
 const ordered=helperOrder({...m,gd:{...gd,unite:{...gd.unite,maxHelpers:2}}},players,results).map(p=>p.playerId),p=planUnite(m,results);
 const residual={perPlayer:{helper:{leaked:[{enemyKey:'enemy_b',mods:{bountyId:'kill'},sourcePlayerId:'0',tag:'bounty',counted:true},{enemyKey:'missing',sourcePlayerId:'3',counted:true},{enemyKey:'enemy_a',sourcePlayerId:'0',counted:false}]}},
  unspawned:[{enemyKey:'enemy_a',time:9,sourcePlayerId:'0'},{enemyKey:'enemy_a',time:8,sourcePlayerId:'0'}],synthetic:scene%8===0};
 const field=[8,9].map((t,i)=>({time:t,enemyKey:'enemy_a',mods:{hpMul:2+i},sourcePlayerId:'0',bounty:{coins:t,ownerPlayerId:'0'}}));
 const relay=p?planUniteRelay(m,p,residual,field):null;
 const first=players[0];first.board=new Map(Array.from({length:5},(_,i)=>[i,{uid:i+1,kind:i>=3?'token':'chess'}]));
 first.battleInput=({carry})=>Array.from({length:5},(_,i)=>{const c=carry.get(i+1);return c?[c.hpPct??null,c.sp??null,+!!c.down]:null;});m.dispatch=()=>{};
 const carry=uniteBattleOpts(m,{helpers:[first],leaked:[]},60).players[0];
 assert.deepEqual(actual[scene],[metrics,ordered,plan(p),plan(relay),p?[...uniteSurvivors(p,residual)]:null,carry],`scene ${scene}`);
}
console.log('48 helper-selection / relay / LP-attribution / carry scenarios matched JS');
