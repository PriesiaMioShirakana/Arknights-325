"""Compile the pinned generic.js inference rules; descriptions are read only at build time."""
import math
import re


def numeric(value):
    if isinstance(value, bool) or value is None or value == '': return None
    try:
        result = float(value)
        return result if math.isfinite(result) else None
    except (TypeError, ValueError):
        return None


def generic_skill(skill, definition=None):
    """Input is the normalized skill/body shape; return static spec and runtime-effect parameters."""
    if not skill: return None
    bb = skill.get('bb') or {}
    desc = str(skill.get('description') or '')
    def get(key, attack=False):
        keys = ['attack@'+key, key, 'skill@'+key] if attack else [key, 'attack@'+key, 'skill@'+key]
        return next((numeric(bb[k]) for k in keys if k in bb and numeric(bb[k]) is not None), None)
    def positive(v): return v is not None and v > 0
    def has(pattern): return re.search(pattern, desc) is not None
    duration = skill.get('duration', 0)
    kind = skill.get('kind')
    if not kind:
        if skill.get('skillType') == 'PASSIVE' or (not (skill.get('spCost', 0) > 0) and skill.get('durationType') != 'AMMO' and not duration > 0): kind = 'passive'
        elif skill.get('durationType') == 'AMMO': kind = 'ammo'
        elif duration > 0: kind = 'duration'
        elif duration < 0 and (get('trigger_time') is not None or get('ammo') is not None): kind = 'ammo'
        elif duration < 0 and has('持续时间无限|无限持续'): kind = 'toggle'
        else: kind = 'charges' if skill.get('maxCharges', 1) > 1 else 'instant'
    counter_context = has('受到(敌人的)?攻击时')
    counter_text = counter_context and not has('该(角色|干员|单位)受到攻击时')
    mods, debuff = {}, {}
    def add(key, value):
        if value is not None and value != 0: mods[key] = mods.get(key, 0) + value
    def pct_flat(value, pct, flat, target):
        if value is not None and value != 0:
            key = flat if abs(value) > 5 else pct
            target[key] = target.get(key, 0) + value
    for field, name, pct, flat in [('atk','攻击力','atkPct','atkFlat'), ('def','防御力','defPct','defFlat')]:
        value = get(field)
        target = debuff if value is not None and value < 0 and has('(敌人|目标|敌方)[^。；]*'+name+'-') else mods
        pct_flat(value, pct, flat, target)
    pct_flat(get('max_hp'), 'hpPct', 'hpFlat', mods)
    add('aspd', get('attack_speed'))
    if get('base_attack_time') not in (None, 0): mods['batPct'] = max(-0.9, get('base_attack_time'))
    mr = get('magic_resistance')
    if mr not in (None, 0):
        target = debuff if mr < 0 and has('(敌人|目标|敌方)[^。；]*法术抗性-') else mods
        if abs(mr) < 1: target['resMul'] = target.get('resMul', 1) * max(0, 1 + mr)
        else: target['resFlat'] = target.get('resFlat', 0) + mr
    if positive(get('damage_scale')): mods['dmgDealtMul'] = get('damage_scale')
    add('blockCnt', get('block_cnt')); add('taunt', get('taunt_level'))
    dr = get('damage_resistance')
    if dr is not None and 0 < dr < 1: mods['dmgTakenMul'] = 1 - dr
    for field, key in [('hp_recovery_per_sec','hpRegen'), ('hp_recovery_per_sec_by_max_hp_ratio','hpRegenRatio')]:
        if positive(get(field)): add(key, get(field))
    for field, key in [('sp_recovery_per_sec','spRecoveryFlat'), ('magic_resist_penetrate_fixed','resIgnoreFlat'), ('def_penetrate_fixed','defIgnoreFlat')]: add(key, get(field))
    passive, timed = kind == 'passive', kind in ('duration', 'ammo', 'toggle')
    bb_duration = numeric(bb.get('duration'))
    own_duration = duration if bb_duration is None and duration > 0 and has('^部署后') else 0
    passive_timed = (bb_duration if positive(bb_duration) and has(r'\d+(\.\d+)?秒内') else own_duration) if passive and mods else 0
    if passive_timed: kind = 'duration'
    targeting, attack = {}, {}
    if passive_timed and has('攻击[^。；]*(造成|变为|变成)[^。；]*法术伤害|伤害类型变为法术'): attack['dmgType'] = 'arts'
    mt = get('max_target', True)
    if not passive:
        if positive(mt): targeting['maxTargets'] = math.floor(mt)
        if positive(get('ability_range_forward_extend')): targeting['rangeExtend'] = math.floor(get('ability_range_forward_extend') + 0.5)
        if skill.get('rangeGrid'): targeting['rangeGrid'] = skill['rangeGrid']
    at, plain = numeric(bb.get('attack@atk_scale')), numeric(bb.get('atk_scale'))
    if plain is None: plain = numeric(bb.get('skill@atk_scale'))
    start_burst = plain if timed and plain is not None and has('立即[^。]*造成') else None
    end_burst = plain if timed and plain is not None and has('技能结束时[^。]*造成[^。]*攻击力') else None
    counter_scale = plain if counter_text and plain is not None and at is None else None
    def extra(value):
        return value is not None and any(abs(float(m[1]) - value * 100) < 1e-6 for m in re.findall(r'额外(造成|附带)[^。；]*?攻击力(\d+(\.\d+)?)%', desc))
    healer = bool(definition and (definition.get('dmgType') == 'heal' or definition.get('profession') == 'MEDIC'))
    heal_scale = get('heal_scale', True)
    heal_ally = not passive and bool(definition) and not healer and positive(heal_scale) and has('(恢复|治疗)[^。]*友(方|军)[^。]*生命')
    if not passive:
        scale = None if extra(at) else at
        if scale is None and at is None and plain is not None and not counter_context and not extra(plain) and start_burst is None and end_burst is None: scale = plain
        if positive(scale): attack['atkScale'] = scale
        if positive(heal_scale) and not heal_ally: attack['healScale'] = heal_scale
        times = get('times', True)
        if times is not None and times > 1: attack['hits'] = min(10, math.floor(times))
        radius = numeric(bb.get('attack@range_radius'))
        if radius is not None and 0 < radius < 5: attack['splashRadius'] = radius
        if kind == 'duration' and has('(^|[，；。,;])停止(主动)?攻击(敌人)?([，；。,;]|$)') and not has('之后'): attack['noAttack'] = True
    hits, starts, self_stun = [], [], 0
    if not passive:
        for key, status in [('stun','stun'),('cold','cold'),('sleep','sleep'),('fear','fear'),('sluggish','sluggish'),('root','bind'),('unmovable','bind')]:
            on_attack = numeric(bb.get('attack@'+key)); value = numeric(bb.get(key))
            if value is None: value = numeric(bb.get('skill@'+key))
            if positive(on_attack): hits.append({'key':status, 'duration':on_attack})
            if positive(value) and not (positive(on_attack) and value == on_attack):
                if key == 'stun' and has('结束后[^，。；]*晕眩'): self_stun = value
                elif timed: starts.append({'key':status, 'duration':value})
                elif not positive(on_attack): hits.append({'key':status, 'duration':value})
    element = None
    if positive(get('ep_damage_ratio')) and has('附带') and not has('其他友方'):
        for text, key in [('凋亡损伤','apoptosis'),('灼燃损伤','burn'),('神经损伤','neural')]:
            if has(text):
                element = {'el':key, 'ratio':get('ep_damage_ratio'), 'ofDamage':has(r'伤害\d+(\.\d+)?%的[^。，；]{0,3}损伤')}
                break
    has_force = not passive and get('force') is not None
    pull = has('拖拽') or (not has('推开|击退') and bool(definition and definition.get('subProf') == 'hookmaster'))
    directional = not pull and (has('攻击方向|朝部署方向|向前|身前方向') or bool(definition and definition.get('subProf') == 'pusher'))
    on_hit = bool(hits or has_force or (element and not counter_text) or heal_ally)
    has_debuff = bool(debuff) and not passive
    aura = has_debuff and timed and has('攻击范围内[^。；]*敌人')
    debuff_duration = 5 if timed else bb_duration if positive(bb_duration) else 5
    if on_hit or (has_debuff and not aura): attack['onHit'] = True
    spec = {k:skill[k] for k in ['id','name'] if k in skill}
    spec['kind'] = kind
    if kind == 'duration': spec['duration'] = passive_timed or duration
    elif kind == 'ammo' and duration > 0: spec['duration'] = duration
    if kind == 'ammo': spec['ammo'] = max(1, math.floor(next((get(k) for k in ['trigger_time','ammo','cnt'] if get(k) is not None), 8)))
    if passive_timed: spec.update(activateOnDeploy=True, spCost=0, spType='none', trigger='NEVER')
    if mods: spec['mods'] = mods
    if targeting: spec['targeting'] = targeting
    if attack or (kind in ('instant','charges') and (mods or targeting)): spec['attack'] = attack
    dp = get('cost') if not passive and has(r'获得\d+点部署费用') else None
    if positive(dp) and skill.get('skillType') == 'AUTO' and not any(k in spec for k in ['mods','targeting','attack']): spec['trigger'] = 'SP_FULL'
    hp_ratio = get('hp_ratio')
    lose_hp = hp_ratio is not None and 0 < hp_ratio <= 1 and has(r'立即流失\d+(\.\d+)?%(的)?当前生命')
    heal_hp = not lose_hp and hp_ratio is not None and 0 < hp_ratio <= 1 and has('(恢复|回复)[^。，]*生命') and not has('流失')
    shield = None
    if has('屏障'):
        shield = get('shield_max_hp_ratio')
        if shield is None and has(r'生命(上限|值)?\d+(\.\d+)?%的屏障'): shield = hp_ratio
    shield_duration = next((x for x in [get('shield_max_duration'), bb_duration, duration if duration > 0 else 0] if x is not None), 0)
    if positive(dp) or lose_hp or heal_hp or positive(shield) or positive(start_burst) or starts: spec['onStart'] = True
    if positive(end_burst) or self_stun > 0: spec['onEnd'] = True
    if aura: spec['onTick'] = True
    counter = None
    if counter_text and (counter_scale is not None or element):
        m = re.search(r'受到(敌人的)?攻击时[^。]*?(攻击力|防御力)(\d+(\.\d+)?)%的(物理|法术|真实)伤害', desc)
        counter = {'scale':counter_scale or 0, 'stat':'def' if m and m[2] == '防御力' else 'atk',
                   'type':{'法术':'arts','真实':'true'}.get(m[5] if m else '', 'phys'),
                   'around':has('受到(敌人的)?攻击时对周围'), 'groundOnly':has('周围的地面敌人'),
                   'cooldown':numeric(bb.get('aoe_cd')) or 0, 'applyElement':True if element else None}
        spec['counter'] = counter
    burst_type = next((key for text, key in [('法术伤害','arts'),('物理伤害','phys'),('真实伤害','true')] if has(text)), None)
    effects = {'hitStatuses':hits, 'startStatuses':starts, 'selfStun':self_stun, 'probability':get('prob'), 'element':element,
               'hitElement':bool(element and not counter_text), 'force':get('force') if has_force else None, 'pull':pull, 'directional':directional,
               'effectPush':skill.get('id') in ('skchr_forcer_1','skchr_forcer_2'), 'healAlly':heal_scale if heal_ally else 0,
               'healOthersOnly':has('其他友(方|军)'), 'healAll':has('所有友(方|军)'), 'debuff':debuff if has_debuff else {},
               'debuffDuration':debuff_duration, 'aura':aura, 'dp':dp if positive(dp) else 0, 'loseHp':hp_ratio if lose_hp else 0,
               'healHp':hp_ratio if heal_hp else 0, 'shield':shield if positive(shield) else 0, 'shieldDuration':shield_duration,
               'shieldDecay':has('衰减') and shield_duration > 0, 'startBurst':start_burst if positive(start_burst) else 0,
               'endBurst':end_burst if positive(end_burst) else 0, 'burstType':burst_type}
    return {'spec':spec, 'effects':effects}


def stat_talent(talent):
    text = str(talent.get('description', talent.get('desc')) or '').strip()
    bb = talent.get('bb') or {}
    if not text: return None
    mods, used = {}, set()
    fields = {'攻击力':('atk','atkPct','atkFlat'), '防御力':('def','defPct','defFlat'), '生命上限':('max_hp','hpPct','hpFlat'), '攻击速度':('attack_speed',None,'aspd')}
    for part in re.split('[，、,]', text):
        match = re.fullmatch(r'(攻击力|防御力|生命上限|攻击速度)\+(\d+(?:\.\d+)?)(%?)', part.strip())
        if not match: return None
        key, pct, flat = fields[match[1]]; value = numeric(bb.get(key)); mod = pct if match[3] else flat
        if value is None or not mod or key in used or abs((value * 100 if match[3] else value) - float(match[2])) > 1e-6: return None
        mods[mod] = value; used.add(key)
    if any(numeric(v) != 0 for k,v in bb.items() if k not in used): return None
    return mods or None


ATTRIBUTES = {
    'atkPct':'ATTACK_PERCENT', 'atkFlat':'ATTACK_FLAT', 'defPct':'DEFENSE_PERCENT', 'defFlat':'DEFENSE_FLAT',
    'hpPct':'HEALTH_PERCENT', 'hpFlat':'HEALTH_FLAT', 'aspd':'ATTACK_SPEED', 'batPct':'ATTACK_TIME_PERCENT',
    'resMul':'RESISTANCE_MULTIPLIER', 'resFlat':'RESISTANCE_FLAT', 'dmgDealtMul':'DAMAGE_DEALT_MULTIPLIER',
    'blockCnt':'BLOCK_COUNT', 'taunt':'TAUNT', 'dmgTakenMul':'DAMAGE_TAKEN_MULTIPLIER', 'hpRegen':'HEALTH_REGEN',
    'hpRegenRatio':'HEALTH_REGEN_RATIO', 'spRecoveryFlat':'SP_RECOVERY_FLAT',
    'resIgnoreFlat':'RESISTANCE_IGNORE_FLAT', 'defIgnoreFlat':'DEFENSE_IGNORE_FLAT', 'maxTargets':'EXTRA_TARGETS',
}


def generic_modifiers(tables, values):
    from generate_combat import record, number
    return tables.array('AttributeChange', [record('AttributeChange', MyAttribute='Attribute::'+ATTRIBUTES[k], MyValue=number(v)) for k,v in values.items()])


def normalized_generic_skill(raw):
    if not raw or str(raw.get('skillId', '')).startswith('skcom_withdraw'): return None
    bb = raw.get('bb') or {}
    return dict(id=raw.get('skillId', 'skill'), name=raw.get('name', ''), kind=raw.get('kind'), bb=bb,
        description=raw.get('description') if raw.get('description') is not None else raw.get('desc') or '',
        skillType=raw.get('skillType', 'MANUAL').upper(), durationType=raw.get('durationType', 'NONE').upper(),
        duration=raw.get('duration', bb.get('duration', 0)), spCost=max(0, raw.get('spCost', 0)),
        maxCharges=max(1, raw.get('maxChargeTime', 1)), rangeGrid=raw.get('rangeGrid'))


def build_generic_skill(tables, raw, damage, token=False):
    from generate_combat import record, quote, number, boolean
    sk = normalized_generic_skill(raw.get('skill'))
    data = generic_skill(sk, dict(dmgType=damage, profession=(raw.get('profession') or ('TOKEN' if token else 'WARRIOR')).upper(), subProf=raw.get('subProfessionId', '')))
    if data is None: return 'nullptr'
    spec, effects = data['spec'], data['effects']
    attack, targeting, counter = spec.get('attack', {}), spec.get('targeting', {}), spec.get('counter') or {}
    types = {'phys':'PHYSICAL', 'arts':'ARTS', 'true':'TRUE_DAMAGE'}
    def damage_type(v): return 'DamageType::'+types[v] if v is not None else '{}'
    def optional(v): return number(v) if v is not None else '{}'
    def statuses(v):
        return tables.array('GenericStatusEffect', [record('GenericStatusEffect', MyStatus='CombatStatus::'+x['key'].upper(), MyDuration=number(x['duration'])) for x in v])
    element = effects['element'] or {}
    result = record('GenericSkillRecord', MyKind='SkillKind::'+spec['kind'].upper(), MyDuration=optional(spec.get('duration')),
        MyAmmo=number(spec.get('ammo', 0)), MyActivateOnDeploy=boolean(spec.get('activateOnDeploy', False)),
        MyTrigger='SkillTrigger::'+spec['trigger'] if 'trigger' in spec else '{}', MyModifiers=generic_modifiers(tables, spec.get('mods', {})),
        MyRange=tables.grid(targeting.get('rangeGrid')), MyRangeExtend=str(targeting.get('rangeExtend', 0)),
        MyMaxTargets=str(targeting['maxTargets'])+'U' if 'maxTargets' in targeting else '{}',
        MyHasAttack=boolean('attack' in spec), MyDamageType=damage_type(attack.get('dmgType')), MyAttackScale=optional(attack.get('atkScale')),
        MyHealScale=optional(attack.get('healScale')), MyHits=str(attack['hits'])+'U' if 'hits' in attack else '{}',
        MySplashRadius=optional(attack.get('splashRadius')), MyNoAttack=boolean(attack.get('noAttack', False)), MyOnHit=boolean(attack.get('onHit', False)),
        MyEffects=record('GenericSkillEffects', MyDebuffKey=quote('generic:debuff:'+sk['id']), MyShieldKey=quote('generic:shield:'+sk['id']),
            MyHitStatuses=statuses(effects['hitStatuses']), MyStartStatuses=statuses(effects['startStatuses']),
            MyDebuff=generic_modifiers(tables, effects['debuff']), MyProbability=number(effects['probability'] if effects['probability'] is not None else 1),
            MySelfStun=number(effects['selfStun']), MyElement='Element::'+element['el'].upper() if element else '{}',
            MyElementRatio=number(element.get('ratio', 0)), MyElementOfDamage=boolean(element.get('ofDamage', False)), MyHitElement=boolean(effects['hitElement']),
            MyForce=optional(effects['force']), MyPull=boolean(effects['pull']), MyDirectional=boolean(effects['directional']), MyEffectPush=boolean(effects['effectPush']),
            MyHealAlly=number(effects['healAlly']), MyHealOthersOnly=boolean(effects['healOthersOnly']), MyHealAll=boolean(effects['healAll']),
            MyDebuffDuration=number(effects['debuffDuration']), MyAura=boolean(effects['aura']), MyDp=number(effects['dp']),
            MyLoseHp=number(effects['loseHp']), MyHealHp=number(effects['healHp']), MyShield=number(effects['shield']),
            MyShieldDuration=number(effects['shieldDuration']), MyShieldDecay=boolean(effects['shieldDecay']),
            MyStartBurst=number(effects['startBurst']), MyEndBurst=number(effects['endBurst']), MyBurstType=damage_type(effects['burstType']),
            MyStartTargets=str(targeting.get('maxTargets', 0))+'U', MyCounter=boolean(bool(counter)), MyCounterScale=number(counter.get('scale', 0)),
            MyCounterDefense=boolean(counter.get('stat') == 'def'), MyCounterType=damage_type(counter.get('type', 'phys')),
            MyCounterAround=boolean(counter.get('around', False)), MyCounterGroundOnly=boolean(counter.get('groundOnly', False)), MyCounterCooldown=number(counter.get('cooldown', 0))))
    return tables.array('GenericSkillRecord', [result])+'.data()'


def build_generic_talents(tables, talents):
    from generate_combat import record, quote
    if isinstance(talents, dict): talents = [talents]
    rows = []
    for i,t in enumerate(talents or []):
        mods = stat_talent(t)
        if mods: rows.append(record('GenericTalentRecord', MyKey=quote('talent:generic:'+str(i)), MyModifiers=generic_modifiers(tables, mods)))
    return tables.array('GenericTalentRecord', rows)
