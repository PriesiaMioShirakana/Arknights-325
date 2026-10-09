import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const args = process.argv.slice(2);
const opt = key => args[args.indexOf(key) + 1];
const root = resolve(opt('--reference'));
const { Unit } = await import(pathToFileURL(resolve(root, 'server/sim/units.js')));
const { makeBuff } = await import(pathToFileURL(resolve(root, 'server/sim/buffs.js')));
const { Battle } = await import(pathToFileURL(resolve(root, 'server/sim/Battle.js')));
const attributes = ['atkFlat','atkPct','atkFinal','defFlat','defPct','hpFlat','hpPct','resFlat','aspd','batPct','blockCnt','rangeExtend','defIgnoreFlat','defIgnorePct','resIgnoreFlat','resIgnorePct','dodgePhys','dodgeArts','spRecoveryFlat','maxTargets','taunt','hpRegen','hpRegenRatio','spCostFlat','moveFlat','massFlat','blockRadiusScale','atkMul','defMul','hpMul','resMul','moveMul','dmgDealtMul','dmgTakenMul','physTakenMul','artsTakenMul','trueTakenMul','elemTakenMul','elementalTakenMul','healingDealtMul','healingTakenMul','spRecoveryMul','redeployMul','atkScaleMul','physDealtMul','artsDealtMul'];
const stats = ['maxHp','atk','def','res','aspd','bat','blockCnt','moveSpeed','rangeExtend','blockRadiusScale','baseRangeExtend','massLevel','maxTargets','taunt','dodgePhys','dodgeArts','defIgnoreFlat','defIgnorePct','resIgnoreFlat','resIgnorePct','dmgDealtMul','physDealtMul','artsDealtMul','dmgTakenMul','physTakenMul','artsTakenMul','trueTakenMul','elemTakenMul','elementalTakenMul','healingDealtMul','healingTakenMul','atkScaleMul','spRecovery','spCostFlat','redeployMul','hpRegen'];
function compare(a,b,path) {
  if (Array.isArray(b)) { assert.equal(a.length,b.length,path); b.forEach((v,i)=>compare(a[i],v,`${path}[${i}]`)); }
  else { assert(Number.isFinite(a) && Number.isFinite(b)); assert(Math.abs(a-b)<=1e-9*Math.max(1,Math.abs(b)),`${path}: native ${a}, JS ${b}`); }
}
function native(mode) {
 const r=spawnSync(resolve(opt('--native')),[mode],{encoding:'utf8',timeout:15000,maxBuffer:4*1024*1024});
 assert.equal(r.error,undefined);assert.equal(r.status,0,r.stderr);return JSON.parse(r.stdout);
}
const cases = native('attributes');
for (const [index,c] of cases.entries()) {
 const u = new Unit({id:1,side:'ally',base:{maxHp:1000,atk:100,def:200,res:30,moveSpeed:2,blockCnt:2,tauntLevel:1,spRecovery:1,hpRecoveryPerSec:5,massLevel:2}});
 u.buffs=c.mods.map(([value,stacks,persist],i)=>makeBuff({key:String(i),mods:{[attributes[i]]:value},stacks,persist:!!persist}));
 compare(c.stats,stats.map(k=>u.s[k]),`attributes ${index}`);
}
const b = new Battle({content:'none',autoFinish:false,timeLimit:60,
 rect:{r0:0,r1:18,c0:0,c1:20},stage:{id:'effects',rows:Array(19).fill('r'.repeat(21))},
 data:{chess:{ally:{chessId:'ally',stats:{maxHp:1000,atk:100,def:50,res:30,blockCnt:1},rangeGrid:[[0,0],[0,1]]}},enemies:{enemy:{stats:{maxHp:2000,atk:200,def:200,res:50,moveSpeed:0,bat:1,blockCnt:0,spRecovery:1,massLevel:0},applyWay:'MELEE'}}},
 players:[{playerId:'one',coords:'field',units:[{uid:1,chessId:'ally',row:9,col:5}]}],
 spawns:[{time:0,enemyKey:'enemy',ownerPlayerId:'one',route:{start:[9,10],end:[9,0]}}],
 setup(b){b.allyUnits[0].profile={noAttack:true};b.on('enemySpawn',({enemy})=>{enemy.profile={noAttack:true};});}
});
b.step(); const [ally,enemy]=b.units; b.loseHp(ally,500);
const expected=[];
for(let tick=0;tick<150;tick++) {
 if(tick===0)b.addBuff(ally,{key:'health',duration:1,mods:{hpPct:1}});
 if(tick===5)b.applyStatus(enemy,'resist',{duration:3});
 if(tick===6)b.applyStatus(enemy,'cold',{duration:2});
 if(tick===7)b.applyStatus(enemy,'cold',{duration:4});
 if(tick===10)b.applyStatus(enemy,'weaken',{duration:0.5,value:0.8});
 if(tick===11)b.applyStatus(enemy,'weaken',{duration:2,value:0.3});
 if(tick===15)b.applyStatus(enemy,'defDown',{duration:1,value:0.5});
 if(tick===20)b.applyStatus(enemy,'resDown',{duration:2,value:25});
 if(tick===30)b.addBuff(ally,{key:'regen',duration:2,mods:{hpRegen:60}});
 if(tick===40)b.applyStatus(ally,'healFree',{duration:2});
 b.step(); expected.push(b.units.map(u=>{const s=u.s;return[u.hp,stats.map(k=>s[k])];}));
}
compare(native('trace'),expected,'buff/status trace');
assert.equal(b.errorCount,0,JSON.stringify(b.errors));
console.log(`${cases.length} attribute combinations and ${expected.length} buff/status frames matched JS`);


