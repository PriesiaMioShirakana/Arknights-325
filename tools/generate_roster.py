"""Static stand-in and DIY compositions: shared/standIn.js and shared/diy.js.

The roster tables enumerate valid data selections only. Match-wide uniqueness and ownership
remain preparation rules; combat kits are not inferred from a selected character's text.
"""
import re
from generate_combat import quote, boolean, record


def helpers():
    # When generate_ally runs as a script its helpers live in __main__; do not import/run it twice.
    import sys
    module = sys.modules.get('__main__')
    if not hasattr(module, 'AllyTables'):
        import generate_ally as module
    return module


def status_key(raw):
    st=raw.get('status') or {}
    return '/'.join(str(st.get(k,d)) for k,d in [('phase',0),('level',1),('skillLevel',1),('equipLevel',0)])


def natural(value):
    return tuple(int(v) if v.isdigit() else v for v in re.split(r'(\d+)',value))


def compose_form(identity, unit, form, si, mid):
    h=helpers();out=dict(identity);level=(identity.get('status') or {}).get('equipLevel') or 0
    skill=next((s for s in form.get('skills',[]) if s['index']==si),None)
    mod=next((m for m in form.get('modules',[]) if m['uniEquipId']==mid),None) if mid else None
    talents=h.merge_talents(form.get('talents',[]),(mod or {}).get('talentChanges',[])) if mod else form.get('talents',[])
    stats=dict(form['stats'])
    for k,v in (mod or {}).get('attr',{}).items(): stats[k]=h.clean6((stats.get(k) or 0)+v)
    for k in ('charId','name','appellation','rarity','profession','subProfessionId','subProfessionName','position','nationId'):out[k]=unit.get(k)
    for k in ('immunities','rangeId','rangeGrid','dmgType','attackKind','projectile','canHitFly','targetPriority'):out[k]=form.get(k)
    known=set(form.get('tokens',[]))
    candidates=[*form.get('displayTokens',[]),(skill or {}).get('overrideTokenKey'),*[t.get('tokenKey') for t in talents]]
    out.update(statsBase=form['stats'],talentsBase=form.get('talents',[]),traitBase=form.get('trait'),
        skills=form.get('skills',[]),modules=form.get('modules') or [],stats=stats,trait=(mod or {}).get('traitOverride') or form.get('trait'),skill=skill,talents=talents,
        tokens=sorted(set(t for t in candidates if t and t in known),key=natural),
        module=dict(id=mid,level=level,active=level>0 and bool(mod)) if mid or level>0 else None)
    if identity.get('isGolden') and level > 0:
        out.update(traitBase=form.get('trait'), modules=form.get('modules') or [])
    return out


def selection(tables,key,raw,si,mid):
    h=helpers();mod=raw.get('module') or {}
    # All composed records have this selection as their own default; the normal form has no module choices.
    default_mid=mid if raw['isGolden'] and raw['status']['equipLevel']>0 and mid else ('none' if raw['isGolden'] and raw['status']['equipLevel']>0 else '')
    return record('AllyLoadoutRecord',MySkillIndex=str(si),MyModuleId=quote(default_mid),MySkillDefault='true',MyModuleDefault='true',
        MyBody=h.body(tables,key,raw,selection=(si,mid)),MyEquippedModuleId=quote(mod.get('id') or ''),MyModuleLevel=str(mod.get('level',0)),MyModuleActive=boolean(mod.get('active',False)))


def stand_in(tables,key,raw,backups):
    b=raw.get('backup')
    if not b or raw.get('chessType')!='NORMAL' or not b.get('charId') or b['charId']==raw.get('charId'): return '{}'
    unit=backups['units'].get(b['charId']);form=(unit or {}).get('forms',{}).get(status_key(raw))
    if not unit or not form:return '{}'
    si=b['skillIndex'];mid=b.get('uniEquipId');data=compose_form(raw,unit,form,si,mid)
    return selection(tables,key,data,si,mid)


def diy_choices(tables,key,raw,backups,chess):
    if not raw.get('isDiy'):return '{}'
    diy=backups['diy'];tier=str(raw['tier']);proto=diy['prototypes'][tier]
    pool=list(dict.fromkeys([*proto,*diy['ownedPool']]))
    normal=chess[raw['baseId']];elite=chess[normal['goldenId']]
    rows=[]
    for char in sorted(pool):
        unit=backups['units'].get(char)
        forms=[(unit or {}).get('forms',{}).get(status_key(r)) for r in (normal,elite)]
        if not all(forms):continue
        skill_ids=sorted(set(s['index'] for s in forms[0].get('skills',[])) & set(s['index'] for s in forms[1].get('skills',[])))
        modules=[None,*[m['uniEquipId'] for m in forms[1].get('modules',[])]]
        if char in proto:
            locked=diy['locked'][tier].get(char)
            picks=[(locked['skillIndex'],locked.get('uniEquipId'))] if locked else []
        else:picks=[(si,mid) for si in skill_ids for mid in modules]
        for si,mid in picks:
            if si not in skill_ids or mid not in modules:continue
            data=compose_form(raw,unit,forms[1 if raw['isGolden'] else 0],si,mid)
            if not data['skill']:continue
            tables.diy_selections.add((char+'@'+status_key(raw),si,mid or 'none'))
            bonds=(diy['operators'].get(char) or {}).get('bonds',raw['bonds'])
            rows.append(record('DiyChoiceRecord',MyCharacterId=quote(char),MySkillIndex=str(si),MyModuleId=quote(mid or 'none'),MyPrototype=boolean(char in proto),
                MyBonds=tables.strings(bonds),MyTokenOwner=quote(char+'@'+status_key(raw)),MyLoadout=selection(tables,key,data,si,mid)))
    return tables.array('DiyChoiceRecord',rows)
