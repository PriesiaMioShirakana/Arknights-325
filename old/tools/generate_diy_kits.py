"""Compile DIY character rules independently of the chess slot's original identity."""
import math
from generate_combat import record, number, boolean, quote
from generate_generic_skill import generic_modifiers, numeric


DIY_KINDS = {'char_188_helage':'HELAGE', 'char_416_zumama':'ZUMAMA', 'char_112_siege':'SIEGE',
    'char_147_shining':'SHINING', 'char_179_cgbird':'CGBIRD', 'char_4011_lessng':'LESSNG',
    'char_340_shwaz':'SHWAZ', 'char_377_gdglow':'GDGLOW', 'char_197_poca':'POCA', 'char_485_pallas':'PALLAS',
    'char_180_amgoat':'AMGOAT', 'char_010_chen':'CHEN'}


def build_diy_operator_kit(tables, raw):
    name = DIY_KINDS.get(raw.get('charId'))
    if name is None: return None
    skill=raw.get('skill') or {};bb=skill.get('bb') or {};sid=skill.get('skillId','')
    mode=int(sid[-1]) if sid[-1:].isdigit() else 1
    def n(values,key,default=0):
        value=numeric(values.get(key));return default if value is None else value
    def talent(index,base=False):
        return next((t.get('bb') or {} for t in raw.get('talentsBase' if base else 'talents',[]) if t.get('index')==index),{})
    t0,t1,t2=talent(0),talent(1),talent(2)
    tb=(raw.get('trait') or {}).get('bb') or {};hb={}
    for t in raw.get('talents',[]):
        if t.get('index')==-1:
            for k,v in (t.get('bb') or {}).items():hb[k]=v
    mod=raw.get('module') or {}
    selected_module=next((m for m in raw.get('modules',[]) if m.get('uniEquipId')==mod.get('id')),{}) if mod.get('active') else {}
    if name=='ZUMAMA' and '生息演算' in str((selected_module.get('traitOverride') or {}).get('moduleDesc','')):
        t0,t1=talent(0,True),talent(1,True)
    rules=dict(MyKind='DiyOperatorKind::'+name,MySkill=str(mode)+'U')
    permanent,auras=[],[]
    def r(key,value):rules[key]=number(value)
    def stats(key,mods):permanent.append(record('GenericTalentRecord',MyKey=quote(key),MyModifiers=generic_modifiers(tables,mods)))
    def aura(key,attr,value,extras=None,ground=0,profession=None,attack_range=False,active=False,interval=.2,duration=.25,strength=None,carried=False):
        auras.append(record('FixedOperatorAura',MyKey=quote(key),MyAttribute='Attribute::'+attr,MyValue=number(value),MyGroundExtra=number(ground),
            MyModifiers=generic_modifiers(tables,extras or {}),MyProfession='OperatorProfession::'+profession if profession else '{}',
            MyAttackRange=boolean(attack_range),MySkillActive=boolean(active),MyInterval=number(interval),MyDuration=number(duration),MyStrength=number(strength) if strength is not None else '{}',MyCarriedSource=boolean(carried)))
    mods,kind,grid,extend={},'DURATION',None,0
    targets,hits,scale,heal_scale,trigger='{}','{}','{}','{}','{}'
    priority='{}';no_attack=False
    has_attack,healing=False,name in ('SHINING','CGBIRD')
    base_grid=None
    duration_value='{}'
    rules['MyBlockingModifiers']=generic_modifiers(tables,dict(atkPct=n(tb,'atk'),defPct=n(tb,'def')) if name in ('SIEGE','ZUMAMA') else {})
    if name=='HELAGE':
        r('MyBerserkSpeed',n(t0,'min_attack_speed'));r('MyBerserkHealthRatio',n(t0,'min_hp_ratio'))
        r('MyRegen',n(t1,'hp_recovery_per_sec'));r('MyLowHealthRegen',n(t2,'hp_recovery_per_sec'));r('MyRegenHealthRatio',n(t2,'hp_ratio'))
        r('MyProtection',n(t0,'damage_resistance'));r('MyProtectHealthRatio',n(t0,'hp_ratio'));r('MyReviveHealthRatio',n(tb,'hp_ratio'))
        has_attack=True
        if mode==1:kind='CHARGES' if n(skill,'maxChargeTime',1)>1 else 'INSTANT';scale=number(n(bb,'atk_scale',1));hits='2U'
        elif mode==2:mods=dict(atkPct=n(bb,'atk'),dodgePhys=n(bb,'prob'));hits='2U'
        else:mods=dict(atkPct=n(bb,'atk'));extend=math.floor(n(bb,'ability_range_forward_extend'));targets=str(max(1,math.floor(n(bb,'attack@max_target',1))))+'U'
    elif name=='ZUMAMA':
        r('MyProtection',n(t0,'damage_resistance'));r('MyProtectHealthRatio',n(t0,'hp_ratio',.5));r('MyHighHealthScale',n(t0,'atk_scale',1));r('MyBlockingSp',n(t1,'sp_recovery_per_sec'))
        rules['MyUnblockedSpMultiplier']=number(max(0,1+n(tb,'sp_recover_ratio'))) if 'sp_recover_ratio' in tb else '{}'
        if mode==1:kind='PASSIVE';mods=dict(atkPct=n(bb,'atk'),defPct=n(bb,'def'))
        elif mode==2:
            bat=n(raw.get('stats') or {},'bat',1) or 1;mods=dict(atkPct=n(bb,'atk'),batPct=max(-.9,n(bb,'base_attack_time')/bat))
        else:
            mods=dict(atkPct=n(bb,'atk'),defPct=n(bb,'def'),blockCnt=n(bb,'block_cnt'),hpRegenRatio=n(bb,'hp_recovery_per_sec_by_max_hp_ratio'));r('MyEndStun',n(bb,'stun'))
    elif name=='SIEGE':
        base=talent(0,True) or t0
        extra=next((t.get('bb') or {} for t in selected_module.get('talentChanges',[]) if t.get('talentIndex')==0),{})
        stats('talent:siege:self',dict(atkPct=n(extra,'atk'),defPct=n(extra,'def')))
        aura('talent:siege:pioneers','ATTACK_PERCENT',n(base,'atk'),dict(defPct=n(base,'def')),profession='PIONEER',interval=.5,duration=.6)
        r('MyKillSp',n(t1,'sp'));r('MyOtherKillSp',n(hb,'sp'))
        tg=next((t.get('rangeGrid') for t in raw.get('talents',[]) if t.get('index')==1),None)
        rules['MyKillSpRange']=tables.grid(tg or [[1,0],[0,-1],[0,0],[0,1],[-1,0]])
        r('MyDp',n(bb,'cost'))
        if mode==1:kind='INSTANT';trigger='SkillTrigger::SP_FULL'
        elif mode==2:
            kind='CHARGES' if n(skill,'maxChargeTime',1)>1 else 'INSTANT';has_attack=True;scale=number(n(bb,'atk_scale',1));grid=skill.get('rangeGrid') or [[1,0],[0,-1],[0,0],[0,1],[-1,0]]
        else:
            bat=n(raw.get('stats') or {},'bat',1) or 1;mods=dict(batPct=max(-.9,n(bb,'base_attack_time')/bat));has_attack=True;scale=number(n(bb,'attack@atk_scale',1));r('MyStunChance',n(bb,'attack@buff_prob'));r('MyStunDuration',n(bb,'attack@stun'))
    elif name=='SHINING':
        aura('talent:shining:def','DEFENSE_FLAT',n(t0,'def'),ground=n(t0,'def_lowland'),attack_range=True)
        stats('talent:shining:aspd',dict(aspd=n(t1,'attack_speed')))
        if mode==2:stats('talent:shining:s2',dict(atkPct=n(hb,'atk')))
        if mode==3:stats('talent:shining:s3',dict(spRecoveryFlat=n(hb,'sp_recovery_per_sec')))
        mods=dict(atkPct=n(bb,'atk')) if mode!=2 else {}
        if mode==1:mods['aspd']=n(bb,'attack_speed')
        if mode==2:
            kind='CHARGES' if n(skill,'maxChargeTime',1)>1 else 'INSTANT';has_attack=True;heal_scale='1.0'
            r('MyBarrierScale',n(bb,'atk_scale'));r('MyBarrierDuration',n(bb,'duration'));rules['MyBarrierModifiers']=generic_modifiers(tables,dict(defPct=n(bb,'def')))
        if mode==3:aura('skill:shining:def','DEFENSE_PERCENT',n(bb,'def'),attack_range=True,active=True)
        r('MyHealScale',n(tb,'heal_scale',1));r('MyHealHealthRatio',n(tb,'hp_ratio'));rules['MyHealGround']=boolean(mod.get('active') and mod.get('id')=='uniequip_002_shining')
    elif name=='CGBIRD':
        aura('talent:cgbird:res','RESISTANCE_FLAT',n(t0,'magic_resistance'),dict(healingTakenMul=n(t0,'heal_scale',1)),attack_range=True,strength=n(t0,'magic_resistance')+(n(t0,'heal_scale',1)-1)*100)
        mods=dict(atkPct=n(bb,'atk')) if mode!=2 else {}
        if mode==2:
            kind='CHARGES' if n(skill,'maxChargeTime',1)>1 else 'INSTANT';has_attack=True;heal_scale='1.0'
            r('MyBarrierScale',n(bb,'atk_scale'));r('MyBarrierDuration',n(bb,'duration'));rules['MyBarrierModifiers']=generic_modifiers(tables,dict(resFlat=n(bb,'magic_resistance')))
        if mode==3:
            grid=skill.get('rangeGrid');aura('skill:cgbird:sanctuary','RESISTANCE_MULTIPLIER',1+n(bb,'magic_resistance'),dict(dodgeArts=n(bb,'prob')),attack_range=True,active=True)
        rules['MyToken']=quote(next((t.get('tokenKey') for t in raw.get('talents',[]) if t.get('tokenKey')),'token_10003_cgbird_bird'))
        rules['MyTokenStock']=str(max(0,math.floor(n(t1,'cnt'))))+'U'
        if mod.get('active') and '攻击范围扩大' in str((raw.get('trait') or {}).get('moduleDesc','')):
            base_grid=next((t.get('rangeGrid') for t in selected_module.get('talentChanges',[]) if t.get('talentIndex')==-1 and t.get('rangeGrid')),None)
        extra_heals=math.floor(n(hb,'attack@max_target'))
        if extra_heals>0:rules['MyBaseHealTargets']=str(extra_heals)+'U'
    elif name=='LESSNG':
        r('MyProtection',n(t0,'damage_resistance'));r('MyBlockedScale',n(tb,'atk_scale',1));r('MyBlockedDefenseIgnore',n(hb,'def_penetrate'))
        r('MyPainAttack',n(t1,'atk'));r('MyPainDuration',n(t1,'add_atk_duration',15))
        if 'hp_ratio' in tb and ('max_hp' in tb or 'value' in tb):
            r('MyReviveHealthRatio',n(tb,'hp_ratio',1));rules['MyReviveModifiers']=generic_modifiers(tables,dict(hpMul=max(.05,n(tb,'max_hp',1-n(tb,'value',.6))),aspd=n(tb,'attack_speed')))
        if mode==1:kind='INSTANT';has_attack=True;scale=number(n(bb,'atk_scale',1))
        elif mode==2:
            kind='PASSIVE';r('MyDuelScale',n(bb,'talent_scale',1));r('MyDuelDuration',n(skill,'duration'));r('MyDuelAttack',n(bb,'atk'))
        else:
            r('MyOathBlockedScale',n(bb,'lessng_s3[atk_scale].atk_scale',1));r('MyOathSelfDamage',n(bb,'magical_value'));r('MyOathHealth',n(bb,'max_hp'))
    elif name=='SHWAZ':
        pierce=dict(talent(0,True) or t0)
        for change in selected_module.get('talentChanges',[]):
            if change.get('talentIndex')==0 and not change.get('hidden') and change.get('name'):pierce.update(change.get('bb') or {})
        r('MyProbability',n(pierce,'prob'));r('MySkillProbability',n(bb,'talent@prob',n(pierce,'prob')))
        r('MyCriticalScale',n(pierce,'atk_scale',1));r('MyDefenseCut',-n(pierce,'def'));r('MyDefenseCutDuration',n(pierce,'defdown_duration',5));r('MyFrontScale',n(tb,'atk_scale',1))
        cross=next((t for t in raw.get('talents',[]) if t.get('index')==1),{})
        r('MyCrossfireAttack',n(t1,'atk'));rules['MySquadCrossfire']=boolean('携带' in str(cross.get('desc','')))
        has_attack=True
        if mode==1:kind='INSTANT';scale=number(n(bb,'atk_scale',1))
        else:
            mods=dict(atkPct=n(bb,'atk'))
            if mode==3:
                bat=n(raw.get('stats') or {},'bat',1) or 1;mods['batPct']=max(-.9,n(bb,'base_attack_time')/bat)
                grid=skill.get('rangeGrid') or [[0,0],[0,1],[0,2],[0,3]]
    elif name=='GDGLOW':
        stats('talent:gdglow:pen',dict(resIgnoreFlat=n(t1,'magic_resist_penetrate_fixed')))
        r('MyProbability',n(t0,'attack@prob'));r('MyBlastScale',n(t0,'attack@atk_scale_2'))
        rules['MyMaxStacks']=str(max(1,math.floor(n(t0,'attack@max_stack_cnt',40))))+'U'
        rules['MyDrones']=str(1+max(0,math.floor(n(bb,'attack@cnt'))))+'U'
        mods=dict(atkPct=n(bb,'atk'));has_attack=True;no_attack=True
        if mode==1:mods['aspd']=n(bb,'attack_speed')
        elif mode==2:kind='TOGGLE';trigger='SkillTrigger::SP_FULL';grid=skill.get('rangeGrid')
        else:grid=[[r,c] for r in range(-18,19) for c in range(-20,21)];r('MySluggish',n(bb,'attack@sluggish'))
    elif name=='POCA':
        priority='TargetPriority::HEAVIEST'
        r('MyHeavyMass',n(t0,'value',3));r('MyHeavyPenetration',n(t0,'def_penetrate'));r('MyHeavyScale',n(t0,'atk_scale',1) if mod.get('active') else 1);r('MyHeavyExtra',n(t0,'extra_atk_scale'))
        r('MyDistanceScale',n(tb,'damage_scale'));r('MyDistanceMinimum',n(tb,'min_dist',1));r('MyDistanceMaximum',n(tb,'max_dist',4.5))
        r('MyStudentAttack',n(t1,'atk'));r('MyStudentPerSkill',n(hb,'init_atk'))
        if 'max_atk' in hb:r('MyStudentSkillCap',n(hb,'max_atk'))
        mods=dict(atkPct=n(bb,'atk'))
        if mode==2:targets=str(max(1,math.floor(n(bb,'attack@max_target',2))))+'U'
        if mode==3:
            has_attack=True;no_attack=True
            rules['MyLinkTargets']=str(max(1,math.floor(n(bb,'max_target',3))))+'U'
            iv=max(.1,n(bb,'hit_interval',1));r('MyLinkInterval',iv)
            rules['MyLinkStrikes']=str(max(1,math.floor(n(bb,'hit_duration',n(skill,'duration'))/iv)))+'U'
    elif name=='PALLAS':
        r('MyPeakHealthRatio',n(t0,'peak_performance.hp_ratio',.8));r('MyPeakAttack',n(t0,'peak_performance.atk'))
        r('MyTalentHeal',n(t1,'value'));r('MyNationHeal',n(t1,'pallas_e_t_2.value'))
        has_attack=True
        if mode==1:kind='INSTANT';scale=number(n(bb,'atk_scale',1));hits='2U'
        else:
            mods=dict(atkPct=n(bb,'atk'))
            if mode==2:
                extend=max(0,math.floor(n(bb,'ability_range_forward_extend')+.5));r('MyStunChance',n(bb,'attack@buff_prob'));r('MyStunDuration',n(bb,'attack@stun'))
            else:
                targets=str(max(1,math.floor(n(bb,'attack@max_target',3))))+'U'
                r('MyBlessHealthRatio',n(bb,'attack@peak_performance.hp_ratio',.8));r('MyBlessAttack',n(bb,'attack@peak_performance.atk'))
                rules['MyBlessModifiers']=generic_modifiers(tables,dict(defPct=n(bb,'attack@def'),blockCnt=n(bb,'attack@block_cnt')))
    elif name=='AMGOAT':
        visible=next((t for t in raw.get('talents',[]) if t.get('index')==0),{})
        aura('talent:amgoat:casters','ATTACK_PERCENT',n(t0,'atk'),profession='CASTER',interval=.5,duration=.6,
            carried='携带时' in str(visible.get('desc','')))
        for field,key in [('MyInitialSpMinimum','sp_min'),('MyInitialSpMaximum','sp_max'),
            ('MyInitialSpeedMinimum','attack_speed_min'),('MyInitialSpeedMaximum','attack_speed_max')]:r(field,n(t1,key))
        rules['MyInitialEliteMaximum']=boolean(n(t1,'factor')>0)
        if '集成战略' not in str((raw.get('trait') or {}).get('moduleDesc','')):
            stats('trait:amgoat:resIgnore',dict(resIgnoreFlat=n(tb,'magic_resist_penetrate_fixed')))
            r('MyEliteHitSp',n(tb,'sp'))
        if mode==1:
            rules['MyFirstCastModifiers']=generic_modifiers(tables,dict(aspd=n(bb,'amgoat_s_1[a].attack_speed')))
            rules['MyLaterCastModifiers']=generic_modifiers(tables,dict(aspd=n(bb,'amgoat_s_1[b].attack_speed'),atkPct=n(bb,'amgoat_s_1[b].atk')))
        elif mode==2:
            kind='CHARGES' if n(skill,'maxChargeTime',1)>1 else 'INSTANT';has_attack=True;scale=number(n(bb,'atk_scale',1))
            r('MyIgniteScale',n(bb,'atk_scale',1));r('MyIgniteSecondScale',n(bb,'atk_scale_2',n(bb,'atk_scale',1)))
            r('MyResistanceCut',n(bb,'magic_resistance'));r('MyResistanceCutDuration',n(bb,'duration'))
        else:
            bat=n(raw.get('stats') or {},'bat',1) or 1;mods=dict(atkPct=n(bb,'atk'),batPct=max(-.9,n(bb,'base_attack_time')/bat));has_attack=True
            grid=skill.get('rangeGrid') or [[r,c] for r in range(-3,4) for c in range(-3,4) if abs(r)+abs(c)<=3]
            rules['MyLavaTargets']=str(max(1,math.floor(n(bb,'attack@max_target',1))))+'U'
    elif name=='CHEN':
        stats('talent:chen:knife',dict(atkPct=n(t1,'atk'),defPct=n(t1,'def'),dodgePhys=n(t1,'prob')))
        stats('trait:chen:pierce',dict(defIgnoreFlat=n(tb,'def_penetrate_fixed')))
        r('MySkillDamageMultiplier',n(tb,'damage_scale',1))
        pulses=[]
        if n(t0,'interval')>0 and n(t0,'sp')>0:
            pulses.append(record('PeriodicOperatorSp',MyAmount=number(n(t0,'sp')),MyInterval=number(n(t0,'interval')),MyAttackHurtOnly='true'))
            extra=next((t.get('bb') or {} for t in selected_module.get('talentChanges',[]) if t.get('talentIndex')==0 and t.get('hidden')), {})
            if n(extra,'sp')>0 and n(extra,'interval',n(t0,'interval'))>0:
                pulses.append(record('PeriodicOperatorSp',MyAmount=number(n(extra,'sp')),MyInterval=number(n(extra,'interval',n(t0,'interval'))),MySelf='true'))
        rules['MyPeriodicSp']=tables.array('PeriodicOperatorSp',pulses)
        r('MyStunDuration',n(bb,'stun'))
        if mode==1:
            kind='CHARGES' if n(skill,'maxChargeTime',1)>1 else 'INSTANT';has_attack=True;scale=number(n(bb,'atk_scale',1));hits='1U'
        else:
            fallback=[[1,0],[1,1],[0,0],[0,1],[0,2],[0,3],[-1,0],[-1,1]] if mode==2 else [[r,c] for r in range(-2,3) for c in range(-2,3) if abs(r)+abs(c)<=2]
            rules['MyCastRange']=tables.grid(skill.get('rangeGrid') or fallback);r('MyCastScale',n(bb,'atk_scale',1))
            if mode==2:
                kind='CHARGES' if n(skill,'maxChargeTime',1)>1 else 'INSTANT';rules['MyCastTargets']=str(max(1,math.floor(n(bb,'max_target',1))))+'U'
            else:
                duration_value=number(2.933);has_attack=True;no_attack=True;rules['MySlashCount']=str(min(10,max(1,math.floor(n(bb,'times',10)))))+'U'
    rules['MyAuras']=tables.array('FixedOperatorAura',auras)
    order=['MyKind','MySkill','MyAuras','MyBlockingModifiers','MyBerserkSpeed','MyBerserkHealthRatio','MyRegen','MyLowHealthRegen','MyRegenHealthRatio','MyProtection','MyProtectHealthRatio','MyHighHealthScale','MyBlockingSp','MyUnblockedSpMultiplier','MyReviveHealthRatio','MyReviveModifiers','MyEndStun','MyDp','MyKillSp','MyOtherKillSp','MyKillSpRange','MyStunChance','MyStunDuration','MyBarrierScale','MyBarrierDuration','MyBarrierModifiers','MyHealScale','MyHealHealthRatio','MyHealGround','MyToken','MyTokenStock','MyBlockedScale','MyOathBlockedScale','MyDuelScale','MyDuelDuration','MyDuelAttack','MyPainAttack','MyPainDuration','MyBlockedDefenseIgnore','MyOathSelfDamage','MyOathHealth','MyBaseHealTargets','MyProbability','MySkillProbability','MyCriticalScale','MyDefenseCut','MyDefenseCutDuration','MyFrontScale','MyCrossfireAttack','MySquadCrossfire','MyDrones','MyMaxStacks','MyBlastScale','MySluggish','MyHeavyMass','MyHeavyPenetration','MyHeavyScale','MyHeavyExtra','MyDistanceScale','MyDistanceMinimum','MyDistanceMaximum','MyStudentAttack','MyStudentPerSkill','MyStudentSkillCap','MyLinkTargets','MyLinkStrikes','MyLinkInterval','MyPeakHealthRatio','MyPeakAttack','MyBlessHealthRatio','MyBlessAttack','MyBlessModifiers','MyTalentHeal','MyNationHeal']
    order+=['MyFirstCastModifiers','MyLaterCastModifiers','MyInitialSpMinimum','MyInitialSpMaximum','MyInitialSpeedMinimum','MyInitialSpeedMaximum','MyInitialEliteMaximum','MyEliteHitSp','MyIgniteScale','MyIgniteSecondScale','MyResistanceCut','MyResistanceCutDuration','MyLavaTargets','MyCastRange','MyCastTargets','MyCastScale','MySkillDamageMultiplier','MySlashCount','MyPeriodicSp']
    spec=record('GenericSkillRecord',MyKind='SkillKind::'+kind,MyDuration=duration_value,MyTrigger=trigger,MyModifiers=generic_modifiers(tables,mods),MyRange=tables.grid(grid),MyRangeExtend=str(extend),MyMaxTargets=targets,MyHasAttack=boolean(has_attack),MyAttackScale=scale,MyHealScale=heal_scale,MyHits=hits,MyNoAttack=boolean(no_attack))
    kit=record('OperatorKitRecord',MyRules=record('DiyOperatorKit',**{k:rules[k] for k in order if k in rules}),MySkill=tables.array('GenericSkillRecord',[spec])+'.data()',MyHealing=boolean(healing),MyBasePriority=priority,MyTalents=tables.array('GenericTalentRecord',permanent),MyBaseRange=tables.grid(base_grid))
    return tables.array('OperatorKitRecord',[kit])+'.data()'
