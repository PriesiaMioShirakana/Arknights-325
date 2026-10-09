import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const args=process.argv.slice(2),opt=k=>args[args.indexOf(k)+1],root=resolve(opt('--reference'));
const { Battle }=await import(pathToFileURL(resolve(root,'server/sim/Battle.js')));
const { install }=await import(pathToFileURL(resolve(root,'server/sim/content/devices.js')));
const stages=JSON.parse(readFileSync(resolve(root,'data/stages.json'),'utf8'));
const rects=[{r0:9,r1:12,c0:2,c1:10},{r0:9,r1:12,c0:2,c1:18},{r0:1,r1:5,c0:2,c1:18}];
const run=spawnSync(resolve(opt('--native')),[],{encoding:'utf8',maxBuffer:16*1024*1024});assert.equal(run.status,0,run.stderr);const actual=JSON.parse(run.stdout);
assert.equal(actual.length,Object.keys(stages).length);
for(const [id,scenes] of actual) {
 const stage=stages[id];
 for(let scene=0;scene<12;scene++) {
  const rect=rects[Math.floor(scene/4)],mode=scene%4,global={},mine={};
  if(mode)for(const d of stage.devices){if(!d.alias)continue;global[d.alias]=mode!==2;if(mode===3)mine[d.alias]=false;}
  const platform=stage.devices.find(d=>['platform','mound'].includes(d.role)&&d.pos[0]>=rect.r0&&d.pos[0]<=rect.r1&&d.pos[1]>=rect.c0&&d.pos[1]<=rect.c1);
  const b=new Battle({content:'none',kind:scene>=8?'boss':'normal',autoFinish:false,timeLimit:60,rect,stage,deviceOverrides:global,
   data:{chess:{ally:{chessId:'ally',stats:{maxHp:1000,blockCnt:0}}}},
   players:[{playerId:'one',half:'L',coords:'field',deviceOverrides:mine,units:platform?[{uid:1,chessId:'ally',row:platform.pos[0],col:platform.pos[1]}]:[]},
    {playerId:'two',side:'R',colOffset:scene<8?8:0,coords:'field',units:[]}],setup(b){if(b.allyUnits[0])b.allyUnits[0].profile={noAttack:true};install(b);}});
  b.start();
  const flags=b.grid.tiles.map((t,k)=>(t.pass==='ALL'?1:0)|(t.pass!=='NONE'?2:0)|(t.height==='LOW'?4:0)|(['NONE','ALL','MELEE','RANGED'].indexOf(t.build)<<3)|
   (t.special==='end'?32:0)|(['','mire','smog','deepsea','infection'].indexOf(t.terrain??'')<<6)|(b.grid.obstacle[k]<<9));
  const units=b.units.map(u=>[u.defId,u.hp,+u.alive,+!!u.removed,u.x,u.y,+u.ground,u.deploySeq]);
  const moved=[];
  if(platform){const u=b.units[0];assert(b.relocate(u,u.tileR,u.tileC));moved.push(+u.ground);assert(b.moveRedeploy(u,u.tileR,u.tileC));moved.push(+u.ground);}
  assert.deepEqual(scenes[scene],[flags,units,moved],`${id} scene ${scene}`);
  assert.equal(b.errorCount,0,JSON.stringify(b.errors));
 }
}
console.log(`${actual.length} stages / ${actual.length*12} device override and deployment setups matched JS`);
