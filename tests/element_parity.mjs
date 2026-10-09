// 五类元素分别命中我方和首领，验证锁定、移除、自然结束及无来源爆发倍率。
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const args=process.argv.slice(2), opt=k=>args[args.indexOf(k)+1], root=resolve(opt('--reference'));
const { Battle }=await import(pathToFileURL(resolve(root,'server/sim/Battle.js')));
const { SkillRuntime }=await import(pathToFileURL(resolve(root,'server/sim/skills.js')));
const { elementView }=await import(pathToFileURL(resolve(root,'server/sim/damage.js')));
const elements=['neural','erosion','burn','apoptosis','necrosis'];
const run=spawnSync(resolve(opt('--native')),[],{encoding:'utf8',timeout:15000,maxBuffer:8*1024*1024});
assert.equal(run.status,0,run.stderr);const actual=JSON.parse(run.stdout);
for(let caseIndex=0;caseIndex<10;caseIndex++) {
 const b=new Battle({content:'none',autoFinish:false,timeLimit:60,
  rect:{r0:0,r1:18,c0:0,c1:20},stage:{id:'elements',rows:Array(19).fill('r'.repeat(21))},
  data:{chess:{ally:{chessId:'ally',stats:{maxHp:100000,atk:100,def:300,res:30,blockCnt:1},rangeGrid:[[0,0],[0,1]]}},
   enemies:{enemy:{rank:'BOSS',epResistance:10,epDamageResistance:20,stats:{maxHp:100000,atk:200,def:500,res:30,moveSpeed:0},applyWay:'MELEE'}}},
  players:[{playerId:'one',coords:'field',units:[{uid:1,chessId:'ally',row:9,col:5}]}],
  spawns:[{time:0,enemyKey:'enemy',ownerPlayerId:'one',route:{start:[9,10],end:[9,0]}}],
  setup(b){ const u=b.allyUnits[0];u.profile={noAttack:true};u.def={...u.def,epResistance:10,epDamageResistance:20};
   u.skill=new SkillRuntime(b,u,{}, {kind:'charges',spCost:10,initSp:25,charges:3,trigger:'NEVER'});
   b.on('enemySpawn',({enemy})=>{enemy.profile={noAttack:true};});
  }
 });
 b.step(); const target=caseIndex<5?b.allyUnits[0]:b.enemies[0],source=caseIndex<5?b.enemies[0]:b.allyUnits[0],element=elements[caseIndex%5];
 b.addBuff(source,{key:'source-multiplier',mods:{dmgDealtMul:5}});
 for(let tick=0;tick<600;tick++) {
  if(tick===0)b.dealDamage(source,target,{type:'element',element,amount:400});
  if(tick===1)b.reduceElement(target,20);
  if(tick===2||tick===61)b.dealDamage(source,target,{type:'element',element,amount:3000});
  if(tick===10||tick===62)b.dealDamage(source,target,{type:'element',element:'burn',amount:2000});
  if(tick===20)b.reduceElement(target,10000);
  if(tick===60)b.removeBuff(target,`${element}Burst`);
  b.step(); const v=elementView(target,b.time);
  const row=[target.hp,target.s.atk,target.s.def,target.s.res,target.skill?.spTotal??0,...elements.map(k=>target.elem[k]),+!!target.s.flags.stun,target.findBuff('palsy')?.stacks??0,+!!target.s.flags.noSp,+!!target.s.flags.burstLock,v?elements.indexOf(v[0]):-1,v?.[1]??0,v?.[2]??0,v?.[3]??0];
  assert.equal(actual[caseIndex][tick].length,row.length);
  row.forEach((v,i)=>assert(Number.isFinite(actual[caseIndex][tick][i]) && Math.abs(actual[caseIndex][tick][i]-v)<=1e-9*Math.max(1,Math.abs(v)),`case ${caseIndex} frame ${tick}.${i}: native ${actual[caseIndex][tick][i]}, JS ${v}`));
 }
 assert.equal(b.errorCount,0,JSON.stringify(b.errors));
}
console.log('10 element burst scenarios / 6000 frames matched JS');
