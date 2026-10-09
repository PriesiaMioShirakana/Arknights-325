import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const args=process.argv.slice(2),opt=k=>args[args.indexOf(k)+1],root=resolve(opt('--reference'));
const { Battle }=await import(pathToFileURL(resolve(root,'server/sim/Battle.js')));
const { normalizeRoute,spawnsFromTemplate }=await import(pathToFileURL(resolve(root,'server/sim/simdata.js')));
const { compileRoute }=await import(pathToFileURL(resolve(root,'server/sim/ai.js')));
const waves=JSON.parse(readFileSync(resolve(root,'data/waves.json'),'utf8'));
const enemies=JSON.parse(readFileSync(resolve(root,'data/enemies.json'),'utf8'));
const run=spawnSync(resolve(opt('--native')),[],{encoding:'utf8',maxBuffer:32*1024*1024});assert.equal(run.status,0,run.stderr);
const actual=JSON.parse(run.stdout), tag=k=>({boss:1,part:2}[k]??0),kind=k=>['MOVE','WAIT','DISAPPEAR','APPEAR'].indexOf(k);
const rects=[{r0:9,r1:12,c0:2,c1:10},{r0:9,r1:12,c0:2,c1:18},{r0:1,r1:5,c0:2,c1:18}];
const group=s=>[s.time,s.key,s.count,s.interval,s.routeIndex,s.slot??'',tag(s.tag),s.group??'',s.pack??'',s.weight??1,+!!s.unharmful,s.action==='ACTIVATE_PREDEFINED'?1:0];
function routes(rs){return rs.map(r=>{const n=normalizeRoute(r);return [...n.start,...n.end,+(n.motion==='FLY'),n.checkpoints.map(c=>[kind(c.type),...c.pos,c.time]),(r.steps??[]).flatMap((s,i)=>s.t==='patrol'?[i]:[]),...(r.spawnRandom??[0,0])];});}
function equal(a,b,path){
 if(Array.isArray(b)){assert.equal(a.length,b.length,path);b.forEach((v,i)=>equal(a[i],v,`${path}.${i}`));}
 else if(b&&typeof b==='object'){assert.deepEqual(Object.keys(a).sort(),Object.keys(b).sort(),path);for(const k in b)equal(a[k],b[k],`${path}.${k}`);}
 else if(typeof b==='number')assert(Number.isFinite(a)&&Math.abs(a-b)<=1e-9*Math.max(1,Math.abs(b)),`${path}: native ${a}, JS ${b}`);
 else assert.equal(a,b,path);
}
let spawnsChecked=0;
for(const row of actual){
 const [id,meta,slots,mainRoutes,extraRoutes,spawns,branches,overrides,devices,uses,scenes,paths]=row,w=waves[id];assert(w,id);
 equal(meta,[['normal','boss','hidden','training','escaped'].indexOf(w.kind),+w.solo,w.bossId??'',w.maxPlayTime,w.dp.init,w.dp.perSec,w.dp.max,w.characterLimit,w.moveMultiplier,w.bgm,w.totalCount],`${id}.meta`);
 equal(slots,w.slotCounts,`${id}.slots`);equal(mainRoutes,routes(w.routes),`${id}.routes`);equal(extraRoutes,routes(w.extraRoutes),`${id}.extra`);
 equal(spawns,w.spawns.map(group),`${id}.spawns`);equal(branches,Object.fromEntries(Object.entries(w.branches).map(([k,phases])=>[k,phases.map(p=>p.map(group))])),`${id}.branches`);
 equal(overrides,Object.keys(w.overrides).sort().map(k=>{const raw=enemies[k],ov=w.overrides[k];return [k,{...raw.talents?.bb,...ov.talents?.bb},{...raw.talents?.bbStr,...ov.talents?.bbStr},(ov.skills??raw.skills??[]).map(s=>[s.prefabKey,s.priority,s.cooldown,s.initCooldown,s.spCost,s.bb,s.bbStr])];}),`${id}.overrides`);
 equal(devices,w.devices.map(d=>[d.key,d.alias,...d.pos,['UP','RIGHT','DOWN','LEFT'].indexOf(d.dir),+!!d.hidden]),`${id}.devices`);
 equal(uses,w.usedBy.map(u=>[u.modeId,u.round,u.bossId??'']),`${id}.uses`);
 for(let index=0;index<=w.routes.length;index++){
  const b=new Battle({content:'none',autoFinish:false,rect:rects[1],stage:{id:'roads',rows:Array(19).fill('r'.repeat(21))},
   routes:w.routes.map(normalizeRoute),players:[{playerId:'one',coords:'field',units:[]}]});
  b._pending=[{routeIndex:index}];
  const tiles=b.groundPathTiles();
  assert.equal(paths[index],Array.from({length:399},(_,i)=>tiles.has(398-i)?'1':'0').join(''),`${id}.groundRoutes${index}`);
 }

 for(let scene=0;scene<3;scene++){
  const rect=rects[scene],mods=scene===1?{hpMul:1.5,atkMul:0.7,defMul:1.2,resMul:0.5,speedMul:0.8}:{};
  const b=new Battle({content:'none',autoFinish:false,rect,stage:{id:'wave-probe',rows:Array(19).fill('r'.repeat(21))},data:{enemies},enemyOverrides:w.overrides,
   players:[{playerId:'one',coords:'field',units:[]},{playerId:'two',coords:'field',colOffset:8,units:[]}]});b.start();
  const tpl=spawnsFromTemplate(w),expanded=[];
  for(const s of tpl.spawns)for(let i=0;i<s.count;i++)expanded.push({...s,time:Math.max(0,s.time)+i*Math.max(0,s.interval)});
  expanded.sort((a,b)=>a.time-b.time);
  const expected=expanded.map(s=>{
   const route=tpl.routes[s.routeIndex]??tpl.routes[0],owner=route.start[1]>=11?'two':'one';
   const u=b.spawnEnemy(s.enemyKey,{route,mods,ownerPlayerId:owner,tag:s.tag,countInTotal:s.countInTotal}),st=u.s;
   const legs=compileRoute(route,rect),end=legs.pop();
   return [s.time,s.enemyKey,owner,+u.counted,u.lpr,tag(u.tag),[u.y,u.x,st.maxHp,st.atk,st.def,st.res,st.aspd,st.bat,st.moveSpeed,st.massLevel,u.blockWeight],
    legs.map(l=>[kind(l.t.toUpperCase()),l.r??0,l.c??0,l.time??0]),end.r,end.c];
  });
  equal(scenes[scene],expected,`${id}.scene${scene}`);spawnsChecked+=expected.length;
  assert.equal(b.errorCount,0,JSON.stringify(b.errors));
 }
}
assert.equal(actual.length,Object.keys(waves).length);
console.log(`${actual.length} wave templates, ${spawnsChecked} expanded spawns and all branches / routes / overrides matched JS`);
