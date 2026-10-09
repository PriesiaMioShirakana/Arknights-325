// 完整比较距离、原始父节点、平滑父节点、偏好代价和几何长度，避免只看最终寻路成功。
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { spawnSync } from 'node:child_process';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const args=process.argv.slice(2),opt=k=>args[args.indexOf(k)+1],root=resolve(opt('--reference'));
const { Grid }=await import(pathToFileURL(resolve(root,'server/sim/grid.js')));
const data=JSON.parse(readFileSync(resolve(root,'data/stages.json'),'utf8'));
const stages=Array.isArray(data)?data:Object.values(data.stages??data);
let state=12345;const random=n=>{state=(Math.imul(state,1664525)+1013904223)>>>0;return state%n;};
const cases=stages.filter(s=>Array.isArray(s.rows));
assert(cases.length>=13);
for(let i=0;i<12;i++)cases.push({rows:Array.from({length:19},()=>Array.from({length:21},()=>['r','r','r','r','f','f','h','X'][random(8)]).join(''))});
const fields=[],words=[cases.length];
for(const [index,stage] of cases.entries()) {
 const rect=index%3===0?{r0:3,r1:15,c0:2,c1:18}:{r0:0,r1:18,c0:0,c1:20};
 const grid=new Grid(stage,rect);
 words.push(rect.r0,rect.r1,rect.c0,rect.c1);
 for(const t of grid.tiles)words.push((t.pass==='ALL'?1:0)|(t.pass!=='NONE'?2:0)|(t.height==='LOW'?4:0)|({NONE:0,ALL:1,MELEE:2,RANGED:3}[t.build]<<3));
 const expected=[];
 words.push(24);
 for(let query=0;query<24;query++) {
  const row=query===23?-1:random(19),col=random(21),diagonal=query%2===0,ignore=query%3===0;
  const br=random(19),bc=random(21),kind=query%2===0?'block':'crate',enabled=query%5!==0;
  words.push(row,col,+diagonal,+ignore,br,bc,kind==='block'?1:2,+enabled);
  grid.setObstacle(br,bc,enabled,kind);
  const f=grid.flowField(row,col,{allowDiagonal:diagonal,ignoreObstacles:ignore});
  expected.push([f.dest,...['dist','parent','pen','next','official','cost'].map(k=>Array.from(f[k])),Array.from({length:399},(_,k)=>{const x=grid.fieldLength(f,k);return Number.isFinite(x)?x:null;})]);
 }
 fields.push(expected);
}
const run=spawnSync(resolve(opt('--native')),[],{input:words.join(' '),encoding:'utf8',timeout:30000,maxBuffer:64*1024*1024});
assert.equal(run.status,0,run.stderr);const actual=JSON.parse(run.stdout);
fields.forEach((scene,i)=>scene.forEach((field,j)=>field.forEach((expected,k)=>{
 if(k!==7)assert.deepEqual(actual[i][j][k],expected,`field ${i}/${j}/${k}`);
 else expected.forEach((v,n)=>{const a=actual[i][j][k][n];if(v===null)assert.equal(a,null);else assert(Math.abs(a-v)<1e-9,`length ${i}/${j}/${n}: ${a} / ${v}`);});
})));
console.log(`${cases.length} stages / ${cases.length*24} complete flow fields matched JS`);
