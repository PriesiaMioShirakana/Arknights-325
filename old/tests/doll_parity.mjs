import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const args=process.argv.slice(2),opt=k=>args[args.indexOf(k)+1],root=resolve(opt('--reference'));
const { Battle }=await import(pathToFileURL(resolve(root,'server/sim/Battle.js')));
const { SkillRuntime }=await import(pathToFileURL(resolve(root,'server/sim/skills.js')));
const run=spawnSync(resolve(opt('--native')),[],{encoding:'utf8',maxBuffer:8*1024*1024});assert.equal(run.status,0,run.stderr);const actual=JSON.parse(run.stdout);
for(let scene=0;scene<8;scene++){
 const b=new Battle({content:'none',autoFinish:false,timeLimit:60,rect:{r0:0,r1:18,c0:0,c1:20},stage:{id:'doll',rows:Array(19).fill('r'.repeat(21))},
  data:{chess:{source:{chessId:'source',profession:'SPECIAL',subProfessionId:'dollkeeper',stats:{maxHp:1000,atk:100,bat:0.5,blockCnt:2,respawnTime:1,cost:0},
   rangeGrid:[[0,0],[0,1]],tokens:scene===3?['token_doll']:[]}},tokens:{token_doll:{stats:{maxHp:1600}}},
   enemies:{enemy:{stats:{maxHp:1000000,moveSpeed:0},applyWay:'NONE'}}},
  players:[{playerId:'one',coords:'field',units:[{uid:1,chessId:'source',row:9,col:5}]}],
  spawns:[{time:0,enemyKey:'enemy',ownerPlayerId:'one',route:{start:[9,5.4],end:[9,0]}}],
  setup(b){const u=b.allyUnits[0];u.profile.dollNoAttack=scene===2;
   u.skill=new SkillRuntime(b,u,{skillType:'MANUAL'},{kind:scene===4?'passive':'duration',spCost:10,initSp:10,duration:50,trigger:'NEVER',mods:{atkFlat:7}});
   if(scene===5)b.on('fatal',ctx=>{if(b.time<0.2)ctx.prevented=true;});}});
 b.start();const u=b.allyUnits[0];u.skill.activate('manual',{free:true});b.addBuff(u,{key:'external',mods:{atkFlat:13}});
 for(let tick=0;tick<750;tick++){
  if(tick===1)b.applyStatus(u,'slow',{duration:10});
  if(tick===2){if(scene===1)b.emit('dollSwitch',{unit:u});else b.loseHp(u,10000);}
  if(tick===10||tick===640)b.loseHp(u,10000);
  if(tick===15&&scene===1)b.removeBuff(u,'trait:dollSwitching');
  if(tick===20)b.applyStatus(u,'freeze',{duration:0.3,force:true});
  if(tick===21)b.applyStatus(u,'stun',{duration:0.3,force:true});
  if(tick===22)b.applyStatus(u,'sleep',{duration:0.3,force:true});
  if(tick===23)b.heal(u,u,200,{self:true});
  if(tick===50&&scene===6)b.loseHp(u,10000);
  if(tick===60&&scene===1)b.removeBuff(u,'trait:substitute');
  if(tick===80)b.moveRedeploy(u,10,5);
  if(tick===100)b.moveRedeploy(u,9,5);
  if(tick===150&&scene===7)b.retreat(u);
  if(tick===160&&scene===7)b.redeploy(u,{free:true});
  b.step();const expected=[u.hp,u.s.maxHp,u.s.atk,u.s.blockCnt,+!!u.trait.doll,+!!u.trait.dollSwitching,+u.alive,+u.skill.active,u.skill.spTotal,u.stats.attacks,b._perPlayer.one.deaths,
   ...['stun','slow','noHeal','healFree','isolated','disarm','noSp','invulnerable'].map(k=>+(k==='slow'?!!u.findBuff('slow'):!!u.s.flags[k]))];
  expected.forEach((v,i)=>assert(Number.isFinite(v)&&Math.abs(actual[scene][tick][i]-v)<=1e-9*Math.max(1,Math.abs(v)),
   `scene ${scene} tick ${tick}.${i}: native ${actual[scene][tick][i]}, JS ${v}`));
 }
 assert.equal(b.errorCount,0,JSON.stringify(b.errors));
}
console.log('8 dollkeeper scenarios / 6000 frames matched JS');
