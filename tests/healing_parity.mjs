import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const args=process.argv.slice(2),opt=k=>args[args.indexOf(k)+1],root=resolve(opt('--reference'));
const { Battle }=await import(pathToFileURL(resolve(root,'server/sim/Battle.js')));
const run=spawnSync(resolve(opt('--native')),[],{encoding:'utf8',maxBuffer:8*1024*1024});assert.equal(run.status,0,run.stderr);const actual=JSON.parse(run.stdout);
for(let scene=0;scene<6;scene++){
 const b=new Battle({content:'none',autoFinish:false,timeLimit:60,rect:{r0:0,r1:18,c0:0,c1:20},stage:{id:'heal',rows:Array(19).fill('r'.repeat(21))},
  data:{chess:{source:{chessId:'source',stats:{maxHp:1000,blockCnt:0}},target:{chessId:'target',stats:{maxHp:1000,blockCnt:0,hpRecoveryPerSec:7.5}}}},
  players:[{playerId:'one',coords:'field',units:[{uid:1,chessId:'source',row:9,col:5},{uid:2,chessId:'target',row:9,col:6}]}],
  setup(b){b.allyUnits[0].profile={noAttack:true,healThrough:()=>scene===3};b.allyUnits[1].profile={noAttack:true,noHeal:scene===5};b.on('heal',c=>{c.amount*=0.5;});}});
 b.start();const [source,target]=b.allyUnits;b.loseHp(target,500);
 b.addBuff(source,{key:'dealt',mods:{healingDealtMul:2}});b.addBuff(target,{key:'taken',mods:{healingTakenMul:0.3}});
 for(let tick=0;tick<180;tick++){
  if(tick===20)b.applyStatus(target,'healFree',{duration:2});
  if(tick===30)b.applyStatus(target,'noHeal',{duration:1});
  if(tick===100||tick===150)b.dealDamage(null,target,{amount:300,type:'true'});
  let healed=0;
  if(tick%13===0)healed=b.heal(source,target,200,{self:scene===2,regen:scene===1,ignoreHealFree:scene===4,overheal:true,overhealDuration:tick===169?0:0.8});
  b.step();const expected=[healed,target.hp,target.buffs.reduce((s,x)=>s+x.shield,0),source.stats.heal,target.stats.heal];
  expected.forEach((v,i)=>assert(Math.abs(actual[scene][tick][i]-v)<=1e-9*Math.max(1,Math.abs(v)),`scene ${scene} tick ${tick}.${i}: native ${actual[scene][tick][i]}, JS ${v}`));
 }
 assert.equal(b.errorCount,0,JSON.stringify(b.errors));
}
console.log('6 healing / regeneration / overheal scenarios / 1080 frames matched JS');
