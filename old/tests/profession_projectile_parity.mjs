import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const args=process.argv.slice(2),opt=k=>args[args.indexOf(k)+1],root=resolve(opt('--reference'));
const { Battle }=await import(pathToFileURL(resolve(root,'server/sim/Battle.js')));
const run=spawnSync(resolve(opt('--native')),[],{encoding:'utf8',maxBuffer:4*1024*1024});assert.equal(run.status,0,run.stderr);const actual=JSON.parse(run.stdout);
for(let scene=0;scene<3;scene++){
 const sub=['loopshooter','bombarder','fortress'][scene],rangeGrid=[];
 for(let r=-1;r<=1;r++)for(let c=0;c<=6;c++)rangeGrid.push([r,c]);
 const b=new Battle({content:'none',autoFinish:false,timeLimit:60,rect:{r0:0,r1:18,c0:0,c1:20},stage:{id:'projectile',rows:Array(19).fill('r'.repeat(21))},
  data:{chess:{source:{chessId:'source',profession:'SNIPER',subProfessionId:sub,trait:{bb:{'attack@append_atk_scale':0.4,'attack@times':4}},
   stats:{maxHp:1000,atk:100,bat:0.2,blockCnt:2,respawnTime:0.5,cost:0},rangeGrid}},
   enemies:Object.fromEntries(Array.from({length:3},(_,i)=>['e'+i,{stats:{maxHp:1000000,def:10,moveSpeed:0},applyWay:'NONE',motion:i===2?'FLY':'WALK'}]))},
  players:[{playerId:'one',coords:'field',units:[{uid:1,chessId:'source',row:9,col:5}]}],
  spawns:Array.from({length:3},(_,i)=>({time:0,enemyKey:'e'+i,ownerPlayerId:'one',route:{start:[9,(scene===2?5.4:9)+i*0.7],end:[9,0]}})),
  setup(b){b.allyUnits[0].profile.maxTargets=scene===0?2:1;}});
 b.step();const u=b.allyUnits[0];
 for(let tick=0;tick<300;tick++){
  if(tick===2)b.loseHp(b.units[1],10000000);
  if(tick===8||tick===20)b.moveRedeploy(u,tick===8?10:9,5);
  if(tick===40||tick===80)b.addBuff(u,{key:'block',mods:{blockCnt:tick===40?-2:0}});
  if(tick===60)b.retreat(u);
  if(tick===100){b.forceAttack(u);b.forceAttack(u);}
  if(tick===140)b.addBuff(u,{key:'atk',mods:{atkPct:1}});
  if(tick===145)b.loseHp(u,10000);
  b.step();const expected=[u.stats.dmg,u.stats.attacks,u.trait.boomerangsOut??0,+u.alive,b.units[2].hp,b.units[3].hp];
  expected.forEach((v,i)=>assert(Math.abs(actual[scene][tick][i]-v)<=1e-9*Math.max(1,Math.abs(v)),
   `${sub} tick ${tick}.${i}: native ${actual[scene][tick][i]}, JS ${v}`));
 }
 assert.equal(b.errorCount,0,JSON.stringify(b.errors));
}
console.log('3 projectile profession scenarios / 900 frames matched JS');
