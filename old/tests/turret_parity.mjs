import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const args=process.argv.slice(2),opt=k=>args[args.indexOf(k)+1],root=resolve(opt('--reference'));
const { Battle }=await import(pathToFileURL(resolve(root,'server/sim/Battle.js')));
const { install,activateTurrets }=await import(pathToFileURL(resolve(root,'server/sim/content/devices.js')));
const run=spawnSync(resolve(opt('--native')),[],{encoding:'utf8',maxBuffer:8*1024*1024});assert.equal(run.status,0,run.stderr);const actual=JSON.parse(run.stdout);
for(let scene=0;scene<3;scene++) {
 const rows=Array(19).fill('r'.repeat(21));if(scene===1)rows[9]='r'.repeat(7)+'#f'+'r'.repeat(12);
 const b=new Battle({content:'none',autoFinish:false,timeLimit:60,rect:{r0:scene===1?9:0,r1:scene===1?12:18,c0:0,c1:20},stage:{id:'turret',rows,
  devices:[{key:'trap_1104_aclasert',alias:'gun',role:'turret',row:scene===1?8:10,col:8,active:true,stats:{maxHp:3000,atk:900,def:200,bat:4,aspd:100},
   rangeTiles:Array.from({length:21},(_,i)=>[9,i]),skill:{desc:'持续2秒',bb:{attack_speed_per_stack:1,max_attack_speed:300,damage_scale_per_stack:0.001,max_damage_scale:1.3}}}]},
 data:{chess:{ally:{chessId:'ally',stats:{maxHp:10000,blockCnt:0}}},enemies:{enemy:{stats:{maxHp:100000,res:10,moveSpeed:0},motion:scene===2?'FLY':'WALK',applyWay:'NONE'}}},
 players:[{playerId:'one',coords:'field',bonds:{alpha:{layers:2},beta:{layers:1}},units:[{uid:1,chessId:'ally',row:11,col:5}]}],spawns:[{time:0,enemyKey:'enemy',ownerPlayerId:'one',route:{start:[9,10],end:[9,0]}}],
 setup(b){b.allyUnits[0].profile={noAttack:true};install(b);}});
 for(let tick=0;tick<360;tick++) {
  if(tick===20)assert.equal(activateTurrets(b)[0].id,2);
  if(tick===50)b.players[0].bonds.alpha.layers=400;
  if(tick===100)b.applyStatus(b.units[1],'stun',{duration:2});
  if(tick===160)b.players[0].bonds.alpha.layers=0;
  if(tick===230)b.players[0].bonds.beta.layers=30;
  b.step();const gun=b.units[1],enemy=b.units[2];
  const expected=[enemy.hp,enemy.s.dmgTakenMul-1,gun.s.aspd,gun.stats.attacks,gun.x,gun.y];
  expected.forEach((v,i)=>assert(Math.abs(actual[scene][tick][i]-v)<=1e-9*Math.max(1,Math.abs(v)),`scene ${scene} tick ${tick}.${i}: native ${actual[scene][tick][i]}, JS ${v}`));
 }
 assert.equal(b.errorCount,0,JSON.stringify(b.errors));
}
console.log('3 turret / bond-layer scenarios / 1080 frames matched JS');
