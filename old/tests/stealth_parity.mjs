import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const args=process.argv.slice(2),opt=k=>args[args.indexOf(k)+1],root=resolve(opt('--reference'));
const { Battle }=await import(pathToFileURL(resolve(root,'server/sim/Battle.js')));
const run=spawnSync(resolve(opt('--native')),[],{encoding:'utf8'});assert.equal(run.status,0,run.stderr);const actual=JSON.parse(run.stdout);
const b=new Battle({content:'none',autoFinish:false,timeLimit:60,rect:{r0:0,r1:18,c0:0,c1:20},stage:{id:'stealth',rows:Array(19).fill('r'.repeat(21))},
 data:{chess:{ally:{chessId:'ally',stats:{maxHp:10000,atk:1,bat:0.1,blockCnt:1},rangeGrid:[[0,0],[0,1]]}},enemies:{enemy:{stats:{maxHp:10000,moveSpeed:0},applyWay:'NONE'}}},
 players:[{playerId:'one',coords:'field',units:[{uid:1,chessId:'ally',row:9,col:5}]}],spawns:[{time:0,enemyKey:'enemy',ownerPlayerId:'one',pos:[9,5.4],route:{start:[9,5],end:[9,0]}}]
});
b.step();const ally=b.allyUnits[0],enemy=b.enemies[0];b.addBuff(enemy,{key:'source-a',flags:{stealth:true}});
for(let tick=0;tick<240;tick++) {
 if(tick===5||tick===45||tick===95)b.applyStatus(ally,'noBlock',{duration:1});
 if(tick===15)b.addBuff(enemy,{key:'source-b',flags:{stealth:true},data:{stealthRestore:0}});
 if(tick===25)b.removeBuff(enemy,'source-b');
 if(tick===50)b.addBuff(enemy,{key:'source-c',flags:{stealth:true},data:{stealthRestore:1}});
 if(tick===55)b.removeBuff(enemy,'source-a');
 if(tick===120)b.relocate(ally,9,6);
 if(tick===125)b.applyStatus(ally,'noBlock',{duration:5});
 b.step();assert.deepEqual(actual[tick],[enemy.hp,enemy.blockedBy?.id??0,ally.stats.attacks],`tick ${tick}`);
}
assert.equal(b.errorCount,0,JSON.stringify(b.errors));console.log('240 multi-source stealth frames matched JS');
