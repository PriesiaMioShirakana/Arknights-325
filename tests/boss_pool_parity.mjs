import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const args=process.argv.slice(2),opt=k=>args[args.indexOf(k)+1],root=resolve(opt('--reference'));
const { Battle }=await import(pathToFileURL(resolve(root,'server/sim/Battle.js')));
const { SharedBossPool }=await import(pathToFileURL(resolve(root,'server/match/finalAssault.js')));
const run=spawnSync(resolve(opt('--native')),[],{encoding:'utf8',maxBuffer:8*1024*1024});assert.equal(run.status,0,run.stderr);const actual=JSON.parse(run.stdout);
function effects(b,tick,field){
 const boss=b.units[1],source=b.units[0];if(!b.started||b.finished||!boss?.alive)return;
 if(tick===1)b.dealDamage(source,boss,{amount:100+10*field,type:'true'});
 if(tick===2)b.addBuff(boss,{key:'hp',mods:{hpPct:0.5}});
 if(tick===3)assert.equal(b.heal(source,boss,10000),0);
 if(tick===4)b.dealDamage(source,boss,{amount:299999.01,type:'true'});
 if(tick===5){b.addBuff(boss,{key:'shield',shield:100});b.dealDamage(source,boss,{amount:60,type:'true'});}
 if(tick===6)b.loseHp(boss,10,{source});
 if(tick===7)b.dealDamage(boss,boss,{amount:50,type:'true'});
 if(tick===8)b.dealDamage(source,boss,{amount:60,type:'true',sourceless:true});
 if(tick===15){b.loseHp(boss,299999.01,{source});b.dealDamage(source,b.units[2],{amount:300000,type:'true'});}
}
function equal(a,b,path){if(Array.isArray(b)){assert.equal(a.length,b.length,path);b.forEach((v,i)=>equal(a[i],v,`${path}.${i}`));}else assert(Math.abs(a-b)<=1e-9*Math.max(1,Math.abs(b)),`${path}: native ${a}, JS ${b}`);}
for(let scene=0;scene<5;scene++){
 const pool=new SharedBossPool(3000);if(scene===3)pool.damage('outside',3000);
 const fields=['one','two'].map(player=>new Battle({content:'none',kind:'boss',timeLimit:scene===2?1:60,sharedBoss:pool,
  rect:{r0:0,r1:18,c0:0,c1:20},stage:{id:'pool',rows:Array(19).fill('r'.repeat(21))},
  data:{chess:{ally:{chessId:'ally',stats:{maxHp:1000,blockCnt:0}}},enemies:Object.fromEntries(['boss','part','minion'].map((k,i)=>[k,{rank:i!==2?'BOSS':'NORMAL',stats:{maxHp:100000,moveSpeed:scene===1?1:0,hpRecoveryPerSec:100,lpr:5},applyWay:'NONE'}]))},
  players:[{playerId:player,coords:'field',units:[{uid:1,chessId:'ally',row:9,col:5}]}],
  spawns:Array.from({length:scene===1?1:3},(_,i)=>({time:0,enemyKey:['boss','part','minion'][i],ownerPlayerId:player,tag:i===0?'boss':i===1?'part':undefined,
   route:{start:[9,8+i],end:[9,scene===1?8:0]}})),setup(b){b.allyUnits[0].profile={noAttack:true};}}));
 for(let tick=0;tick<90;tick++){
  if(tick===10)pool.damage('outside',100);
  if(tick===50)pool.damage('outside',pool.hp-0.5);
  if(tick===20&&scene===4)fields[1].forceEnd();
  fields.forEach((b,i)=>{effects(b,tick,i);b.step();});
  const expected=[pool.hp,...['one','two','outside'].map(p=>pool.byPlayer.get(p)??0),...fields.map((b,i)=>{
   const boss=b.units[1],pp=b._perPlayer[i===0?'one':'two'];
   return [b.time,['running','cleared','timeout','forced'].indexOf(b.finished?b.reason:'running'),b.finished?b.result().bossHpLeft:pool.hp,
    boss.hp,boss.s.maxHp,+boss.alive,boss.stats.taken,pp.bossDamage,pp.damageDealt,pp.leaked.reduce((n,e)=>n+e.lpr,0),b.total,b.killed,b.leakedCount,b.units[0].stats.kills];
  })];
  equal(actual[scene][tick],expected,`scene ${scene} tick ${tick}`);
 }
 for(const b of fields)assert.equal(b.errorCount,0,JSON.stringify(b.errors));
}
console.log('5 shared-pool scenarios / 900 field frames matched JS');
