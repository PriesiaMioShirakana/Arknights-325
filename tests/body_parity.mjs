import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const args=process.argv.slice(2),opt=k=>args[args.indexOf(k)+1],root=resolve(opt('--reference'));
const body=await import(pathToFileURL(resolve(root,'server/sim/body.js')));
const cases=[];
for(let i=0;i<600;i++) {
 const u={x:i%25-2+(i%4)*0.25,y:Math.floor(i/25)%23-2+(i%3)*1e-9,hitArea:i%5?{w:[4.95,2,1,0.2][i%4],h:[2.95,1,5][i%3],dx:0.5,dy:1}:null};
 cases.push({u,x:(i*7)%21,y:(i*13)%19,r:(i*3)%19,c:(i*11)%21});
}
const words=[cases.length];for(const c of cases)words.push(c.u.x,c.u.y,c.u.hitArea?.w??0,c.u.hitArea?.h??0,c.u.hitArea?.dx??0,c.u.hitArea?.dy??0,c.x,c.y,c.r,c.c);
const run=spawnSync(resolve(opt('--native')),[],{input:words.join(' '),encoding:'utf8'});assert.equal(run.status,0,run.stderr);const actual=JSON.parse(run.stdout);
for(const [i,c]of cases.entries()) {
 const keys=body.bodyKeys(c.u),reach=body.bodyTileReach(c.u,c.r,c.c);
 const expected=[body.bodyDist(c.u,c.x,c.y),Number.isFinite(reach)?reach:null,+body.bodyOnTile(c.u,c.r,c.c),keys,+body.bodyInKeys(c.u,keys)];
 assert(Math.abs(actual[i][0]-expected[0])<=1e-12,`distance ${i}`);
 assert.deepEqual(actual[i].slice(1),expected.slice(1),`body ${i}`);
}
console.log('600 body geometry cases matched JS');
