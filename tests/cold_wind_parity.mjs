import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const args=process.argv.slice(2),opt=k=>args[args.indexOf(k)+1],root=resolve(opt('--reference'));
const { Battle }=await import(pathToFileURL(resolve(root,'server/sim/Battle.js')));
const { startColdWind }=await import(pathToFileURL(resolve(root,'server/sim/content/devices.js')));
const run=spawnSync(resolve(opt('--native')),[],{encoding:'utf8',maxBuffer:8*1024*1024});assert.equal(run.status,0,run.stderr);const actual=JSON.parse(run.stdout);
for(let scene=0;scene<6;scene++) {
 let handle;
 const b=new Battle({content:'none',autoFinish:false,timeLimit:60,rect:{r0:0,r1:18,c0:0,c1:20},stage:{id:'wind',rows:Array(19).fill('r'.repeat(21))},
  data:{enemies:{enemy:{stats:{maxHp:10000,res:40,moveSpeed:0},applyWay:'NONE'},immune:{stats:{maxHp:10000,res:40,moveSpeed:0},applyWay:'NONE',immunities:{frozen:true}}}},
  players:[{playerId:'one',coords:'field',units:[],bonds:{kjeragShip:{layers:2}}},{playerId:'two',coords:'field',units:[]}],
  spawns:Array.from({length:4},(_,i)=>({time:0,enemyKey:i===2?'immune':'enemy',ownerPlayerId:i===1?'two':'one',route:{start:[9,10],end:[9,0],checkpoints:i===3?[{type:'DISAPPEAR'},{type:'WAIT',time:2},{type:'APPEAR',pos:[9,10]}]:[]}})),
  setup(b){handle=startColdWind(b,{playerId:'one',interval:scene===5?0.001:0.47,duration:()=>(scene===4?-1:0.2)+0.3*b.players[0].bonds.kjeragShip.layers,first:scene===0?0:scene===1?0.13:-0.1,ownerOnly:scene===2});}});
 for(let tick=0;tick<300;tick++) {
  if(tick===1)b.applyStatus(b.enemies[0],'resist',{duration:3});
  if(tick===50)b.players[0].bonds.kjeragShip.layers=5;
  if(tick===110&&scene===3)handle.cancel();
  if(tick===170)b.players[0].bonds.kjeragShip.layers=0;
  b.step();
  const expected=[handle.gusts,...b.units.flatMap(u=>['cold','freeze'].map(k=>u.buffs.find(x=>x.status===k)?.timeLeft??0).concat([u.s.aspd,u.s.res,+u.hidden]))];
  assert.equal(actual[scene][tick].length,expected.length);
  expected.forEach((v,i)=>assert(Math.abs(actual[scene][tick][i]-v)<=1e-9*Math.max(1,Math.abs(v)),`scene ${scene} tick ${tick}.${i}: native ${actual[scene][tick][i]}, JS ${v}`));
 }
 assert.equal(b.errorCount,0,JSON.stringify(b.errors));
}
console.log('6 cold-wind scenarios / 1800 frames matched JS');
