import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const args=process.argv.slice(2),opt=k=>args[args.indexOf(k)+1],root=resolve(opt('--reference'));
const { Battle }=await import(pathToFileURL(resolve(root,'server/sim/Battle.js')));
const { SkillRuntime }=await import(pathToFileURL(resolve(root,'server/sim/skills.js')));
const run=spawnSync(resolve(opt('--native')),[],{encoding:'utf8',maxBuffer:16*1024*1024});assert.equal(run.status,0,run.stderr);const actual=JSON.parse(run.stdout);
for(let scene=0;scene<8;scene++){
 const sub=['musha','reaper','incantationmedic','charger','geek','merchant','librator','bard'][scene];
 const b=new Battle({content:'none',autoFinish:false,timeLimit:60,flags:{dpInit:9,dpPerSec:0.5},rect:{r0:0,r1:18,c0:0,c1:20},stage:{id:'prof',rows:Array(19).fill('r'.repeat(21))},
  data:{chess:{source:{chessId:'source',profession:'WARRIOR',subProfessionId:sub,
   trait:{bb:{value:35,scale:0.25,cost:sub==='charger'?2:4,hp_ratio:0.4,interval:0.7,atk:2,max_stack_cnt:4,init_atk:0.5,'attack@atk_to_hp_recovery_ratio':0.2}},
   stats:{maxHp:1000,atk:100,bat:0.5,blockCnt:2,respawnTime:1,cost:0},rangeGrid:[[0,0],[0,1],[0,2]]},
   patient:{chessId:'patient',stats:{maxHp:1000,blockCnt:0}}},enemies:{enemy:{stats:{maxHp:1000000,def:10,moveSpeed:0},applyWay:'NONE'}}},
  players:[{playerId:'one',coords:'field',units:[{uid:1,chessId:'source',row:9,col:5},{uid:2,chessId:'patient',row:9,col:6}]}],
  spawns:Array.from({length:2},(_,i)=>({time:0,enemyKey:'enemy',ownerPlayerId:'one',route:{start:[9,5.4+i*1.2],end:[9,0]}})),
  setup(b){const u=b.allyUnits[0];Object.assign(u.profile,{dmgType:'phys',attack:'melee',projectile:'none',hits:2,canHitFly:true});
   b.allyUnits[1].profile={noAttack:true,noHeal:scene===7};
   u.skill=new SkillRuntime(b,u,{skillType:'AUTO'},{kind:'duration',trigger:'NEVER',spCost:2,initSp:2,duration:1.2});
   b.on('merchantPay',c=>{if(b.time<3)c.cancel=true;else if(b.time<6)c.cost*=0.4;});
   b.on('bardRegen',c=>{if(c.target.id===2)c.value*=1.5;});}});
 b.step();const [u,patient]=b.allyUnits;b.loseHp(u,600);b.loseHp(patient,500);
 for(let tick=0;tick<450;tick++){
  const target=b.units[3];
  if(tick%17===0){b.loseHp(u,30);b.loseHp(patient,20);}
  if(tick%25===0)for(let i=0;i<3;i++)b.dealDamage(u,target,{amount:50,type:'arts',tags:tick%100===0?['dot']:[],traitAlly:tick%100===50?patient:null});
  if(tick===90)b.dealDamage(u,target,{amount:50,type:'element',element:'burn'});
  if(tick===92)b.loseHp(target,25,{source:u});
  if(tick===93)b.dealDamage(u,target,{amount:50,type:'elemental'});
  if(tick===100)b.loseHp(b.units[2],10000000,{source:u});
  if(tick===110||tick===190)u.skill.activate('manual',{free:true});
  if(tick===120){b.applyStatus(u,'healFree',{duration:0.8});b.applyStatus(patient,'healFree',{duration:0.8});}
  if(tick===150)b.applyStatus(u,'stun',{duration:0.6});
  if(tick===180)b.moveRedeploy(u,10,5);
  if(tick===230)b.moveRedeploy(u,9,5);
  if(tick===270)b.loseHp(u,10000);
  b.step();const s=u.s;
  const expected=[u.hp,patient.hp,u.stats.dmg,u.stats.heal,b.getPlayer('one').dp,u.trait.ramp??0,s.atk,s.blockCnt,+u.alive,+u.skill.active,b._perPlayer.one.deaths,s.hpRegen,patient.s.hpRegen];
  expected.forEach((v,i)=>assert(Number.isFinite(v)&&Math.abs(actual[scene][tick][i]-v)<=1e-9*Math.max(1,Math.abs(v)),
   `${sub} tick ${tick}.${i}: native ${actual[scene][tick][i]}, JS ${v}`));
 }
 assert.equal(b.errorCount,0,JSON.stringify(b.errors));
}
console.log('8 profession effect scenarios / 3600 frames matched JS');
