// 测试调用真实 devices.install；C++ 不依赖 JavaScript 执行游戏规则。
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const args=process.argv.slice(2),opt=k=>args[args.indexOf(k)+1],root=resolve(opt('--reference'));
const { Battle }=await import(pathToFileURL(resolve(root,'server/sim/Battle.js')));
const { install }=await import(pathToFileURL(resolve(root,'server/sim/content/devices.js')));
const { SkillRuntime }=await import(pathToFileURL(resolve(root,'server/sim/skills.js')));
const run=spawnSync(resolve(opt('--native')),[],{encoding:'utf8',maxBuffer:16*1024*1024});
assert.equal(run.status,0,run.stderr);const actual=JSON.parse(run.stdout);
for(let scene=0;scene<15;scene++) {
 const wind=scene>=12,flying=scene<12&&scene%3===2,dir=scene===13?'LEFT':scene===14?'UP':'RIGHT';
 const rows=Array(19).fill(Array.from({length:21},(_,c)=>!wind&&c>=13?'mgdi'[Math.floor(scene/3)]:'r').join(''));
 const devices=wind?[{key:'blower',role:'blower',row:0,col:0,active:true,dir:'RIGHT',rangeTiles:Array.from({length:19*13},(_,i)=>[Math.floor(i/13),4+i%13]),skill:{bb:{
  'blower_s_character[equal].atk':0.3,'blower_s_character[opposite].atk':-0.2,'blower_s_character[vertical].atk':0.1,
  'blower_s_enemy[equal].move_speed':0.4,'blower_s_enemy[opposite].move_speed':-0.3}}}]:[];
 const b=new Battle({content:'none',autoFinish:false,timeLimit:60,rect:{r0:0,r1:18,c0:0,c1:20},stage:{id:'terrain',rows,devices,special:{infection:{bb:{duration:3}}}},
 data:{chess:{ally:{chessId:'ally',stats:{maxHp:10000,atk:100,blockCnt:0},rangeGrid:[[0,0],[0,1]]}},enemies:{enemy:{stats:{maxHp:10000,atk:100,moveSpeed:0.8,massLevel:scene%3===1?3:1},motion:flying?'FLY':'WALK',applyWay:'NONE'}}},
 players:[{playerId:'one',coords:'field',units:[{uid:1,chessId:'ally',row:5,col:15,dir}]}],
 spawns:[{time:0,enemyKey:'enemy',ownerPlayerId:'one',route:{start:[9,scene===13?4:16],end:[9,scene===13?20:0]}}],
 setup(b){b.allyUnits[0].profile={noAttack:true};b.allyUnits[0].skill=new SkillRuntime(b,b.allyUnits[0],{}, {kind:'instant',spType:'hurt',spCost:100,trigger:'NEVER'});install(b);}
 });
 for(let tick=0;tick<600;tick++) {
  const ally=b.allyUnits[0];
  if(tick===120)b.relocate(ally,5,2);
  if(tick===240)b.moveRedeploy(ally,6,15);
  if(tick===180)b.applyStatus(b.enemies[0],'stun',{duration:2});
  b.step();
  const row=[ally,b.enemies[0]].flatMap(u=>[u.hp,u.s.atk,u.s.aspd,u.s.moveSpeed,u.x,u.y,+!!u.s.flags.stealth,u.skill?.sp??0]);
  row.forEach((v,i)=>assert(Number.isFinite(actual[scene][tick][i])&&Math.abs(actual[scene][tick][i]-v)<=1e-9*Math.max(1,Math.abs(v)),`scene ${scene} frame ${tick}.${i}: native ${actual[scene][tick][i]}, JS ${v}`));
 }
 assert.equal(b.errorCount,0,JSON.stringify(b.errors));
}
console.log('15 terrain/airflow scenarios / 9000 frames matched JS');
