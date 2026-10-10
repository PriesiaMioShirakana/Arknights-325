// 原引擎独立核对悬赏归属、漏怪倍率、完美判定、超时队列和结算钩子。
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const args=process.argv.slice(2),opt=k=>args[args.indexOf(k)+1],root=resolve(opt('--reference'));
const { Battle }=await import(pathToFileURL(resolve(root,'server/sim/Battle.js')));
const run=spawnSync(resolve(opt('--native')),[],{encoding:'utf8',maxBuffer:8*1024*1024});assert.equal(run.status,0,run.stderr);const actual=JSON.parse(run.stdout);
const tag=t=>({boss:1,part:2,bounty:3}[t]??0);
const mods=m=>m?[m.hpMul??null,m.atkMul??null,m.defMul??null,m.resMul??null,m.speedMul??null,m.supplyHpMul??null,m.slot??'',m.bountyId??'']:null;
function snapshot(b){const r=b.result();return [r.killed,r.total,b.leakedCount,b.players.map(p=>{const pp=r.perPlayer[p.playerId];return [pp.coins,+pp.perfect,pp.killed,pp.total,
 pp.leaked.map(l=>[l.enemyKey,mods(l.mods),l.lpr,l.sourcePlayerId??'',tag(l.tag),+l.counted,+!!l.boss])];}),
 (r.unspawned??[]).map(p=>[p.enemyKey,p.time,tag(p.tag),p.sourcePlayerId??''])];}
const ally={stats:{maxHp:100,blockCnt:0},rangeGrid:[[0,0]]};
for(let scene=0;scene<16;scene++){
 const route={start:[9,15],end:[9,16]},spawn={enemyKey:'enemy_test',time:0,ownerPlayerId:'one',route,countInTotal:scene!==10&&scene!==11,tag:scene===10?'boss':'bounty',
  mods:{hpMul:2,atkMul:1.2,defMul:1.1,resMul:0.8,speedMul:1,supplyHpMul:1.6,slot:'S',bountyId:'card'},sourcePlayerId:scene===8||scene===9?'leaker':undefined,
  bounty:{coins:17,ownerPlayerId:scene===4?'one':scene===6?undefined:'away'}};
 const spawns=[spawn];if(scene===9)spawns.push({...spawn,time:4},{...spawn,time:5,countInTotal:false,tag:'part'});
 const b=new Battle({content:'none',autoFinish:false,timeLimit:0.5,rect:{r0:0,r1:18,c0:0,c1:20},
  data:{chess:{ally:{chessId:'ally',...ally}},enemies:{enemy_test:{stats:{maxHp:100,moveSpeed:scene>=8&&scene<=11&&scene!==9?60:0},applyWay:'NONE',lpr:3},enemy_ally:{...ally,applyWay:'NONE'}}},
  players:[{playerId:'one',coords:'field',units:[{uid:1,chessId:'ally',row:3,col:5}]},{playerId:'two',coords:'field',colOffset:8,units:[{uid:2,chessId:'ally',row:3,col:15}]}],spawns,
  setup(b){for(const u of b.allyUnits)u.profile={noAttack:true};b.on('enemyLeak',({enemy})=>{if(enemy.defId==='enemy_test')b.addCoins('one',1);});
   if(scene===12)b.on('battleEnd',()=>b.addCoins('two',2));}});
 b.start();b.step();const target=b.units[2];let source=scene===0?b.units[0]:scene===1?b.units[1]:null;
 if(scene===2){source=b.spawnToken(b.units[1],'ally',3,16,{def:ally});source.profile={noAttack:true};}
 if(scene===5)source=b.spawnEnemy('enemy_ally',{ownerPlayerId:'one',route:{start:[9,4],end:[9,3]},countInTotal:false});
 if(scene<7||scene>=12)b.loseHp(target,100000,{source});
 if(scene===14)b.loseHp(target,100000,{source});
 if(scene===13){b.addCoins('one',2.5);b.addCoins('one',-1);b.addCoins('one',NaN);b.addCoins('missing',5);}
 assert.deepEqual(actual[scene][0],snapshot(b),`scene ${scene} active`);
 for(let i=0;i<30&&!b.finished;i++)b.step();
 assert.deepEqual(actual[scene][1],snapshot(b),`scene ${scene} settled`);
 assert.equal(b.errorCount,0,JSON.stringify(b.errors));
}
console.log('16 bounty / leak / timeout settlement scenarios matched JS');
