// 原项目 SkillRuntime + Battle 作为独立行为预言机；覆盖完整充能、攻击、死亡／复活轨迹。
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const args=process.argv.slice(2), opt=k=>args[args.indexOf(k)+1], root=resolve(opt('--reference'));
const { Battle }=await import(pathToFileURL(resolve(root,'server/sim/Battle.js')));
const { SkillRuntime }=await import(pathToFileURL(resolve(root,'server/sim/skills.js')));
const run=spawnSync(resolve(opt('--native')),['trace'],{encoding:'utf8',timeout:15000,maxBuffer:8*1024*1024});
assert.equal(run.status,0,run.stderr); const actual=JSON.parse(run.stdout);
function spec(i) {
 const s={kind:'duration',spCost:2,initSp:2,duration:0.7,ammo:3,attack:{atkScale:2},mods:{atkFlat:5}};
 if(i===1)Object.assign(s,{kind:'ammo',duration:0,spType:'attack'});
 if(i===2)Object.assign(s,{kind:'instant',spType:'attack'});
 if(i===3)Object.assign(s,{kind:'charges',charges:3,initSp:6,trigger:'SP_FULL'});
 if(i===4)s.kind='passive';
 if(i===5)s.kind='toggle';
 if(i===6)Object.assign(s,{kind:'instant',spType:'hurt',trigger:'TAKE_DAMAGE'});
 if(i>=7&&i<=9)Object.assign(s,{trigger:{rule:['CUSTOM_RANGE','SKILL_RANGE','ACTIVE_RANGE'][i-7],grid:[[0,3]]},targeting:{rangeGrid:[[0,3]]}});
 if(i===10)s.trigger='SEARCH';
 if(i===11)s.trigger='GDGLOW_SKILL_2';
 if(i>=7&&i<=11)s.attack.noAttack=false;
 if(i===12)Object.assign(s,{kind:'instant',heal:true,trigger:{rule:'DEFAULT',allies:true,hpAtMost:0.7},attack:{atkScale:2,dmgType:'heal',heal:{mode:'single'}}});
 if(i===13)Object.assign(s,{spCost:0,initSp:0,trigger:'SP_FULL'});
 if(i===14)return null;
 if(i===15)Object.assign(s,{kind:'ammo',duration:0.2});
 if(i===16)Object.assign(s,{kind:'instant',spType:'attack',attack:{atkScale:2,attack:'ranged',projectile:'test',projectileSpeed:2,dmgType:'arts'}});
 if(i===17)Object.assign(s,{kind:'charges',charges:3,trigger:'NEVER'});
 return s;
}
function compare(a,b,path) {
 assert.equal(a.length,b.length,path);
 for(let i=0;i<b.length;i++) {
  if(b[i]===null)assert.equal(a[i],null,`${path}.${i}`);
  else assert(Number.isFinite(a[i]) && Math.abs(a[i]-b[i])<=1e-9*Math.max(1,Math.abs(b[i])),`${path}.${i}: native ${a[i]}, JS ${b[i]}`);
 }
}
for(let caseIndex=0;caseIndex<18;caseIndex++) {
 const enemyColumn=caseIndex>=7&&caseIndex<=9||caseIndex===11?8:6;
 const b=new Battle({content:'none',autoFinish:false,timeLimit:60,
  rect:{r0:0,r1:18,c0:0,c1:20},stage:{id:'skill',rows:Array(19).fill('r'.repeat(21))},
  data:{chess:{ally:{chessId:'ally',stats:{maxHp:1000,atk:10,def:0,bat:0.2,blockCnt:1,respawnTime:1},rangeGrid:[[0,0],[0,1]]}},
   enemies:{enemy:{stats:{maxHp:1000000,atk:0,def:0,moveSpeed:0},applyWay:'MELEE'}}},
  players:[{playerId:'one',coords:'field',units:[{uid:1,chessId:'ally',row:9,col:5}]}],
  spawns:[{time:0,enemyKey:'enemy',ownerPlayerId:'one',route:{start:[9,enemyColumn],end:[9,0]}}],
  setup(b) {
   const u=b.allyUnits[0]; u.profile={dmgType:'phys',attack:'melee',maxTargets:1,noAttack:caseIndex>=7&&caseIndex<=11};
   if(caseIndex===12)Object.assign(u.profile,{dmgType:'heal',heal:{mode:'single'}});
   u.skill=new SkillRuntime(b,u,{skillType:caseIndex===3?'MANUAL':'AUTO'},spec(caseIndex));
   b.on('enemySpawn',({enemy})=>{enemy.profile={noAttack:true};});
  }
 });
 b.start(); const u=b.allyUnits[0],sk=u.skill;
 for(let tick=0;tick<420;tick++) {
  const enemy=b.enemies[0];
  if(tick===1&&caseIndex===8)b.applyStatus(enemy,'untargetable',{duration:10});
  if(tick%17===1)b.dealDamage(enemy,u,{amount:20,type:'true'});
  if(tick===80)b.applyStatus(u,'noSp',{duration:1});
  if(tick===100)b.applyStatus(u,'stun',{duration:1});
  if(tick===140)b.applyStatus(u,'silence',{duration:0.5});
  if(tick===180)sk.activate();
  if(tick===220)sk.setSpTotal(5);
  if(tick===250)sk.spCostMul=0.5;
  if(tick===280)sk.addAmmo(2);
  if(tick===290)sk.extend(0.4);
  if(tick===300)sk.stop();
  if(tick===330)b.loseHp(u,100000);
  b.step();
  const row=[sk.sp,sk.charges,+sk.active,+sk.pending,Number.isFinite(sk.timeLeft)?sk.timeLeft:null,sk.ammoLeft,sk.ammoMax,sk.activations,u.s.atk,b.enemies[0].hp,u.hp,u.stats.attacks,sk.spTotal,Number.isFinite(sk.opReadyAt)?sk.opReadyAt:null];
  compare(actual[caseIndex][tick],row,`case ${caseIndex} frame ${tick}`);
 }
 assert.equal(b.errorCount,0,JSON.stringify(b.errors));
}
console.log('18 skill scenarios / 7560 frames matched JS');

