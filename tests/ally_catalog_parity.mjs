// 对原 DataSource 的所有技能/模组组合与召唤物变体作数据级比较，避免用生成器自身作预言机。
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const args=process.argv.slice(2),opt=k=>args[args.indexOf(k)+1],root=resolve(opt('--reference'));
const { DataSource, normalizeToken }=await import(pathToFileURL(resolve(root,'server/sim/simdata.js')));
const data=Object.fromEntries(['chess','tokens','backups'].map(k=>[k,JSON.parse(readFileSync(resolve(root,`data/${k}.json`),'utf8'))]));
const ds=new DataSource(data);
const { resolveProfile }=await import(pathToFileURL(resolve(root,'server/sim/professions.js')));
const { PROJECTILE_SPEEDS, PROJECTILE_SPEED }=await import(pathToFileURL(resolve(root,'server/sim/constants.js')));
const allTokens={...data.backups.tokens,...data.tokens};
const run=spawnSync(resolve(opt('--native')),[],{encoding:'utf8',maxBuffer:64*1024*1024});assert.equal(run.status,0,run.stderr);const actual=JSON.parse(run.stdout);
function equal(a,b,path){
 if(Array.isArray(b)){assert(Array.isArray(a),path);assert.equal(a.length,b.length,path);b.forEach((v,i)=>equal(a[i],v,`${path}.${i}`));}
 else if(b&&typeof b==='object'){assert.deepEqual(Object.keys(a).sort(),Object.keys(b).sort(),path);for(const k in b)equal(a[k],b[k],`${path}.${k}`);}
 else if(typeof b==='number')assert(Number.isFinite(a)&&Math.abs(a-b)<=1e-9*Math.max(1,Math.abs(b)),`${path}: native ${a}, JS ${b}`);
 else assert.equal(a,b,path);
}
function profile(d){
 const p=resolveProfile(d),heal=p.dmgType==='heal'&&!!p.heal;
 const priorities=[null,'fly','lowDef','lowestHp','highestHp','nearest','farthest','highDef','ranged'];
 const scales={tactician:[7,p.reinforceScale],hunter:[5,p.ammoScale],funnel:[6,1],fastshot:[1,p.flyScale],instructor:[2,p.unblockedScale],lord:[3,p.rangedScale],reaperrange:[4,p.frontScale]};
 const [scaling,mul]=scales[d.subProf]??[0,1];
 return [{phys:0,arts:1,true:2,heal:0,none:0}[p.dmgType],+!!p.noAttack,+heal,+!!p.canHitFly,+!!p.blockFly,+(p.attack==='ranged'),
  heal&&p.heal.mode==='multi'?p.heal.count:p.maxTargets,priorities.indexOf(p.priority),['none','beam'].includes(p.projectile)?0:PROJECTILE_SPEEDS[p.projectile]??PROJECTILE_SPEED,
  +!!p.groundOnly,+!!p.noHeal,p.hits,+!!p.allInRange,p.splashRadius,p.splashScale,p.chain?.count??1,p.chain?.falloff??0.15,p.chain?.sluggish??0,
  heal&&p.heal.mode==='chain'?p.heal.count:1,heal?p.heal.falloff??0.25:0.25,heal?p.heal.elementHealRatio??0:0,
  p.onHitStatus?.key??'',p.onHitStatus?.duration??0,p.onHitStatus?.value??null,+!!p.noAttackUnlessSkill,+!!p.hitAllBlocked,scaling,mul,heal?p.heal.farMul??1:1,p.heal?.nearDist??2,+!!p.boomerang,+!!p.fortress];
}
function traits(d){
 const p=resolveProfile(d),kind=Math.max(0,['','hunter','funnel','mystic','phalanx','bearer','stalker','musha','reaper','incantationmedic','charger','geek','merchant','librator','bard','loopshooter','bombarder','tactician','skywalker','dollkeeper'].indexOf(d.subProf));
 const tokenId=d.subProf==='dollkeeper'?d.tokens.map(t=>typeof t==='string'?t:t.tokenId).find(t=>/shadow|doll/.test(t)):null;
 const token=tokenId?ds.getToken(tokenId,d.id,d.loadout):null;
 const hp=token?Math.max(0.05,token.stats.maxHp/Math.max(1,d.stats.maxHp)):1;
 return [kind,p.ammoMax??8,p.funnel?.init??0.2,p.funnel?.delta??0.15,p.funnel?.max??1.1,p.storeMax??3,p.guardDef??2,p.guardRes??20,p.dodge??0.5,p.selfHeal??50,p.healRatio??0.5,p.dpOnKill??1,p.hpDrain??0.03,p.merchantInterval??3,p.merchantCost??3,p.rampMax??2,p.rampTime??40,p.rampInit??0,p.auraRatio??0.1,p.shockScale??0.5,Math.max(1,Math.floor((p.shockTimes??2)-1)),p.dollDuration??20,hp,+!!p.dollNoAttack];
}
function body(d,raw){
 const s=d.stats,sk=d.skill,ab=d.abnormal??[];
 return [d.id,d.name,d.charId??'',d.profession,d.subProf??'',d.position,
  [s.maxHp,s.atk,s.def,s.res,s.aspd,s.bat,s.moveSpeed,s.blockCnt,s.tauntLevel,s.respawnTime,s.cost,s.spRecovery,s.hpRecoveryPerSec,s.massLevel],
  d.rangeGrid,d.dmgType??'',d.attackKind??'',d.projectile??'',d.canHitFly==null?null:+d.canHitFly,d.targetPriority??'',d.trait,d.traitBb,
  ['stun','silence','sleep','frozen','levitate'].map(k=>+d.immune.has(k)),
  sk?[sk.id,sk.name,sk.index,sk.skillType,sk.durationType,sk.duration,['time','attack','hurt','none'].indexOf(sk.spType),sk.spCost,sk.initSp,sk.maxCharges,
    sk.rangeGrid,sk.trigger.rule,sk.trigger.grid,+!!sk.trigger.allies,sk.bb,raw.skill.bbStr??{},sk.description]:null,
  d.talents.map(t=>[t.name,t.description,t.bb,t.bbStr,t.rangeGrid,t.tokenKey??'']),d.tokens,
  [+!!d.untargetable,+(ab.includes('healFree')||ab.includes('isolated')),+ab.includes('isolated')],
  raw.skill?.skillId?.startsWith('skcom_withdraw')?(raw.skill.duration??0):null,profile(d),raw.trait?.rangeGrid??null,traits(d)];
}
let loadouts=0,variants=0,standIns=0,diyChoices=0,diyTokenVariants=0;
const diyInputs=[];
assert.equal(actual[0].length,Object.keys(data.chess).length);
for(const row of actual[0]){
 const [id,base,golden,diy,tier,bonds,garrisons,choices,standIn,diyPicks]=row,r=data.chess[id];
 equal([base,golden,diy,tier,bonds,garrisons],[r.baseId,+r.isGolden,+r.isDiy,r.tier,r.bonds,r.garrisonIds],id);
 const expectedCount=Math.max(1,r.skills?.length??0)*Math.max(1,(r.modules?.length??0)+(+!!r.modules));assert.equal(choices.length,expectedCount,id);
 for(const [si,mid,sd,md,b,mod,level,active] of choices){
  const d=ds.getChess(id,{skillIndex:si,moduleId:mid}),raw=d.raw;
  equal([sd,md],[+d.loadout.skillIsDefault,+d.loadout.moduleIsDefault],`${id}.${si}.${mid}.defaults`);
  equal(b,body(d,raw),`${id}.${si}.${mid}`);
  equal([mod,level,active],[raw.module?.id??'',raw.module?.level??0,+!!raw.module?.active],`${id}.module`);loadouts++;
 }

 const selection=(actual,d,path)=>{
  equal(actual,[d.loadout.skillIndex??-1,d.loadout.moduleId??'',body(d,d.raw),d.raw.module?.id??'',d.raw.module?.level??0,+!!d.raw.module?.active],path);
 };
 const backup=ds.getStandIn(id);assert.equal(!!standIn,!!backup,`${id}.standIn`);
 if(backup){selection(standIn,backup,`${id}.standIn`);standIns++;}
 for(const [char,si,mid,proto,bonds,tokenOwner,composed] of diyPicks){
  const pick={charId:char,skillIndex:si,uniEquipId:mid==='none'?null:mid};
  const d=ds.getDiy(id,pick);assert(d,`${id}.${char}.diy`);
  equal([proto,bonds,tokenOwner],[+(data.backups.diy.prototypes[tier]??[]).includes(char),d.bonds,d.tokenOwner],`${id}.${char}.identity`);
  selection(composed,d,`${id}.${char}.${si}.${mid}.diy`);diyChoices++;diyInputs.push({id,pick,owner:tokenOwner});
 }
}
assert.equal(actual[1].length,Object.keys(allTokens).length);
for(const [id,placeable,range,limit,fallback,choices,queries,diyVariants,diyQueries,unowned] of actual[1]){
 const raw=allTokens[id];equal([placeable,range,limit,fallback],[+raw.placeable,+raw.ownerRange,raw.deployLimit??0,Object.keys(raw.variants)[0]??''],id);
 for(const [owner,si,mid,defaultChoice,b,count,sources] of choices){
  let d=ds.getToken(id,owner||null,{skillIndex:si,moduleId:mid});
  // DataSource.raw 在 token 上保留顶层表；技能和寿命须用已选择的变体解析。
  let v=raw.variants[owner];const lo=owner?ds.getChess(owner,{skillIndex:si,moduleId:mid})?.loadout:null;
  if(v&&lo&&!lo.isDefault){if(!lo.skillIsDefault)v={...v,...v.bySkill?.[si]};if(!lo.moduleIsDefault)v={...v,...v.byModule?.[mid]};}
  if(owner.includes('@')){
   v={...v,...(si>=0?v.bySkill?.[si]:null),...(mid?v.byModule?.[mid]:null)};
   d=normalizeToken(id,raw,owner,v);
  }
  const merged={...raw,...v};
  equal(defaultChoice,lo?+lo.isDefault:+(si===-1&&mid===''),`${id}.${owner}.default`);
  equal(b,body(d,merged),`${id}.${owner}.${si}.${mid}`);equal([count,sources],[d.count,d.sources],`${id}.summoning`);variants++;
 }

 let qi=0;
 for(const op of actual[0])for(const [si,mid] of op[7]){
  const owner=op[0],lo=ds.getChess(owner,{skillIndex:si,moduleId:mid}).loadout;
  const resolved=raw.variants[owner]?owner:raw.variants[owner.replace(/_b$/, '_a')]?owner.replace(/_b$/, '_a'):Object.keys(raw.variants)[0];
  let v=raw.variants[resolved];
  if(v&&!lo.isDefault){if(!lo.skillIsDefault)v={...v,...v.bySkill?.[si]};if(!lo.moduleIsDefault)v={...v,...v.byModule?.[mid]};}
  const d=ds.getToken(id,owner,{skillIndex:si,moduleId:mid}),actualChoice=choices[queries[qi++]];
  assert(actualChoice,`${id}.${owner}.query`);
  equal(actualChoice[4],body(d,{...raw,...v}),`${id}.${owner}.${si}.${mid}.fallback`);
  equal(actualChoice.slice(5),[d.count,d.sources],`${id}.${owner}.fallback-count`);
 }
 assert.equal(qi,queries.length);
 const pickByKey=new Map(diyInputs.map(p=>[[p.owner,p.pick.skillIndex,p.pick.uniEquipId??'none'].join('|'),p]));
 for(const [owner,si,mid,b,count,sources] of diyVariants){
  const input=pickByKey.get([owner,si,mid].join('|'));assert(input,`${id}.${owner}.diy-choice`);
  let v=raw.variants[owner];assert(v);v={...v,...v.bySkill?.[si]};if(mid!=='none')v={...v,...v.byModule?.[mid]};
  const d=ds.getDiyToken(id,input.id,input.pick);
  equal([b,count,sources],[body(d,{...raw,...v}),d.count,d.sources],`${id}.${owner}.${si}.${mid}.diy-token`);diyTokenVariants++;
 }
 const missing=normalizeToken(id,raw,'missing',null);equal(unowned,[body(missing,raw),missing.count,null],`${id}.unowned`);
 assert.equal(diyQueries.length,diyInputs.length);
 for(let i=0;i<diyInputs.length;i++){
  const input=diyInputs[i],ix=diyQueries[i],v=raw.variants[input.owner];
  if(!v)assert.equal(ix,-1,`${id}.${input.owner}.unowned`);
  else {assert(ix>=0);equal(diyVariants[ix].slice(0,3),[input.owner,input.pick.skillIndex,input.pick.uniEquipId??'none'],`${id}.${input.owner}.diy-index`);}
 }
}
console.log(`${actual[0].length} operators / ${loadouts} loadouts and ${actual[1].length} tokens / ${variants} variants, ${standIns} stand-ins, ${diyChoices} DIY choices, ${diyTokenVariants} DIY token variants matched JS`);
