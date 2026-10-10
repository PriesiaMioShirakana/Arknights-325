"""Compile all nine pinned stand-in kits into static shared rules and skill records."""
import math
from generate_combat import record, number, boolean, quote
from generate_generic_skill import generic_modifiers, numeric


STANDINS = {
    'char_609_acguad': 'SHARP', 'char_617_sharp2': 'SHARP_LORD',
    'char_615_acspec': 'MISERY', 'char_610_acfend': 'MECHANIST',
    'char_612_accast': 'PITH', 'char_611_acnipe': 'STORMEYE',
    'char_608_acpion': 'TULIP', 'char_613_acmedc': 'TOUCH', 'char_614_acsupo': 'RAIDIAN',
}


def build_standin_kit(tables, raw):
    name = STANDINS.get(raw.get('charId'))
    if name is None: return None
    skill = raw.get('skill') or {}
    bb = skill.get('bb') or {}
    sid = skill.get('skillId', '')
    mode = int(sid[-1]) if sid[-1:].isdigit() else 1
    def n(values, key, default=0):
        value = numeric(values.get(key))
        return default if value is None else value
    talents = raw.get('talents') or []
    t0 = next((t.get('bb') or {} for t in talents if t.get('index') == 0), {})
    t1 = next((t.get('bb') or {} for t in talents if t.get('index') == 1), {})
    module = {}
    for talent in talents:
        if talent.get('index') == -1:
            for key, value in (talent.get('bb') or {}).items(): module[key] = value
    trait = (raw.get('trait') or {}).get('bb') or {}
    rules = dict(MyKind='StandinKind::'+name, MySkill=str(mode)+'U')
    permanent, conditions = [], []
    def stats(key, mods):
        permanent.append(record('GenericTalentRecord', MyKey=quote(key), MyModifiers=generic_modifiers(tables, mods)))
    def conditional(key, condition, mods, threshold=0, count=1, periodic=False):
        if not any(mods.values()): return
        conditions.append(record('ConditionalOperatorBuff', MyKey=quote(key), MyCondition='OperatorBuffCondition::'+condition,
            MyModifiers=generic_modifiers(tables, mods), MyThreshold=number(threshold), MyCount=str(count)+'U', MyPeriodic=boolean(periodic)))
    def r(key, value): rules[key] = number(value)
    def count(value, default=1): return max(1, math.floor(default if numeric(value) is None else numeric(value)))
    mods, kind, grid = {}, 'DURATION', None
    targets, hits, scale, splash, trigger, duration = '{}', '{}', '{}', '{}', '{}', '{}'
    has_attack, on_deploy, healing, sluggish = False, False, name == 'TOUCH', '{}'
    base_grid = None
    if name == 'SHARP':
        stats('acguad:t1', dict(atkPct=n(t0,'atk'), dodgePhys=n(t0,'prob')))
        conditional('acguad:t2', 'DEPLOY_ELAPSED', dict(aspd=n(t1,'attack_speed')), n(t1,'interval',30))
        r('MyBlockedScale', n(trait,'atk_scale',1))
        if mode == 1:
            mods = dict(atkPct=n(bb,'atk')); r('MySkillProbability', n(bb,'attack@prob1',.2))
        elif mode == 2:
            mods = dict(defMul=0,hpPct=n(bb,'max_hp')); has_attack=True; scale=number(n(bb,'attack@atk_scale',1))
        else:
            mods = dict(atkPct=n(bb,'atk')); r('MyStackAttack',n(bb,'atk_each_stack')); rules['MyMaxStacks']=str(count(bb.get('max_atk_stack_cnt'),8))+'U'
    elif name == 'SHARP_LORD':
        stats('sharp2:t1',dict(atkPct=n(t0,'atk'),dodgeArts=n(t0,'prob')))
        conditional('sharp2:t2','TARGETABLE_RANGE',dict(aspd=n(t1,'attack_speed')),count=count(t1.get('cnt'),2))
        r('MyModuleHealthLoss',n(module,'magic_atk_scale'))
        mods=dict(atkPct=n(bb,'atk'),aspd=n(bb,'attack_speed'));has_attack=True;targets=str(count(bb.get('attack@max_target'),2))+'U'
    elif name == 'MISERY':
        stats('acspec:t1',dict(atkPct=n(t0,'atk')))
        conditional('acspec:t2','EXACT_CROSS',dict(atkPct=n(t1,'atk')),count=count(t1.get('cnt')))
        conditional('acspec:module','LONELY',dict(atkPct=n(trait,'atk')))
        r('MyProbability',n(t0,'attack@prob'));r('MySkillProbability',n(bb,'attack@prob',n(t0,'attack@prob')))
        if mode < 3:
            on_deploy=True;trigger='SkillTrigger::NEVER';duration=number(n(skill,'duration',10))
            mods=dict(dodgePhys=n(bb,'prob')) if mode == 1 else dict(atkPct=n(bb,'atk'),aspd=n(bb,'attack_speed'))
        else:
            kind='PASSIVE';r('MyBurstScale',n(bb,'atk_scale'));rules['MyBurstRange']=tables.grid(skill.get('rangeGrid') or [[r,c] for r in (1,0,-1) for c in (-1,0,1)])
            r('MyPullForce',n(bb,'force'));r('MySluggish',n(bb,'sluggish',2.5))
    elif name == 'MECHANIST':
        stats('acfend:t1',dict(defPct=n(t0,'def')))
        conditional('acfend:t1:extra','HEALTH_BELOW',dict(defPct=n(t0,'acfend_t_1[extra].def')),n(t0,'hp_ratio'),periodic=True)
        conditional('acfend:pro','BLOCKING',dict(defPct=n(trait,'def')))
        r('MyFeedbackSpeed',n(t1,'attack_speed'));r('MyFeedbackScale',n(bb,'talent_mult',1))
        mods=dict(defPct=n(bb,'def'))
        if mode == 1: mods['hpPct']=n(bb,'max_hp')
        if mode == 3: mods['blockCnt']=n(bb,'block_cnt');r('MyBurstScale',n(bb,'atk_scale'))
    elif name == 'PITH':
        stats('talent:accast:res',dict(resIgnoreFlat=n(t0,'magic_resist_penetrate_fixed')))
        r('MyAuraValue',n(t1,'atk'));r('MySkillAuraValue',n(bb,'atk') if mode==2 else 0)
        mods=dict(aspd=n(bb,'attack_speed'));has_attack=True
        splash=number(n(bb,'attack@projectile_range',1.5 if mode==1 else 1.2 if mode==3 else n(trait,'attack@projectile_range',1.1)))
        if mode==3: targets=str(count(bb.get('attack@max_target'),2))+'U';r('MyBurstScale',n(bb,'atk_scale_aoe',1))
        if (raw.get('module') or {}).get('active'):
            rec=next((m for m in raw.get('modules',[]) if m.get('uniEquipId')==(raw.get('module') or {}).get('id')),{})
            base_grid=next((t.get('rangeGrid') for t in rec.get('talentChanges',[]) if t.get('rangeGrid')),None)
    elif name == 'STORMEYE':
        r('MyProbability',n(t0,'prob'));r('MySkillProbability',n(bb,'talent@prob',n(t0,'prob')));r('MyCriticalScale',n(t0,'atk_scale',1))
        r('MySpPerSecond',n(t1,'sp_recovery_per_sec'));r('MyIdleDelay',n(t1,'delay'))
        if mode==1: mods=dict(atkPct=n(bb,'atk'),aspd=n(bb,'attack_speed'),defIgnoreFlat=n(bb,'def_penetrate_fixed'))
        else:
            has_attack=True;targets=str(count(bb.get('attack@max_target'),2 if mode==2 else 3))+'U'
            if mode==2: kind='TOGGLE';mods=dict(atkPct=n(bb,'atk'))
            else:
                hits=str(count(bb.get('attack@times'),2))+'U';r('MyExtraHealthRatio',n(bb,'attack@hp_ratio',.9));r('MyExtraDamageScale',n(bb,'attack@atk_scale_extra',1))
    elif name == 'TULIP':
        stats('acpion:tide',dict(atkPct=n(t1,'atk')))
        conditional('acpion:sol','BLOCKING',dict(atkPct=n(trait,'atk'),defPct=n(trait,'def')))
        r('MySpPerSecond',n(t0,'sp_recovery_per_sec'));rules['MyBoundlessCasts']=str(count(t0.get('cnt'),2))+'U';r('MyKillDp',n(t1,'cost'));r('MyDp',n(bb,'cost',1 if mode==2 else 0))
        if mode==2:
            mods=dict(aspd=n(bb,'attack_speed'),defIgnorePct=n(bb,'def_penetrate'));rules['MyDpCount']=str(max(0,math.floor(n(bb,'trig_cnt'))))+'U';r('MyDpInterval',max(.1,n(bb,'interval',1)))
        else:
            kind='INSTANT';r('MyBurstScale',n(bb,'atk_scale',1));r('MyDefenseIgnore',n(bb,'def_penetrate'))
            if mode==1: rules['MyDpCount']=str(count(bb.get('max_target'),2))+'U'
            else: rules['MySlashes']=str(count(bb.get('times'),8))+'U';rules['MyBurstRange']=tables.grid(skill.get('rangeGrid') or [[0,0],[1,0],[-1,0],[0,1],[0,-1]])
    elif name == 'TOUCH':
        r('MyHealHealthRatio',n(trait,'hp_ratio'));r('MyHealScale',n(trait,'heal_scale',1))
        for talent in talents:
            if talent.get('name')=='攫升':r('MyHealSp',n(talent.get('bb') or {},'sp'))
            elif talent.get('name')=='超脱':r('MyDeathSp',n(talent.get('bb') or {},'sp'))
        if mode==1: mods=dict(atkPct=n(bb,'atk'));r('MySkillProbability',n(bb,'attack@prob'))
        else:
            has_attack=True;grid=skill.get('rangeGrid');targets=str(count(bb.get('attack@max_target'),2))+'U'
            if mode==2:r('MySkillAuraValue',n(bb,'attack@atk'))
            else:
                mods=dict(atkPct=n(bb,'atk'));r('MyGospelHealthRatio',n(bb,'hp_ratio'));r('MyGospelHealScale',n(bb,'heal_scale',1));r('MyGospelExtraHeal',n(bb,'attack@addition_heal_scale',.3))
    elif name == 'RAIDIAN':
        stats('talent:acsupo',dict(aspd=n(t0,'attack_speed')))
        r('MyAuraValue',n(t0,'acsupo_t_1[ally].attack_speed'));r('MySkillAuraScale',n(bb,'talent_scale',1))
        r('MyPhysicalMiss',abs(n(t1,'damage_hitrate_physical')));r('MyArtsMiss',abs(n(t1,'damage_hitrate_magical')));r('MySpPerSecond',n(module,'sp_recovery_per_sec'))
        if mode==1:mods=dict(atkPct=n(bb,'atk'));targets=str(count(bb.get('attack@max_target'),2))+'U';has_attack=True
        elif mode==2:
            bat=n(raw.get('stats') or {},'bat',1) or 1
            mods=dict(batPct=max(-.9,n(bb,'base_attack_time')/bat));targets=str(count(bb.get('attack@max_target'),3))+'U';has_attack=True;sluggish=number(n(bb,'attack@sluggish',1.1))
        else:
            mods=dict(atkPct=n(bb,'acsupo_s_3.atk'),aspd=n(t0,'attack_speed')*(n(bb,'talent_scale',1)-1));grid=skill.get('rangeGrid')
            r('MyFragile',n(bb,'damage_scale',1)-1);r('MyWeakenMultiplier',n(bb,'atk',1))
    # declaration order is kept here rather than relying on each branch's insertion order.
    order=['MyKind','MySkill','MyConditions','MyProbability','MySkillProbability','MyCriticalScale','MyBlockedScale','MyStackAttack','MyMaxStacks','MyModuleHealthLoss','MyBurstScale','MyBurstRange','MyPullForce','MySluggish','MyFeedbackSpeed','MyFeedbackScale','MyAuraValue','MySkillAuraValue','MySkillAuraScale','MySpPerSecond','MyIdleDelay','MyExtraHealthRatio','MyExtraDamageScale','MyDp','MyDpCount','MyDpInterval','MyBoundlessCasts','MyKillDp','MySlashes','MyDefenseIgnore','MyHealHealthRatio','MyHealScale','MyHealSp','MyDeathSp','MyGospelHealthRatio','MyGospelHealScale','MyGospelExtraHeal','MyPhysicalMiss','MyArtsMiss','MyFragile','MyWeakenMultiplier']
    rules['MyConditions']=tables.array('ConditionalOperatorBuff',conditions)
    spec=record('GenericSkillRecord',MyKind='SkillKind::'+kind,MyDuration=duration,MyActivateOnDeploy=boolean(on_deploy),MyTrigger=trigger,
        MyModifiers=generic_modifiers(tables,mods),MyRange=tables.grid(grid),MyMaxTargets=targets,MyHasAttack=boolean(has_attack),MyAttackScale=scale,MyHits=hits,MySplashRadius=splash)
    kit=record('OperatorKitRecord',MyRules=record('StandinKit',**{k:rules[k] for k in order if k in rules}),MySkill=tables.array('GenericSkillRecord',[spec])+'.data()',
        MySluggish=sluggish,MyHealing=boolean(healing),MyTalents=tables.array('GenericTalentRecord',permanent),MyBaseRange=tables.grid(base_grid))
    return tables.array('OperatorKitRecord',[kit])+'.data()'
