// 直接执行原职业 dmgMul，覆盖朝向、矩形身体、阻挡与远距离治疗，避免复制被测公式。
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const args=process.argv.slice(2),opt=k=>args[args.indexOf(k)+1],root=resolve(opt('--reference'));
const { Battle }=await import(pathToFileURL(resolve(root,'server/sim/Battle.js')));
const { SUB }=await import(pathToFileURL(resolve(root,'server/sim/professions.js')));
const run=spawnSync(resolve(opt('--native')),[],{encoding:'utf8',maxBuffer:16*1024*1024});assert.equal(run.status,0,run.stderr);const actual=JSON.parse(run.stdout);
for(let scene=0;scene<8;scene++)for(let facing=0;facing<4;facing++){
 const dir=['UP','RIGHT','DOWN','LEFT'][facing],fwd=[[1,0],[0,1],[-1,0],[0,-1]][facing],lat=[[0,-1],[1,0],[0,1],[-1,0]][facing];
 const point=(f,l)=>[9+f*fwd[0]+l*lat[0],10+f*fwd[1]+l*lat[1]];
 const profile={...SUB[['fastshot','instructor','lord','reaperrange','reaperrange','reaperrange','centurion','healer'][scene]],
  attack:'melee',dmgType:scene===7?'heal':'phys',canHitFly:true,hits:2,allInRange:scene<6,projectile:'none',maxTargets:1,
  flyScale:1.7,unblockedScale:1.7,rangedScale:1.7,frontScale:1.7};
 if(scene===0)Object.assign(profile,{splashRadius:1.1,splashScale:0.7});
 if(scene===1)profile.chain={count:3,falloff:0.15};
 if(scene===4)profile.frontGrid=[[0,1],[1,2]];
 if(scene===5)profile.frontGrid=[];
 if(scene===7)profile.heal={mode:'chain',count:3,farMul:0.6,nearDist:2};
 const units=[{uid:1,chessId:'source',row:9,col:10,dir}];
 if(scene===7)for(let i=0;i<3;i++){const [row,col]=point(1+i,1);units.push({uid:i+2,chessId:'patient',row,col});}
 const rangeGrid=[];for(let r=-5;r<=5;r++)for(let c=-5;c<=5;c++)rangeGrid.push([r,c]);
 const enemies=Object.fromEntries(Array.from({length:6},(_,i)=>['e'+i,{stats:{maxHp:1000000,def:30,moveSpeed:0},applyWay:'NONE',motion:i===4?'FLY':'WALK',...(i===3?{hitArea:{w:2.2,h:2.2}}:{})}]));
 const b=new Battle({content:'none',autoFinish:false,timeLimit:60,rect:{r0:0,r1:18,c0:0,c1:20},stage:{id:'prof',rows:Array(19).fill('r'.repeat(21))},
  data:{chess:{source:{chessId:'source',stats:{maxHp:1000000,atk:100,bat:0.5,blockCnt:2},rangeGrid},patient:{chessId:'patient',stats:{maxHp:1000000,blockCnt:0}}},enemies},
  players:[{playerId:'one',coords:'field',units}],spawns:[[-0.2,0],[0.4,0],[1,0],[2.3,0.6],[3,-1],[-2,0]].map(([f,l],i)=>({time:0,enemyKey:'e'+i,ownerPlayerId:'one',route:{start:point(f,l),end:[9,0]}})),
  setup(b){b.allyUnits.forEach((u,i)=>{u.profile=i?{noAttack:true}:profile;});}});
 b.step();if(scene===7)b.allyUnits.slice(1).forEach((u,i)=>b.loseHp(u,(i+2)*10000));
 for(let tick=0;tick<150;tick++){
  if(tick===40||tick===90)b.addBuff(b.allyUnits[0],{key:'block',mods:{blockCnt:tick===40?2:-8}});
  if(tick===20||tick===70)b.addBuff(b.allyUnits[0],{key:'targets',mods:{maxTargets:tick===20?-4:2}});
  b.step();const expected=b.units.map(u=>[u.hp,u.stats.dmg,u.stats.heal,u.stats.attacks]);
  expected.forEach((u,i)=>u.forEach((v,j)=>assert(Math.abs(actual[scene*4+facing][tick][i][j]-v)<=1e-9*Math.max(1,Math.abs(v)),
   `scene ${scene} ${dir} tick ${tick} unit ${i}.${j}: native ${actual[scene*4+facing][tick][i][j]}, JS ${v}`)));
 }
 assert.equal(b.errorCount,0,JSON.stringify(b.errors));
}
console.log('32 profession attack scenarios / 4800 frames matched JS');
