import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { spawnSync } from 'node:child_process';
const args = process.argv.slice(2), opt = k => args[args.indexOf(k) + 1], root = resolve(opt('--reference'));
const load = p => import(pathToFileURL(resolve(root, p)));
const { DataSource, normalizeToken } = await load('server/sim/simdata.js');
const { genericSkillSpec, genericKit, statTalentMods } = await load('server/sim/content/generic.js');
const { SkillRuntime } = await load('server/sim/skills.js');
const { Battle } = await load('server/sim/Battle.js');
const { resolveProfile } = await load('server/sim/professions.js');
const data = Object.fromEntries(['chess', 'tokens', 'backups'].map(k => [k, JSON.parse(readFileSync(resolve(root, `data/${k}.json`), 'utf8'))]));
const ds = new DataSource(data), allTokens = {...data.backups.tokens, ...data.tokens};
const kinds = ['none','duration','ammo','instant','charges','passive','toggle'];
const triggers = ['DEFAULT','SP_FULL','SEARCH','CUSTOM_RANGE','SKILL_RANGE','ACTIVE_RANGE','GDGLOW_SKILL_2','TAKE_DAMAGE','NEVER'];
const attributes = ['atkFlat','atkPct','atkFinal','defFlat','defPct','hpFlat','hpPct','resFlat','aspd','batPct','blockCnt','rangeExtend','defIgnoreFlat','defIgnorePct','resIgnoreFlat','resIgnorePct','physDodge','artsDodge','spRecoveryFlat','maxTargets','taunt','hpRegen','hpRegenRatio','spCostFlat','moveFlat','massFlat','blockRadiusScale','atkMul','defMul','hpMul','resMul','moveMul','dmgDealtMul','dmgTakenMul'];
const mods = m => Object.entries(m ?? {}).map(([k,v]) => { const i = attributes.indexOf(k); assert(i >= 0, k); return [i,v]; });
function equal(a,b,path) {
 if (Array.isArray(b)) { assert.equal(a?.length,b.length,path); b.forEach((v,i) => equal(a[i],v,`${path}.${i}`)); }
 else if (typeof b === 'number') assert(Number.isFinite(a) && Math.abs(a-b) <= 1e-8*Math.max(1,Math.abs(b)),`${path}: C++ ${a}, JS ${b}`);
 else assert.equal(a,b,path);
}
function run(input, argv = []) {
 const r=spawnSync(resolve(opt('--native')),argv,{input,encoding:'utf8',timeout:180000,maxBuffer:256*1024*1024});
 assert.equal(r.error,undefined); assert.equal(r.status,0,r.stderr);
 return r.stdout.trim().split('\n').filter(Boolean).map(JSON.parse);
}
function config(d) {
 const s=genericSkillSpec(d.skill,d.skill?.bb,d), a=s?.attack??{}, t=s?.targeting??{};
 let out=null;
 if(s) {
  const u={profile:resolveProfile(d)};
  const r=new SkillRuntime({},u,d.skill,s,d.skill.bb);
  let rule=r.rule; if(rule==='GLOBAL')rule='GDGLOW_SKILL_2'; if(rule==='MANUAL')rule='NEVER';
  out=[kinds.indexOf(s.kind),s.duration??null,s.ammo??0,+!!s.activateOnDeploy,s.trigger?triggers.indexOf(s.trigger):null,mods(s.mods),t.rangeGrid??[],t.rangeExtend??0,t.maxTargets??null,+('attack'in s),
    s.attack?.dmgType?{phys:0,arts:1,true:2}[s.attack.dmgType]:null,a.atkScale??null,a.healScale??null,a.hits??null,a.splashRadius??null,+!!a.noAttack,+!!a.onHit,
    [kinds.indexOf(r.kind),['time','attack','hurt','none'].indexOf(r.spType),Math.max(0,triggers.indexOf(rule)),r.baseSpCost,d.skill.initSp,r.maxCharges,r.duration,r.ammo,+r.manual,+!!s.activateOnDeploy,+!!r.healSkill,+r.triggerAllies,r.triggerGrid??[]]];
 }
 const talents=(d.talents??[]).flatMap((t,i)=>{const m=statTalentMods(t);return m?[[`talent:generic:${i}`,mods(m)]]:[];});
 return [out,talents];
}
if(args.includes('--catalog')) {
 const actual=run('',['config']); const counts={};
 for(const row of actual) {
  const [kind,id]=row; let d;
  if(kind==='chess')d=ds.getChess(id,{skillIndex:row[2],moduleId:row[3]});
  if(kind==='stand')d=ds.getStandIn(id);
  if(kind==='diy')d=ds.getDiy(id,{charId:row[2],skillIndex:row[3],uniEquipId:row[4]==='none'?null:row[4]});
  if(kind==='token'||kind==='diytoken') {
   const raw=allTokens[id],owner=row[2],si=row[3],mid=row[4];
   if(!owner.includes('@'))d=ds.getToken(id,owner||null,{skillIndex:si,moduleId:mid});
   else { let v=raw.variants[owner]; v={...v,...v?.bySkill?.[si],...(mid!=='none'?v?.byModule?.[mid]:null)}; d=normalizeToken(id,raw,owner,v); }
  }
  if(kind==='unowned')d=normalizeToken(id,allTokens[id],'missing',null);
  assert(d,row.slice(0,-2).join('/')); equal(row.slice(-2),config(d),row.slice(0,-2).join('/'));
  counts[kind]=(counts[kind]??0)+1;
 }
 assert(actual.length>6000); console.log(`generic skill static records: ${actual.length} bodies ${JSON.stringify(counts)}`);
} else {
 const seed=Number(opt('--seed')??1),scene=args.includes('--professions')?2:seed===1?0:1,cases=[],seen=new Set();
 for(const [id,raw] of Object.entries(data.chess)) {
  for(const sk of raw.skills??[raw.skill]) for(const mid of raw.modules?['none',...raw.modules.map(m=>m.uniEquipId)]:['']) {
   const si=sk?.index??-1,d=ds.getChess(id,{skillIndex:si,moduleId:mid}); if(!d?.skill)continue;
   const key=JSON.stringify([d.skill,d.dmgType,d.profession,d.subProf]); if(seen.has(key))continue;seen.add(key);
   cases.push({id,si,mid,d});
  }
 }
 const stats={maxHp:20000,atk:100,def:50,res:10,aspd:100,bat:1,moveSpeed:0,blockCnt:0,respawnTime:100,cost:0};
 const grid=[[0,0],[0,1],[0,2],[1,1],[-1,1]], positions=[[9,5],[8,6],[9,4],[10,4]];
 const snapshot=b=>[b.rng.state(),b.players[0].dp,b.units.map(u=>[u.hp,+u.alive,u.x,u.y,u.s.atk,u.s.def,u.s.maxHp,u.s.res,u.s.aspd,u.s.bat,+!!u.skill?.active,+!!u.skill?.pending,u.skill?.spTotal??0,u.skill?.ammoLeft??0,
   Number.isFinite(u.skill?.timeLeft??0)?u.skill?.timeLeft??0:null,u.skill?.activations??0,['stun','disarm','noMove','unblockable','freeze','cold','sleep','slow'].map(k=>u.findBuff(k)?.timeLeft??0),
   ['neural','erosion','burn','apoptosis','necrosis'].map(k=>u.elem[k]??0),u.buffs.reduce((s,v)=>s+(v.shield??0),0),u.atkCd,u.stats.attacks])];
 let snapshots=0;
 for(let begin=0;begin<cases.length;begin+=32) {
  const batch=cases.slice(begin,begin+32),actual=run(batch.map(c=>`${c.id} ${c.si} ${c.mid||'-'} ${seed} ${scene}`).join('\n'));
  assert.equal(actual.length,batch.length);
  for(const [ix,c] of batch.entries()) {
   const kit=genericKit(c.d.skill.bb,c.d.raw,c.d);const spec={...kit.skill,trigger:'NEVER'};
   if(spec.attack&&scene<2)spec.attack={...spec.attack,noAttack:true};
   const ally={chessId:'source',stats,rangeGrid:grid,profession:'WARRIOR'};
   const b=new Battle({content:'none',seed,autoFinish:false,timeLimit:120,stage:{rows:Array(19).fill('r'.repeat(21))},rect:{r0:0,r1:18,c0:0,c1:20},
    data:{...data,chess:{...data.chess,source:ally,target:{...ally,chessId:'target',stats:{...stats,def:0,res:0}}},enemies:{enemy:{stats:{...stats,maxHp:100000,atk:10,def:20,res:10,massLevel:1},applyWay:'NONE'}}},
    players:[{playerId:'p',coords:'field',units:positions.map(([row,col],i)=>({uid:i+1,chessId:i?'target':scene===2?c.id:'source',row,col,...(!i&&scene===2?{skillIndex:c.si,moduleId:c.mid}:{} )}))}],
    setup(b){
     for(const [i,u] of b.allyUnits.entries())if(i||scene<2)u.profile={dmgType:'phys',attack:'melee',projectile:'none',atkScale:1,hits:1,maxTargets:1,canHitFly:true,noAttack:true};
     const u=b.allyUnits[0];if(scene===1)u.profile={...u.profile,attack:'ranged',projectile:'bomb'};
     u.skill=new SkillRuntime(b,u,{...c.d.skill,skillType:'AUTO'},spec,c.d.skill.bb);kit.install?.(b,u);
    }});
   b.start();for(let i=0;i<4;++i)b.spawnEnemy('enemy',{ownerPlayerId:'p',route:{start:[9+Math.floor(i/2),6+i%2],end:[9,0]}});
   b._buildEnemyIndex(); // 手工出生后立刻施放，先建立原版正常帧会更新的空间索引。
   const u=b.allyUnits[0],enemy=b.enemies[0];let sample=0;const check=()=>{equal(actual[ix][sample++],snapshot(b),`${c.id}/${c.si}/${c.mid}/seed${seed}/sample${sample-1}`);++snapshots;};check();
   for(let tick=0;tick<360;++tick) {
    if(tick%30===0){for(const a of b.allyUnits)b.loseHp(a,500);u.skill.setSpTotal(1000);u.skill.activate();}
    if(tick%17===0)b.forceAttack(u,[enemy]);
    if(tick%11===0)b.dealDamage(enemy,u,{amount:20,type:'true',isAttack:true});
    if(tick===90)u.skill.stop();if(tick===160)b.retreat(u);if(tick===170)b.redeploy(u,{free:true});
    b.step();if(tick%10===0||tick===90||tick===160||tick===170)check();
   }
   assert.equal(sample,actual[ix].length);assert.equal(b.errorCount,0,JSON.stringify(b.errors));
  }
 }
 console.log(`generic skill runtime: ${cases.length} skills, seed ${seed}, scene ${scene}, ${snapshots} snapshots`);
}
