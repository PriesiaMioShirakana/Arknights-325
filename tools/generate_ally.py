"""Compile chess loadouts and token owner/skill/module variants into immutable C++ records.

Port of shared/loadoutRecord.js and sim/simdata.js for the pinned generated data shape.
Only data is read; generic skill inference is compiled into typed static records.
"""
import argparse
import hashlib
import json
import math
import re
from pathlib import Path
from generate_combat import Tables, quote, number, boolean, record
from generate_profession import profile, traits
from generate_generic_skill import build_generic_skill, build_generic_talents
from generate_operator_kits import build_operator_kit


class AllyTables(Tables):
    """Many loadouts reuse identical grids/blackboards; share their static arrays."""
    def __init__(self):
        super().__init__()
        self.cache = {}
        self.diy_selections = set()

    def array(self, kind, rows):
        key = (kind, tuple(rows))
        if key not in self.cache:
            self.cache[key] = super().array(kind, rows)
        return self.cache[key]

    def grid(self, values):
        return self.array('RangeOffset', [record('RangeOffset', MyRow=str(int(r)), MyColumn=str(int(c))) for r,c in (values or [])])

    def strings(self, values):
        return self.array('std::string_view', [quote(v) for v in (values or [])])


def clean6(v):
    return v if abs(v) >= 1e6 or float(v).is_integer() else math.floor(v * 1e6 + 0.5) / 1e6


def options(raw):
    skills = raw.get('skills')
    default_skill = (raw.get('skill') or {}).get('index', next((s['index'] for s in (skills or []) if s.get('isDefault')), -1))
    modules = raw.get('modules')
    default_module = next((m['uniEquipId'] for m in (modules or []) if m.get('isDefault')), 'none') if modules is not None else ''
    return default_skill, default_module, sorted(set([default_skill] + [s['index'] for s in (skills or [])])), sorted(set([default_module] + (['none'] + [m['uniEquipId'] for m in modules] if modules is not None else [])))


def merge_talents(base, changes):
    talents = [dict(t) for t in base]
    for change in changes:
        ix = change['talentIndex']; rec = {k:v for k,v in change.items() if k != 'talentIndex'}
        rec.update(index=ix, fromModule=True)
        at = next((i for i,t in enumerate(talents) if ix >= 0 and t.get('index') == ix), None)
        if at is not None:
            old = talents[at]
            rec.update(name=rec.get('name') or old.get('name'), desc=rec.get('desc') if rec.get('desc') is not None else old.get('desc'),
                bb={**old.get('bb', {}), **rec.get('bb', {})}, bbStr={**old.get('bbStr', {}), **rec.get('bbStr', {})},
                rangeGrid=rec.get('rangeGrid') or old.get('rangeGrid'), tokenKey=rec.get('tokenKey') or old.get('tokenKey'))
            talents[at] = rec
        else: talents.append(rec)
    return [t for t in talents if t.get('name') or t.get('desc') or t.get('bb') or t.get('tokenKey')]


def compose(raw, si, mid):
    ds, dm, _, _ = options(raw)
    out = dict(raw)
    if mid != dm and 'modules' in raw:
        mod = next((m for m in raw['modules'] if m['uniEquipId'] == mid), None)
        stats = dict(raw.get('statsBase') or raw['stats'])
        for k,v in (mod or {}).get('attr', {}).items(): stats[k] = clean6((stats.get(k) or 0) + v)
        out['stats'] = stats
        out['trait'] = (mod or {}).get('traitOverride') or raw.get('traitBase') or raw.get('trait')
        out['talents'] = merge_talents(raw.get('talentsBase', raw.get('talents', [])), (mod or {}).get('talentChanges', []))
        out['module'] = dict(id=mod['uniEquipId'] if mod else None, active=bool(mod), level=(mod or {}).get('level', (raw.get('module') or {}).get('level', 0)))
    if si != ds:
        out['skill'] = next((s for s in raw.get('skills', []) if s['index'] == si), raw.get('skill'))
        tok = (out.get('skill') or {}).get('overrideTokenKey')
        if tok and tok in raw.get('tokens', []):
            out['talents'] = [{**t, 'tokenKey':tok} if t.get('containerTokenKey') else t for t in out.get('talents', [])]
    return out


def skill(tables, raw):
    if not raw or raw.get('skillId', '').startswith('skcom_withdraw'): return '{}'
    tr = raw.get('trigger') or {}
    rule = tr if isinstance(tr, str) else tr.get('rule', 'DEFAULT')
    rule = rule.upper()
    if rule in ('ALWAYS', 'SP_FULL'): rule = 'SP_FULL'
    if rule.startswith('CUSTOM_RANGE'): rule = 'CUSTOM_RANGE'
    kind = raw.get('skillType', 'MANUAL').upper()
    if not isinstance(tr, str) and tr.get('matchedBy', '').startswith(('profession', 'subProfession')) and kind != 'MANUAL': rule = 'DEFAULT'
    grid = None if isinstance(tr, str) else tr.get('customRangeGrid', tr.get('rangeGrid', tr.get('grid')))
    bb = raw.get('bb') or {}
    sp = raw.get('spType', '').upper()
    sp = 'TIME' if 'TIME' in sp else 'ATTACK' if 'ATTACK' in sp else 'HURT' if 'DAMAGE' in sp or 'HURT' in sp else 'NONE'
    return record('AllySkillRecord', MyId=quote(raw.get('skillId', 'skill')), MyName=quote(raw.get('name', '')), MyIndex=str(raw.get('index', -1)),
        MyType=quote(kind), MyDurationType=quote(raw.get('durationType', 'NONE').upper()), MyDuration=number(raw.get('duration', bb.get('duration', 0))),
        MySpType='SpType::'+sp, MySpCost=number(max(0, raw.get('spCost', 0))), MyInitialSp=number(max(0, raw.get('initSp', 0))),
        MyMaxCharges=str(max(1, int(raw.get('maxChargeTime', 1)))), MyRange=tables.grid(raw.get('rangeGrid')), MyHasRange=boolean(raw.get('rangeGrid') is not None),
        MyTrigger=quote(rule), MyTriggerRange=tables.grid(grid), MyHasTriggerRange=boolean(grid is not None), MyTriggerAllies=boolean(isinstance(tr,dict) and tr.get('allies') is True),
        MyBlackboard=tables.blackboard(bb), MyStrings=tables.blackboard(raw.get('bbStr')), MyDescription=quote(raw.get('description') if raw.get('description') is not None else raw.get('desc') or ''))


def doll_health(tables, key, raw, selection):
    """Resolve only this operator's listed doll token using DataSource.getToken loadout semantics."""
    if raw.get('subProfessionId') != 'dollkeeper': return 1
    token_id=next((t for t in raw.get('tokens',[]) if re.search(r'shadow|doll',t)),None)
    token=tables.tokens.get(token_id)
    if not token:return 1
    si,mid=selection or ((raw.get('skill') or {}).get('index',-1),(raw.get('module') or {}).get('id') or 'none')
    variants=token.get('variants') or {};variant={}
    if raw.get('isDiy'):
        owner=raw['charId']+'@'+status_key(raw)
        variant=dict(variants.get(owner) or {})
        variant.update((variant.get('bySkill') or {}).get(str(si),{}))
        variant.update((variant.get('byModule') or {}).get(mid,{}))
    else:
        owner=key if key in variants else re.sub(r'_b$','_a',key)
        if owner not in variants:owner=next(iter(variants),'')
        variant=dict(variants.get(owner) or {})
        ds,dm,skills,modules=options(tables.chess[key])
        si=si if si in skills else ds;mid=mid if mid in modules else dm
        if si!=ds:variant.update((variant.get('bySkill') or {}).get(str(si),{}))
        if mid!=dm:variant.update((variant.get('byModule') or {}).get(mid,{}))
    merged={**token,**variant};hp=max(1,(merged.get('stats') or {}).get('maxHp',1000))
    return max(0.05,hp/max(1,(raw.get('stats') or {}).get('maxHp',1000)))


def preparation_range(raw):
    """shared/loadoutRecord.js attackRangeGrid; field previews include permanent range changes."""
    grid = raw.get('rangeGrid') or []
    skill = raw.get('skill') or {}
    module = raw.get('module') or {}
    trait = raw.get('trait') or {}
    if skill.get('rangeGrid') and '被动效果：攻击范围扩大' in (skill.get('desc') or ''):
        grid = skill['rangeGrid']
    elif raw.get('isGolden') and module.get('active') and module.get('id') and '攻击范围扩大' in (trait.get('moduleDesc') or ''):
        mod = next((m for m in raw.get('modules', []) if m['uniEquipId'] == module['id']), {})
        grid = next((t['rangeGrid'] for t in mod.get('talentChanges', []) if t.get('talentIndex') == -1 and t.get('rangeGrid')), grid)
    extend = math.floor((trait.get('bb') or {}).get('ability_range_forward_extend') or 0) if '集成战略' not in (trait.get('moduleDesc') or '') else 0
    if extend > 0:
        unique = list(dict.fromkeys(tuple(p) for p in grid))
        rows = {}
        for dr, dc in grid: rows[dr] = max(rows.get(dr, dc), dc)
        for dr, end in rows.items():
            for k in range(1, min(extend, 21) + 1):
                if (dr, end + k) not in unique: unique.append((dr, end + k))
        return unique
    return grid


def preparation_placement(raw):
    trait = raw.get('traitBase') or raw.get('trait') or {}
    if raw.get('rangedTilesOnly'): return 'HIGH_ONLY'
    widened = raw.get('position') == 'MELEE' and '可以放置于远程位' in (trait.get('desc') or '')
    return 'MELEE' if str(raw.get('position', '')).upper() == 'MELEE' and not widened else 'ANY'


def body(tables, key, raw, token=False, abnormal=(), selection=None):
    st = raw.get('stats') or {}
    stats = record('CombatStats', MyMaxHealth=number(max(1, st.get('maxHp', 1000))), MyAttack=number(max(0, st.get('atk', 0))),
        MyDefense=number(max(0, st.get('def', 0))), MyResistance=number(max(0, st.get('res', 0))), MyAttackSpeed=number(st.get('aspd', 100) or 100),
        MyBaseAttackTime=number(max(0.05, st.get('bat', 1))), MyMoveSpeed=number(st.get('moveSpeed', 1)), MyBlockCount=str(int(max(0, st.get('blockCnt', 1)))),
        MyTaunt=number(st.get('tauntLevel', 0)), MyRedeploySeconds=number(max(0, st.get('respawnTime', 70))), MyDeploymentCost=number(max(0, st.get('cost', 10))),
        MySpRecovery=number(st.get('spRecovery', 1)), MyHealthRegen=number(st.get('hpRecoveryPerSec', 0)), MyMass=number(st.get('massLevel', 0)))
    trait = raw.get('trait') or {}
    trait_text = trait if isinstance(trait, str) else trait.get('desc', '')
    desc = raw.get('description', raw.get('desc', trait_text)) if token else trait_text
    talents = raw.get('talents') or []
    if isinstance(talents, dict): talents = [talents]
    talents = tables.array('AllyTalentRecord', [record('AllyTalentRecord', MyName=quote(t.get('name') or ''),
        MyDescription=quote(t.get('desc', t.get('description', '')) or ''), MyBlackboard=tables.blackboard(t.get('bb')),
        MyStrings=tables.blackboard(t.get('bbStr')), MyRange=tables.grid(t.get('rangeGrid')), MyHasRange=boolean(t.get('rangeGrid') is not None),
        MyTokenKey=quote(t.get('tokenKey') or '')) for t in talents])
    status = {'stun':'STUN', 'silence':'SILENCE', 'sleep':'SLEEP', 'frozen':'FREEZE', 'levitate':'LEVITATE'}
    flags = lambda names: ' | '.join('(std::uint64_t{1} << static_cast<unsigned>(CombatStatus::'+n+'))' for n in names) or '0'
    immune = flags(status[k] for k,v in (raw.get('immunities') or {}).items() if v)
    starting = []
    if token:
        if raw.get('untargetable', '不会受到攻击' in desc or '不会成为' in desc): starting.append('UNTARGETABLE')
        if 'healFree' in abnormal or 'isolated' in abnormal: starting.append('NO_HEAL')
        if 'isolated' in abnormal: starting.append('ISOLATED')
    damage = raw.get('dmgType') or ('arts' if '法术' in desc else 'true' if '真实' in desc else 'heal' if '恢复' in desc or '治疗' in desc else 'phys') if token else raw.get('dmgType') or ''
    raw_skill = raw.get('skill') or {}
    return record('AllyRecord', MyId=quote(key), MyName=quote(raw.get('name') or key), MyCharacterId=quote(key if token else raw.get('charId') or ''),
        MyProfession=quote((raw.get('profession') or ('TOKEN' if token else 'WARRIOR')).upper()), MySubProfession=quote(raw.get('subProfessionId') or ''),
        MyPosition=quote((raw.get('position') or 'MELEE').upper()), MyStats=stats, MyRange=tables.grid(raw.get('rangeGrid') if raw.get('rangeGrid') is not None else [[0,0],[0,1]]),
        MyDamageType=quote(damage), MyAttackKind=quote(raw.get('attackKind') or ''), MyProjectile=quote(raw.get('projectile') or ''),
        MyCanHitFlying=boolean(raw['canHitFly']) if raw.get('canHitFly') is not None else '{}', MyTargetPriority=quote(raw.get('targetPriority') or ''),
        MyTraitText=quote(desc), MyTraitBlackboard=tables.blackboard(trait.get('bb') if isinstance(trait,dict) else {}), MyImmunities=immune,
        MySkill=skill(tables, raw_skill), MyTalents=talents, MyTokens=tables.strings([] if token else raw.get('tokens')),
        MyStartingFlags=flags(starting), MyWithdrawDuration=number(raw_skill.get('duration', 0)) if raw_skill.get('skillId','').startswith('skcom_withdraw') else '{}',
        MyBaseAttack=profile(raw,token), MyTraitFrontRange=tables.grid(trait.get('rangeGrid') if isinstance(trait,dict) else None),
        MyHasTraitFrontRange=boolean(isinstance(trait,dict) and trait.get('rangeGrid') is not None), MyProfessionTraits=traits(raw, doll_health(tables,key,raw,selection) if not token else 1),
        MyPreparationPlacement='PlacementClass::'+preparation_placement(raw), MyPreparationRange=tables.grid(preparation_range(raw)),
        MyGenericSkill=build_generic_skill(tables, raw, damage, token), MyGenericTalents=build_generic_talents(tables, raw.get('talents')),
        MyOperatorKit=build_operator_kit(tables, key, raw, token), MyOperatorProfession='OperatorProfession::'+
            (raw['profession'].upper() if (raw.get('profession') or '').upper() in {'PIONEER','WARRIOR','TANK','SNIPER','CASTER','MEDIC','SUPPORT','SPECIAL'} else 'NONE'))


def operator(tables, key, raw, backups, chess):
    ds, dm, skills, modules = options(raw)
    loadouts=[]
    for si in skills:
        for mid in modules:
            data=compose(raw, si, mid); mod=data.get('module') or {}
            loadouts.append(record('AllyLoadoutRecord', MySkillIndex=str(si), MyModuleId=quote(mid), MySkillDefault=boolean(si==ds), MyModuleDefault=boolean(mid==dm),
                MyBody=body(tables,key,data,selection=(si,mid)), MyEquippedModuleId=quote(mod.get('id') or ''), MyModuleLevel=str(mod.get('level',0)), MyModuleActive=boolean(mod.get('active',False))))
    return record('OperatorRecord', MyId=quote(key), MyBaseId=quote(raw['baseId']), MyGolden=boolean(raw['isGolden']), MyDiy=boolean(raw['isDiy']), MyTier=str(raw['tier']),
        MyBonds=tables.strings(raw['bonds']), MyGarrisons=tables.strings(raw['garrisonIds']), MyDefaultSkill=str(ds), MyDefaultModule=quote(dm), MyLoadouts=tables.array('AllyLoadoutRecord',loadouts),
        MyStandIn=stand_in(tables, key, raw, backups), MyDiyChoices=diy_choices(tables, key, raw, backups, chess))


def token(tables,key,raw,chess):
    variants=[]
    for owner,variant in (raw['variants'] or {'':{}}).items():
        ds,dm,skills,modules=options(chess[owner]) if owner in chess else (-1,'',[-1,*sorted(int(k) for k in variant.get('bySkill',{}))],['',*sorted(variant.get('byModule',{}))])
        for si in skills:
            for mid in modules:
                selected=dict(variant)
                if si!=ds: selected.update((selected.get('bySkill') or {}).get(str(si),{}))
                if mid!=dm: selected.update((selected.get('byModule') or {}).get(mid,{}))
                data={**raw,**selected}
                sources=data.get('sources') if owner else None
                variants.append(record('TokenVariantRecord',MyOwnerId=quote(owner),MySkillIndex=str(si),MyModuleId=quote(mid),MyDefault=boolean(si==ds and mid==dm),
                    MyBody=body(tables,key,data,True,raw.get('abnormal',[])),MyCount=number(data.get('count') if data.get('count') is not None else 1),
                    MySources=tables.strings(sources),MyHasSources=boolean(sources is not None)))
    diy_variants=[]
    for owner,si,mid in sorted(tables.diy_selections):
        if owner not in raw['variants']:continue
        selected=dict(raw['variants'][owner])
        selected.update((selected.get('bySkill') or {}).get(str(si),{}))
        if mid!='none':selected.update((selected.get('byModule') or {}).get(mid,{}))
        data={**raw,**selected}
        sources=data.get('sources')
        diy_variants.append(record('TokenVariantRecord',MyOwnerId=quote(owner),MySkillIndex=str(si),MyModuleId=quote(mid),
            MyBody=body(tables,key,data,True,raw.get('abnormal',[])),MyCount=number(data.get('count') if data.get('count') is not None else 1),
            MySources=tables.strings(sources),MyHasSources=boolean(sources is not None)))
    unowned=record('TokenVariantRecord',MyBody=body(tables,key,raw,True,raw.get('abnormal',[])),MyCount=number(raw.get('count') if raw.get('count') is not None else 1))
    return record('TokenRecord',MyId=quote(key),MyPlaceable=boolean(raw['placeable']),MyOwnerRange=boolean(raw['ownerRange']),MyDeployLimit=number(raw.get('deployLimit') or 0),
        MyFallbackOwner=quote(next(iter(raw['variants']),'')),MyVariants=tables.array('TokenVariantRecord',variants), MyDiyVariants=tables.array('TokenVariantRecord',diy_variants),MyUnowned=unowned)


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--data',required=True,type=Path);parser.add_argument('--out',required=True,type=Path);parser.add_argument('--shards',type=int,default=12);args=parser.parse_args()
    if args.shards < 1: parser.error('--shards must be positive')
    chess_bytes=(args.data/'chess.json').read_bytes(); token_bytes=(args.data/'tokens.json').read_bytes()
    backups_bytes=(args.data/'backups.json').read_bytes();backups=json.loads(backups_bytes)
    chess=json.loads(chess_bytes);tokens={**backups['tokens'],**json.loads(token_bytes)};tables=AllyTables();tables.chess=chess;tables.tokens=tokens
    op=tables.array('OperatorRecord',[operator(tables,k,v,backups,chess) for k,v in sorted(chess.items())])
    tok=tables.array('TokenRecord',[token(tables,k,v,chess) for k,v in sorted(tokens.items())])
    # External constant arrays allow every translation unit to reference shared grids by address.
    # No array values are read during another table's constant initialization; only static spans are formed.
    header=args.out.with_name(args.out.stem+'_data.hpp')
    declarations=['// Generated declarations for statically initialized ally data.', '#pragma once', '#include <stronghold/adapters/reference_ally.hpp>', 'namespace Stronghold::AllyData {']
    shards=[[] for _ in range(args.shards)];sizes=[0]*args.shards
    for line in tables.lines:
        signature=line.split('\n',1)[0]
        matched=re.fullmatch(r'constexpr std::array<(.+), (\d+)> (\w+)\{\{',signature)
        if not matched: raise ValueError('invalid generated table declaration')
        kind,count,name=matched.groups()
        declarations.append(f'extern const std::array<{kind}, {count}> {name};')
        index=min(range(args.shards),key=lambda i:sizes[i])
        shards[index].append(line);sizes[index]+=len(line)
    declarations.append('}')
    args.out.parent.mkdir(parents=True,exist_ok=True)
    header.write_text('\n'.join(declarations)+'\n',encoding='utf-8')
    for i,part in enumerate(shards):
        path=args.out.with_name(args.out.stem+f'_{i}.cpp')
        path.write_text('\n'.join([f'#include "{header.name}"', 'namespace Stronghold::AllyData {',*part,'}'])+'\n',encoding='utf-8')
    lines=['// Generated from chess.json, tokens.json and backups.json; edit tools/generate_ally.py.', f'#include "{header.name}"',
        'namespace Stronghold {',f'std::span<const OperatorRecord> ReferenceOperators() noexcept {{ return AllyData::{op}; }}',
        f'std::span<const TokenRecord> ReferenceTokens() noexcept {{ return AllyData::{tok}; }}',
        f'std::string_view ReferenceAllyFingerprint() noexcept {{ return "{hashlib.sha256(chess_bytes+token_bytes+backups_bytes).hexdigest()}"; }}','}']
    args.out.write_text('\n'.join(lines)+'\n',encoding='utf-8')



from generate_roster import stand_in, diy_choices, status_key

if __name__=='__main__': main()
