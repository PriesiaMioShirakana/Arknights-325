import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const args=process.argv.slice(2),opt=k=>args[args.indexOf(k)+1],root=resolve(opt('--reference'));
const { Battle }=await import(pathToFileURL(resolve(root,'server/sim/Battle.js')));
const { install }=await import(pathToFileURL(resolve(root,'server/sim/content/devices.js')));
const run=spawnSync(resolve(opt('--native')),['trace'],{encoding:'utf8',maxBuffer:8*1024*1024});
assert.equal(run.status,0,run.stderr);const actual=JSON.parse(run.stdout);
for(let scene=0;scene<6;scene++) {
 const route={start:[9,10],end:[9,3]};
 const b=new Battle({content:'none',autoFinish:false,timeLimit:60,rect:{r0:0,r1:18,c0:0,c1:20},
  stage:{id:'spawn',rows:Array.from({length:19},(_,r)=>(scene>=4&&r!==9?'#':'r').repeat(21)),devices:scene===2||scene>=4?[{key:scene>=4?'trap_1105_accrate':'crate',row:9,col:9,role:'crate',stats:{maxHp:80}}]:[]},
  data:{chess:{ally:{chessId:'ally',stats:{maxHp:500,blockCnt:1,respawnTime:1,cost:3}}},enemies:{enemy:{stats:{maxHp:1000,atk:30,bat:0.5,moveSpeed:1.5},applyWay:scene>=4?'NONE':'MELEE'}}},
  players:[{playerId:'one',coords:'field',units:[{uid:1,chessId:'ally',row:9,col:5}]}],spawns:[{time:0,enemyKey:'enemy',ownerPlayerId:'one',route}],
  setup(b){b.allyUnits[0].profile={noAttack:true};install(b);}
 });
 const tokenDef={stats:{maxHp:200,atk:40,bat:0.5,blockCnt:1},rangeGrid:[[0,0],[0,1]]};
 let token=null,device=null;
 for(let tick=0;tick<240;tick++) {
  if(tick===10&&scene<4)token=b.spawnToken(b.allyUnits[0],'token',9,8,{def:tokenDef,duration:scene===0?2:0,kit:{profile:{}}});
  if(tick===15&&scene===1)device=b.spawnDevice('crate',9,9,{hp:80,obstacle:true});
  if(scene===3) {
   if(tick===15)assert(b.relocate(b.allyUnits[0],9,6));
   if(tick===18)b.allyUnits[0].downAtHome=true;
   if(tick===60)assert(b.moveRedeploy(b.allyUnits[0],9,7,{clearSp:true}));
   if(tick===100)assert(b.relocate(b.allyUnits[0],9,8));
   if(tick===110)b.loseHp(b.allyUnits[0],10000);
  }
  if(tick===20)b.loseHp(b.allyUnits[0],10000);
  if(tick===25)assert.equal(b.spawnToken(b.allyUnits[0],'token',9,5,{def:tokenDef}),null);
  if(tick===35)b.spawnEnemy('enemy',{ownerPlayerId:'one',route,countInTotal:false});
  if(tick===90&&scene!==0&&scene<4)b.retreat(token,{permanent:true});
  if(tick===25&&scene===5)b.displace(b.enemies[0],{x:1,y:0},2);
  if(tick===100&&device)b.loseHp(device,10000);
  b.step();const expected=b.units.map(u=>[u.hp,+u.alive,+!!u.removed,u.x,u.y,u.blockedBy?.id??0,u.body?.[1]??-1,u.body?.[0]??-1,u.deploySeq]);
  assert.equal(actual[scene][tick].length,expected.length,`scene ${scene} tick ${tick}: unit count`);
  expected.forEach((u,i)=>u.forEach((v,j)=>assert(Math.abs(actual[scene][tick][i][j]-v)<=1e-9,`scene ${scene} tick ${tick} unit ${i} field ${j}: native ${actual[scene][tick][i][j]}, JS ${v}`)));
 }
 assert.equal(b.errorCount,0,JSON.stringify(b.errors));
}
console.log('6 spawn/removal/device/relocation/crate scenarios / 1440 frames matched JS');
