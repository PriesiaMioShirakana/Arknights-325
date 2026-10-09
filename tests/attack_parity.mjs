import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const args=process.argv.slice(2),opt=k=>args[args.indexOf(k)+1],root=resolve(opt('--reference'));
const { Battle }=await import(pathToFileURL(resolve(root,'server/sim/Battle.js')));
const run=spawnSync(resolve(opt('--native')),['trace'],{encoding:'utf8',maxBuffer:8*1024*1024});
assert.equal(run.status,0,run.stderr);const actual=JSON.parse(run.stdout);
for(let ci=0;ci<6;ci++) {
 const profile={dmgType:'phys',attack:ci===2?'ranged':'melee',projectile:ci===2?'bomb':'none',maxTargets:1};
 if(ci===0||ci===2)Object.assign(profile,{splashRadius:1.1,splashScale:0.7});
 if(ci===0)profile.hits=2;
 if(ci===1)profile.chain={count:4,sluggish:0.2};
 if(ci===3)profile.allInRange=true;
 if(ci===4)Object.assign(profile,{dmgType:'heal',heal:{mode:'chain',count:4}});
 if(ci===5)Object.assign(profile,{hits:3,hitDmgMul:0.5});
 const units=[{uid:1,chessId:'ally',row:9,col:5}];
 if(ci===4)for(let i=0;i<3;i++)units.push({uid:i+2,chessId:'ally',row:10,col:5+i});
 const b=new Battle({content:'none',autoFinish:false,timeLimit:60,
  rect:{r0:0,r1:18,c0:0,c1:20},stage:{id:'attacks',rows:Array(19).fill('r'.repeat(21))},
  data:{chess:{ally:{chessId:'ally',stats:{maxHp:1000,atk:100,bat:0.5,blockCnt:1},rangeGrid:[[0,0],[1,0],[0,1],[0,2],[0,3]]}},enemies:{enemy:{stats:{maxHp:10000,def:30,moveSpeed:0},applyWay:'MELEE'}}},
  players:[{playerId:'one',coords:'field',units}],
  spawns:Array.from({length:4},(_,i)=>({time:0,enemyKey:'enemy',ownerPlayerId:'one',route:{start:[9,6+i],end:[9,0]}})),
  setup(b){b.allyUnits.forEach((u,i)=>{u.profile=i?{noAttack:true}:profile;});b.on('enemySpawn',({enemy})=>{enemy.profile={noAttack:true};});}
 });
 b.step();if(ci===4)b.allyUnits.forEach((u,i)=>b.loseHp(u,100*(i+1)));
 for(let tick=0;tick<120;tick++) {
  if(ci===2&&tick===0)b.loseHp(b.enemies[0],100000);
  if(tick===20)b.forceAttack(b.allyUnits[0]);
  b.step();const expected=b.units.map(u=>[u.hp,u.stats.dmg,u.stats.heal,u.stats.attacks]);
  assert.equal(actual[ci][tick].length,expected.length);
  expected.forEach((u,i)=>u.forEach((v,j)=>assert(Math.abs(actual[ci][tick][i][j]-v)<=1e-9*Math.max(1,Math.abs(v)),`case ${ci} tick ${tick} unit ${i} field ${j}: native ${actual[ci][tick][i][j]}, JS ${v}`)));
 }
 assert.equal(b.errorCount,0,JSON.stringify(b.errors));
}
console.log('6 attack scenarios / 720 frames matched JS');
