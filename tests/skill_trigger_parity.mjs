import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const args=process.argv.slice(2),opt=k=>args[args.indexOf(k)+1],root=resolve(opt('--reference'));
const { Battle }=await import(pathToFileURL(resolve(root,'server/sim/Battle.js')));
const { SkillRuntime }=await import(pathToFileURL(resolve(root,'server/sim/skills.js')));
const run=spawnSync(resolve(opt('--native')),[],{encoding:'utf8',maxBuffer:8*1024*1024});assert.equal(run.status,0,run.stderr);const actual=JSON.parse(run.stdout);
for(let scene=0;scene<8;scene++){
 const nearby=scene===5||scene===6;
 let remove=()=>{},source=scene===0||scene===4||scene===7,canFly=scene!==2;
 const register=b=>b.allyUnits[0].skill.addTriggerRange(()=>source?[b.allyUnits[1]]:[{keys:[9*21+11],profile:{canHitFly:canFly}}]);
 const b=new Battle({content:'none',autoFinish:false,timeLimit:60,rect:{r0:0,r1:18,c0:0,c1:20},stage:{id:'skill-trigger',rows:Array(19).fill('r'.repeat(21))},
  data:{chess:{owner:{chessId:'owner',stats:{maxHp:1000,atk:1,bat:0.3,blockCnt:0},rangeGrid:[[0,0],[0,1]]},helper:{chessId:'helper',stats:{maxHp:1000,blockCnt:0},rangeGrid:[[0,0],[0,1]]}},
   enemies:{enemy:{stats:{maxHp:100000,moveSpeed:0,motion:scene===1||scene===2||scene===6?'FLY':'WALK'},applyWay:'NONE'}}},
  players:[{playerId:'one',coords:'field',units:[{uid:1,chessId:'owner',row:9,col:5},{uid:2,chessId:'helper',row:9,col:10}]}],
  spawns:[{time:0,enemyKey:'enemy',ownerPlayerId:'one',route:{start:[9,nearby?6:11],end:[9,0]}}],
  setup(b){
   const u=b.allyUnits[0];u.profile={dmgType:'phys',attack:'melee',maxTargets:1,noAttackUnlessSkill:scene===5||scene===7};b.allyUnits[1].profile={noAttack:true};
   u.skill=new SkillRuntime(b,u,{skillType:'AUTO'},{kind:'duration',spCost:1,initSp:1,duration:0.4,heal:scene===3});
   if(!nearby)remove=register(b);if(scene===6)u.skill.setTrigger('CUSTOM_RANGE');
  }});
 for(let tick=0;tick<300;tick++){
  const u=b.allyUnits[0],helper=b.allyUnits[1];
  if(tick===20)b.relocate(helper,9,8);
  if(tick===25)b.addBuff(helper,{key:'extend',persist:true,mods:{rangeExtend:2}});
  if(tick===40)remove();
  if(tick===60&&!nearby)remove=register(b);
  if(tick===80)u.skill.setTrigger('ACTIVE_RANGE',[[0,6]]);
  if(tick===110)b.retreat(helper,{permanent:true});
  if(tick===120)u.skill.setTrigger('SKILL_RANGE',[[0,6]]);
  if(tick===140)b.applyStatus(b.enemies[0],'untargetable',{duration:1});
  if(tick===170)u.skill.setTrigger('DEFAULT');
  if(tick===190){source=false;canFly=true;}
  if(tick===220)b.applyStatus(b.enemies[0],'sleep',{duration:1});
  if(tick===260)u.skill.setTrigger('SP_FULL');
  b.step();const s=u.skill,expected=[s.activations,s.sp,s.charges,+s.active,s.timeLeft,u.stats.attacks,b.enemies[0].hp];
  expected.forEach((v,i)=>assert(Math.abs(actual[scene][tick][i]-v)<=1e-9*Math.max(1,Math.abs(v)),`scene ${scene} tick ${tick}.${i}: native ${actual[scene][tick][i]}, JS ${v}`));
 }
 assert.equal(b.errorCount,0,JSON.stringify(b.errors));
}
console.log('8 dynamic skill-trigger scenarios / 2400 frames matched JS');
