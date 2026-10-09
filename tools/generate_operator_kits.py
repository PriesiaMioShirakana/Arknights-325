"""Compile hand-authored operator kits from the pinned data; no runtime blackboard parsing."""
import math
import re
from generate_combat import record, number, boolean
from generate_generic_skill import generic_modifiers, numeric


def num(value, fallback=0):
    result = numeric(value)
    return fallback if result is None else result


def build_operator_kit(tables, key, raw, token=False):
    original = tables.chess.get(key)
    if token or not original or raw.get('isDiy') or raw.get('charId') != original.get('charId'):
        return 'nullptr'
    base = original.get('baseId')
    names = {f'chess_char_1_{i:02}_a': name for i, name in enumerate(['Insider', 'Yak', 'Leizi', 'Udflow', 'Vigna', 'Vendla', 'Prove', 'Texas', 'Caper', 'Sunbr',
        'Skgoat', 'Estell', 'Podego', 'Greyy', 'Pithst', 'Tinman', 'Indigo', 'Utage'], 1)}
    names['chess_char_2_19_a'] = 'Tinman'
    if base not in names: return 'nullptr'
    name = names[base]
    skill = raw.get('skill') or {}
    bb = skill.get('bb') or {}
    talent = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') != -1), {})
    trait = (raw.get('trait') or {}).get('bb') or {}
    default = skill.get('skillId') == (original.get('skill') or {}).get('skillId')
    selected = skill.get('skillId', '')
    mods = None
    kind = 'DURATION'
    ammo = 0
    has_attack = False
    scale = '{}'
    targets = '{}'
    grid = None
    priority = '{}'
    sluggish = '{}'
    duration = '{}'
    trigger = '{}'
    heal_scale = '{}'
    hits = '{}'
    healing = False
    base_sluggish = '{}'
    base_priority = '{}'
    no_heal = False
    on_deploy = False
    no_attack = False
    bat = num((raw.get('stats') or {}).get('bat'), 1) or 1
    delta = num(bb.get('base_attack_time'))
    description = skill.get('description') or skill.get('desc') or ''
    bat_pct = delta - 1 if 0 < delta < 1 and re.search(r'间隔[^，。；]*缩短', description) else max(-0.9, delta / bat)
    if name == 'Insider':
        rules = record('InsiderKit', MyDelay=number(num(talent.get('duration'))), MySelfAmmo=number(num(talent.get('self_ammo'))), MyAllyAmmo=number(num(talent.get('ally_ammo'))))
        if selected == 'skchr_inside_1':
            mods = {}; kind = 'AMMO'; ammo = max(1, math.floor(num(bb.get('attack@trigger_time'), 4)))
            has_attack = True; scale = number(num(bb.get('attack@atk_scale'), 1))
        elif default:
            mods = dict(atkPct=num(bb.get('atk')), batPct=bat_pct, taunt=-1)
            kind = 'AMMO'; ammo = num(bb.get('attack@trigger_time'), 1); priority = 'TargetPriority::RANGED'
    elif name == 'Yak':
        rules = record('YakKit', MyResistance=number(num(talent.get('magic_resistance'))))
        if selected == 'skchr_yak_1': mods = dict(hpPct=num(bb.get('max_hp')), hpRegen=num(bb.get('hp_recovery_per_sec')))
        elif default: mods = dict(hpPct=num(bb.get('max_hp')), defPct=num(bb.get('def')), resMul=1+num(bb.get('magic_resistance')))
    elif name == 'Leizi':
        rules = record('LeiziKit', MyUnblockedScale=number(num(talent.get('atk_scale'), 1)))
        if selected == 'skcom_atk_up[3]' or default:
            mods = dict(atkPct=num(bb.get('atk'))); has_attack = selected != 'skcom_atk_up[3]'
    elif name == 'Udflow':
        damage = num(talent.get('damage'))
        rules = record('UdflowKit', MyDuration=number(num(talent.get('duration'))), MyInterval=number(num(talent.get('interval'), 1)),
            MyDamage=number(damage), MySeaDamage=number(num(talent.get('damage_seamonster'), damage*2)),
            MyReveal=boolean(bool((raw.get('module') or {}).get('active') and raw.get('isGolden'))))
        if selected == 'skchr_udflow_1': mods = dict(atkPct=num(bb.get('atk')), defPct=num(bb.get('def')))
        elif default:
            mods = dict(atkPct=num(bb.get('atk')), aspd=num(bb.get('attack_speed')))
            has_attack = True; targets = str(max(1, int(num(bb.get('attack@max_target'), 1))))+'U'
            grid = skill.get('rangeGrid'); sluggish = number(num(bb.get('attack@sluggish'), 1))
    elif name == 'Vigna':
        rules = record('VignaKit', MyProbability=number(num(talent.get('prob1'))), MySkillProbability=number(num(talent.get('prob2'))),
            MyAttack=number(num(talent.get('atk'))), MyHealthThreshold=number(num(trait.get('hp_ratio'))), MyLowHealthScale=number(num(trait.get('atk_scale'), 1)))
        if default: mods = dict(atkPct=num(bb.get('atk')), batPct=max(-0.9, delta / bat))
    elif name == 'Vendla':
        rules = record('VendlaKit', MyHealingScale=number(num(talent.get('heal_scale'), 1)), MyCounterScale=number(num(bb.get('atk_scale'))),
            MyTaunt=number(num(bb.get('taunt_level'), 1)), MyProtection=boolean(default))
        if selected == 'skcom_magic_rage[3]': mods = dict(aspd=num(bb.get('attack_speed')))
        elif default: mods = dict(atkPct=num(bb.get('atk')))
    elif name == 'Prove':
        hunt = selected == 'skchr_prove_2'
        rules = record('ProveKit', MyHealthDrop=number(num(bb.get('hp_ratio_drop'))), MyScalePerDrop=number(num(bb.get('atk_scale_up'))),
            MyProbability=number(num(talent.get('prob'))), MyFrontProbability=number(num(talent.get('prob2'))),
            MyCriticalScale=number(num(talent.get('atk_scale'), 1)), MyHunt=boolean(hunt))
        if hunt: mods = dict(atkPct=num(bb.get('atk')))
        elif default: mods = {}; kind = 'PASSIVE'
    elif name == 'Texas':
        rules = record('TexasKit', MyInitialDp=number(num(talent.get('cost'))), MyDp=number(num(bb.get('cost'))),
            MyScale=number(num(bb.get('atk_scale'))), MyStun=number(num(bb.get('stun'))), MyRange=tables.grid(skill.get('rangeGrid')), MySwordRain=boolean(default))
        if selected == 'skcom_charge_cost[3]': mods = {}; kind = 'INSTANT'; trigger = 'SkillTrigger::SP_FULL'
        elif default: mods = {}; kind = 'INSTANT'
    elif name == 'Caper':
        rules = record('CaperKit', MyProbability=number(num(talent.get('prob'))), MyCriticalScale=number(num(talent.get('atk_scale'), 1)), MyNearScale=number(num(trait.get('atk_scale'), 1)))
        if selected == 'skchr_caper_1': mods = {}; kind = 'INSTANT'; has_attack = True; scale = number(num(bb.get('atk_scale'), 1))
        elif default:
            mods = dict(atkPct=num(bb.get('atk'))); has_attack = True; hits = str(max(1, math.floor(num(bb.get('cnt'), 1))))+'U'
    elif name == 'Sunbr':
        cooking = selected == 'skchr_sunbr_2'
        rules = record('SunbrKit', MyProbability=number(num(talent.get('prob'))), MyCriticalScale=number(num(talent.get('atk_scale'), 1)),
            MyStun=number(num(talent.get('stun'))), MyHealthThreshold=number(num(trait.get('hp_ratio'))), MyHealingScale=number(num(trait.get('heal_scale'), 1)),
            MyCookingSeconds=number(max(0, num(bb.get('disarm'), 10))) if cooking else '0', MyCookingDefense=number(num(bb.get('def'))),
            MyServingAttack=number(num(bb.get('atk'))), MyCooking=boolean(cooking))
        if cooking:
            mods = dict(batPct=max(0, num(bb.get('base_attack_time')))); grid = skill.get('rangeGrid'); has_attack = healing = True
            duration = number(max(0, num(bb.get('disarm'), 10)) + max(0, num(skill.get('duration'), 30)))
        elif default:
            mods = {}; kind = 'CHARGES' if num(skill.get('maxChargeTime'), 1) > 1 else 'INSTANT'
            grid = skill.get('rangeGrid'); has_attack = healing = True; heal_scale = number(num(bb.get('heal_scale'), 1))
    elif name == 'Skgoat':
        rules = 'SkgoatKit{}'
        add = num(talent.get('sluggish'))
        if add > 0: base_sluggish = number(num(trait.get('sluggish'), 0.8) + add)
        if default: mods = dict(atkPct=num(bb.get('atk')))
    elif name == 'Estell':
        tr = next((t.get('rangeGrid') for t in raw.get('talents', []) if t.get('index') != -1), None)
        rules = record('EstellKit', MyHealRatio=number(num(talent.get('hp_ratio'))), MyHealthThreshold=number(num(trait.get('hp_ratio'))),
            MyPhysicalReduction=number(num(trait.get('damage_resistance'))), MyRange=tables.grid(tr), MyHasRange=boolean(tr is not None))
        if default or selected == 'skcom_atk_up[2]': mods = dict(atkPct=num(bb.get('atk')))
        no_heal = default
    elif name == 'Podego':
        hidden = {}
        for t in raw.get('talents', []):
            if t.get('index') == -1: hidden.update(t.get('bb') or {})
        healing = selected == 'skchr_podego_1'
        rules = record('PodegoKit', MyAuraAttack=number(num(talent.get('atk'))), MySpPerSecond=number(num(hidden.get('sp_recovery_per_sec'))),
            MyZoneDuration=number(num(bb.get('projectile_delay_time'), 5)), MyZoneScale=number(num(bb.get('atk_scale'))), MyHealing=boolean(healing))
        if healing: mods = dict(atkPct=num(bb.get('atk'))); has_attack = True
        elif default: mods = {}; kind = 'INSTANT'
    elif name == 'Greyy':
        rules = 'GreyyKit{}'
        sl = num(talent.get('sluggish'))
        if sl > 0: base_sluggish = number(sl)
        if default or selected == 'skcom_magic_rage[2]': mods = dict(aspd=num(bb.get('attack_speed')))
        if default and sl > 0: sluggish = number(sl * num(bb.get('talent_scale'), 1))
    elif name == 'Pithst':
        ratio = num(talent.get('ep_damage_ratio'))
        rules = record('PithstKit', MyElementRatio=number(ratio), MyEliteElementRatio=number(num(talent.get('ep_damage_ratio_boss'), ratio)))
        base_priority = 'TargetPriority::NOT_BURST'
        if default:
            mods = dict(aspd=num(bb.get('attack_speed'))); targets = str(max(1, math.floor(num(bb.get('attack@max_target'), 1))))+'U'
    elif name == 'Tinman':
        named = [t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') != -1]
        hidden = {}
        for t in raw.get('talents', []):
            if t.get('index') == -1: hidden.update(t.get('bb') or {})
        weak = selected == 'skchr_tinman_1' and base == 'chess_char_2_19_a'
        rules = record('TinmanKit', MyDuration=number(num(bb.get('projectile_delay_time'), 8 if weak else 10)),
            MyRadius=number(num(bb.get('projectile_range'), 1 if weak else 1.5)), MyDamageScale=number(num(bb.get('atk_scale'))),
            MyRegenRatio=number(num(bb.get('hp_recovery_per_sec_ratio'))), MyWeaken=number(min(1, max(0, -num(bb.get('atk'))))),
            MyWitherScale=number(num((named[1] if len(named) > 1 else {}).get('skill@damage_scale'), 1)),
            MySpPerSecond=number(num(hidden.get('sp_recovery_per_sec'))), MyZone=boolean(default or weak), MyWeakZone=boolean(weak))
        if default or weak: mods = {}; kind = 'CHARGES' if num(skill.get('maxChargeTime'), 1) > 1 else 'INSTANT'
    elif name == 'Indigo':
        rules = record('IndigoKit', MyProbability=number(num(talent.get('prob'))), MyBindDuration=number(num(talent.get('duration'))),
            MySkillProbabilityScale=number(num(bb.get('talent_scale'), 1)), MyDamageScale=number(num(bb.get('indigo_s_2[damage].atk_scale'))),
            MyInterval=number(num(bb.get('indigo_s_2[damage].interval'), 0.5)), MyMaze=boolean(default))
        if selected == 'skchr_indigo_1':
            mods = dict(batPct=max(-0.95, min(0, delta))); grid = skill.get('rangeGrid')
            has_attack = True; scale = number(num(bb.get('attack@atk_scale'), 1))
        elif default: mods = dict(batPct=bat_pct)
    elif name == 'Utage':
        rest = selected == 'skchr_utage_1'
        rules = record('UtageKit', MyMaxAttackSpeed=number(num(talent.get('min_attack_speed'))), MyMinHealthRatio=number(num(talent.get('min_hp_ratio'))),
            MyProtectThreshold=number(num(talent.get('hp_ratio'))), MyProtection=number(num(talent.get('damage_resistance'))),
            MyHealthLoss=number(num(bb.get('hp_ratio'))), MyBreach=boolean(selected == 'skchr_utage_2'), MyRest=boolean(rest))
        if rest:
            mods = dict(defPct=num(bb.get('def')), hpRegenRatio=num(bb.get('hp_recovery_per_sec_by_max_hp_ratio')), blockCnt=-99)
            has_attack = no_attack = True
        elif default:
            mods = dict(atkPct=num(bb.get('atk'))); on_deploy = True; duration = number(num(bb.get('duration'))); trigger = 'SkillTrigger::NEVER'
    override = 'nullptr'
    if mods is not None:
        spec = record('GenericSkillRecord', MyKind='SkillKind::'+kind, MyDuration=duration, MyAmmo=number(ammo), MyActivateOnDeploy=boolean(on_deploy), MyTrigger=trigger, MyModifiers=generic_modifiers(tables, mods),
            MyRange=tables.grid(grid), MyMaxTargets=targets, MyHasAttack=boolean(has_attack), MyAttackScale=scale, MyHealScale=heal_scale, MyHits=hits, MyNoAttack=boolean(no_attack))
        override = tables.array('GenericSkillRecord', [spec])+'.data()'
    kit = record('OperatorKitRecord', MyRules=rules, MySkill=override, MyPriority=priority, MyChainNoFalloff=boolean(name == 'Leizi'), MySluggish=sluggish, MyHealing=boolean(healing),
        MyBaseSluggish=base_sluggish, MyBasePriority=base_priority, MySkillNoHeal=boolean(no_heal))
    return tables.array('OperatorKitRecord', [kit])+'.data()'
