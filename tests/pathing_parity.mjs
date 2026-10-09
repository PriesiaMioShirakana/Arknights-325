import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { spawnSync } from 'node:child_process';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const args=process.argv.slice(2),opt=k=>args[args.indexOf(k)+1],root=resolve(opt('--reference'));
const { Grid }=await import(pathToFileURL(resolve(root,'server/sim/grid.js')));
const { Battle }=await import(pathToFileURL(resolve(root,'server/sim/Battle.js')));
const { remainingDistance }=await import(pathToFileURL(resolve(root,'server/sim/ai.js')));
const data=JSON.parse(readFileSync(resolve(root,'data/stages.json'),'utf8'));
const stages=(Array.isArray(data)?data:Object.values(data.stages??data)).filter(s=>Array.isArray(s.rows));
const displaced=args.includes('--displaced'),induced=args.includes('--induced')||displaced;
const cases=[],words=[stages.length],rect={r0:0,r1:18,c0:0,c1:20};
for(const stage of stages) {
 const g=new Grid(stage,rect);
 for(const t of g.tiles)words.push((t.pass==='ALL'?1:0)|(t.pass!=='NONE'?2:0)|(t.height==='LOW'?4:0)|({NONE:0,ALL:1,MELEE:2,RANGED:3}[t.build]<<3)|(t.special==='end'?32:0));
 const fallback=g.tiles.map((t,k)=>({t,k})).filter(x=>x.t.pass==='ALL');
 const start=g.specialTiles('start')[0]??[Math.floor(fallback.at(-1).k/21),fallback.at(-1).k%21];
 const end=g.specialTiles('end')[0]??[Math.floor(fallback[0].k/21),fallback[0].k%21];
 words.push(...start,...end);cases.push({stage,start,end});
}
const run=spawnSync(resolve(opt('--native')),displaced?['displaced']:induced?['induced']:[],{input:words.join(' '),encoding:'utf8',timeout:30000,maxBuffer:64*1024*1024});
assert.equal(run.status,0,run.stderr);const actual=JSON.parse(run.stdout);
for(const [scene,{stage,start,end}] of cases.entries()) {
 const b=new Battle({content:'none',autoFinish:false,timeLimit:60,rect,stage:{...stage,devices:[]},
  data:{enemies:{walk:{stats:{maxHp:1000,moveSpeed:4},motion:'WALK',applyWay:'NONE'},fly:{stats:{maxHp:1000,moveSpeed:4},motion:'FLY',applyWay:'NONE'}}},
  players:[{playerId:'one',units:[]}],
  spawns:Array.from({length:3},(_,i)=>({time:i*0.2,enemyKey:i===2?'fly':'walk',ownerPlayerId:'one',route:{start,end,checkpoints:i===1?[{type:'WAIT',time:0.7}]:[]}})),
 });
 for(let tick=0;tick<900;tick++) {
  if(tick===60||tick===120)b.grid.setObstacle(start[0],start[1]+1,tick===60);
  if(tick===80||tick===150)b.grid.setObstacle(end[0],end[1]-1,tick===80,'crate');
  if(induced) {
   const enemies=b.units.filter(u=>u.side==='enemy');
   if(tick===20)b.applyStatus(enemies[0],'attract',{duration:5,source:enemies[1],point:[9,10]});
   if(tick===30)b.applyStatus(enemies[0],'fear',{duration:3,source:enemies[1]});
   if(tick===70)b.applyStatus(enemies[0],'fear',{duration:2,source:enemies[0]});
   if(tick===180)b.applyStatus(enemies[1],'fear',{duration:3,source:enemies[0]});
   if(tick===200)b.applyStatus(enemies[2],'fear',{duration:3,source:enemies[0]});
  }
  if(displaced) {
   const e=b.enemies;
   if(tick===25||tick===35)b.push(e[0],5,{from:e[1],dir:{x:1,y:1}});
   if(tick===50)b.pull(e[1],1,{to:{x:6,y:7}});
   if(tick===90)b.displace(e[2],{x:-1,y:2},5);
   if(tick===100)b.pull(e[2],2,{to:{x:10,y:8},stop:1});
  }
  b.step();const expected=b.units.filter(u=>u.side==='enemy').map(u=>[u.x,u.y,+u.alive,remainingDistance(b,u)]);
  assert.equal(actual[scene][tick].length,expected.length);
  expected.forEach((u,i)=>u.forEach((v,j)=>assert(Math.abs(actual[scene][tick][i][j]-v)<=1e-9*Math.max(1,Math.abs(v)),`stage ${stage.id} tick ${tick} enemy ${i} field ${j}: native ${actual[scene][tick][i][j]}, JS ${v}`)));
 }
 assert.equal(b.errorCount,0,JSON.stringify(b.errors));
}
console.log(`${cases.length} stages / ${cases.length*900} battle path frames matched JS`);
