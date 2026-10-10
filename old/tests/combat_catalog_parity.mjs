import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const args=process.argv.slice(2),opt=k=>args[args.indexOf(k)+1],root=resolve(opt('--reference'));
const { Battle }=await import(pathToFileURL(resolve(root,'server/sim/Battle.js')));
const { normalizeEnemy }=await import(pathToFileURL(resolve(root,'server/sim/simdata.js')));
const data=JSON.parse(readFileSync(resolve(root,'data/enemies.json'),'utf8'));
const run=spawnSync(resolve(opt('--native')),[],{encoding:'utf8',maxBuffer:24*1024*1024});
assert.equal(run.status,0,run.stderr);const actual=JSON.parse(run.stdout),entries=Object.entries(data).sort(([a],[b])=>a<b?-1:a>b?1:0);
assert.equal(actual.length,entries.length);
for(const [i,[key,raw]] of entries.entries()) {
 const e=normalizeEnemy(key,raw);
 const immutable=[key,e.name,[e.maxHp,e.atk,e.def,e.res,e.aspd,e.bat,e.moveSpeed,e.tauntLevel,e.massLevel,e.hpRecoveryPerSec,e.epResistance,e.epDamageResistance,
 e.blockCnt,e.lpr,+!e.notCountInTotal,+(e.motion==='FLY'),+e.staticBody,['NORMAL','ELITE','BOSS'].indexOf(e.rank)],raw.talents.bb,raw.talents.bbStr,
 raw.skills.map(s=>[s.prefabKey,s.priority,s.cooldown,s.initCooldown,s.spCost,s.bb,s.bbStr]),
 ['stun','silence','sleep','frozen','levitate','feared','attract','disarmedcombat','palsy'].map(k=>+e.immune.has(k))];
 assert.deepEqual(actual[i].slice(0,-1),immutable,key);
 const b=new Battle({content:'none',autoFinish:false,timeLimit:60,rect:{r0:0,r1:18,c0:0,c1:20},stage:{id:'catalog',rows:Array(19).fill('r'.repeat(21))},
 data:{chess:{ally:{chessId:'ally',stats:{maxHp:100000,def:100,blockCnt:10}}},enemies:data},
 players:[{playerId:'one',coords:'field',units:[{uid:1,chessId:'ally',row:9,col:5}]}],spawns:[{time:0,enemyKey:key,ownerPlayerId:'one',route:{start:[9,10],end:[9,4]}}],
 setup(b){b.allyUnits[0].profile={noAttack:true};}});
 for(let tick=0;tick<180;tick++) {
  b.step();const enemy=b.units[1],expected=[enemy.hp,+enemy.alive,enemy.x,enemy.y,enemy.blockedBy?.id??0,enemy.stats.attacks,b.allyUnits[0].hp];
  expected.forEach((v,j)=>assert(Math.abs(actual[i][7][tick][j]-v)<=1e-9*Math.max(1,Math.abs(v)),`${key} tick ${tick} field ${j}: native ${actual[i][7][tick][j]}, JS ${v}`));
 }
 assert.equal(b.errorCount,0,JSON.stringify(b.errors));
}
console.log(`${entries.length} real enemy records and ${entries.length*180} base-combat frames matched JS`);
