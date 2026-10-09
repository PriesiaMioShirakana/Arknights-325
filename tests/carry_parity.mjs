// 初始棋盘召唤物、部署钩子、联防强制退场和下一场输入均运行原 JS 引擎。
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const args=process.argv.slice(2),opt=k=>args[args.indexOf(k)+1],root=resolve(opt('--reference'));
const { Battle }=await import(pathToFileURL(resolve(root,'server/sim/Battle.js')));
const { SkillRuntime }=await import(pathToFileURL(resolve(root,'server/sim/skills.js')));
const run=spawnSync(resolve(opt('--native')),[],{encoding:'utf8',maxBuffer:16*1024*1024});assert.equal(run.status,0,run.stderr);const actual=JSON.parse(run.stdout);
const def={stats:{maxHp:1000,blockCnt:0,respawnTime:2},rangeGrid:[[0,0],[0,1]]};
function make(scene,players,hooks=true){
 return new Battle({content:'none',autoFinish:false,timeLimit:60,flags:{dpInit:scene===7?0:10},
  rect:{r0:0,r1:18,c0:0,c1:20},stage:{id:'carry',rows:Array(19).fill('r'.repeat(21))},
  data:{chess:{ally:{chessId:'ally',...def}}},players,
  setup(b){
   for(const u of b.allyUnits){
    u.profile={noAttack:true};
    u.skill=new SkillRuntime(b,u,{skillType:'AUTO'},scene===3?null:{kind:scene===1?'duration':scene===2?'passive':'charges',
     spCost:10,initSp:2,charges:3,trigger:'NEVER',duration:0.7,activateOnDeploy:scene===1});
    if(hooks&&u.id===3&&(scene===4||scene===5))u.deferDeploy=true;
   }
   if(!hooks)return;
   b.on('deploy',({unit,initial})=>{
    unit.skill.gainSp(20);
    if(unit.id!==1||!initial)return;
    const token=b.units[2];if(token.deferDeploy&&token.carry.down)b.redeploy(token);
    const t=b.spawnToken(unit,'extra',7,7,{def});t.profile={noAttack:true};
   });
   b.on('battleStart',()=>{for(const id of [1,2,4,5])b.addBuff(b.units[id-1],{key:'redeploy',mods:{redeployMul:0.5},persist:true,allowDead:true});});
  }});
}
function snapshot(b){
 const r=b.result();
 return [b._perPlayer.one.deaths,b.getPlayer('one').dp,b.units.map(u=>[u.hp,+u.alive,+u.removed,u.deploySeq,u.aggroSeq,Number.isFinite(u.respawnAt)?u.respawnAt:null,
  u.skill.spTotal,+u.skill.active,u.skill.activations,u.ownerUnit?.id??0,Number.isFinite(u.skill.opReadyAt)?u.skill.opReadyAt:null]),
  b.players.map(p=>r.perPlayer[p.playerId].unitsEnd.map(u=>[u.uid,u.id,u.hpPct,u.sp,+u.skillActive,+u.alive]))];
}
function equal(a,b,path){if(Array.isArray(b)){assert.equal(a.length,b.length,path);b.forEach((v,i)=>equal(a[i],v,`${path}.${i}`));}else if(b===null)assert.equal(a,null,path);else assert(Math.abs(a-b)<=1e-9*Math.max(1,Math.abs(b)),`${path}: native ${a}, JS ${b}`);}
for(let scene=0;scene<8;scene++){
 const units=Array.from({length:4},(_,i)=>({uid:i+1,...(i===2?{kind:'token',tokenId:'token',def,ownerUid:1}:{chessId:'ally'}),row:9,col:i===2?4:5+i,
  carryState:scene===6&&i===0?{hpPct:NaN,sp:Infinity}:{hpPct:i===0?0.4:i===1?-1:2,sp:i===0?25.126:i===1?-10:15,down:i===1||(scene===4&&i===2)}}));
 const b=make(scene,[{playerId:'one',coords:'field',units},{playerId:'two',coords:'field',mirror:true,units:[{uid:9,chessId:'ally',row:9,col:15}]}]);
 b.start();equal(actual[scene][0],snapshot(b),`scene ${scene} start`);
 for(let tick=0;tick<150;tick++){
  if(tick===10&&scene===5)b.redeploy(b.units[2]);
  if(tick===40){b.retreat(b.units[0]);b.retreat(b.units[2]);}
  if(tick===75)b.loseHp(b.units[3],100000);
  b.step();equal(actual[scene][tick+1],snapshot(b),`scene ${scene} tick ${tick}`);
 }
 assert.equal(b.errorCount,0,JSON.stringify(b.errors));b.forceEnd();
 const carried=b.result().perPlayer.one.unitsEnd.map(u=>{
  const old=units.find(v=>v.uid===u.uid);return {...old,ownerUid:undefined,carryState:{hpPct:u.hpPct,sp:u.sp,down:!u.alive}};
 });
 const next=make(scene,[{playerId:'one',coords:'field',units:carried}],false);next.start();
 // 每场独立初始 DP；上场缺费只影响上场再部署。
 next.getPlayer('one').dp=10;
 equal(actual[scene][151],snapshot(next),`scene ${scene} continuation`);
 assert.equal(next.errorCount,0,JSON.stringify(next.errors));
}
console.log('8 carry scenarios / 1200 frames and 8 continuation battles matched JS');
