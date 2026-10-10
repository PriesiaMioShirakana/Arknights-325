import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {spawnSync} from 'node:child_process';

const args=process.argv.slice(2),opt=k=>args[args.indexOf(k)+1],root=resolve(opt('--reference'));
const load=p=>import(pathToFileURL(resolve(root,p)));
const {Battle}=await load('server/sim/Battle.js');
const {SkillRuntime}=await load('server/sim/skills.js');
const data=Object.fromEntries(['chess','tokens','backups'].map(k=>[k,JSON.parse(readFileSync(resolve(root,`data/${k}.json`),'utf8'))]));
const names=['inside','yak','leizi','udflow','vigna','vendla','prove','texas','caper','sunbr','skgoat','estell','podego','greyy','pithst','tinman','indigo','utage','wildmn','liskam'],builders={};
for(const [i,name] of names.entries())Object.assign(builders,(await load(`server/sim/content/kits/ops/chess_char_1_${String(i+1).padStart(2,'0')}-${name}.js`)).default);
Object.assign(builders,(await load('server/sim/content/kits/ops/chess_char_2_19-tinman.js')).default);
const extended=new Set(names.slice(5).map((_,i)=>`chess_char_1_${String(i+6).padStart(2,'0')}_a`));
const mixedKits=new Set(names.slice(10).map((_,i)=>`chess_char_1_${String(i+11).padStart(2,'0')}_a`));
mixedKits.add('chess_char_2_19_a');
const zoneKits=new Set(['chess_char_1_16_a','chess_char_2_19_a']);
const seed=Number(opt('--seed')??1),runs=[];
for(const [id,raw] of Object.entries(data.chess)) {
 if(!builders[raw.baseId]||(args.includes('--id')&&opt('--id')!==id))continue;
 for(const sk of raw.skills??[raw.skill])for(const mid of raw.modules?['none',...raw.modules.map(m=>m.uniEquipId)]:[''])
  for(const scene of raw.baseId==='chess_char_1_20_a'?[0,1,2,3,4,5,14,15]:raw.baseId==='chess_char_1_19_a'?[0,1,2,3,4,5,12,13]:raw.baseId==='chess_char_1_18_a'?[0,1,2,3,4,5,10,11]:raw.baseId==='chess_char_1_17_a'?[0,1,2,3,4,5,8,9]:zoneKits.has(raw.baseId)?[0,1,2,3,4,5,6,7]:mixedKits.has(raw.baseId)?[0,1,2,3,4,5]:extended.has(raw.baseId)?[0,1,2,3]:[0,1])runs.push({id,si:sk.index,mid,scene});
}
const r=spawnSync(resolve(opt('--native')),[],{input:runs.map(c=>`${c.id} ${c.si} ${c.mid||'-'} ${seed} ${c.scene}`).join('\n'),encoding:'utf8',maxBuffer:256*1024*1024,timeout:180000});
assert.equal(r.error,undefined);assert.equal(r.status,0,r.stderr);
const actual=r.stdout.trim().split('\n');assert.equal(actual.length,runs.length);r.stdout='';
function equal(a,b,path) {
 if(Array.isArray(b)){assert.equal(a?.length,b.length,path);b.forEach((v,i)=>equal(a[i],v,`${path}.${i}`));}
 else if(typeof b==='number')assert(Number.isFinite(a)&&Math.abs(a-b)<=1e-8*Math.max(1,Math.abs(b)),`${path}: C++ ${a}, JS ${b}`);
 else assert.equal(a,b,path);
}
const stats={maxHp:20000,atk:100,def:0,res:0,aspd:100,bat:1,moveSpeed:0,blockCnt:0,respawnTime:100,cost:0};
const buddy={chessId:'buddy',stats,profession:'WARRIOR',rangeGrid:[[0,0],[0,1]],bonds:['lateranoShip'],skill:{skillId:'buddy',skillType:'AUTO',spType:'NONE',spCost:1,initSp:1,duration:0}};
let snapshots=0;
for(const [index,c] of runs.entries()) {
 const stress=c.scene>=2,dual=!!(c.scene%2),mixed=c.scene>=4;
 const expected=JSON.parse(actual[index]);actual[index]=null;
 const enemies=Object.fromEntries(Array.from({length:5},(_,i)=>[`e${i}`,{stats:{...stats,maxHp:2000000,atk:10,def:40,res:20,massLevel:1,rangeRadius:i===1?2:0},tags:i%2===0?['seamonster']:[],applyWay:i===1?'RANGED':'NONE',rank:i===2?'ELITE':i===3?'BOSS':'NORMAL',motion:c.scene>=6&&i===0?'FLY':'WALK'}]));
 const source={chessId:c.id,skillIndex:c.si,moduleId:c.mid};
 const clone=mixed?{...source,chessId:c.id.slice(0,-1)+(c.id.endsWith('a')?'b':'a'),moduleId:undefined}:source;
 const b=new Battle({content:'generic',kits:builders,seed,autoFinish:false,timeLimit:200,rect:{r0:0,r1:18,c0:0,c1:20},stage:{rows:Array(19).fill('r'.repeat(21))},
  data:{...data,chess:{...data.chess,buddy,cost_buddy:{...buddy,chessId:'cost_buddy',stats:{...stats,cost:11}},support_buddy:{...buddy,chessId:'support_buddy',profession:'SUPPORT'}},enemies},players:[{playerId:'p',coords:'field',units:[{uid:1,...source,row:9,col:5},{uid:2,chessId:mixed?'support_buddy':'buddy',row:9,col:stress?6:4},{uid:3,chessId:c.scene===12||c.scene===13?'cost_buddy':'buddy',row:10,col:stress?6:4},...(dual?[{uid:4,...clone,row:10,col:5}]:[])]},
   {playerId:'q',coords:'field',units:[{uid:5,chessId:mixed?'support_buddy':'buddy',row:stress?8:9,col:stress?6:12}]}],
  setup(b){for(const u of b.allyUnits){if(u.def.id.endsWith('buddy')) {
   u.profile.noAttack=true;u.skill=new SkillRuntime(b,u,u.def.skill,{kind:'ammo',ammo:4,trigger:'NEVER'},{});
  }}}
 });
 b.start();const es=[];
 for(let i=0;i<5;++i){const e=b.spawnEnemy(`e${i}`,{ownerPlayerId:'p',route:{start:[9+Math.floor(i/3),i===0?5.4:6+i%3],end:[9,0]}});e.profile.noAttack=true;es.push(e);}
 b._buildEnemyIndex();if(!stress)b.loseHp(es[1],1500000);b.applyStatus(es[4],'stealth',{duration:60});
 if(stress)for(const a of b.allyUnits)b.loseHp(a,a.s.maxHp*(a.def.id.endsWith('buddy')?0.6:0.25));
 const snapshot=()=>[b.rng.state(),b.players[0].dp,b.units.map(u=>[u.hp,+u.alive,u.s.atk,u.s.def,u.s.maxHp,u.s.res,u.s.aspd,u.s.bat,u.s.taunt,u.skill?.spTotal??0,u.skill?.ammoLeft??0,u.skill?.ammoMax??0,+!!u.skill?.active,
  Number.isFinite(u.skill?.timeLeft??0)?u.skill?.timeLeft??0:null,u.stats.attacks,u.blockedBy?b.units.indexOf(u.blockedBy)+1:0,u.atkCd,u.findBuff('sluggish')?.timeLeft??0,+!!u.s.flags.stealth,+!!u.s.flags.reveal,
  +!!u.s.flags.stun,+!!u.s.flags.disarm,u.stats.heal,u.stats.dmg,
  ['neural','erosion','burn','apoptosis','necrosis'].map(k=>u.elem[k]??0),+!!u.s.flags.silence,+!!u.s.flags.burstLock,+!!u.s.flags.noHeal,u.s.hpRegen,u.mem.tinZones??0,
  u.trait.stored??0,u.findBuff('bind')?.timeLeft??0,u.s.blockCnt,u.s.physTakenMul,u.s.artsTakenMul,+!!u.removed,u.x,u.y,u.base.cost,u.mem.wildmnCut??0,u.findBuff('liskam:block')?.timeLeft??0])];
 let sample=0;const check=()=>{equal(expected[sample++],snapshot(),`${c.id}/${c.si}/${c.mid}/scene${c.scene}/seed${seed}/sample${sample-1}`);++snapshots;};check();
 const u=b.allyUnits[0];
 for(let tick=0;tick<2400;++tick){
  if(c.scene===8||c.scene===9){
   if(tick===0||tick===400)for(const e of es)b.applyStatus(e,'bind',{duration:12});
   if(tick===10)b.applyStatus(u,'disarm',{duration:3});
   if(tick===120)b.applyStatus(u,'stun',{duration:3});
   if(tick===300)b.removeStatus(es[0],'bind');
   if(tick===550)b.removeStatus(es[1],'bind');
  }
  if(tick%150===0)for(const a of b.allyUnits){a.skill.setSpTotal(1000);a.skill.activate();}
  if(tick%17===0)b.forceAttack(u,[stress?(b.effectiveProfile(u).dmgType==='heal'?b.allyUnits[1]:es[Math.floor(tick/17)%2]):es[1]]);
  if(tick%91===0&&u.alive&&u.hp>400)b.loseHp(u,100);
  if(!stress&&(tick===450||tick===1350))for(const a of b.allyUnits)a.skill.stop();
  if(tick===(stress?1500:800))b.retreat(u);if(tick===(stress?1560:900))b.redeploy(u,{free:true});
  if(tick===(stress?1800:1000)&&dual)b.retreat(b.allyUnits[3]);
  if(tick===1200)b.loseHp(es[1],10000000);
  if(tick===(stress?1900:1600))b.retreat(b.allyUnits[1]);if(tick===(stress?1950:1650))b.redeploy(b.allyUnits[1],{free:true});
  if(stress){
   if(tick===120)b.loseHp(es[1],1500000);
   if(tick%73===0){const a=b.allyUnits[[1,2,0][Math.floor(tick/73)%3]],n=Math.floor(tick/73)%8;
    if(a.alive&&a.hp>400){
     if(n===6)b.loseHp(a,35,{source:es[2]});
     else if(n===7)b.dealDamage(es[2],a,{amount:35,type:'element',element:'neural'});
     else b.dealDamage(es[2],a,{amount:35,type:'true',isAttack:n===2,sourceless:n===5,tags:n===3?['counter']:n===4?['reflect']:[]});
    }
   }
   if(tick%89===0&&u.alive){const n=Math.floor(tick/89)%4;b.dealDamage(u,es[0],{amount:130,type:'phys',isAttack:n!==3,isSplash:n===1,tags:n===2?['chain']:[]});}
   if(tick%137===0){const a=b.allyUnits[1],q=b.allyUnits.at(-1);b.heal(q,a,20);b.heal(q,a,7,{regen:true});b.heal(u,u,9,{self:true});}
   if(tick===300)b.addBuff(es[0],{key:'test:dodge',duration:2,mods:{dodgePhys:1}});
   if(tick===330)b.applyStatus(u,'stun',{duration:2});
   if(tick===660)b.addBuff(b.allyUnits[1],{key:'test:noheal',duration:3,flags:{healFree:true}});
   if(tick===870)b.addBuff(b.allyUnits[2],{key:'test:isolated',duration:3,flags:{isolated:true}});
  }
  if(mixed){
   if(tick===450)b.loseHp(es[0],10000000);
   if(tick===600)b.addBuff(u,{key:'test:power',duration:2,mods:{atkPct:0.5}});
   if(tick%137===0)b.heal(b.allyUnits.at(-1),u,100);
   if(tick%193===0&&u.alive&&u.hp>500)b.dealDamage(es[2],u,{amount:500,type:'phys',isAttack:true});
   if(dual&&tick===1000)b.retreat(b.allyUnits[3]);
   if(dual&&tick===1100)b.redeploy(b.allyUnits[3],{free:true});
  }
  if(c.scene>=6){
   if(tick===30||tick===900){b.dealDamage(b.allyUnits[1],es[2],{amount:5000,type:'element',element:'necrosis'});b.dealDamage(b.allyUnits[1],es[3],{amount:5000,type:'element',element:'apoptosis'});}
   if(tick%101===0)for(const tags of [['dot'],['burst'],[]])b.dealDamage(b.allyUnits[1],es[2],{amount:100,type:'true',canDodge:false,tags});
   if(tick===2000)b.retreat(u,{permanent:true});
   if(dual&&tick===2100){b.redeploy(b.allyUnits[3],{free:true});b.retreat(b.allyUnits[3],{permanent:true});}
   if((c.scene===8||c.scene===9)&&(tick===2050||tick===2200))b.forceAttack(b.allyUnits[1],[es[0]]);
  }
  if(c.scene>=10){
   if(tick===200||tick===201)b.applyStrongest(u,'protect',{duration:tick===200?1:3,value:tick===200?0.5:0.1,mods:v=>({physTakenMul:1-v,artsTakenMul:1-v}),source:b.allyUnits[1]});
   if(tick===205)for(const type of ['phys','arts','true'])b.dealDamage(es[2],u,{amount:100,type,canDodge:false});
   if(tick===250)b.heal(u,u,u.s.maxHp,{self:true});
  }
  if(c.scene===12||c.scene===13){
   if(tick===20||tick===600||tick===1990)b.retreat(b.allyUnits[2]);
   if((tick>=50&&tick<=200&&tick%30===20)||tick===650||tick===680||tick===1995){b.retreat(u);b.redeploy(u,{free:true});}
   if(tick===240||tick===720||tick===2200)b.redeploy(b.allyUnits[2],{free:false});
  }
  if(c.scene===14||c.scene===15){
   if(tick===1||tick===601)for(const a of [b.allyUnits[1],b.allyUnits[2],b.allyUnits.at(-1)]){a.skill.stop();a.skill.setSpTotal(0);}
   if(tick%31===0&&u.alive&&u.hp>300){const n=Math.floor(tick/31)%5;b.dealDamage(es[2],u,{amount:n===4?0:70,type:n===0?'phys':n===1?'arts':'true',canDodge:false,noSp:n===3,tags:['dot']});}
  }
  b.step();if(tick%30===0)check();
 }
 assert.equal(sample,expected.length);assert.equal(b.errorCount,0,JSON.stringify(b.errors));
}
console.log(`operator kits: ${runs.length} scenes, ${snapshots} snapshots, seed ${seed}`);
