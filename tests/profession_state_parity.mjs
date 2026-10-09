import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const args=process.argv.slice(2),opt=k=>args[args.indexOf(k)+1],root=resolve(opt('--reference'));
const { Battle }=await import(pathToFileURL(resolve(root,'server/sim/Battle.js')));
const { SkillRuntime }=await import(pathToFileURL(resolve(root,'server/sim/skills.js')));
const run=spawnSync(resolve(opt('--native')),[],{encoding:'utf8',maxBuffer:16*1024*1024});assert.equal(run.status,0,run.stderr);const actual=JSON.parse(run.stdout);
for(let scene=0;scene<6;scene++)for(let ranged=0;ranged<2;ranged++){
 const sub=['hunter','funnel','mystic','phalanx','bearer','stalker'][scene];
 const b=new Battle({content:'none',autoFinish:false,timeLimit:60,rect:{r0:0,r1:18,c0:0,c1:20},stage:{id:'prof',rows:Array(19).fill('r'.repeat(21))},
  data:{chess:{source:{chessId:'source',profession:'WARRIOR',subProfessionId:sub,
   trait:{bb:{value:3,atk_scale:1.3,init_atk_scale:0.3,delta_atk_scale:0.2,max_atk_scale:1.1,times:4,def:2.5,magic_resistance:25,prob:0.4}},
   stats:{maxHp:1000,atk:100,def:20,res:5,bat:0.2,blockCnt:2,respawnTime:0.5},rangeGrid:[[0,0],[0,1],[0,2]]}},
   enemies:{enemy:{stats:{maxHp:1000000,def:10,moveSpeed:0},applyWay:'NONE'}}},players:[{playerId:'one',coords:'field',units:[{uid:1,chessId:'source',row:9,col:5}]}],
  spawns:Array.from({length:2},(_,i)=>({time:2,enemyKey:'enemy',ownerPlayerId:'one',route:{start:[9,5.4+i*1.2],end:[9,0]}})),
  setup(b){const u=b.allyUnits[0];Object.assign(u.profile,{dmgType:'phys',attack:ranged?'ranged':'melee',projectile:ranged?'bolt':'none',allInRange:false,hits:2,maxTargets:scene===2?2:1,canHitFly:true});
   u.skill=new SkillRuntime(b,u,{skillType:'AUTO'},{kind:'duration',trigger:'NEVER',spCost:2,initSp:2,duration:0.7});}});
 b.start();const u=b.allyUnits[0];
 for(let tick=0;tick<540;tick++){
  if(tick===20||tick===220)b.applyStatus(u,'disarm',{duration:0.6});
  if(tick===100||tick===150)u.skill.activate('manual',{free:true});
  if(tick===120)b.applyStatus(b.enemies[0],'untargetable',{duration:1});
  if(tick===140)b.moveRedeploy(u,10,5);
  if(tick===200)b.moveRedeploy(u,9,5);
  if(tick===280)b.forceAttack(u);
  if(tick===300)b.loseHp(u,10000);
  if(tick===400)b.applyStatus(u,'stun',{duration:0.5});
  b.step();const t=u.trait,s=u.s;
  const expected=[u.hp,u.stats.dmg,u.stats.attacks,t.ammo??0,t.reloadAcc??0,t.funnelScale??0,t.stored??0,s.def,s.res,s.blockCnt,s.taunt,s.dodgePhys,s.dodgeArts,u.blocking.length];
  expected.forEach((v,i)=>assert(Number.isFinite(v)&&Math.abs(actual[scene*2+ranged][tick][i]-v)<=1e-9*Math.max(1,Math.abs(v)),
   `${sub} ranged ${ranged} tick ${tick}.${i}: native ${actual[scene*2+ranged][tick][i]}, JS ${v}`));
 }
 assert.equal(b.errorCount,0,JSON.stringify(b.errors));
}
console.log('12 profession state scenarios / 6480 frames matched JS');
