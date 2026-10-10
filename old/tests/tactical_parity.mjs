import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const args=process.argv.slice(2),opt=k=>args[args.indexOf(k)+1],root=resolve(opt('--reference'));
const { Battle }=await import(pathToFileURL(resolve(root,'server/sim/Battle.js')));
const run=spawnSync(resolve(opt('--native')),[],{encoding:'utf8',maxBuffer:8*1024*1024});assert.equal(run.status,0,run.stderr);const actual=JSON.parse(run.stdout);
for(let scene=0;scene<12;scene++){
 const mode=Math.floor(scene/4),rangeGrid=[];for(let r=-2;r<=2;r++)for(let c=-1;c<=3;c++)rangeGrid.push([r,c]);
 const routes=mode?[{start:[9,-0.5],end:[9,20],motion:'WALK',checkpoints:[{type:'MOVE',pos:[9,6]},{type:'WAIT',time:2}]}]:[];
 if(mode===2)routes.push({start:[6,0],end:[6,20],motion:'WALK',checkpoints:[{type:'APPEAR',pos:[7,10]}]});
 const b=new Battle({content:'none',autoFinish:false,timeLimit:60,rect:{r0:0,r1:18,c0:0,c1:20},stage:{id:'tactical',rows:Array(19).fill('r'.repeat(21))},routes,
  data:{chess:{source:{chessId:'source',profession:'PIONEER',subProfessionId:'tactician',attackKind:'ranged',projectile:'none',trait:{bb:{atk_scale:1.75}},
   stats:{maxHp:1000,atk:100,def:50,bat:0.5,blockCnt:0,cost:0},rangeGrid},waiting:{chessId:'waiting',stats:{maxHp:1,atk:0,cost:0},attackKind:'none'}},
   enemies:{enemy:{stats:{maxHp:100000,moveSpeed:0},applyWay:'NONE'}}},
  players:[{playerId:'one',coords:'field',units:[{uid:1,chessId:'source',row:8,col:10,dir:['UP','RIGHT','DOWN','LEFT'][scene%4]},{uid:2,chessId:'waiting',row:9,col:10}]}],
  setup(b){b.allyUnits[1].deferDeploy=true;}});
 b.start();const u=b.allyUnits[0];
 for(let tick=0;tick<180;tick++){
  if(tick===2){const t=u.trait.reinforcement;b.spawnEnemy('enemy',{ownerPlayerId:'one',route:{start:[t.y,t.x],end:[0,0]}});}
  if(tick===20)b.moveRedeploy(u,8,11);
  if(tick===30||tick===70)b.setObstacle(9,12,tick===30);
  if(tick===40)b.addBuff(u,{key:'long',mods:{rangeExtend:2,atkPct:1}});
  if(tick===50||tick===90){b.loseHp(u.trait.reinforcement,100000);b.moveRedeploy(u,8,tick===50?10:11);}
  b.step();const t=u.trait.reinforcement,next=b.findTacticalPoint(u),paths=b.groundPathTiles();
  const expected=[t.id,t.x,t.y,t.hp,t.s.atk,t.s.def,u.stats.dmg,next?.[1]??-1,next?.[0]??-1,Array.from({length:399},(_,i)=>paths.has(398-i)?'1':'0').join('')];
  expected.forEach((v,i)=>typeof v==='string'?assert.equal(actual[scene][tick][i],v,`scene ${scene} tick ${tick} road mask`):
   assert(Math.abs(actual[scene][tick][i]-v)<=1e-9*Math.max(1,Math.abs(v)),`scene ${scene} tick ${tick}.${i}: native ${actual[scene][tick][i]}, JS ${v}`));
 }
 assert.equal(b.errorCount,0,JSON.stringify(b.errors));
}
console.log('12 tactical point scenarios / 2160 frames matched JS');
