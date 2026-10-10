import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const args=process.argv.slice(2),opt=k=>args[args.indexOf(k)+1],root=resolve(opt('--reference'));
const { Battle }=await import(pathToFileURL(resolve(root,'server/sim/Battle.js')));
// 力度/质量差、偏轴修正、向内推、急停交点、悬浮/刚体/首领、临墙和阻挡共同覆盖。
const cases=[];
for(let i=0;i<1200;i++) {
 const mode=i%5,terrain=Math.floor(i/5)%3,flags=[0,1,3,4,8,16,32][Math.floor(i/15)%7],facing=i%4;
 const options=Math.floor(i/7)%32,mass=(i%7)*0.5,force=(Math.floor(i/7)%11)-3;
 const start=i%23===0?{x:5.4,y:9}:{x:8+(i%4)*0.37,y:7+(i%5)*0.6};
 const from=i%11===0?{...start}:{x:7,y:9},direction=i%13===0?{x:0,y:0}:{x:(i%3)-1,y:(Math.floor(i/3)%3)-1};
 cases.push({mode,terrain,flags,facing,options,mass,force,distance:[0.03,0.12,2.8,50][i%4],stop:[0,0.6708,1.5][i%3],start,from,direction,to:{x:6,y:10},center:{x:5,y:9}});
}
const words=[cases.length];
for(const c of cases)words.push(c.mode,c.terrain,c.flags,c.facing,c.options,c.mass,c.force,c.distance,c.stop,...['start','from','direction','to','center'].flatMap(k=>[c[k].x,c[k].y]));
const run=spawnSync(resolve(opt('--native')),[],{input:words.join(' '),encoding:'utf8',timeout:30000,maxBuffer:8*1024*1024});
assert.equal(run.status,0,run.stderr);const actual=JSON.parse(run.stdout);
for(const [i,c] of cases.entries()) {
 const rows=Array(19).fill(c.terrain===1?'r'.repeat(11)+'b'+'r'.repeat(9):'r'.repeat(21));
 const b=new Battle({content:'none',autoFinish:false,timeLimit:60,
  rect:c.terrain===2?{r0:5,r1:13,c0:4,c1:16}:{r0:0,r1:18,c0:0,c1:20},stage:{id:'displace',rows},
  data:{chess:{ally:{chessId:'ally',stats:{maxHp:1000,blockCnt:3}}},enemies:{enemy:{stats:{maxHp:1000,moveSpeed:0,massLevel:c.mass},motion:c.flags&1?'FLY':'WALK',applyWay:'NONE',staticBody:!!(c.flags&4)}}},
  players:[{playerId:'one',coords:'field',units:[{uid:1,chessId:'ally',row:9,col:5}]}],
  spawns:[{time:0,enemyKey:'enemy',ownerPlayerId:'one',route:{start:[c.start.y,c.start.x],end:[9,15]}}],
  setup(b){b.allyUnits[0].profile={noAttack:true};b.allyUnits[0].dir=['UP','RIGHT','DOWN','LEFT'][c.facing];}
 });
 b.step();const e=b.enemies[0];
 // 路由适配器会把入点取整；本组直接设置亚格坐标，再执行一次阻挡判断。
 e.x=c.start.x;e.y=c.start.y;
 if(c.flags&2){e.motion='WALK';e.s.flags.float=true;}
 e.isBoss=!!(c.flags&8);
 if(c.flags&16)e.s.flags.noDisplace=true;
 b._checkBlock(e);
 let moved=0;
 if(c.mode===0)moved=b.displace(e,c.direction,c.distance);
 if(c.mode===1)moved=b.push(e,c.force,{from:c.options&1?{...c.from,fwd:b.allyUnits[0].fwd}:null,dir:c.direction,fixed:!!(c.options&2),fixedAngle:!!(c.options&4),inward:!!(c.options&8),effect:!!(c.options&16)});
 if(c.mode===2)moved=b.pull(e,c.force,{to:c.to,center:c.options&1?c.center:null,stop:c.stop});
 if(c.mode===3)moved=b.pullToFront(e,b.allyUnits[0],c.force);
 if(c.mode===4)moved=b.pushDistance(e,c.force,{effect:!!(c.options&16)});
 [moved,e.x,e.y,+!!e.blockedBy].forEach((v,j)=>assert(Math.abs(actual[i][j]-v)<=1e-9,`case ${i} field ${j}: native ${actual[i][j]}, JS ${v}; ${JSON.stringify(c)}`));
 assert.equal(b.errorCount,0,JSON.stringify(b.errors));
}
console.log(`${cases.length} displacement cases matched JS`);
