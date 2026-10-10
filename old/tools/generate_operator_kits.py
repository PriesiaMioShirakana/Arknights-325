"""Compile hand-authored operator kits from the pinned data; no runtime blackboard parsing."""
import math
import re
from generate_combat import record, number, boolean, quote
from generate_generic_skill import generic_modifiers, numeric
from generate_standin_kits import build_standin_kit
from generate_diy_kits import build_diy_operator_kit


def num(value, fallback=0):
    result = numeric(value)
    return fallback if result is None else result


def bv(bb, key, fallback=0):
    value = numeric(bb.get(key))
    if value is not None:
        return value
    for candidate, value in bb.items():
        if candidate.endswith('.' + key) or candidate.endswith(']' + key):
            result = numeric(value)
            if result is not None:
                return result
    return fallback


def build_operator_kit(tables, key, raw, token=False):
    if not token:
        standin = build_standin_kit(tables, raw)
        if standin is not None: return standin
        diy = build_diy_operator_kit(tables, raw)
        if diy is not None: return diy
    original = tables.chess.get(key)
    if token or not original or raw.get('isDiy') or raw.get('charId') != original.get('charId'):
        return 'nullptr'
    base = original.get('baseId')
    names = {f'chess_char_1_{i:02}_a': name for i, name in enumerate(['Insider', 'Yak', 'Leizi', 'Udflow', 'Vigna', 'Vendla', 'Prove', 'Texas', 'Caper', 'Sunbr',
        'Skgoat', 'Estell', 'Podego', 'Greyy', 'Pithst', 'Tinman', 'Indigo', 'Utage', 'Wildmn', 'Liskam'], 1)}
    names['chess_char_2_19_a'] = 'Tinman'
    names.update({f'chess_char_2_{i:02}_a': name for i, name in enumerate(['Excu', 'Silent', 'Slchan', 'Grabds', 'Harold',
        'Papyrs', 'Ghost', 'Bubble', 'Humus', 'Rockr', 'Kazema', 'Gravel', 'Tippi', 'Flower', 'Akkord', 'Whitew', 'Branch', 'Ashlok'], 1)})
    names.update({f'chess_char_3_{i:02}_a': name for i, name in enumerate(['Angel', 'Ayer', 'Swire'], 1)})
    names['chess_char_3_05_a'] = 'Skadi'
    names['chess_char_3_04_a'] = 'Swire2'
    names.update({f'chess_char_3_{i:02}_a': name for i, name in enumerate(['Philae', 'Forcer', 'Mint', 'Haini', 'Pinecn', 'Snhunt', 'Blemsh', 'Malist', 'Slbell', 'Vodfox', 'Snakek', 'Shotst'], 6)})
    names.update({'chess_char_3_18_a':'Vulpis', 'chess_char_3_19_a':'Vigil', 'chess_char_3_20_a':'Kjera', 'chess_char_3_21_a':'Archet'})
    names.update({'chess_char_4_01_a':'Rmixer', 'chess_char_4_02_a':'Mostma', 'chess_char_4_03_a':'Kjera', 'chess_char_4_04_a':'Ines', 'chess_char_4_08_a':'Rosesa', 'chess_char_4_05_a':'Beewax', 'chess_char_4_06_a':'Kroos2', 'chess_char_4_07_a':'Bpipe', 'chess_char_4_09_a':'Mizuki', 'chess_char_4_10_a':'Aroma', 'chess_char_4_11_a':'Cathy', 'chess_char_4_12_a':'Glady', 'chess_char_4_13_a':'Gnosis', 'chess_char_4_14_a':'Lionhd', 'chess_char_4_15_a':'Reckpr', 'chess_char_4_16_a':'Texas2', 'chess_char_4_17_a':'Hsguma', 'chess_char_4_18_a':'Mudrok', 'chess_char_4_19_a':'Flamtl', 'chess_char_4_20_a':'Fartth', 'chess_char_4_21_a':'Plosis', 'chess_char_4_22_a':'Svrash', 'chess_char_4_23_a':'Gvial2', 'chess_char_4_24_a':'Billro', 'chess_char_4_25_a':'Cetsyr', 'chess_char_4_26_a':'Bldsk'})
    names.update({'chess_char_5_01_a':'Excu2', 'chess_char_5_02_a':'Titi', 'chess_char_5_03_a':'Blaze2', 'chess_char_5_05_a':'Ulpia', 'chess_char_5_06_a':'Etlchi', 'chess_char_5_07_a':'Surtr', 'chess_char_5_08_a':'Horn', 'chess_char_5_10_a':'Lisa', 'chess_char_5_11_a':'Demkni', 'chess_char_5_12_a':'Dusk', 'chess_char_5_13_a':'Ghost2', 'chess_char_5_14_a':'Svash2', 'chess_char_5_17_a':'F12yin', 'chess_char_5_20_a':'Aglina', 'chess_char_5_21_a':'Sntlla', 'chess_char_5_22_a':'Nymph', 'chess_char_5_19_a':'Mlynar', 'chess_char_5_15_a':'Thorn2', 'chess_char_5_04_a':'Bldsk', 'chess_char_5_09_a':'Cetsyr', 'chess_char_5_16_a':'Plosis', 'chess_char_5_18_a':'Gvial2', 'chess_char_5_23_a':'Reckpr'})
    names.update({'chess_char_6_01_a':'Lemuen', 'chess_char_6_10_a':'Nymph', 'chess_char_6_05_a':'Pasngr', 'chess_char_6_06_a':'Pepe', 'chess_char_6_15_a':'Qiubai', 'chess_char_6_14_a':'Lumen', 'chess_char_6_19_a':'Blkkgt', 'chess_char_6_03_a':'Yu', 'chess_char_6_02_a':'Sbell2', 'chess_char_6_17_a':'Nearl2', 'chess_char_6_07_a':'Siege2', 'chess_char_6_16_a':'Halo2', 'chess_char_6_20_a':'Agoat2', 'chess_char_6_09_a':'Cello', 'chess_char_6_08_a':'Reed2', 'chess_char_6_12_a':'Rosmon', 'chess_char_6_04_a':'Skadi2', 'chess_char_6_13_a':'Angel2', 'chess_char_6_18_a':'Whitw2', 'chess_char_6_11_a':'Mlyss'})
    if base not in names: return 'nullptr'
    name = names[base]
    skill = raw.get('skill') or {}
    bb = skill.get('bb') or {}
    talent = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') != -1), {})
    trait = (raw.get('trait') or {}).get('bb') or {}
    default = skill.get('skillId') == (original.get('skill') or {}).get('skillId')
    selected = skill.get('skillId', '')
    permanent_talents = []
    base_grid = None

    def permanent(key, values, keep_zero=False):
        if keep_zero or any(values.values()):
            permanent_talents.append(record('GenericTalentRecord', MyKey=quote(key), MyModifiers=generic_modifiers(tables, values)))

    mods = None
    kind = 'DURATION'
    ammo = 0
    has_attack = False
    damage_type = '{}'
    scale = '{}'
    targets = '{}'
    grid = None
    range_extend = 0
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
        rules = 'BasicOperatorKit{}'
        permanent('talent:yak', dict(resFlat=num(talent.get('magic_resistance'))))
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
        rules = 'BasicOperatorKit{}'
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
        rules = 'BasicOperatorKit{}'
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
    elif name == 'Wildmn':
        rules = record('WildmnKit', MyCostCut=number(abs(num(talent.get('value'), -1))), MyCostCap=number(num(talent.get('max_stack_cnt'), 1)),
            MyForce=number(num(bb.get('attack@force'), 1)), MyEveryDeploy=boolean(num(talent.get('flag')) == 1), MyCharge=boolean(default))
        if selected == 'skchr_wildmn_1':
            mods = dict(aspd=num(bb.get('attack_speed'))); on_deploy = True; duration = number(num(skill.get('duration'))); trigger = 'SkillTrigger::NEVER'
        elif default: mods = dict(atkPct=num(bb.get('atk'))); grid = skill.get('rangeGrid'); has_attack = True
    elif name == 'Liskam':
        defense = selected == 'skchr_liskam_1'
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        tgrid = next((t.get('rangeGrid') for t in raw.get('talents', []) if t.get('index') == 0), None)
        permanent('talent:liskam', dict(resFlat=num(t1.get('magic_resistance'))))
        rules = record('LiskamKit', MySp=number(num(talent.get('sp'))),
            MyProbability=number(num(bb.get('attack@buff_prob'))), MyStun=number(num(bb.get('attack@stun'))), MySelfStun=number(num(bb.get('stun'))),
            MyRange=tables.grid(tgrid), MyHasRange=boolean(tgrid is not None), MyDefense=boolean(defense), MyArc=boolean(default),
            MyReveal=boolean(bool((raw.get('module') or {}).get('active') and raw.get('isGolden'))))
        if defense: mods = dict(defPct=num(bb.get('def'))); duration = number(num(bb.get('duration'), 8)); trigger = 'SkillTrigger::SP_FULL'
        elif default:
            mods = dict(atkPct=num(bb.get('atk')), batPct=max(0, delta)); trigger = 'SkillTrigger::DEFAULT'
            has_attack = True; damage_type = 'DamageType::ARTS'; targets = str(max(1, math.floor(num(bb.get('attack@max_target'), 1))))+'U'
    elif name == 'Excu':
        all_front = selected == 'skchr_excu_1'
        permanent('talent:excu', dict(defIgnoreFlat=num(talent.get('def_penetrate_fixed'))))
        rules = record('ExcuKit', MyAllFront=boolean(all_front))
        if all_front: mods = dict(atkPct=num(bb.get('atk'))); has_attack = True
        elif default: mods = dict(batPct=bat_pct); has_attack = True; hits = '2U'
    elif name == 'Silent':
        tokens = raw.get('tokens') or ['token_10000_silent_healrb']
        rules = record('SilentKit', MyAuraAttackSpeed=number(num(talent.get('attack_speed'))), MyGroundHealScale=number(num(trait.get('heal_scale'), 1)),
            MyToken=quote(skill.get('overrideTokenKey') or tokens[0]), MyStockCap=str(max(1, math.floor(num(bb.get('cnt'), 1))))+'U', MyDrone=boolean(default))
        if default: mods = {}; kind = 'INSTANT'; trigger = 'SkillTrigger::SP_FULL'; healing = True
        elif selected == 'skcom_heal_up[3]': mods = dict(atkPct=num(bb.get('atk'))); healing = True
    elif name == 'Slchan':
        rules = record('SlchanKit', MyAttack=number(num(talent.get('atk'))), MyDefense=number(num(talent.get('def'))), MyForce=number(num(bb.get('force'), 1)),
            MyDragDamage=number(num(trait.get('value'))), MyDragDistance=number(max(0.01, num(trait.get('dist'), 1))), MyScale=number(num(bb.get('atk_scale'))),
            MyStun=number(num(bb.get('stun'))), MyTargets=str(max(1, math.floor(num(bb.get('max_target'), 1))))+'U',
            MyRange=tables.grid(skill.get('rangeGrid')), MyHasRange=boolean(skill.get('rangeGrid') is not None), MyChain=boolean(default))
        if default: mods = {}; kind = 'INSTANT'
    elif name == 'Grabds':
        hidden = {}
        for t in raw.get('talents', []):
            if t.get('index') == -1: hidden.update(t.get('bb') or {})
        permanent('talent:grabds', dict(aspd=num(talent.get('attack_speed'))))
        rules = record('GrabdsKit', MySluggish=number(num(trait.get('sluggish'), 0.8)),
            MyBeastSluggish=number(num(talent.get('sluggish_addition'))), MySpPerSecond=number(num(hidden.get('sp_recovery_per_sec'))),
            MySleep=number(num(bb.get('sleep'))), MyTargets=str(max(1, math.floor(num(bb.get('max_target'), 1))))+'U',
            MyRange=tables.grid(skill.get('rangeGrid')), MyHasRange=boolean(skill.get('rangeGrid') is not None), MyQuiet=boolean(default))
        if default:
            mods = dict(aspd=num(bb.get('attack_speed'))); grid = skill.get('rangeGrid'); targets = str(max(1, math.floor(num(bb.get('max_target'), 1))))+'U'
        elif selected == 'skchr_grabds_1':
            mods = dict(maxTargets=1); kind = 'CHARGES' if num(skill.get('maxChargeTime'), 1) > 1 else 'INSTANT'
            has_attack = True; damage_type = 'DamageType::ARTS'; scale = number(num(bb.get('atk_scale'), 1))
    elif name == 'Harold':
        rules = record('HaroldKit', MyResistance=number(min(1, max(0, num(talent.get('ep_damage_resistance'))))),
            MyRecoveryScale=number(num(bb.get('trait_scale'), 1)), MyTriage=boolean(default))
        if default: mods = dict(aspd=num(bb.get('attack_speed'))); healing = True
        elif selected == 'skcom_heal_up[3]': mods = dict(atkPct=num(bb.get('atk'))); healing = True
    elif name == 'Papyrs':
        lock = selected == 'skchr_papyrs_2'
        rules = record('PapyrsKit', MyShieldScale=number(num(talent.get('attack@scale'))), MyShieldDuration=number(num(talent.get('attack@shield_duration'), 8)),
            MySkillShieldScale=number(num(bb.get('shield_scale_skill'), 1)), MyExtraChains=str(max(0, math.floor(num(bb.get('attack@chain.extra_value')))))+'U', MyLockSkill=boolean(lock))
        if lock: mods = dict(atkPct=num(bb.get('atk')), batPct=bat_pct); healing = True
        elif default:
            mods = {}; kind = 'CHARGES' if num(skill.get('maxChargeTime'), 1) > 1 else 'INSTANT'
            healing = has_attack = True; heal_scale = number(num(bb.get('heal_scale'), 1))
    elif name == 'Ghost':
        permanent('talent:ghost', dict(hpPct=num(talent.get('max_hp')), hpRegenRatio=num(talent.get('hp_recovery_per_sec_by_max_hp_ratio'))), keep_zero=True)
        rules = record('GhostKit', MyBlockedScale=number(num(trait.get('atk_scale'), 1)), MyStun=number(num(bb.get('stun'))), MyUndying=boolean(default))
        if default or selected == 'skcom_atk_up[3]': mods = dict(atkPct=num(bb.get('atk')))
    elif name == 'Bubble':
        rules = record('BubbleKit', MyAttackDebuff=number(num(talent.get('atk'))), MyDebuffDuration=number(num(talent.get('duration'), 5)),
            MyBlockingDefense=number(num(trait.get('def'))), MyCounterScale=number(num(bb.get('atk_scale'))), MyCounter=boolean(default))
        if default: mods = dict(defPct=num(bb.get('def')), taunt=num(bb.get('taunt_level'))); has_attack = no_attack = True
        elif selected == 'skcom_def_up[2]': mods = dict(defPct=num(bb.get('def')))
    elif name == 'Humus':
        peaks = {}
        for key, value in bb.items():
            match = re.search(r'\[(peak_\d+)\]\.peak_performance\.(atk|hp_ratio)$', key)
            if match: peaks.setdefault(match[1], dict(atk=0, hp_ratio=1))[match[2]] = num(value)
        peaks = sorted(peaks.values(), key=lambda p: -p['hp_ratio'])
        rules = record('HumusKit', MyOverhealCap=number(num(talent.get('max_hp_ratio'))), MyHeal=number(num(bb.get('value'))),
            MyPeaks=tables.array('HumusPeak', [record('HumusPeak', MyHealthRatio=number(p['hp_ratio']), MyAttack=number(p['atk'])) for p in peaks]),
            MyPeakSkill=boolean(default), MyCut=boolean(selected == 'skchr_humus_1'))
        if default: mods = dict(blockCnt=num(bb.get('block_cnt')))
        elif selected == 'skchr_humus_1': mods = {}; kind = 'INSTANT'; has_attack = True; scale = number(num(bb.get('atk_scale'), 1))
    elif name == 'Rockr':
        rules = record('RockrKit', MyStackAttack=number(num(talent.get('atk'))), MyStackInterval=number(num(talent.get('interval'))),
            MyMaxStacks=str(max(0, math.floor(num(talent.get('max_stack_cnt')))))+'U', MyOverloadAttack=number(num(bb.get('atk'))),
            MyOverloadScale=number(num(bb.get('scale'), 1)), MyOverload=boolean(default))
        if default or selected == 'skcom_magic_rage[3]': mods = dict(aspd=num(bb.get('attack_speed')))
    elif name == 'Kazema':
        token_id = skill.get('overrideTokenKey') or next((s for s in raw.get('tokens', []) if re.search(r'shadow|doll', s)), 'token_10022_kazema_shadow')
        rules = record('KazemaKit', MyToken=quote(token_id), MyBurstScale=number(num(talent.get('damage_scale'))), MyHealthLoss=number(num(bb.get('hp_ratio'))),
            MyDollAttack=number(num(trait.get('atk'))), MySummon=boolean(default), MyCut=boolean(selected == 'skchr_kazema_1'))
        if default: mods = dict(atkPct=num(bb.get('atk')))
        elif selected == 'skchr_kazema_1': mods = {}; kind = 'INSTANT'; has_attack = True; scale = number(num(bb.get('atk_scale'), 1))
    elif name == 'Gravel':
        shadow = selected == 'skchr_gravel_1'
        life = num(bb.get('duration'), 0 if shadow else 10)
        rules = record('GravelKit', MyCost=number(num(talent.get('cost'))), MyAuraDefense=number(num(talent.get('def'))),
            MyAuraCostLimit=number(num(talent['cond.cost'])) if talent.get('cond.cost') is not None else '{}',
            MyShieldRatio=number(num(bb.get('hp_ratio'))), MyDefense=number(num(bb.get('def'))), MyDuration=number(life), MyShadow=boolean(shadow))
        if default or shadow: mods = {}; on_deploy = True; trigger = 'SkillTrigger::NEVER'; duration = number(life)
    elif name == 'Tippi':
        rules = record('TippiKit', MyStackTime=number(num(talent.get('stack_time'))), MyProbability=number(num(talent.get('prob'), 1)), MyOnDamage=boolean(default))
        if default or selected == 'skchr_tippi_1':
            mods = dict(atkPct=num(bb.get('atk'))); grid = skill.get('rangeGrid')
        if default: trigger = 'SkillTrigger::NEVER'; has_attack = True; hits = '3U'
    elif name == 'Flower':
        hidden = {}
        for t in raw.get('talents', []):
            if t.get('index') == -1: hidden.update(t.get('bb') or {})
        rules = record('FlowerKit', MyRegenRatio=number(num(talent.get('atk_to_hp_recovery_ratio'))),
            MyTargets=str(max(0, math.floor(num(hidden.get('attack@max_target')))))+'U')
        if default: mods = dict(atkPct=num(bb.get('atk')), aspd=num(bb.get('attack_speed'))); healing = True
        elif selected == 'skcom_heal_up[2]': mods = dict(atkPct=num(bb.get('atk'))); healing = True
    elif name == 'Akkord':
        rules = record('AkkordKit', MyAllyAttack=number(num(talent.get('atk'))), MySonicScale=number(num(bb.get('attack@aoe_atk_scale'))),
            MyRadius=number(num(bb.get('attack@range_radius'), 0.9)), MyMinDistance=number(num(trait.get('min_dist'))),
            MyMaxDistance=number(num(trait.get('max_dist'), 4)), MyDistanceScale=number(num(trait.get('damage_scale'))), MySonic=boolean(default))
        if default: mods = dict(atkPct=num(bb.get('atk')))
    elif name == 'Whitew':
        sundial = selected == 'skchr_whitew_1'
        rules = record('WhitewKit', MySilence=number(num(talent.get('duration'))), MyAdditionScale=number(num(trait.get('atk_scale_m'))),
            MyBlockProbability=number(num(bb.get('prob'))), MySundial=boolean(sundial), MyWolfSoul=boolean(default))
        if sundial: mods = dict(atkPct=num(bb.get('atk'))); kind = 'TOGGLE'
        elif default: mods = dict(atkPct=num(bb.get('atk'))); has_attack = True; damage_type = 'DamageType::ARTS'; targets = '2U'
    elif name == 'Branch':
        rules = record('BranchKit', MyHealRatio=number(num(talent.get('hp_ratio'))), MyBlockedReduction=number(num(trait.get('damage_scale'), 1)),
            MyTremble=number(num(bb.get('not_combat'))), MyResistance=number(min(0.95, max(0, -num(bb.get('one_minus_status_resistance'))))),
            MyRange=tables.grid(skill.get('rangeGrid')), MyHasRange=boolean(skill.get('rangeGrid') is not None), MyResolve=boolean(default))
        if default: mods = dict(atkPct=num(bb.get('atk')), defPct=num(bb.get('def'))); has_attack = True
        elif selected == 'skchr_branch_1': mods = dict(defPct=num(bb.get('def')))
    elif name == 'Ashlok':
        ranged = selected == 'skchr_ashlok_2'
        rules = record('AshlokKit', MyAttack=number(num(talent.get('atk'))), MyGroundAttack=number(num(talent.get('ashlok_t_1.atk'), num(talent.get('atk')))),
            MyGroundCount=number(num(talent.get('cnt'), 4)), MyBlockedScale=number(num(trait.get('atk_scale'), 1)), MyRangedSkill=boolean(ranged))
        if ranged: mods = dict(atkPct=num(bb.get('atk')), batPct=max(-0.9, delta / bat)); has_attack = True
        elif default: mods = dict(atkPct=num(bb.get('atk')))
    elif name == 'Angel':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        permanent('talent:angel_mag', dict(aspd=num(talent.get('attack_speed'))), keep_zero=True)
        permanent('talent:angel_bless', dict(atkPct=num(t1.get('atk')), hpPct=num(t1.get('max_hp'))), keep_zero=True)
        rules = record('AngelKit', MyBlessAttack=number(num(t1.get('atk'))), MyBlessHealth=number(num(t1.get('max_hp'))),
            MyGroundAttackSpeed=number(num(trait.get('attack_speed'))), MyGroundCount=str(max(1, math.floor(num(trait.get('cnt'), 1))))+'U')
        if default: mods = dict(batPct=num(bb.get('base_attack_time')))
        else: mods = {}
        has_attack = True
        if selected == 'skchr_angel_1':
            kind = 'CHARGES' if num(skill.get('maxChargeTime'), 1) > 1 else 'INSTANT'
            scale = number(num(bb.get('atk_scale'), 1)); hits = str(max(1, math.floor(num(bb.get('times'), 1))))+'U'
        else: scale = number(num(bb.get('attack@atk_scale'), 1)); hits = str(max(1, math.floor(num(bb.get('attack@times'), 1))))+'U'
    elif name == 'Ayer':
        tgrid = next((t.get('rangeGrid') for t in raw.get('talents', []) if t.get('index') != -1), None) or [[r,c] for r in [1,0,-1] for c in [-1,0,1]]
        rules = record('AyerKit', MyAuraAttackSpeed=number(num(talent.get('attack_speed'))), MyBladeScale=number(num(bb.get('atk_scale'), 1)),
            MyAdditionScale=number(num(trait.get('atk_scale_m'))), MyTalentRange=tables.grid(tgrid), MyBlade=boolean(default))
        mods = {}; has_attack = True; damage_type = 'DamageType::ARTS'
        if default: scale = number(num(bb.get('attack@atk_scale'), 1)); grid = skill.get('rangeGrid')
        else:
            kind = 'CHARGES' if num(skill.get('maxChargeTime'), 1) > 1 else 'INSTANT'
            scale = number(num(bb.get('atk_scale'), 1)); targets = str(max(1, math.floor(num(bb.get('max_target'), 1))))+'U'
            if num(bb.get('sluggish')) > 0: sluggish = number(num(bb.get('sluggish')))
    elif name == 'Swire':
        tgrid = next((t.get('rangeGrid') for t in raw.get('talents', []) if t.get('index') != -1), None) or [[r,c] for r in [1,0,-1] for c in [-1,0,1]]
        rules = record('SwireKit', MyAuraAttack=number(num(talent.get('atk'))), MySkillScale=number(num(bb.get('talent_scale'), 1)),
            MyTalentRange=tables.grid(tgrid), MySkillRange=tables.grid(skill.get('rangeGrid') if num(bb.get('talent_range_flag')) > 0 else None))
        mods = dict(atkPct=num(bb.get('atk'))) if default else {}
    elif name == 'Skadi':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        hp_mul = 1 - num(trait.get('value')) if trait.get('value') is not None else num(trait.get('max_hp'), 0.4)
        rules = record('SkadiKit', MyTeamAttack=number(num(talent.get('atk'))), MyRedeploy=number(num(t1.get('respawn_time'))),
            MyBlockedScale=number(num(trait.get('atk_scale'), 1) if num(trait.get('atk_scale')) > 0 else 1),
            MyReviveHealthMultiplier=number(max(0.05, hp_mul)), MyReviveAttackSpeed=number(num(trait.get('attack_speed'))),
            MyReviveHealthRatio=number(num(trait.get('hp_ratio'), 1)), MyRevive=boolean(trait.get('value') is not None or trait.get('hp_ratio') is not None))
        if selected == 'skchr_skadi_2':
            mods = dict(atkPct=num(bb.get('atk'))); on_deploy = True; trigger = 'SkillTrigger::NEVER'; duration = number(num(bb.get('duration'), 20))
        elif default: mods = dict(atkPct=num(bb.get('atk')), defPct=num(bb.get('def')), hpPct=num(bb.get('max_hp')))
        elif selected == 'skcom_quickattack[3]': mods = {k:num(bb.get(v)) for k,v in [('atkPct','atk'),('defPct','def'),('hpPct','max_hp'),('aspd','attack_speed')] if num(bb.get(v))}
    elif name == 'Swire2':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        hidden = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('hidden') and t.get('fromModule')), {})
        mode = 'HEAL' if selected == 'skchr_swire2_1' else 'CASH' if selected == 'skchr_swire2_3' else 'BOMB'
        ratio = re.search(r'血量不足(\d+)%', description)
        token_id = skill.get('overrideTokenKey') or next((t for t in raw.get('tokens', []) if 'gdtrap' in t), 'token_10031_swire2_gdtrap')
        rules = record('Swire2Kit', MySkill='Swire2SkillKind::'+mode, MyToken=quote(token_id), MyCoinCap=number(num(bb.get('sp'), 3)),
            MyCoinCost=number(abs(num(bb.get('attack@sp'), -1)) or 1), MyStartCoins=number(num(talent.get('sp'), 1)), MyPaymentCoins=number(num(talent.get('trait_sp'), 1)),
            MyPaymentAttack=number(num(talent.get('atk'))), MyPaymentStacks=str(max(1, math.ceil(num(talent.get('max_stack_cnt'), 8))))+'U',
            MyModuleAttack=number(num(hidden.get('atk'))), MyModuleStacks=str(max(1, math.ceil(num(hidden.get('max_stack_cnt'), 5))))+'U',
            MyReviveCost=number(abs(num(t1.get('cost'), -5))), MyReviveCostScale=number(num(t1.get('cost_multi'), 2)), MyReviveHealth=number(num(t1.get('hp_ratio'), 0.7)),
            MyHealScale=number(num(bb.get('attack@heal_scale'), num(bb.get('heal_scale')))), MyHealRatio=number(float(ratio[1])/100 if ratio else 0.7),
            MyDamageScale=number(num(bb.get('atk_scale'), 1)), MySluggish=number(num(bb.get('sluggish'), 2)), MyForce=number(num(bb.get('force'))))
        mods = {}; kind = 'TOGGLE' if mode == 'CASH' else 'PASSIVE'
        if mode == 'CASH': has_attack = True; hits = '2U'
    elif name == 'Philae':
        counter = selected != 'skchr_philae_1'
        rules = record('PhilaeKit', MyElementResistance=number(max(0, min(1, num(talent.get('damage_resistance'))))),
            MyApoptosisSp=number(num(talent.get('sp'))), MyElementScale=number(num(trait.get('ep_damage_scale'), 1)),
            MyBarrier=number(num(bb.get('shield_value'))), MyRageAttack=number(num(bb.get('atk'))), MyCounterScale=number(num(bb.get('atk_scale'), 1)),
            MyCounterElement=number(num(bb.get('ep_damage_ratio'))), MyCounterCooldown=number(num(bb.get('aoe_cd'), 2)), MyCounter=boolean(counter))
        mods = dict(hpPct=num(bb.get('max_hp'))); no_attack = counter; has_attack = counter
    elif name == 'Forcer':
        direct = num(bb.get('forcer_s_2[hit_directly].stun'), num(bb.get('stun'), 1))
        rules = record('ForcerKit', MyHeavyMass=number(num(talent.get('value'), 3)), MyDefenseIgnore=number(num(talent.get('def_penetrate_fixed'))),
            MyRefundRatio=number(num(trait.get('value'))), MyForce=number(num(bb.get('force'), 1)), MyDirectStun=number(direct),
            MyWallStun=number(num(bb.get('stun'), direct)), MyBrushStun=number(num(bb.get('forcer_s_2[brush].stun'), direct)),
            MyRange=tables.grid(skill.get('rangeGrid')), MyHasRange=boolean(skill.get('rangeGrid') is not None), MyPush=boolean(default))
        if default: mods = {}; kind = 'INSTANT'
    elif name == 'Mint':
        vortex = selected != 'skchr_mint_1'
        tgrid = next((t.get('rangeGrid') for t in raw.get('talents', []) if t.get('index') != -1), None)
        rules = record('MintKit', MyAuraDefense=number(num(talent.get('def'))), MyTaunt=number(num(talent.get('taunt_level'), -1)),
            MyKeepDefense=number(num(trait.get('soil_e_002[buff].def'))), MyKeepResistance=number(num(trait.get('soil_e_002[buff].magic_resistance'))),
            MyForce=number(num(bb.get('attack@force'), num(bb.get('force')))), MyEndScale=number(num(bb.get('atk_scale'))),
            MyTalentRange=tables.grid(tgrid if tgrid is not None else [[1,0],[0,-1],[0,0],[0,1],[-1,0]]), MyVortex=boolean(vortex))
        mods = {}; has_attack = True; scale = number(num(bb.get('attack@atk_scale'), 1 if vortex else num(bb.get('atk_scale'), 1)))
        if not vortex: grid = skill.get('rangeGrid')
    elif name == 'Haini':
        slow = selected != 'skchr_haini_1'
        rules = record('HainiKit', MyFragile=number(num(talent.get('damage_scale'), 1)), MyMoveMultiplier=number(max(0, 1+num(bb.get('attack@move_speed')))),
            MyKillStep=number(num(bb.get('attack@talent_up'))), MyMaxMultiplier=number(num(bb.get('attack@max_talent_up'), 1)), MySlow=boolean(slow))
        mods = dict(atkPct=num(bb.get('atk'))) if slow else {}; has_attack = True
        targets = str(max(1, math.floor(num(bb.get('attack@max_target' if slow else 'max_target'), 1))))+'U'
        if not slow: kind = 'CHARGES' if num(skill.get('maxCharges'), 1) > 1 else 'INSTANT'; scale = number(num(bb.get('atk_scale'), 1))
    elif name == 'Pinecn':
        spike = selected == 'skchr_pinecn_1'
        steps = [number(num(bb[k])) for k in sorted(bb) if re.search(r'\[[a-z]\]\.atk$', k)] or [number(num(bb.get('atk')))]
        rules = record('PinecnKit', MySpDuration=number(num(talent.get('duration'), 60)), MySpRecovery=number(num(talent.get('sp_recovery_per_sec'))),
            MyDefenseIgnore=number(num(bb.get('def_penetrate_fixed'))), MyAttackSteps=tables.array('double', steps), MySpike=boolean(spike))
        mods = {}
        if spike: kind = 'CHARGES' if num(skill.get('maxCharges'), 1) > 1 else 'INSTANT'; has_attack = True; scale = number(num(bb.get('atk_scale'), 1))
        else:
            grid = skill.get('rangeGrid')
            if grid is not None: trigger = 'SkillTrigger::CUSTOM_RANGE'
    elif name == 'Snhunt':
        volley = selected != 'skchr_snhunt_1'
        shots = re.search(r'(\d+|[一二两三四五])连击', description)
        count = ({'一':1,'二':2,'两':2,'三':3,'四':4,'五':5}.get(shots[1]) or int(shots[1])) if shots else 2
        rules = record('SnhuntKit', MyMovingScale=number(num(bb.get('atk_scale_1'), 1)), MyStillScale=number(num(bb.get('atk_scale_2'), num(bb.get('atk_scale_1'), 1))),
            MyShots=str(max(1, count))+'U', MyBeastScale=number(num(talent.get('atk_scale'), 1)), MyCold=number(num(talent.get('cold'), 3)),
            MyReloadExtra=number(num(trait.get('extra_add'))), MyVolley=boolean(volley))
        mods = {}; kind = 'CHARGES' if num(skill.get('maxCharges'), num(bb.get('ct'), 1)) > 1 else 'INSTANT'
        if not volley: has_attack = True; scale = number(num(bb.get('atk_scale'), 1))
    elif name == 'Blemsh':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        mode = 'HEAL' if selected == 'skchr_blemsh_1' else 'SLEEP' if selected == 'skchr_blemsh_2' else 'INCARNATE'
        cut = num(trait.get('damage_resistance'))
        if cut > 0: permanent('trait:blemsh_guard', dict(dmgTakenMul=1-cut), True)
        heal_grid = skill.get('rangeGrid')
        if heal_grid is None: heal_grid = [[r,c] for r in [1,0,-1] for c in [-1,0,1]]
        rules = record('BlemshKit', MySkill='BlemshSkillKind::'+mode, MyAttackSp=number(num(talent.get('sp'), 1)), MySleepScale=number(num(t1.get('atk_scale'), 1)),
            MyAdditionScale=number(num(bb.get('attack@blemsh_s_3_extra_dmg[magic].atk_scale'))), MyHealScale=number(num(bb.get('heal_scale'))),
            MyRegenRatio=number(num(bb.get('attack@atk_to_hp_recovery_ratio'), num(bb.get('atk_to_hp_recovery_ratio')))),
            MyLowHealthThreshold=number(num(trait.get('hp_ratio'), 0.5)), MyLowHealthHealScale=number(num(trait.get('heal_scale'), 1) if cut <= 0 and num(trait.get('heal_scale')) > 0 else 1), MyHealRange=tables.grid(heal_grid))
        mods = dict(atkPct=num(bb.get('atk'))); has_attack = mode != 'SLEEP'
        if mode == 'INCARNATE': mods['defPct'] = num(bb.get('def'))
        elif mode == 'HEAL': mods = {}; kind = 'CHARGES' if num(skill.get('maxCharges'), 1) > 1 else 'INSTANT'; scale = number(num(bb.get('atk_scale'), 1))
    elif name == 'Malist':
        double = selected != 'skcom_quickattack[3]'
        rules = record('MalistKit', MyProbability=number(num(talent.get('prob'))), MyCriticalScale=number(num(talent.get('atk_scale'), 1)), MyDouble=boolean(double))
        if double:
            count = re.search(r'连续攻击(\d+|[一二两三四五])次', description)
            value = ({'一':1,'二':2,'两':2,'三':3,'四':4,'五':5}.get(count[1]) or int(count[1])) if count else 2
            mods = {}; kind = 'CHARGES' if num(skill.get('maxCharges'), num(bb.get('ct'), 1)) > 1 else 'INSTANT'
            has_attack = True; damage_type = 'DamageType::ARTS'; scale = number(num(bb.get('atk_scale'), 1)); hits = str(max(1, value))+'U'
        else: mods = dict(atkPct=num(bb.get('atk')), aspd=num(bb.get('attack_speed')))
    elif name == 'Slbell':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        slow = selected == 'skchr_slbell_1'
        shred = dict(aspd=num(bb.get('attack_speed'))) if slow else dict(defPct=num(bb.get('def')))
        mr = num(bb.get('magic_resistance'))
        if not slow and mr: shred['resMul' if abs(mr) < 1 else 'resFlat'] = max(0, 1+mr) if abs(mr) < 1 else mr
        rules = record('WeakeningKit', MyHealthRatio=number(num(talent.get('hp_ratio'), 0.4)), MyFragile=number(num(talent.get('damage_scale'), 1)),
            MyTargets=str(max(1, math.floor(num(t1.get('attack@max_target'), 1))))+'U', MySkillModifiers=generic_modifiers(tables, shred), MySlow=boolean(slow))
        mods = {}
        if slow: targets = str(max(1, math.floor(num(bb.get('attack@max_target'), num(bb.get('max_target'), 1)))))+'U'
    elif name == 'Vodfox':
        token_id = skill.get('overrideTokenKey') or next((t for t in raw.get('tokens', []) if 'doll' in t), 'token_10006_vodfox_doll')
        rules = record('WeakeningKit', MyHealthRatio=number(num(talent.get('hp_ratio'), 0.4)), MyFragile=number(num(talent.get('damage_scale'), 1)),
            MySkillAura='false', MyToken=quote(token_id), MySummon=boolean(default))
        if default: mods = {}; kind = 'INSTANT'
    elif name == 'Snakek':
        rules = record('BlockingDefenseKit', MyDefense=number(max(0, num(trait.get('def')))))
        permanent('talent:snakek_def', dict(defPct=num(talent.get('def'))), True)
        mods = dict(defPct=num(bb.get('def')))
        if default:
            mods.update(blockCnt=num(bb.get('block_cnt')), hpRegenRatio=num(bb.get('hp_recovery_per_sec_by_max_hp_ratio')))
            has_attack = True; no_attack = True
    elif name == 'Shotst':
        burst = selected != 'skchr_shotst_1'
        count = re.search(r'至多(\d+)个', description)
        rules = record('ShotstKit', MyFlyingScale=number(num(talent.get('atk_scale'), 1)), MySkillScale=number(num(bb.get('atk_scale'), 1)),
            MyShred=number(num(bb.get('def'))), MyShredDuration=number(num(bb.get('duration'), 5)), MyTargets=str(int(count[1]) if count else 5)+'U', MyBurst=boolean(burst))
        mods = {}; kind = 'INSTANT' if burst else 'CHARGES' if num(skill.get('maxCharges'), 1) > 1 else 'INSTANT'
        if not burst: has_attack = True; scale = number(num(bb.get('atk_scale'), 1))
    elif name == 'Vulpis':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        mode = 'PUNISH' if selected == 'skchr_vulpis_1' else 'TORTURE' if selected == 'skchr_vulpis_2' else 'CAMOUFLAGE'
        rules = record('VulpisKit', MySkill='VulpisSkillKind::'+mode, MyDp=number(num(bb.get('cost'))),
            MyDamageScale=number(num(bb.get('extra_damage_ratio')) if mode == 'PUNISH' else num(bb.get('atk_scale'), 1)),
            MySluggish=number(num(bb.get('sluggish'))), MyStun=number(num(bb.get('stun')) if mode == 'TORTURE' else num(bb.get('attack@stun'), 0.2)),
            MyTargets=str(max(1, math.floor(num(bb.get('max_target'), 6))))+'U', MyRange=tables.grid(skill.get('rangeGrid')), MyHasRange=boolean(skill.get('rangeGrid') is not None),
            MyAttackSpeed=number(num(bb.get('attack_speed'))), MyMarkDuration=number(num(talent.get('interval'), 10)), MyMarkScale=number(num(talent.get('atk_scale'))),
            MyDpBonus=number(num(t1.get('delta_cost_increase_time'), 1)-1), MyQuietTime=number(num(t1.get('interval'), 4)),
            MyRegenRatio=number(num(t1.get('vulpis_t_2[heal][interval].hp_recovery_per_sec_by_max_hp_ratio'))), MyBlockingAttack=number(num(trait.get('atk'))), MyBlockingDefense=number(num(trait.get('def'))))
        mods = {}; has_attack = mode != 'TORTURE'
        if mode == 'CAMOUFLAGE': mods = dict(atkPct=num(bb.get('atk'))); grid = skill.get('rangeGrid')
        else: kind = 'CHARGES' if num(skill.get('maxCharges'), 1) > 1 else 'INSTANT'
    elif name == 'Kjera':
        locked = base == 'chess_char_4_03_a'
        extra = max(0, math.floor(num(bb.get('attack@cnt'), 1 if locked else 0)))
        rules = record('KjeraKit', MyAttack=number(num(talent.get('atk'))), MyGroundAttack=number(num(talent.get('kjera_t_1[high].atk'), num(talent.get('atk')))),
            MyGroundTiles=number(num(talent.get('cnt'), 2)), MyDrones=str(1+extra)+'U', MyColdProbability=number(num(bb.get('attack@prob'))), MyCold=number(num(bb.get('attack@cold'))), MyExtraDrones=boolean(default), MyLockDrones=boolean(locked))
        mods = dict(atkPct=num(bb.get('atk')))
        if default: has_attack = True; targets = str(1+extra)+'U'
    elif name == 'Archet':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        hidden = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('hidden') and t.get('fromModule')), {})
        mode = 'SCATTER' if selected == 'skchr_archet_1' else 'PURSUIT' if selected == 'skchr_archet_2' else 'STORM'
        rules = record('ArchetKit', MySkill='ArchetSkillKind::'+mode, MyTacticsInterval=number(num(talent.get('interval'), 2.5)), MyTacticsSp=number(num(talent.get('sp'), 1)),
            MyShieldSp=number(num(t1.get('sp'))), MyGroundAttackSpeed=number(num(hidden.get('attack_speed'))), MyHits=str(max(1, math.floor(num(bb.get('times'), 5))))+'U',
            MyScatterTargets=str(max(0, math.floor(num(bb.get('max_target'), 4))-1))+'U', MyScale=number(num(bb.get('atk_scale_2' if mode == 'SCATTER' else 'atk_scale'), 1)))
        mods = {}; has_attack = mode != 'PURSUIT'
        if mode == 'STORM':
            mods = dict(atkPct=num(bb.get('atk'))); hits = str(max(1, math.floor(num(bb.get('attack@times'), 1))))+'U'
            if num(bb.get('ability_range_forward_extend')) > 0: range_extend = math.floor(num(bb.get('ability_range_forward_extend'))+0.5)
            if num(bb.get('attack@max_target')) > 1: targets = str(math.floor(num(bb.get('attack@max_target'))))+'U'
        else:
            kind = 'CHARGES' if num(skill.get('maxCharges'), 1) > 1 else 'INSTANT'
            if mode == 'SCATTER': scale = number(num(bb.get('atk_scale'), 1))
    elif name == 'Vigil':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        first = next((t for t in raw.get('talents', []) if t.get('index') != -1), {})
        desc = first.get('desc') or first.get('description') or ''
        def text_count(text, pattern, fallback):
            match = re.search(pattern, text)
            return ({'一':1,'二':2,'两':2,'三':3,'四':4,'五':5}.get(match[1]) or int(match[1])) if match else fallback
        initial = text_count(desc, r'初始(\d+|[一二两三四五])只', 2)
        maximum = text_count(desc, r'至多(\d+|[一二两三四五])只', 3)
        mode = 'CALL' if selected == 'skchr_vigil_1' else 'GIFT' if selected == 'skchr_vigil_2' else 'DIGNITY'
        token_id = first.get('tokenKey') or next((t for t in raw.get('tokens', []) if 'wolf' in t), 'token_10028_vigil_wolf')
        rules = record('VigilKit', MySkill='VigilSkillKind::'+mode, MyToken=quote(token_id), MyInitialWolves=str(max(1, initial))+'U', MyMaxWolves=str(max(1, maximum))+'U',
            MyDefenseIgnore=number(num(t1.get('def_penetrate_fixed'))), MyAdditionScale=number(num(bb.get('attack@vigil_s_3.atk_scale'))),
            MyDp=number(num(bb.get('cost'))), MyDpInterval=number(num(bb.get('interval'), 1.5)), MyDpCap=number(num(bb.get('value'))),
            MyGiftHeal=number(num(bb.get('vigil_wolf_s_2.hp_ratio'))), MyGiftScale=number(num(bb.get('vigil_wolf_s_2.atk_scale'), 1)), MyGiftDp=number(num(bb.get('vigil_wolf_s_2.cost'))))
        mods = {}
        if mode == 'DIGNITY': has_attack = True; hits = str(max(1, text_count(description, r'(\d+|[一二两三四五])连击', 3)))+'U'
        else: kind = 'INSTANT'; trigger = 'SkillTrigger::NEVER'
    elif name == 'Mlyss':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        module_bb = {}
        for t in raw.get('talents', []):
            if t.get('index') == -1:
                for k, v in (t.get('bb') or {}).items(): module_bb[k] = v
        mode = 'LUBRICATION' if selected == 'skchr_mlyss_1' else 'ECOLOGY' if selected == 'skchr_mlyss_2' else 'ADAPTATION'
        cut_match = re.search(r'伤害降低(\d+(?:\.\d+)?)%', (raw.get('trait') or {}).get('moduleDesc') or '')
        rules = record('MlyssKit', MySkill='MlyssSkillKind::'+mode, MyManifold=quote(next((t.get('tokenKey') for t in raw.get('talents', []) if t.get('index') == 0 and t.get('tokenKey')), (raw.get('tokens') or ['token_10030_mlyss_wtrman'])[0])),
            MyAttack=number(num(bb.get('atk'))), MyAttackSpeed=number(num(bb.get('attack_speed'))), MyDp=number(num(bb.get('cost'), 1 if mode == 'LUBRICATION' else 0)), MyDpCount=str(max(0, math.floor(num(bb.get('fake_cost'), 11))))+'U', MyDpInterval=number(max(0.1, bv(bb, 'interval', 1.364))), MyPulseInterval=number(max(0.2, bv(bb, 'interval', 2))), MyBind=number(bv(bb, 'duration', 1.5)) if default else '0',
            MyRegen=number(num(bb.get('hp_recovery_per_sec_by_max_hp_ratio'))), MyProtection=number(num(bb.get('damage_resistance'))), MyRhineSp=number(num(module_bb.get('sp'))), MyRhineOtherSp=number(num(module_bb.get('sp_other'))), MyCostCut=number(num(t1.get('cost'))), MyFirstCostCut=number(num(t1.get('runtime_cost'))), MyReinforcementCut=number(float(cut_match[1])/100 if cut_match else 0))
        mods = dict(atkPct=num(bb.get('atk')))
        if mode == 'LUBRICATION': mods['aspd'] = num(bb.get('attack_speed'))
    elif name == 'Whitw2':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        mode = 'LAZY' if selected == 'skchr_whitw2_1' else 'HUNT' if selected == 'skchr_whitw2_2' else 'HAVOC'
        rules = record('Whitw2Kit', MySkill='Whitw2SkillKind::'+mode, MyDrones=str(max(1, 1+math.floor(num(bb.get('attack@cnt')))))+'U', MySpreadTime=number(max(0, num(bb.get('attack@times'), 1.3))), MyRadius=number(num(bb.get('attack@range_radius'), 0.9)),
            MyDotScale=number(num(bb.get('attack@magic_atk_scale'), 1)), MyFear=number(num(bb.get('attack@fear'))), MyFearChance=number(num(bb.get('attack@prob'))), MySlow=number(num(bb.get('attack@move_speed'))), MyStageInterval=number(max(1, num(talent.get('interval'), 20))),
            MyCapScale=number(num(talent.get('scale'), 1)), MySilence=number(num(talent.get('attack@silence_duration'))), MyTeamSp=number(num(t1.get('sp'))), MyTeamSpeed=number(num(t1.get('attack_speed'))))
        mods = dict(atkPct=num(bb.get('atk')))
        if mode == 'LAZY': kind = 'TOGGLE'; grid = [[r,c] for r in range(-18,19) for c in range(-20,21)]
        elif mode == 'HUNT': grid = skill.get('rangeGrid'); has_attack = True; hits = '1U'
        else: has_attack = no_attack = True
    elif name == 'Skadi2':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        module_bb = {}
        for t in raw.get('talents', []):
            if t.get('index') == -1:
                for k, v in (t.get('bb') or {}).items(): module_bb[k] = v
        a1 = num(t1.get('skadi2_e_003_t_2[atk][1].atk'), bv(t1, 'skadi2_t_2[atk][1].atk'))
        a2 = num(t1.get('skadi2_e_003_t_2[atk][2].atk'), bv(t1, 'skadi2_t_2[atk][2].atk', a1))
        mode = 'SEPARATE' if selected == 'skchr_skadi2_1' else 'PRAYER' if selected == 'skchr_skadi2_2' else 'TIDE'
        life_text = next((t.get('description') or t.get('desc') or '' for t in raw.get('talents', []) if t.get('index') == 0), '')
        life_match = re.search(r'持续(\d+(?:\.\d+)?)秒', life_text)
        rules = record('Skadi2Kit', MySkill='Skadi2SkillKind::'+mode, MySeaborn=quote(next((t.get('tokenKey') for t in raw.get('talents', []) if t.get('index') == 0 and t.get('tokenKey')), (raw.get('tokens') or ['token_10017_skadi2_dedant'])[0])), MyDefault=boolean(default),
            MySeabornLifetime=number(float(life_match.group(1)) if life_match else 25), MyBaseRatio=number(num(trait.get('attack@atk_to_hp_recovery_ratio'), 0.1)), MySkillRatio=number(num(bb.get('attack@atk_to_hp_recovery_ratio'), num(trait.get('attack@atk_to_hp_recovery_ratio'), 0.1))),
            MyInspireAttack=number(num(bb.get('atk'))), MyInspireDefense=number(num(bb.get('def'))), MyTideScale=number(num(bb.get('atk_scale'))), MyHealthLoss=number(num(bb.get('hp_ratio'))), MyShare=number(max(0, min(1, num(bb.get('damage_resistance'))))), MyFlatReduction=number(max(0, -num(module_bb.get('damage_resistance')))),
            MyPredatorAttack=number(a1), MyAbyssalAttack=number(a2), MyPredatorDefense=number(num(t1.get('skadi2_e_003_t_2[def].def'))), MyDeploySp=number(num(t1.get('sp'))), MyModuleCount=number(num(module_bb.get('cnt'))) if 'cnt' in module_bb else 'std::numeric_limits<double>::infinity()', MyModuleAttack=number(num(module_bb.get('atk'))))
        mods = dict(hpPct=num(bb.get('max_hp'))) if mode == 'SEPARATE' else {}
        if mode == 'PRAYER': kind = 'TOGGLE'; trigger = 'SkillTrigger::SP_FULL'
    elif name == 'Angel2':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        mode = 'SKY' if selected == 'skchr_angel2_1' else 'DELIVERY' if selected == 'skchr_angel2_3' else 'ADDICTION'
        rules = record('Angel2Kit', MySkill='Angel2SkillKind::'+mode, MyCoordinate=quote(next((t for t in raw.get('tokens') or [] if 'angel2_target' in t), 'token_10056_angel2_target')),
            MyAttackScale=number(num(bb.get('attack@atk_scale'), 1)), MyCannonScale=number(num(bb.get('attack@cannon_atk_scale'), 1)), MyDeliverySp=number(num(bb.get('attack@sp'))), MyStealSpeed=number(num(bb.get('steal'))), MyExtraAmmo=str(math.floor(num(bb.get('addtional_ammo_each')))),
            MyShieldRatio=number(num(bb.get('shield_max_hp_ratio'))), MyShieldDuration=number(num(bb.get('shield_max_duration'))), MyAmmoHeal=number(num(talent.get('hp_ratio'))), MyAirstrikeChance=number(num(talent.get('prob'))), MyAirstrikeScale=number(num(talent.get('aoe_atk_scale'), num(talent.get('damage_scale')))),
            MyCovenantAttack=number(num(t1.get('atk'))), MyCovenantMultiplier=number(num(t1.get('mult'), 1)), MyCalmHealth=number(num(trait.get('angel2_tr[e].hp_ratio'), bv(trait, 'hp_ratio'))), MyCalmSp=number(bv(trait, 'sp_recovery_per_sec')) if 'angel2_tr[e].hp_ratio' in trait else '0')
        kind = 'AMMO'; ammo = max(1, math.floor(num(bb.get('attack@trigger_time'), 8 if mode == 'SKY' else 50 if mode == 'DELIVERY' else 10)))
        has_attack = True; scale = number(num(bb.get('attack@atk_scale'), 1))
        mods = dict(atkPct=num(bb.get('atk'))) if mode == 'DELIVERY' else dict(batPct=max(-0.9, delta if delta > 0 else delta/bat)) if mode == 'ADDICTION' else {}
        if mode == 'SKY': priority = 'TargetPriority::FLYING'
        if mode == 'DELIVERY': hits = '5U'
    elif name == 'Rosmon':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        mode = 'THOUGHT' if selected == 'skchr_rosmon_1' else 'WISH' if selected == 'skchr_rosmon_3' else 'NERVES'
        rules = record('RosmonKit', MySkill='RosmonSkillKind::'+mode, MyGear=quote(next((t for t in raw.get('tokens') or [] if 'rosmon_shield' in t), 'token_10012_rosmon_shield')),
            MyExtraScale=number(num(bb.get('extra_atk_scale'), 1)), MyExtraShocks=str(math.floor(num(bb.get('add_times')))), MyStunChance=number(num(bb.get('attack@prob'))), MyStun=number(num(bb.get('attack@stun'))), MyStableAttack=number(num(t1.get('atk'))))
        permanent('rosmon:pierce', dict(defIgnoreFlat=num(talent.get('def_penetrate_fixed'))))
        mods = {}; has_attack = mode == 'THOUGHT'
        if mode == 'THOUGHT': kind = 'INSTANT'
        else:
            mods = dict(atkPct=num(bb.get('atk')), batPct=max(-0.9, delta if mode == 'WISH' or delta > 0 else delta/bat))
            if mode == 'WISH': targets = str(max(1, math.floor(num(bb.get('attack@max_target'), 2))))+'U'
    elif name == 'Cello':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        module_bb = {}
        for t in raw.get('talents', []):
            if t.get('index') == -1:
                for k, v in (t.get('bb') or {}).items(): module_bb[k] = v
        mode = 'REQUIEM' if selected == 'skchr_cello_2' else 'TANGO' if selected == 'skchr_cello_3' else 'ECSTASY'
        rules = record('CelloKit', MySkill='CelloSkillKind::'+mode, MySkillElement=number(num(bb.get('ep_damage_ratio'))), MyElementRatio=number(num(talent.get('ep_damage_ratio'))), MySluggish=number(num(talent.get('sluggish'))),
            MyAmplification=number(num(t1.get('ep_damage_scale'), 1)), MyFragile=number(num(t1.get('damage_scale'), 1)-1), MyEliteScale=number(num(trait.get('ep_damage_scale'), 1)), MyFieldWide=boolean(t1.get('damage_value') is not None),
            MyDot=number(num(t1.get('damage_value'))), MyDotInterval=number(max(0.1, num(t1.get('interval'), 1))), MyModuleFragile=number(num(module_bb.get('damage_scale'), 1)-1), MyTalentBoost=number(num(bb.get('scale_delta_to_one'), 1)),
            MyGrantHealth=number(num(bb.get('cello_s_3[max_hp].max_hp'))), MyGrantAttack=number(num(bb.get('cello_s_3[atk].atk'))), MyGrantDefense=number(num(bb.get('cello_s_3[def].def'))), MyOnlySkill=boolean(selected == 'skchr_cello_1' or not raw.get('skills')))
        mods = {}; has_attack = mode != 'REQUIEM'
        if mode == 'ECSTASY': kind = 'CHARGES'; scale = number(num(bb.get('atk_scale'), 1)); priority = 'TargetPriority::NOT_BURST'
        elif mode == 'REQUIEM': mods = dict(aspd=num(bb.get('attack_speed'))); targets = '2U'
        else: mods = dict(atkPct=num(bb.get('atk'))); grid = skill.get('rangeGrid'); no_attack = True
    elif name == 'Reed2':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        module_bb = {}
        for t in raw.get('talents', []):
            if t.get('index') == -1:
                for k, v in (t.get('bb') or {}).items(): module_bb[k] = v
        mode = 'QUICK' if selected == 'skcom_quickattack[3]' else 'FIREBALLS' if selected == 'skchr_reed2_2' else 'SEEDS'
        rules = record('Reed2Kit', MySkill='Reed2SkillKind::'+mode, MyChance=number(num(talent.get('prob'))), MySkillChance=number(num(bb.get('talent@prob'), num(talent.get('prob')))),
            MyScorchAttack=number(num(talent.get('atk'))), MyScorchFragile=number(num(talent.get('damage_scale'), 1)-1), MyScorchDuration=number(num(talent.get('duration'), 6)), MyDot=number(num(bb.get('talent@s3_atk_scale'))),
            MyBurstScale=number(num(bb.get('talent@aoe_scale'))), MyBurstRadius=number(num(bb.get('talent@range_radius'), 1.7)), MyCarriers=str(max(1, math.floor(num(bb.get('max_target'), 1))))+'U', MyFireballCooldown=number(max(0.1, num(bb.get('cooldown'), 1.5))),
            MyFireballScale=number(num(bb.get('atk_scale'), 1)), MyHealShare=number(num(t1.get('scale'))), MyHealBoost=number(num(t1.get('heal_scale'), 1)), MyInjuredScale=number(num(module_bb.get('damage_scale'), 1)), MyDefault=boolean(default))
        mods = {}
        if mode == 'QUICK': mods = dict(atkPct=num(bb.get('atk')), aspd=num(bb.get('attack_speed')))
        elif mode == 'FIREBALLS': duration = number(num(bb.get('projectile_life_time'))) if num(bb.get('projectile_life_time')) > 0 else '{}'
        else:
            self_atk = next((num(v) for k,v in bb.items() if not k.startswith('talent@') and (k == 'atk' or k.endswith('.atk'))), 0)
            mods = dict(atkPct=self_atk); targets = str(max(1, math.floor(num(bb.get('max_target'), 1))))+'U'
    elif name == 'Halo2':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        module_bb = {}
        for t in raw.get('talents', []):
            if t.get('index') == -1:
                for k, v in (t.get('bb') or {}).items(): module_bb[k] = v
        mode = 'STARS' if selected == 'skchr_halo2_1' else 'GRAVITY' if selected == 'skchr_halo2_2' else 'LINKS'
        rules = record('Halo2Kit', MySkill='Halo2SkillKind::'+mode, MyTargets=str(max(1, math.floor(num(bb.get('attack@max_target'), 1))))+'U', MyShare=number(num(bb.get('attack@atk_share'))),
            MyBounces=str(max(0, math.floor(num(bb.get('attack@chain.max_target'), 3))))+'U', MyBounceRadius=number(num(bb.get('attack@projectile_range'), 1.7)), MyPullTargets=str(max(0, math.floor(num(bb.get('max_target'), 2))))+'U',
            MyPullRadius=number(num(bb.get('ability_range_radius'), 2)), MyPullForce=number(num(bb.get('force'))), MyLinkScale=number(num(bb.get('atk_scale_link'), num(bb.get('atk_scale'), 1))),
            MyStackSpeed=number(num(talent.get('attack_speed'))), MyMaxStacks=str(max(1, math.floor(num(talent.get('max_stack_cnt'), 18))))+'U', MyFullAttack=number(num(module_bb.get('atk'))),
            MyFragile=number(bv(t1, 'damage_scale', 1)), MyMatureFragile=number(bv(t1, 'damage_scale_max', bv(t1, 'damage_scale', 1))), MyMatureAfter=number(bv(t1, 'interval', 7)), MyDefault=boolean(default))
        mods = {}; has_attack = mode != 'LINKS'
        if mode == 'GRAVITY': kind = 'INSTANT'; scale = number(num(bb.get('atk_scale'), 1)); sluggish = number(num(bb.get('sluggish'), 3))
        else:
            mods = dict(atkPct=num(bb.get('atk')))
            if mode == 'LINKS': mods['batPct'] = max(-0.9, delta if delta > 0 else delta/bat); grid = skill.get('rangeGrid'); targets = str(max(1, math.floor(num(bb.get('attack@max_target'), 1))))+'U'
    elif name == 'Agoat2':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        mode = 'DRIZZLE' if selected == 'skchr_agoat2_1' else 'VEIL' if selected == 'skchr_agoat2_2' else 'ECHO'
        match = re.search(r'(\d+)连发', description); shots = max(1, int(match[1]) if match else 1)
        rules = record('Agoat2Kit', MySkill='Agoat2SkillKind::'+mode, MyShots=str(shots)+'U', MyHealScale=number(num(bb.get('attack@heal_scale'), 1)), MyRecovery=number(bv(bb, 'ep_heal_ratio')),
            MyVeilScale=number(bv(bb, 'atk_scale', 5)), MyVeilDuration=number(num(bb.get('duration'), 12)), MyMistScale=number(num(talent.get('heal_scale'))), MyMistDuration=number(num(talent.get('duration'), 6)), MyMaxStacks=str(max(1, math.floor(num(talent.get('max_stack_cnt'), 3))))+'U',
            MyHealth=number(num(t1.get('max_hp'))), MyElementCut=number(num(t1.get('ep_damage_resistance'))), MyTalentScale=number(num(bb.get('talent_scale'), 1)), MyModuleSpeed=number(num(trait.get('attack_speed'))))
        mods = {}; healing = True
        if mode == 'DRIZZLE': kind = 'TOGGLE'; trigger = 'SkillTrigger::SP_FULL'; mods = dict(atkPct=num(bb.get('atk'))); targets = '2U'
        elif mode == 'VEIL': kind = 'INSTANT'
        else: has_attack = True; heal_scale = number(num(bb.get('attack@heal_scale'), 1)); targets = str(shots)+'U'; grid = [[r,c] for r in range(-18,19) for c in range(-20,21)]
    elif name == 'Nearl2':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        tgrid = next((t.get('rangeGrid') for t in raw.get('talents', []) if t.get('index') == 0), None) or [[0,0],[1,0],[-1,0],[0,1],[0,-1]]
        mode = 'BLADE' if selected == 'skchr_nearl2_1' else 'NIGHT' if selected == 'skchr_nearl2_2' else 'SUN'
        rules = record('Nearl2Kit', MySkill='Nearl2SkillKind::'+mode, MyDawnRange=tables.grid(tgrid), MySword=quote(next((t for t in raw.get('tokens') or [] if 'nearl2_sword' in t), 'token_10019_nearl2_sword')),
            MyDawnScale=number(num(talent.get('atk_scale'))), MyDawnStun=number(num(talent.get('stun'))), MySunScale=number(num(bb.get('value'))), MySunStun=number(num(bb.get('value2'))), MyShieldHits=str(max(0, math.floor(num(bb.get('times')))))+'U',
            MyRespawnMultiplier=number(num(bb.get('respawn_time'), 1)), MyComboRespawn=number(num(bb.get('nearl2_s_2[withdraw][combo].respawn_time'), 1)), MyStandHealthMultiplier=number(max(0.05, 1-num(trait.get('value')))),
            MyStandSpeed=number(num(trait.get('attack_speed'))), MyStandHealth=number(num(trait.get('hp_ratio'), 1)), MyBlockedScale=number(num(trait.get('atk_scale'), 1)), MyCanStand=boolean(num(trait.get('value')) > 0), MyDefault=boolean(default))
        permanent('nearl2:dawn', dict(defIgnorePct=num(t1.get('def_penetrate'))))
        mods = dict(atkPct=num(bb.get('atk')))
        if mode == 'BLADE': kind = 'TOGGLE'; mods['aspd'] = num(bb.get('attack_speed')); grid = skill.get('rangeGrid')
        elif mode == 'NIGHT': on_deploy = True; trigger = 'SkillTrigger::NEVER'; duration = number(num(skill.get('duration'), 22))
        else: mods['defPct'] = num(bb.get('def')); grid = skill.get('rangeGrid')
    elif name == 'Siege2':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        tgrid = next((t.get('rangeGrid') for t in raw.get('talents', []) if t.get('index') == 0), None) or [[1,-1],[1,0],[1,1],[0,-1],[0,0],[0,1],[-1,-1],[-1,0],[-1,1]]
        module_bb = {}
        for t in raw.get('talents', []):
            if t.get('index') == -1:
                for k, v in (t.get('bb') or {}).items(): module_bb[k] = v
        mode = 'REFORGE' if selected == 'skchr_siege2_1' else 'HOMELAND' if selected == 'skchr_siege2_2' else 'NAME'
        rules = record('Siege2Kit', MySkill='Siege2SkillKind::'+mode, MyTalentRange=tables.grid(tgrid), MySkillRange=tables.grid(skill.get('rangeGrid') or [[0,0],[1,0],[-1,0],[0,1],[0,-1]]),
            MyLion=quote(next((t for t in raw.get('tokens') or [] if 'vlion' in t), 'token_10040_siege2_vlion')), MyBurstScale=number(num(bb.get('atk_scale'))), MyReduction=number(num(talent.get('damage_resistance'))),
            MyAttackPerAlly=number(num(talent.get('atk'))), MySpCount=str(max(1, math.floor(num(bb.get('buff_stack_cnt'), 2))))+'U', MySpRecovery=number(num(bb.get('sp_recovery_per_sec'))), MyFragile=number(num(trait.get('damage_scale'), 1)-1),
            MyTrembleScale=number(num(module_bb.get('damage_scale'), 1)), MyTremble=number(num(t1.get('not_combat_normal'), num(t1.get('not_combat')))), MyEliteTremble=number(num(t1.get('not_combat_elite'), num(t1.get('not_combat_normal'), num(t1.get('not_combat'))))), MyFreeSpeed=number(num(trait.get('attack_speed'))), MyDefault=boolean(default))
        mods = {}; has_attack = True
        if mode == 'REFORGE': kind = 'INSTANT'
        else:
            mods = dict(atkPct=num(bb.get('atk'))); targets = str(max(1, math.floor(num(bb.get('attack@max_target'), 2 if mode == 'HOMELAND' else 1))))+'U'
            if mode == 'HOMELAND': kind = 'TOGGLE'; grid = skill.get('rangeGrid')
            else: mods['batPct'] = max(-0.9, delta if delta > 0 else delta/bat); damage_type = 'DamageType::TRUE_DAMAGE'
    elif name == 'Sbell2':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        module_bb = {}
        for t in raw.get('talents', []):
            if t.get('index') == -1:
                for k, v in (t.get('bb') or {}).items(): module_bb[k] = v
        mode = 'BREEZE' if selected == 'skchr_sbell2_1' else 'WAVES' if selected == 'skchr_sbell2_2' else 'BOW'
        rules = record('Sbell2Kit', MySkill='Sbell2SkillKind::'+mode, MyIceToken=quote(next((t for t in raw.get('tokens') or [] if 'icetgt' in t), 'token_10058_sbell2_icetgt')),
            MyMaxSnow=str(max(1, math.floor(num(talent.get('max_cast_cnt'), 5))))+'U', MyInterval=number(num(talent.get('interval'), 5.5)), MySkillInterval=number(num(bb.get('interval'), num(talent.get('interval'), 5.5))),
            MyFirstSnow=boolean(num(talent.get('first_snow')) > 0), MySlow=number(num(talent.get('move_speed'))), MyEntryScale=number(num(talent.get('talent_magic_scale'))), MySpreads=str(max(0, math.floor(num(bb.get('talent@max_cast_tile_count'), 20))))+'U',
            MySnowDamage=number(num(bb.get('talent@s2_magic_scale'))), MySnowCold=number(num(bb.get('talent@cold'))), MyCounterCold=number(num(t1.get('cold'))), MySelfFreeze=number(num(t1.get('freeze'))), MyEnemyFreeze=number(num(t1.get('c2e_freeze'))),
            MyReviveHealth=number(num(t1.get('hp_ratio'), 1)), MyCrowdDamage=number(num(module_bb.get('damage_scale'))), MyCrowdMax=str(max(1, math.floor(num(module_bb.get('max_valid_stack_cnt'), 5))))+'U',
            MyBurstScale=number(num(bb.get('atk_scale'), 1)), MyCold=number(num(bb.get('cold'))), MyForce=number(num(bb.get('force'))), MyForwardSnow=str(max(0, math.floor(num(bb.get('trig_cnt'), 5))))+'U', MyAttract=number(num(bb.get('attract_time'))))
        mods = {}
        if mode == 'BREEZE': kind = 'CHARGES' if num(skill.get('maxChargeTime'), 1) > 1 else 'INSTANT'; trigger = 'SkillTrigger::DEFAULT'
        elif mode == 'WAVES': kind = 'TOGGLE'; has_attack = True; scale = number(num(bb.get('attack@atk_scale_s2'), 1))
        else:
            mods = dict(atkPct=num(bb.get('atk')), aspd=num(bb.get('attack_speed')), resIgnoreFlat=num(bb.get('magic_resist_penetrate_fixed')))
            grid = skill.get('rangeGrid'); has_attack = True; scale = number(num(bb.get('attack@atk_scale_s3'), 1))
    elif name == 'Blkkgt':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        mode = 'MIGHT' if selected == 'skchr_blkkgt_1' else 'LAUGH' if selected == 'skchr_blkkgt_2' else 'SILENCE'
        match = re.search(r'总计(\d+)次', description); slashes = max(1, int(match[1]) if match else 10); interval = max(0.05, num(bb.get('d_hit_interval'), 0.3))
        rules = record('BlkkgtKit', MySkill='BlkkgtSkillKind::'+mode, MyRange=tables.grid(skill.get('rangeGrid') or [[0,0],[0,1]]),
            MyTargets=str(max(1, math.floor(num(bb.get('max_target'), 1 if mode == 'SILENCE' else 5))))+'U', MyFreeHits=str(max(0, math.floor(num(bb.get('blkkgt_s_2[not_blocked].trig_cnt'), 2))))+'U',
            MyBlockedHits=str(max(0, math.floor(num(bb.get('blkkgt_s_2[blocked].trig_cnt'), 3))))+'U', MySlashes=str(slashes)+'U', MyInterval=number(interval), MyPullInterval=number(max(0.1, num(bb.get('p_hit_interval'), 1))),
            MyScale=number(num(bb.get('dot_scale' if mode == 'LAUGH' else 'd_atk_scale'), 1)), MyFinalScale=number(num(bb.get('e_atk_scale_end'), 1)), MyPullForce=number(num(bb.get('p_force'))), MyFinalForce=number(num(bb.get('e_force'))),
            MyChance=number(num(talent.get('prob'))), MySkillChance=number(num(bb.get('prob'), num(talent.get('prob')))), MyCriticalScale=number(num(talent.get('atk_scale'), 1)), MyTremble=number(num(talent.get('not_combat'))),
            MyFirstTremble=number(num(t1.get('not_combat'))), MyPenetration=number(num(t1.get('def_penetrate'))), MySkillScale=number(num(trait.get('damage_scale'), 1)))
        permanent('blkkgt:contract', dict(defIgnoreFlat=num(trait.get('def_penetrate_fixed')), atkPct=num(t1.get('atk'))))
        mods = {}; has_attack = mode != 'LAUGH'
        if mode == 'MIGHT': kind = 'INSTANT'; scale = number(num(bb.get('atk_scale_s1'), 1)); targets = str(max(1, math.floor(num(bb.get('max_target'), 5))))+'U'; grid = skill.get('rangeGrid')
        elif mode == 'LAUGH': kind = 'CHARGES' if num(skill.get('maxChargeTime'), 1) > 1 else 'INSTANT'
        else: duration = number(slashes*interval); no_attack = True; grid = skill.get('rangeGrid')
    elif name == 'Yu':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        module_bb = {}
        for t in raw.get('talents', []):
            if t.get('index') == -1:
                for k, v in (t.get('bb') or {}).items(): module_bb[k] = v
        mode = 'HOST' if selected == 'skchr_yu_1' else 'GUEST' if selected == 'skchr_yu_2' else 'WALL'
        rules = record('YuKit', MySkill='YuSkillKind::'+mode, MyRange=tables.grid(skill.get('rangeGrid') or [[1,-1],[1,0],[1,1],[0,-1],[0,1],[-1,-1],[-1,0],[-1,1]]),
            MyProtection=number(num(talent.get('damage_resistance'))), MyDamageScale=number(bv(talent, 'atk_scale')), MyElementRatio=number(bv(talent, 'ep_damage_ratio')), MyInterval=number(max(0.1, bv(talent, 'interval', 1))),
            MyOperatorCount=number(num(t1.get('cnt'), 4)), MyHealthRatio=number(num(t1.get('hp_recovery_per_sec_by_max_hp_ratio'))), MyElementHealRatio=number(num(t1.get('ep_heal_ratio'))), MyHealInterval=number(max(0.1, num(t1.get('interval'), 1))),
            MySkillScale=number(num(bb.get('atk_scale'), 1)), MyCounterRatio=number(num(bb.get('ep_damage_ratio'))), MyWallChance=number(num(bb.get('prob')) if default else 0), MyWallBurn=number(num(bb.get('ep_damage_ratio')) if default else 0),
            MyElementMultiplier=number(num(trait.get('ep_damage_scale'), 1)), MyModuleCount=number(num(module_bb.get('cnt'))) if 'cnt' in module_bb else 'std::numeric_limits<double>::infinity()', MyArtsMultiplier=number(num(module_bb.get('damage_scale'), 1)), MyDefault=boolean(default))
        mods = dict(hpPct=num(bb.get('max_hp')))
        if mode == 'HOST': mods['defPct'] = num(bb.get('def')); permanent('yu:host', dict(taunt=num(bb.get('taunt_level'), 1)))
        elif mode == 'GUEST': mods.update(atkPct=num(bb.get('atk')), blockCnt=num(bb.get('block_cnt'))); has_attack = True; damage_type = 'DamageType::ARTS'
        else: mods.update(atkPct=num(bb.get('atk')), defPct=num(bb.get('def')))
    elif name == 'Lumen':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        mode = 'RAIN' if selected == 'skchr_lumen_1' else 'SHOWER' if selected == 'skchr_lumen_2' else 'LIGHT'
        resist_duration = bv(talent, 'status_resistance[limit]')
        rules = record('LumenKit', MySkill='LumenSkillKind::'+mode, MyHealScale=number(num(bb.get('heal_scale'), 1)), MyTargets=str(max(1, math.floor(num(bb.get('max_target'), 2))))+'U',
            MyRainScale=number(bv(bb, 'heal_scale')), MyRainDuration=number(bv(bb, 'projectile_life_time', 4)), MyRainInterval=number(max(0.2, bv(bb, 'interval', 1))),
            MyResist=number(min(1, -num(talent.get('one_minus_status_resistance'), -0.5))), MyResistDuration=number(resist_duration),
            MyHealthyDuration=number(num(talent.get('lumen_t_1[special].status_resistance[limit]'), resist_duration)), MyHealthyThreshold=number(num(talent.get('hp_ratio'), 0.75)),
            MyEmergencyScale=number(num(t1.get('heal_scale'))), MyEmergencyCooldown=number(num(t1.get('duration'), 12)), MyPermanentResist=number(min(1, -num(trait.get('one_minus_status_resistance')))), MyDefault=boolean(default))
        permanent('lumen:scope', dict(taunt=num(trait.get('taunt_level'))))
        mods = {}; healing = True; has_attack = mode != 'SHOWER'; heal_scale = '1'
        if mode == 'RAIN': kind = 'INSTANT'
        elif mode == 'SHOWER': kind = 'CHARGES' if num(skill.get('maxChargeTime'), 1) > 1 else 'INSTANT'
        else: kind = 'AMMO'; duration = '0'; ammo = max(1, math.floor(num(bb.get('attack@trigger_time'), 4))); mods = dict(atkPct=num(bb.get('atk')), aspd=num(bb.get('attack_speed')))
    elif name == 'Pasngr':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        mode = 'TOUCH' if selected == 'skchr_pasngr_1' else 'FOCUS' if selected == 'skchr_pasngr_2' else 'STORM'
        interval = max(0.1, num(bb.get('interval'), 0.5)); falloff_key = 'skill@pasngr_s_1.chain.atk_scale' if mode == 'TOUCH' else 'skill@chain.atk_scale'
        rules = record('PasngrKit', MySkill='PasngrSkillKind::'+mode, MyRange=tables.grid(skill.get('rangeGrid')),
            MyChainCount=str(max(1, math.floor(bv(bb, 'max_target', 4) if mode == 'TOUCH' else num(bb.get('attack@max_target') if mode == 'FOCUS' else bb.get('chain.max_target'), 5 if mode == 'FOCUS' else 4))))+'U',
            MyFalloff=number(1-num(trait[falloff_key])) if mode != 'FOCUS' and falloff_key in trait else '{}',
            MySluggish=number(bv(bb, 'sluggish', 0.5)) if mode == 'TOUCH' else number(num(trait.get('skill@sluggish'), num(bb.get('sluggish')))) if mode == 'STORM' and ('skill@sluggish' in trait or 'sluggish' in bb) else '{}',
            MyInterval=number(interval), MyStrikes=str(max(1, math.floor(num(bb.get('duration'), 4)/interval+0.5)))+'U', MyStormScale=number(num(bb.get('atk_scale'), 1)),
            MyHealthThreshold=number(num(talent.get('hp_ratio'), 0.8)), MyEnhanceScale=number(bv(talent, 'damage_scale', 1)), MyEnhanceDuration=number(bv(talent, 'duration', 3)),
            MyLonelyAttack=number(num(t1.get('atk'))), MyLonelySp=number(num(t1.get('sp_recovery_per_sec'))))
        mods = {}
        if mode == 'TOUCH': kind = 'INSTANT'; has_attack = True; scale = number(bv(bb, 'atk_scale', 1))
        elif mode == 'FOCUS': mods = dict(atkPct=num(bb.get('atk')), batPct=max(-0.9, delta)); range_extend = math.floor(num(bb.get('ability_range_forward_extend'))+0.5); has_attack = True
        else: kind = 'CHARGES'
    elif name == 'Pepe':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        mode = 'STAMP' if selected == 'skchr_pepe_1' else 'CHAOS' if selected == 'skchr_pepe_2' else 'TREMOR'
        rules = record('PepeKit', MySkill='PepeSkillKind::'+mode, MyKillSp=number(num(talent.get('sp'))), MyMaxSp=number(num(talent.get('max_sp'))) if 'max_sp' in talent else 'std::numeric_limits<double>::infinity()',
            MyTeamAttack=number(num(t1.get('atk'))), MyRageSpeed=number(num(bb.get('attack_speed_extra'))), MyMaxRage=str(max(0, math.floor(num(bb.get('max_stack_cnt'), 2))))+'U',
            MyStackAttack=number(num(bb.get('attack@atk'))), MyMaxStacks=str(max(0, math.floor(num(bb.get('attack@max_stack_cnt')))))+'U', MyRadiusGrowth=number(num(bb.get('attack@ability_range_forward_extend'))),
            MyStun=number(num(bb.get('attack@stun'))), MyMainStun=number(num(bb.get('attack@stun_main'), num(bb.get('attack@stun')))), MyCrowdScale=number(num(trait.get('atk_scale_e'), 1)), MyCrowdCount=number(num(trait['cnt'])) if 'cnt' in trait else 'std::numeric_limits<double>::infinity()')
        mods = {}; has_attack = mode != 'CHAOS'
        if mode == 'STAMP': kind = 'INSTANT'; scale = number(num(bb.get('atk_scale'), 1))
        elif mode == 'CHAOS': mods = dict(atkPct=num(bb.get('atk')), aspd=num(bb.get('attack_speed'))); grid = skill.get('rangeGrid')
        else: mods = dict(atkPct=num(bb.get('atk')), batPct=max(-0.9, delta if delta > 0 else delta/bat))
    elif name == 'Qiubai':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        mode = 'FEATHER' if selected == 'skchr_qiubai_1' else 'SHADOW' if selected == 'skchr_qiubai_2' else 'SNOW'
        rules = record('QiubaiKit', MySkill='QiubaiSkillKind::'+mode, MyGapScale=number(num(talent.get('atk_scale_t'), num(talent.get('atk_scale')))), MyBothScale=number(num(talent.get('value'), 1) if 'atk_scale_t' in talent else 1),
            MyModuleArts=number(num(trait.get('atk_scale_m'))), MyFirstBind=number(num(t1.get('duration_advanced'))), MyBindChance=number(num(t1.get('prob'))), MyBindDuration=number(num(t1.get('duration'))),
            MyCrowdCount=number(num(talent.get('cnt'))), MyCrowdSpeed=number(num(talent.get('attack_speed'))), MySkillBind=number(num(bb.get('duration'), 2)), MyBurstScale=number(num(bb.get('aoe_scale'), 1)),
            MyStartScale=number(num(bb.get('sword_begin_atk_scale'), 1)), MyEndScale=number(num(bb.get('sword_end_atk_scale'), 1)), MyMaxStacks=str(max(0, math.floor(num(bb.get('max_stack_cnt')))))+'U', MyStackSpeed=number(num(bb.get('attack_speed'))))
        mods = {}; has_attack = mode != 'SHADOW'
        if mode == 'FEATHER': kind = 'INSTANT'
        else:
            mods = dict(atkPct=num(bb.get('atk'))); grid = skill.get('rangeGrid')
            if mode == 'SNOW':
                match = re.search(r'额外攻击(\d+)个目标', description)
                targets = str(1 + (int(match[1]) if match else 0))+'U'; damage_type = 'DamageType::ARTS'
    elif name == 'Lemuen':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        mode = 'GREETING' if selected == 'skchr_lemuen_1' else 'INVITATION' if selected == 'skchr_lemuen_2' else 'SALUTE'
        rules = record('LemuenKit', MySkill='LemuenSkillKind::'+mode, MyWantedInterval=number(num(talent.get('interval'), 8)), MyWantedScale=number(num(talent.get('damage_scale'), 1)),
            MyExtraditionDelay=number(num(t1.get('interval'), 20)), MyExtraditionAttack=number(num(t1.get('atk'))), MyOwnAmmo=number(math.floor(num(t1.get('add_count')))), MyAllyAmmo=number(math.floor(num(t1.get('ex_add_count')))), MySurvivorSp=number(num(trait.get('sp'))),
            MyLockInterval=number(max(0.05, num(bb.get('attack@aim_interval'), 0.5))), MyInnerRadius=number(num(bb.get('attack@dist_1'), 0.8)), MyOuterRadius=number(num(bb.get('attack@dist_2'), num(bb.get('attack@projectile_range'), 1.5))),
            MyInnerScale=number(num(bb.get('attack@proj_atk_scale_1'), 1)), MyOuterScale=number(num(bb.get('attack@proj_atk_scale_2'), 1)), MySpread=number(max(0, num(bb.get('attack@emit_offset'), 0.2))),
            MyAimBase=number(num(bb.get('attack@main_atk_scale'), 1)), MyAimIncrement=number(num(bb.get('attack@ex_atk_scale'))), MyAimFinal=number(num(bb.get('attack@fin_atk_scale'), 1)),
            MyAimInterval=number(max(0.05, num(bb.get('attack@interval'), 0.25))), MyAimSteps=number(math.floor(num(bb.get('attack@trig_cnt'), 10))), MyAimDuration=number(num(bb.get('attack@aim_duration'), 2.5)))
        kind = 'AMMO'; duration = '0'; ammo = max(1, math.floor(num(bb.get('attack@trigger_time'), 6 if mode == 'INVITATION' else 5))); mods = {}
        if mode == 'GREETING': has_attack = True; targets = '2U'; scale = number(num(bb.get('attack@atk_scale'), 1))
        elif mode == 'INVITATION': mods = dict(aspd=num(bb.get('attack_speed')), atkPct=num(bb.get('atk')))
        else: no_attack = True
    elif name == 'Thorn2':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        hidden = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == -1), {})
        mode = 'GUARD' if selected == 'skchr_thorn2_1' else 'SEA' if selected == 'skchr_thorn2_3' else 'TIDE'
        def ramp(prefix):
            return record('LinearRamp', MyBase=number(num(bb.get(prefix))), MyIncrement=number(num(bb.get(prefix+'_per_interval'))), MyLimit=number(num(bb['max_'+prefix])) if 'max_'+prefix in bb else '{}')
        rules = record('Thorn2Kit', MySkill='Thorn2SkillKind::'+mode, MyDuration=number(num(bb.get('projectile_delay_time'), 6 if mode == 'GUARD' else 19 if mode == 'SEA' else num(bb.get('remaining_time'), 12))),
            MyExtraDuration=number(min(num(talent.get('projectile_extend')), num(talent.get('projectile_extend_max'), math.inf))), MyRadius=number(num(bb.get('projectile_range'), 1.1)), MyGrowth=number(num(bb.get('value'))), MySpeed=number(num(bb.get('projectile_move_speed'))),
            MyHealingMultiplier=number(max(0, num(bb.get('heal_scale'), 1))), MyDamageScale=number(num(bb.get('atk_scale'))), MyHealingScale=number(num(bb.get('hp_recovery_per_sec_ratio') if mode == 'GUARD' else bb.get('hp_recovery_per_sec_ratio_chr'))),
            MyDefense=number(num(bb.get('def'))), MyAnchors=str(max(1, math.ceil(num(bb.get('max_target_token'), 3))))+'U', MyInterval=number(max(0.1, num(bb.get('interval'), 1))), MyMaxSteps=number(max(0, num(bb.get('max_stack_cnt'), 15))),
            MyAttackRamp=ramp('atk'), MyDefenseRamp=ramp('def'), MyResistanceRamp=ramp('magic_resistance'), MyDamageRamp=ramp('atk_scale'), MyRoadLength=number(num(t1.get('cnt'), 6)),
            MyAllySpeed=number(num(t1.get('attack_speed_ally'))), MyAllyRoadSpeed=number(num(t1.get('attack_speed_ally_extra'))), MyEnemySpeed=number(num(t1.get('attack_speed_enemy'))), MyEnemyRoadSpeed=number(num(t1.get('attack_speed_enemy_extra'))),
            MyZoneSp=number(num(hidden.get('sp_recovery_per_sec')) if raw.get('isGolden') else 0))
        permanent('thorn2:mind', dict(atkPct=num(talent.get('atk'))))
        mods = {}; kind = 'CHARGES' if num(skill.get('maxChargeTime'), 1) > 1 else 'INSTANT'
        if mode == 'GUARD': trigger = 'SkillTrigger::SP_FULL'
        if mode == 'SEA': base_grid = skill.get('rangeGrid')
    elif name == 'Mlynar':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        mode = 'ANGER' if selected == 'skchr_mlynar_1' else 'SORROW' if selected == 'skchr_mlynar_2' else 'GLORY'
        rules = record('MlynarKit', MySkill='MlynarSkillKind::'+mode, MyNearCount=number(num(talent.get('cnt'), 3)), MyAttackScale=number(num(talent.get('atk_scale_base'), 1)),
            MyNearScale=number(num(talent.get('atk_scale_up'), 1)), MyDamageReduction=number(num(talent.get('damage_resistance'))), MyReflectScale=number(num(t1.get('atk_scale'))),
            MyTraitScale=number(num(bb.get('trait_up'), 1)), MyPerKill=number(num(bb.get('per_kill_reduce'))), MyMarkScale=number(num(bb.get('atk_scale'))))
        permanent('mlynar:unmoved', dict(taunt=num(t1.get('taunt_level'))))
        mods = dict(defPct=num(bb.get('def'))) if mode == 'ANGER' else dict(batPct=max(-0.9, delta/bat)) if mode == 'SORROW' else {}
        has_attack = True; scale = number(num(bb.get('attack@atk_scale'), 1))
        if mode != 'ANGER': grid = skill.get('rangeGrid')
        if mode == 'SORROW': hits = '2U'
        if mode == 'GLORY': targets = str(max(1, math.floor(num(bb.get('attack@max_target'), 1))))+'U'
    elif name == 'F12yin':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        mode = 'HOOK' if selected == 'skchr_f12yin_1' else 'QUAKE' if selected == 'skchr_f12yin_3' else 'SWEEP'
        rules = record('F12yinKit', MySkill='F12yinSkillKind::'+mode, MyChance=number(num(talent.get('prob'))), MySkillChance=number(num(bb.get('talent@prob'), num(talent.get('prob')))),
            MyCriticalScale=number(num(talent.get('atk_scale'), 1)), MyWeaken=number(num(talent.get('atk'))), MyWeakenDuration=number(num(talent.get('duration'), 3)), MyForce=number(num(bb.get('attack@force'), 1)), MyHealthySpeed=number(num(trait.get('attack_speed'))))
        permanent('f12yin:body', dict(defPct=num(t1.get('def')), dodgePhys=num(t1.get('prob'))))
        if num(trait.get('prob')) > 0: permanent('f12yin:moduleX', dict(dodgePhys=num(trait.get('prob'))))
        has_attack = True; mods = {}
        if mode == 'HOOK': kind = 'CHARGES' if num(skill.get('maxChargeTime'), 1) > 1 else 'INSTANT'; scale = number(num(bb.get('atk_scale'), 1)); targets = str(max(1, math.floor(num(bb.get('max_target'), 2))))+'U'
        elif mode == 'QUAKE': mods = dict(atkPct=num(bb.get('atk')), batPct=max(-0.9, delta/bat)); grid = skill.get('rangeGrid'); hits = '2U'; targets = str(max(1, math.floor(num(bb.get('attack@max_target'), 3))))+'U'
        else: kind = 'TOGGLE'; grid = skill.get('rangeGrid'); mods = dict(atkPct=num(bb.get('atk')), defPct=num(bb.get('def')), blockCnt=num(bb.get('block_cnt')), hpRegenRatio=num(bb.get('hp_recovery_per_sec_by_max_hp_ratio')))
    elif name == 'Aglina':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        hidden = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == -1), {})
        mode = 'CHARGE' if selected == 'skchr_aglina_1' else 'PARTICLE' if selected == 'skchr_aglina_2' else 'GRAVITY'
        rules = record('AglinaKit', MySkill='AglinaSkillKind::'+mode, MyAttackSpeed=number(num(talent.get('attack_speed'))), MyRegeneration=number(num(t1.get('hp_recovery_per_sec'))), MyEnemySp=number(num(hidden.get('sp_recovery_per_sec'))))
        mods = dict(atkPct=num(bb.get('atk')))
        if mode == 'PARTICLE': mods = dict(batPct=delta-1 if 0 < delta < 1 else max(-0.9, delta/bat)); has_attack = True; scale = number(num(bb.get('damage_scale'), 1))
        elif mode == 'GRAVITY': grid = skill.get('rangeGrid'); has_attack = True; targets = str(max(1, math.floor(num(bb.get('attack@max_target'), 1))))+'U'
    elif name == 'Sntlla':
        icicle = selected != 'skcom_quickattack[3]'
        rules = record('SntllaKit', MyIcicle=boolean(icicle), MyDelay=number(num(talent.get('interval'), 20)), MyAttack=number(num(talent.get('atk'))),
            MyResist=number(min(1, max(0, -num(talent.get('one_minus_status_resistance'))))), MyScale=number(num(bb.get('attack@atk_scale'), 1)), MyCold=number(num(bb.get('attack@cold'))))
        mods = dict(batPct=max(-0.9, delta/bat)) if icicle else dict(atkPct=num(bb.get('atk')), aspd=num(bb.get('attack_speed')))
        if icicle: grid = skill.get('rangeGrid'); no_attack = True
    elif name == 'Nymph':
        hidden_variant = base == 'chess_char_6_10_a'
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        hidden = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == -1), {})
        mode = 'FEAR' if hidden_variant else 'LASH' if selected == 'skchr_nymph_1' else 'BREAK' if selected == 'skchr_nymph_3' else 'FEAR'
        rules = record('NymphKit', MySkill='NymphSkillKind::'+mode, MyElementRatio=number(num(bb.get('ep_damage_ratio'), num(bb.get('attack@ep_damage_ratio')))), MySoulScale=number(num(talent.get('element_atk_scale'))),
            MySkillSoulScale=number(num(bb.get('element_atk_scale'), num(talent.get('element_atk_scale')) if hidden_variant else 0)), MyGrowth=number(num(t1.get('atk'))), MyMaxStacks=str(max(1, math.ceil(num(t1.get('max_stack_cnt'), 10))))+'U',
            MyBurstMultiplier=number(num(trait.get('damage_scale'), 1)), MyBurstSp=number(0 if hidden_variant else num(hidden.get('sp_recovery_per_sec'))), MyExtraScale=number(num(bb.get('attack@extra_ep_damage_scale'))), MySplashRadius=number(num(bb.get('projectile_range'), 1.5)), MyFear=number(num(bb.get('fear'))), MyHiddenVariant=boolean(hidden_variant), MyFieldWideGrowth=boolean('stack_cnt_check' in t1), MyMaxAttackSpeed=number(num(t1.get('attack_speed'))))
        mods = dict(atkPct=num(bb.get('atk')))
        if mode == 'FEAR': mods = {}; kind = 'CHARGES' if num(skill.get('maxChargeTime'), 1) > 1 else 'INSTANT'; has_attack = True; scale = number(num(bb.get('atk_scale'), 1))
        if hidden_variant: kind = 'CHARGES'
        elif mode == 'BREAK': mods['aspd'] = num(bb.get('attack_speed')); grid = skill.get('rangeGrid'); has_attack = True; scale = number(num(bb.get('attack@split_atk_scale'), 1)); targets = str(max(1, math.floor(num(bb.get('attack@max_target'), 2))))+'U'
    elif name == 'Svash2':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        first = next((t for t in raw.get('talents', []) if t.get('index') == 0), {})
        mode = 'PLAN' if selected == 'skchr_svash2_1' else 'CHANGE' if selected == 'skchr_svash2_3' else 'EDGE'
        rules = record('Svash2Kit', MySkill='Svash2SkillKind::'+mode, MyEye=quote(skill.get('overrideTokenKey') or first.get('tokenKey') or 'token_10057_svash2_eagle2'),
            MyRange=tables.grid(skill.get('rangeGrid') or [[0,0],[0,1]]), MyTargets=str(max(1, math.ceil(num(bb.get('max_target'), 6))))+'U', MyMaxCasts=str(max(1, math.ceil(num(bb.get('max_stack_cnt'), 2))))+'U',
            MyScale=number(num(bb.get('atk_scale'), 1)), MyCold=number(num(bb.get('cold'))), MyCostCut=number(num(bb.get('svash2_s_1[deck].cost') if mode == 'PLAN' else bb.get('cost'))),
            MyShieldRatio=number(num(bb.get('svash2_s_1[deck].shield'))), MyDp=number(num(bb.get('svash2_s_3[start_cost].cost') if mode == 'CHANGE' else bb.get('cost'))),
            MyPeriodicDp=number(num(bb.get('svash2_s_3[cost].cost'), 1)), MyDpInterval=number(max(0.1, num(bb.get('svash2_s_3[cost].interval'), 2))),
            MyDeploySp=number(num(talent.get('sp'))), MyRedeployMultiplier=number(num(talent.get('respawn_time'), 1)), MyDefense=number(num(t1.get('def'))), MyRegen=number(num(t1.get('hp_recovery_per_sec_by_max_hp_ratio'))),
            MyTalentDelay=number(num(t1.get('interval'), 15)), MyFragile=number(num(bb.get('damage_scale'), 1)-1), MyFragileDuration=number(num(bb.get('weak[limit]'), 2)))
        mods = {}
        if mode == 'CHANGE': grid = skill.get('rangeGrid'); has_attack = True; scale = number(num(bb.get('bird_atk_scale'), 1))
        elif mode == 'EDGE': kind = 'CHARGES'
        else: kind = 'CHARGES' if num(skill.get('maxChargeTime'), 1) > 1 else 'INSTANT'; trigger = 'SkillTrigger::SP_FULL'
    elif name == 'Ghost2':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        around = (raw.get('trait') or {}).get('rangeGrid') or [[r,c] for r in (1,0,-1) for c in (-1,0,1)]
        mode = 'SHARE' if selected == 'skchr_ghost2_1' else 'WEIGHT' if selected == 'skchr_ghost2_3' else 'DESIRE'
        rules = record('Ghost2Kit', MySkill='Ghost2SkillKind::'+mode, MyAround=tables.grid(around), MyShareRange=tables.grid(skill.get('rangeGrid') or around),
            MySlow=number(num(talent.get('move_speed'))), MyDamageScale=number(num(talent.get('atk_scale'))), MyTeamHealth=number(num(t1.get('max_hp'))),
            MyDollAttack=number(num(trait.get('atk'))), MyDollHealth=number(num(trait.get('max_hp'))), MyExtraScale=number(num(bb.get('attack@atk_scale_ex'))), MyHealthLoss=number(num(bb.get('attack@hp_ratio'))))
        mods = dict(atkPct=num(bb.get('atk')))
        if mode == 'DESIRE': mods['aspd'] = num(bb.get('attack_speed'))
        elif mode == 'WEIGHT': mods.update(hpPct=num(bb.get('max_hp')), batPct=max(-0.9, delta/bat)); has_attack = True
    elif name == 'Dusk':
        second = next((t for t in raw.get('talents', []) if t.get('index') == 1), {})
        t1 = second.get('bb') or {}
        mode = 'INK' if selected == 'skchr_dusk_2' else 'FREEHAND' if selected == 'skchr_dusk_3' else 'BRUSH'
        token_id = second.get('tokenKey') or next(iter(raw.get('tokens') or []), 'token_10015_dusk_drgn')
        rules = record('DuskKit', MySkill='DuskSkillKind::'+mode, MyToken=quote(token_id), MyTokenDuration=number(num(t1.get('attack@tokenduration'), 25)),
            MyKillAttack=number(num(talent.get('atk'))), MyMaxStacks=str(max(1, math.ceil(num(talent.get('max_stack_cnt'), 15))))+'U',
            MyHealthThreshold=number(num(bb.get('hp_ratio'), 0.5)), MyLowHealthScale=number(num(bb.get('damage_scale'), 1)))
        mods = {}; has_attack = True
        if mode == 'BRUSH': kind = 'CHARGES'; scale = number(num(bb.get('atk_scale'), 1))
        else:
            grid = skill.get('rangeGrid'); mods = dict(atkPct=num(bb.get('atk')))
            if mode == 'INK': mods['aspd'] = num(bb.get('attack_speed'))
            else: mods['batPct'] = max(-0.9, delta/bat)
        if raw.get('isGolden') and '攻击范围扩大' in str((raw.get('trait') or {}).get('moduleDesc', '')):
            module = raw.get('module') or {}
            rec = next((m for m in raw.get('modules', []) if module.get('active') and m.get('uniEquipId') == module.get('id')), {})
            base_grid = next((t.get('rangeGrid') for t in rec.get('talentChanges', []) if t.get('rangeGrid') and t.get('talentIndex') == -1), None)
            if base_grid is None: permanent('t5:moduleRange', dict(rangeExtend=1))
    elif name == 'Lisa':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        hidden = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == -1), {})
        fox = selected not in ('skchr_lisa_1', 'skchr_lisa_2')
        rules = record('LisaKit', MyFox=boolean(fox), MySpRecovery=number(num(talent.get('sp_recovery_per_sec'))), MyFragile=number(max(0, num(t1.get('damage_scale'), 1)-1)),
            MyBoost=number(num(bb.get('scale_delta_to_one'), 1)), MyHealRatio=number(num(bb.get('attack@atk_to_hp_recovery_ratio'))), MyModuleSp=number(num(hidden.get('sp_recovery_per_sec')) if raw.get('isGolden') else 0))
        mods = {} if fox else dict(atkPct=num(bb.get('atk')))
        if fox: grid = skill.get('rangeGrid'); no_attack = True
        elif selected == 'skchr_lisa_1': mods['aspd'] = num(bb.get('attack_speed'))
        else: kind = 'TOGGLE'; targets = str(max(1, math.floor(num(bb.get('attack@max_target'), 2))))+'U'
    elif name == 'Demkni':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        mode = 'TRIAGE' if selected == 'skchr_demkni_1' else 'CALCIFY' if selected == 'skchr_demkni_3' else 'MEDICINE'
        area = skill.get('rangeGrid') or [[0,0]]
        rules = record('DemkniKit', MySkill='DemkniSkillKind::'+mode, MyRange=tables.grid(area), MyGrowthInterval=number(max(1, num(talent.get('interval'), 20))),
            MyMaxStacks=str(max(1, math.ceil(num(talent.get('max_stack_cnt'), 5))))+'U', MyGrowth=generic_modifiers(tables, dict(atkPct=num(talent.get('atk')), defPct=num(talent.get('def')))),
            MyHealSp=number(num(t1.get('sp'))), MyHealScale=number(num(bb.get('attack@heal_scale' if mode == 'CALCIFY' else 'heal_scale'), 0 if mode == 'CALCIFY' else 1)),
            MyLowHealthThreshold=number(num(trait.get('hp_ratio'))), MyLowHealScale=number(num(trait.get('heal_scale'))),
            MyCalcify=generic_modifiers(tables, dict(artsTakenMul=num(bb.get('demkni_s_3.damage_scale'), 1), moveMul=max(0, 1+num(bb.get('demkni_s_3.move_speed'))))))
        mods = {}
        if mode == 'TRIAGE':
            kind = 'CHARGES' if num(skill.get('maxChargeTime'), 1) > 1 else 'INSTANT'; has_attack = True; grid = area; trigger = 'SkillTrigger::DEFAULT'
            heal_scale = number(num(bb.get('heal_scale'), 1))
        elif mode == 'MEDICINE': kind = 'INSTANT'; trigger = 'SkillTrigger::NEVER'
        if num(trait.get('damage_resistance')) > 0: permanent('saria:moduleY', dict(dmgTakenMul=1-num(trait.get('damage_resistance'))))
    elif name == 'Horn':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        hidden = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == -1), {})
        mode = 'FLARE' if selected == 'skchr_horn_1' else 'STORM' if selected == 'skchr_horn_2' else 'DEFENSE'
        total = max(0.1, num(skill.get('duration'), 24)); overload = max(0.1, min(total, num(bb.get('horn_s_3[overload_start].damage_duration'), total/2)))
        rules = record('HornKit', MySkill='HornSkillKind::'+mode, MyTeamAttack=number(num(talent.get('atk'))),
            MyReviveModifiers=generic_modifiers(tables, dict(hpMul=1-num(t1.get('max_hp')), aspd=num(t1.get('attack_speed')), defPct=num(t1.get('def')))),
            MyReviveHeal=number(num(t1.get('hp_ratio'), 1)), MyBlockedScale=number(num(trait.get('atk_scale'), 1)), MyUnblockedSpeed=number(num(hidden.get('attack_speed'))),
            MyFlareRadius=number(num(bb.get('projectile_range'), 1.7)), MyFlareDuration=number(num(bb.get('projectile_delay_time'), 6)), MyMagicScale=number(num(bb.get('attack@s2.magic_atk_scale'))),
            MyOverloadAttack=number(num(bb.get('horn_s_3[overload_start].atk'), num(bb.get('atk')))-num(bb.get('atk'))), MyMainDuration=number(total-overload), MyOverloadDuration=number(overload),
            MyLossInterval=number(max(0.05, num(bb.get('horn_s_3[overload_start].interval'), 0.2))), MyPeakLoss=number(num(bb.get('horn_s_3[overload_start].hp_ratio'))))
        mods = {}; has_attack = mode != 'DEFENSE'
        if mode == 'FLARE': kind = 'CHARGES' if num(skill.get('maxChargeTime'), 1) > 1 else 'INSTANT'; scale = number(num(bb.get('atk_scale'), 1))
        elif mode == 'STORM': kind = 'AMMO'; ammo = max(1, num(bb.get('attack@s2.trigger_time'), 10)); scale = number(num(bb.get('attack@s2.atk_scale'), 1))
        else: duration = number(total); mods = dict(atkPct=num(bb.get('atk')), batPct=max(-0.9, delta/bat))
    elif name == 'Surtr':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        mode = 'BLADE' if selected == 'skchr_surtr_1' else 'GIANT' if selected == 'skchr_surtr_2' else 'TWILIGHT'
        rules = record('SurtrKit', MySkill='SurtrSkillKind::'+mode, MyEmberDuration=number(num(t1.get('surtr_t_2[withdraw].interval'), 8)),
            MyInterval=number(max(0.05, num(bb.get('interval'), 0.2))), MyPeakLoss=number(num(bb.get('hp_ratio'))), MyRamp=number(max(0.1, num(bb.get('duration'), 60))),
            MySoloScale=number(num(bb.get('attack@surtr_s_2[critical].atk_scale'), 1)), MyUnblockedSpeed=number(num(trait.get('attack_speed'))), MyArtsFragile=number(max(0, num(trait.get('damage_scale'), 1)-1)))
        permanent('surtr:magma', dict(resIgnoreFlat=num(talent.get('magic_resist_penetrate_fixed'))))
        mods = dict(atkPct=num(bb.get('atk')))
        if mode == 'BLADE': kind = 'CHARGES' if num(skill.get('maxChargeTime'), 1) > 1 else 'INSTANT'; has_attack = True; scale = number(num(bb.get('atk_scale'), 1)); mods = {}
        else:
            range_extend = math.floor(num(bb.get('ability_range_forward_extend'))+0.5)
            if num(bb.get('attack@max_target')) > 0: targets = str(math.floor(num(bb.get('attack@max_target'))))+'U'
            if mode == 'TWILIGHT': kind = 'TOGGLE'; hp = num(bb.get('max_hp')); mods['hpFlat' if abs(hp) > 5 else 'hpPct'] = hp
    elif name == 'Etlchi':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        t2 = next((t.get('bbStr') or {} for t in raw.get('talents', []) if t.get('index') == 2), {})
        mode = 'ROSE' if selected == 'skchr_etlchi_1' else 'SICKLE' if selected == 'skchr_etlchi_2' else 'CANDLE'
        rules = record('EtlchiKit', MySkill='EtlchiSkillKind::'+mode, MyCandleId=quote(t2.get('take_extra_enemy_key') or 'enemy_5601_entlec'),
            MyCandleHealth=number(num(bb.get('attack@max_hp_scale'), 0.6)), MyCandleDefense=number(num(bb.get('attack@def_scale'), 1)), MyCandleResistance=number(num(bb.get('attack@magic_resistance_scale'), 1)),
            MyCandles=str(max(1, math.ceil(num(bb.get('attack@max_target'), 3))))+'U', MySteal=number(num(talent.get('attack@steal_hp'))), MyStealCap=number(num(talent.get('attack@steal_hp_max'))),
            MyDot=number(num(talent.get('magic_value'))), MyDotDuration=number(num(talent.get('dot_duration'), 5)), MyDotInterval=number(num(talent.get('interval'), 1)),
            MyHealthThreshold=number(num(t1.get('hp_ratio'))), MyHealRatio=number(num(t1.get('etlchi_t_2[heal].hp_ratio'))), MyReduction=number(num(t1.get('damage_resistance'))),
            MyCrowdSpeed=number(num(trait.get('attack_speed'))), MyCrowdCount=number(num(trait.get('cnt'))), MySickleScale=number(num(bb.get('atk_scale'))), MySickleInterval=number(max(0.1, num(bb.get('interval'), 0.5))))
        mods = {}; has_attack = mode == 'ROSE'
        if mode == 'ROSE': kind = 'CHARGES' if num(skill.get('maxChargeTime'), 1) > 1 else 'INSTANT'; scale = number(num(bb.get('atk_scale'), 1)); hits = '2U'
        elif mode == 'SICKLE': no_attack = True
        else: mods = dict(atkPct=num(bb.get('atk')), aspd=num(bb.get('attack_speed'))); grid = skill.get('rangeGrid')
    elif name == 'Ulpia':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        mode = 'CONTACT' if selected == 'skchr_ulpia_1' else 'BOUNDARY' if selected == 'skchr_ulpia_2' else 'PATH'
        trig = (skill.get('trigger') or {}).get('customRangeGrid') or [[0, n] for n in range(1, 7)]
        rules = record('UlpiaKit', MySkill='UlpiaSkillKind::'+mode, MyToken=quote(skill.get('overrideTokenKey') or 'token_10039_ulpia_block'), MyRange=tables.grid(skill.get('rangeGrid') or raw.get('rangeGrid')),
            MyReach=str(max([1] + [int(p[1]) for p in trig if p[0] == 0]))+'U', MyRadius=number(num(bb.get('projectile_range'), 1.5)), MyScale=number(num(bb.get('atk_scale'))),
            MyStun=number(num(bb.get('stun'))), MyForce=number(num(bb.get('force'), 1)), MyTargets=str(max(1, math.ceil(num(bb.get('max_target'), 2))))+'U',
            MyHealthThreshold=number(num(talent.get('hp_ratio'), 0.5)), MyHeal=number(num(talent.get('value1'))), MyLowHeal=number(num(talent.get('value2'))), MyTalentScale=number(num(bb.get('talent_scale'), 1)),
            MyGrowth=generic_modifiers(tables, dict(hpFlat=num(t1.get('max_hp')), atkFlat=num(t1.get('atk')))),
            MySharedGrowth=generic_modifiers(tables, dict(hpFlat=num(t1.get('ulpia_t_1[abyssal].max_hp')), atkFlat=num(t1.get('ulpia_t_1[abyssal].atk')))),
            MyMaxStacks=str(max(1, math.ceil(num(t1.get('max_stack_cnt'), 9))))+'U', MySharedMaxStacks=str(max(1, math.ceil(num(t1.get('ulpia_t_1[abyssal].max_stack_cnt'), 9))))+'U')
        mods = dict(atkPct=num(bb.get('atk')), hpPct=num(bb.get('max_hp')))
        if mode == 'BOUNDARY': kind = 'TOGGLE'; mods['blockCnt'] = num(bb.get('block_cnt'))
        elif mode == 'CONTACT': kind = 'CHARGES' if num(skill.get('maxChargeTime'), 1) > 1 else 'INSTANT'; mods = {}
        if num(trait.get('heal_scale')) > 0: permanent('ulpia:module', dict(healingTakenMul=num(trait.get('heal_scale'))))
    elif name == 'Blaze2':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        hidden = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == -1), {})
        mode = 'AID' if selected == 'skchr_blaze2_1' else 'GROUND' if selected == 'skchr_blaze2_2' else 'FURNACE'
        rules = record('Blaze2Kit', MySkill='Blaze2SkillKind::'+mode, MyMeltdownScale=number(num(talent.get('ep_damage_scale'))), MyMeltdownHeal=number(num(talent.get('hp_ratio'))),
            MyDownShield=number(num(t1.get('dynamic'))), MyDownRegen=number(num(t1.get('hp_recovery_per_sec_by_max_hp_ratio'))), MyReviveStun=number(num(t1.get('stun'))),
            MyBurstMultiplier=number(num(trait.get('damage_scale'), 1)), MyBurstSp=number(num(hidden.get('sp_recovery_per_sec'))), MyLoss=number(num(bb.get('lose_hp_scale'))),
            MyAttackLoss=number(num(bb.get('attack@hp_ratio'))), MyBurnBonus=number(num(bb.get('attack@atk_scale'))), MyAmmoRefill=number(num(bb.get('ammo_recover'))),
            MyRadius=number(num(bb.get('range_radius'), 1.5)), MyAidDuration=number(num(bb.get('max_duration'), 20)), MyAidInterval=number(max(0.1, num(bb.get('interval'), 1))),
            MyArtsScale=number(num(bb.get('atk_scale'))), MyElementScale=number(num(bb.get('element_multiplier' if mode == 'AID' else 'element_damage_scale'))), MyMoveMultiplier=number(max(0, 1+num(bb.get('move_speed')))))
        mods = {}
        if mode == 'AID': kind = 'CHARGES' if num(skill.get('maxChargeTime'), 1) > 1 else 'INSTANT'
        else:
            mods = dict(atkPct=num(bb.get('atk')), batPct=max(-0.9, delta/bat)); grid = skill.get('rangeGrid'); has_attack = True
            if mode == 'GROUND': targets = str(max(1, math.floor(num(bb.get('attack@max_target'), 3))))+'U'
            else: kind = 'AMMO'; ammo = num(bb.get('attack@trigger_time'), 18)
    elif name == 'Titi':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        mode = 'ERODE' if selected == 'skchr_titi_1' else 'WARD' if selected == 'skchr_titi_2' else 'BLOOM'
        sleep = num(bb.get('attack@sleep'), 0 if mode == 'ERODE' else num(bb.get('sleep'), 5))
        rules = record('TitiKit', MySkill='TitiSkillKind::'+mode, MyStillScale=number(num(talent.get('extra_atk_scale'))), MyDreamScale=number(num(talent.get('damage_atk_scale'))),
            MyTalentScale=number(num(bb.get('talent_scale'), 1)), MyHealthThreshold=number(num(t1.get('hp_ratio'), 0.5)), MyAuraSpeed=number(num(t1.get('attack_speed'))),
            MySleep=number(sleep), MySleepChance=number(num(bb.get('attack@prob'))), MyMinScale=number(num(bb.get('min_atk_scale'), 1)),
            MyMaxScale=number(num(bb.get('max_atk_scale'), num(bb.get('min_atk_scale'), 1))), MyChainSleep=number(num(bb.get('sleep'), sleep)),
            MyRadius=number(num(bb.get('range_radius'), 1.5)), MyChainTargets=str(max(1, math.ceil(num(bb.get('max_target'), 1))))+'U')
        mods = dict(atkPct=num(bb.get('atk'))); has_attack = True; no_attack = mode == 'WARD'
        if mode == 'BLOOM': targets = str(max(1, math.floor(num(bb.get('attack@max_target'), 2))))+'U'
    elif name == 'Excu2':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        hidden = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == -1), {})
        base_heal = (raw.get('traitBase') or {}).get('bb') or {}
        mode = 'EXECUTE' if selected == 'skchr_excu2_1' else 'VERDICT' if selected == 'skchr_excu2_3' else 'GUNFIGHT'
        rules = record('Excu2Kit', MySkill='Excu2SkillKind::'+mode, MyExtraChance=number(num(talent.get('prob'))), MyChancePerAmmo=number(num(talent.get('prob_add'))),
            MyFactionAmmo=number(num(t1.get('add_count'), 1)), MyFactionCap=number(num(t1.get('add_count_max_stack'), 4)), MyDodgeChance=number(num(bb.get('prob'))),
            MyRefill=number(num(bb.get('recover_cnt'), 1)), MyAttackPerAmmo=number(num(bb.get('attack@atk'))), MyMaxStacks=str(max(1, math.floor(num(bb.get('attack@max_stack_cnt'), 30))))+'U',
            MyFinalScale=number(num(bb.get('attack@final_atk_scale'))), MyHealScale=number(num(bb.get('trait_ratio'), 1)),
            MyBaseHeal=number(num(base_heal.get('value'), 50)) if hidden.get('trigger_cnt[equip]') is not None and base_heal.get('value') is not None else '{}',
            MyCrowdSpeed=number(num(hidden.get('attack_speed'))), MyCrowdCount=number(num(hidden.get('trigger_cnt[equip]'))))
        kind = 'AMMO'; ammo = num(bb.get('attack@trigger_time'), 8 if mode == 'EXECUTE' else 16 if mode == 'VERDICT' else 12)
        mods = dict(atkPct=num(bb.get('atk')))
        if mode == 'GUNFIGHT': mods.update(defPct=num(bb.get('def')), blockCnt=num(bb.get('block_cnt')))
        else:
            grid = skill.get('rangeGrid')
            if mode == 'EXECUTE': mods['defIgnoreFlat'] = num(bb.get('def_penetrate_fixed'))
            else: mods['batPct'] = max(-0.9, delta/bat); has_attack = True
    elif name == 'Cetsyr':
        orbit = base == 'chess_char_5_09_a'
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        hidden = {}
        for t in raw.get('talents', []):
            if not t.get('name'): hidden.update(t.get('bb') or {})
        mode = 'REWEAVE' if orbit else 'PAST' if selected == 'skchr_cetsyr_1' else 'TOMORROW' if selected == 'skchr_cetsyr_2' else 'REWEAVE'
        ratio = numeric(bb.get('attack@atk_to_hp_recovery_ratio'))
        base_ratio = num(trait.get('attack@atk_to_hp_recovery_ratio'), 0.1)
        if orbit and ratio is None: ratio = base_ratio
        rules = record('CetsyrKit', MySkill='CetsyrSkillKind::'+mode, MyTraitRatio=number(ratio) if ratio is not None else '{}', MyMotes=str(max(0, math.floor(num(talent.get('cnt'), 3))))+'U',
            MyCooldown=number(num(talent.get('cooldown'), 6)), MySkillCooldown=number(num(bb.get('talent_cool_down'), 3)), MyAllyRadius=number(num(talent.get('range_radius'), 1.15)),
            MyMoteDuration=number(num(talent.get('talent_duration'), 6)), MyTraitScale=number(num(talent.get('attack@trait_mul'), 1.5)),
            MyEnemyRadius=number(num(bb.get('outside_radius'), 2)), MyMoteScale=number(num(bb.get('atk_scale'), 2.2)), MyBind=number(num(bb.get('unmoveable_duration'), 3)),
            MyInspire=number(num(bb.get('attack@atk' if mode == 'TOMORROW' else 'max_hp'), 0 if orbit else 0.65)), MyRedistributeInterval=number(max(0.1 if orbit else 0.5, num(bb.get('attack@cetsyr_s_3[cal_hp_ratio].interval'), 2))),
            MySarkazReduction=number(num(t1.get('damage_resistance'), 0 if orbit else 0.1)), MyModuleAttack=number(num(hidden.get('atk')) if not orbit or raw.get('isGolden') else 0), MyModuleCount=number(num(hidden.get('cnt'), 2)),
            MyOrbitMotes=boolean(orbit), MyAngularSpeed=number(num(talent.get('dynamic_spd'), 30)*math.pi/180), MyBaseRatio=number(base_ratio))
        mods = {}
        if mode == 'PAST': kind = 'TOGGLE'; trigger = 'SkillTrigger::SP_FULL'
        elif mode == 'REWEAVE': grid = skill.get('rangeGrid')
    elif name == 'Gvial2':
        hidden_variant = base == 'chess_char_5_18_a'
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        mode = 'PULL' if hidden_variant else 'HEAL' if selected == 'skchr_gvial2_1' else 'PULL' if selected == 'skchr_gvial2_2' else 'DEFER'
        rules = record('GvialKit', MySkill='GvialSkillKind::'+mode, MyAttack=number(num(talent.get('atk'), 0.1)), MyDefense=number(num(talent.get('def'), 0.1)),
            MyAttackPerBlock=number(num(talent.get('atk_add'), 0.04)), MyDefensePerBlock=number(num(talent.get('def_add'), 0.04)),
            MyHealthThreshold=number(num(t1.get('hp_ratio'), 0.5)), MyHealingScale=number(num(t1.get('heal_scale_1'), 1.2)), MyLowHealthHealingScale=number(num(t1.get('heal_scale_2'), 1.4)),
            MyBlockedScale=number(num(trait.get('atk_scale'))), MyReductionThreshold=number(num(trait.get('hp_ratio'))), MyPhysicalReduction=number(num(trait.get('damage_resistance'))),
            MyDeferral=number(min(0.99, max(0, num(bb.get('damage_resistance'), 0.5)))), MyDelayDuration=number(num(bb.get('final_duration'), 20)), MyDelayInterval=number(max(1/30, num(bb.get('interval'), 0.1))),
            MyLifeSteal=number(num(bb.get('heal_scale'), 0.3)), MyForce=number(num(bb.get('attack@force'), 1)), MyHiddenVariant=boolean(hidden_variant))
        mods = dict(atkPct=num(bb.get('atk')))
        if mode == 'PULL':
            mods['defPct'] = num(bb.get('def')); grid = skill.get('rangeGrid'); has_attack = True
            if hidden_variant: mods['blockCnt'] = num(bb.get('block_cnt'))
        elif mode == 'DEFER': mods.update(aspd=num(bb.get('attack_speed')), blockCnt=num(bb.get('block_cnt'), 2))
    elif name == 'Billro':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        hidden = {}
        for t in raw.get('talents', []):
            if not t.get('name'): hidden.update(t.get('bb') or {})
        mode = 'GUARD' if selected == 'skchr_billro_1' else 'DEVOUR' if selected == 'skchr_billro_3' else 'CHAINS'
        rules = record('BillroKit', MySkill='BillroSkillKind::'+mode, MyAttack=number(num(bb.get('atk'), 0.1)), MyKeepDefense=number(num(trait.get('billro_e_002[buff].def'))),
            MyKeepResistance=number(num(trait.get('billro_e_002[buff].magic_resistance'))), MyHeal=number(num(talent.get('heal_scale'), 0.4)),
            MyChargedHeal=number(num(talent.get('billro_t_1[enhance].heal_scale'), 0.8)), MySpRecovery=number(num(t1.get('sp_recovery_per_sec'), 0.6)),
            MyEnemyScale=number(num(hidden.get('damage_scale'))), MyEnemyCap=number(num(hidden.get('max_valid_stack_cnt'), 5)), MyMarkScale=number(num(bb.get('attack@damage_scale'), 0.2)),
            MyBind=number(num(bb.get('attack@root'), 0.3)), MySluggish=number(num(bb.get('attack@sluggish'), 0.3)))
        mods = dict(atkPct=num(bb.get('atk')), defPct=num(bb.get('def'))) if mode == 'GUARD' else dict(batPct=max(-0.9, delta/bat)) if mode == 'CHAINS' else {}
        if mode == 'DEVOUR': grid = skill.get('rangeGrid')
    elif name == 'Bldsk':
        merged = base == 'chess_char_5_04_a'
        bandage = merged or selected == 'skchr_bldsk_1'
        rules = record('BldskKit', MyBandage=boolean(bandage), MyBandageHeal=number(num(bb.get('hp_ratio'), 0 if merged else 0.15)), MyAttack=number(num(bb.get('atk'))),
            MyDuration=number(num(bb.get('duration'), 15)), MyInterval=number(max(0.1, num(bb.get('interval'), 1))), MyHealthLoss=number(num(bb.get('hp_ratio'), 0.03)),
            MySelfSp=number(num(talent.get('bldsk_t_1[self].sp'), 0 if merged else 2)), MyAllySp=number(num(talent.get('bldsk_t_1[rand].sp'), 0 if merged else 2)),
            MyHealthRatio=number(num(trait.get('hp_ratio'))), MyHealScale=number(num(trait.get('heal_scale'))), MyMergedHeal=boolean(merged))
        mods = {}; healing = True
        kind = 'CHARGES' if bandage and num(skill.get('maxChargeTime'), 1) > 1 else 'INSTANT'
        if bandage: trigger = 'SkillTrigger::NEVER'
        if merged: kind = 'CHARGES'
    elif name == 'Flamtl':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        mode = 'EVADE' if selected == 'skchr_flamtl_1' else 'RED_PINE' if selected == 'skchr_flamtl_2' else 'FLAME'
        rules = record('FlamtlKit', MySkill='FlamtlSkillKind::'+mode, MyRange=tables.grid(skill.get('rangeGrid') or [[0,0],[0,1]]),
            MyBlockingModifiers=generic_modifiers(tables, dict(atkPct=num(trait.get('atk')), defPct=num(trait.get('def')))),
            MyDp=number(num(bb.get('cost'), 6 if mode == 'EVADE' else 11 if mode == 'RED_PINE' else 1)), MyPulses=str(max(0, math.floor(num(bb.get('value'), 8))))+'U',
            MyProbability=number(num(bb.get('prob'), 0.6)), MyScale=number(num(bb.get('atk_scale'), 1.8)), MyStun=number(num(bb.get('stun'), 0.5)),
            MyTargets=str(max(1, math.floor(num(bb.get('max_target'), 6))))+'U', MyDodge=number(num(bb.get('flamtl_s_2.prob'), 0.4)), MyDodgeDuration=number(num(bb.get('flamtl_s_2.duration'), 10)),
            MyNationDodge=number(num(t1.get('prob'), 0.22)))
        mods = {}
        if mode == 'FLAME': mods = dict(atkPct=num(bb.get('atk')), batPct=max(-0.9, num(bb.get('base_attack_time'), 1)-1), blockCnt=num(bb.get('block_cnt'), 1))
        else:
            kind = 'INSTANT'
            if mode == 'EVADE': trigger = 'SkillTrigger::SP_FULL'
    elif name == 'Fartth':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        mode = 'QUICK' if selected == 'skcom_quickattack[3]' else 'ALLIED' if selected == 'skchr_fartth_2' else 'LINE'
        rules = record('FartthKit', MySkill='FartthSkillKind::'+mode, MyQuietTime=number(num(talent.get('delay'), 10)), MyAttack=number(num(talent.get('atk'), 0.15)),
            MySurvivorSp=number(num(trait.get('sp'))), MyFarScale=number(num(bb.get('damage_scale'), 1.25)), MyDistanceScale=number(num(trait.get('damage_scale'))),
            MyMinDistance=number(num(trait.get('min_dist'), 1)), MyMaxDistance=number(num(trait.get('max_dist'), 4.5)))
        mods = dict(taunt=num(t1.get('taunt_level'), -1))
        if mode != 'ALLIED': mods['atkPct'] = num(bb.get('atk'))
        if mode != 'LINE': mods['aspd'] = num(bb.get('attack_speed'))
        else: grid = [[0, i] for i in range(21)]
    elif name == 'Plosis':
        global_aura = base == 'chess_char_5_16_a'
        rules = record('SpAuraKit', MyRecovery=number(num(talent.get('sp_recovery_per_sec'), 0 if global_aura else 0.3)), MyGlobal=boolean(global_aura))
        healing = True
        if not global_aura and selected == 'skcom_heal_up[3]': mods = dict(atkPct=num(bb.get('atk')))
        else: mods = dict(batPct=max(-0.9, delta/bat)); grid = skill.get('rangeGrid')
        module = raw.get('module') or {}
        if module.get('active') and (not global_aura or raw.get('isGolden') and '攻击范围扩大' in str((raw.get('trait') or {}).get('moduleDesc', ''))):
            rec = next((m for m in raw.get('modules', []) if m.get('uniEquipId') == module.get('id')), {})
            base_grid = next((t.get('rangeGrid') for t in rec.get('talentChanges', []) if t.get('rangeGrid') and (not global_aura or t.get('talentIndex') == -1)), None)
        if global_aura and raw.get('isGolden') and '攻击范围扩大' in str((raw.get('trait') or {}).get('moduleDesc', '')) and base_grid is None: permanent('t5:moduleRange', dict(rangeExtend=1))
    elif name == 'Svrash':
        slash = selected == 'skchr_svrash_3'
        rules = record('SvrashKit', MyRedeployMultiplier=number(max(0, 1+num(talent.get('respawn_time'), -0.1))), MyAdditionScale=number(num(trait.get('atk_scale_m'))), MySlash=boolean(slash))
        permanent('svrash:t1', dict(atkPct=num(talent.get('atk'), 0.1)), True)
        if selected == 'skchr_svrash_1': mods = {}; kind = 'CHARGES' if num(skill.get('maxChargeTime'), 1) > 1 else 'INSTANT'; has_attack = True; scale = number(num(bb.get('atk_scale'), 2.05))
        elif selected == 'skchr_svrash_2': kind = 'TOGGLE'; mods = dict(defPct=num(bb.get('def')), hpRegenRatio=num(bb.get('hp_recovery_per_sec_by_max_hp_ratio'))); grid = skill.get('rangeGrid')
        else: mods = dict(defPct=num(bb.get('def')), atkPct=num(bb.get('atk'))); grid = skill.get('rangeGrid'); has_attack = True; targets = str(max(1, math.floor(num(bb.get('attack@max_target'), 4))))+'U'
    elif name == 'Texas2':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        mode = 'DRIZZLE' if selected == 'skchr_texas2_1' else 'STORM' if selected == 'skchr_texas2_2' else 'RAIN'
        area = skill.get('rangeGrid') or [[-1,-1],[-1,0],[-1,1],[0,-1],[0,0],[0,1],[1,-1],[1,0],[1,1]]
        rules = record('Texas2Kit', MySkill='Texas2SkillKind::'+mode, MyRange=tables.grid(area),
            MyBurstScale=number(num(bb.get('atk_scale'), 1.5) if mode == 'STORM' else num(bb.get('appear.atk_scale'), 1.15)), MyBurstStun=number(num(bb.get('appear.stun'), 1.5)),
            MyScale=number(num(bb.get('atk_scale'), 0.85)), MyStun=number(num(bb.get('stun'), 0.2)), MyInterval=number(max(0.1, num(bb.get('texas2_s_3[sword].interval'), 1))),
            MyTargets=str(max(1, math.floor(num(bb.get('max_target'), 2))))+'U', MyResistance=number(num(bb.get('magic_resistance'))), MyDebuffDuration=number(num(bb.get('debuff_duration'), 8)),
            MySilence=number(num(bb.get('attack@silence'), 5)), MyDotDuration=number(num(bb.get('attack@texas2_s_1[dot].duration'), num(bb.get('attack@silence'), 5))),
            MyDotDamage=number(num(bb.get('attack@texas2_s_1[dot].dot_damage'), 260)), MyDotInterval=number(max(0.1, num(bb.get('attack@texas2_s_1[dot].interval'), 1))),
            MyHealRatio=number(num(talent.get('hp_ratio'), 1)), MyAttackSpeed=number(num(t1.get('attack_speed'), 8)), MyDamageReduction=number(num(t1.get('damage_resistance'), 0.25)), MyLonelyAttack=number(num(trait.get('atk'))))
        mods = dict(atkPct=num(talent.get('atk'), 0.2)+(num(bb.get('atk')) if mode != 'RAIN' else 0)); on_deploy = True; trigger = 'SkillTrigger::NEVER'
        duration = number(num(skill.get('duration'), 11 if mode == 'DRIZZLE' else 8 if mode == 'STORM' else 6))
        if mode == 'STORM': has_attack = True; hits = '2U'
    elif name == 'Hsguma':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        counter = selected == 'skchr_hsguma_2'
        saw = selected == 'skchr_hsguma_3'
        rules = record('HsgumaKit', MyBlockProbability=number(num(talent.get('prob'), 0.25)), MyAuraDefense=number(num(t1.get('def'), 0.06)),
            MyBlockingDefense=number(num(trait.get('def'))), MyCounterScale=number(num(bb.get('atk_scale'), 0.65)), MyCounter=boolean(counter), MySaw=boolean(saw))
        mods = dict(defPct=num(bb.get('def')))
        if counter: kind = 'PASSIVE'
        else: mods['atkPct'] = num(bb.get('atk'))
        has_attack = saw
    elif name == 'Mudrok':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        mode = 'DEFENSE' if selected == 'skcom_def_up[3]' else 'DORMANT' if selected == 'skchr_mudrok_3' else 'HAMMER'
        area = skill.get('rangeGrid') or [[-1,-1],[-1,0],[-1,1],[0,-1],[0,0],[0,1],[1,-1],[1,0],[1,1]]
        rules = record('MudrokKit', MySkill='MudrokSkillKind::'+mode, MyRange=tables.grid(area),
            MyAwakeModifiers=generic_modifiers(tables, dict(atkPct=num(bb.get('atk')), defPct=num(bb.get('def')), batPct=max(-0.9, delta/bat))),
            MyLonelyModifiers=generic_modifiers(tables, dict(atkPct=num(trait.get('atk')), defPct=num(trait.get('def')))), MySleep=number(num(bb.get('sleep'), 10)),
            MyMoveMultiplier=number(max(0, 1+num(bb.get('move_speed'), -0.6))), MyStun=number(num(bb.get('stun'), 3 if mode == 'DORMANT' else 0.5)),
            MyProbability=number(num(bb.get('buff_prob'), 0.3)), MySkillHeal=number(num(bb.get('hp_ratio'), 0.04)),
            MyMaxLayers=str(max(0, math.floor(num(talent.get('max_times'), 3)))), MyLayerGain=str(max(0, math.floor(num(talent.get('times'), 1)))),
            MyLayerInterval=number(num(talent.get('interval'), 9)), MyLayerHeal=number(num(talent.get('hp_ratio'), 0.2)),
            MySarkazReduction=number(num(t1.get('damage_resistance'), 0.3)), MyBlockedScale=number(num(trait.get('damage_scale'))))
        mods = {}; has_attack = mode != 'DEFENSE'
        if mode == 'DEFENSE': mods = dict(defPct=num(bb.get('def')))
        elif mode == 'HAMMER': kind = 'INSTANT'; grid = area; scale = number(num(bb.get('atk_scale'), 1.9))
    elif name == 'Gnosis':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        mode = 'THOUGHT' if selected == 'skchr_gnosis_1' else 'HYPOTHERMIA' if selected == 'skchr_gnosis_3' else 'BURST'
        rules = record('GnosisKit', MySkill='GnosisSkillKind::'+mode, MyScale=number(num(bb.get('atk_scale'), 3 if mode == 'HYPOTHERMIA' else 1.3)),
            MyCold=number(num(bb.get('cold'), 2.5)), MyAttackCold=number(num(talent.get('cold'), 1)), MyColdFragile=number(num(talent.get('damage_scale_cold'), 1.25)),
            MyFreezeFragile=number(num(talent.get('damage_scale_freeze'), 1.5)), MySpRecovery=number(num(trait.get('sp_recovery_per_sec'))),
            MyResistDelay=number(num(t1.get('interval'), 10)), MyResist=number(min(1, max(0, -num(t1.get('one_minus_status_resistance'), -0.5)))))
        mods = {}
        if mode == 'HYPOTHERMIA': mods = dict(aspd=num(bb.get('attack_speed'))); has_attack = True; targets = str(max(1, math.floor(num(bb.get('max_target'), 2))))+'U'
        else:
            kind = 'CHARGES' if num(skill.get('maxChargeTime'), 1) > 1 else 'INSTANT'
            if mode == 'THOUGHT': has_attack = True; scale = number(num(bb.get('atk_scale'), 1.35)); hits = '2U'
    elif name == 'Lionhd':
        burst = selected != 'skcom_atk_up[3]'
        rules = record('LionhdKit', MyScale=number(num(bb.get('atk_scale'), 1.7)), MyResistance=number(num(bb.get('magic_resistance'))),
            MyDuration=number(num(bb.get('duration'), 6)), MyAttack=number(num(talent.get('atk'), 0.04)), MyMaxStacks=number(num(talent.get('max_valid_stack_cnt'), 5)), MyBurst=boolean(burst))
        if burst: mods = {}; kind = 'CHARGES' if num(skill.get('maxChargeTime'), 1) > 1 else 'INSTANT'; grid = skill.get('rangeGrid')
        else: mods = dict(atkPct=num(bb.get('atk')))
        module = raw.get('module') or {}
        if module.get('active'):
            rec = next((m for m in raw.get('modules', []) if m.get('uniEquipId') == module.get('id')), {})
            base_grid = next((t.get('rangeGrid') for t in rec.get('talentChanges', []) if t.get('rangeGrid')), None)
    elif name == 'Reckpr':
        shared = base == 'chess_char_5_23_a'
        guard = not shared or selected != 'skchr_reckpr_1'
        rules = record('ReckprKit', MyGuardDuration=number(num(bb.get('attack@buff_duration'), 10)), MyGuardHeal=number(num(bb.get('attack@fixed_heal_value'), 0 if shared else 80)),
            MyProbability=number(num(talent.get('prob'), 1)), MySp=number(num(talent.get('sp'), 0 if shared else 1)), MyDuration=number(num(talent.get('duration'), 8)),
            MyAttackSpeed=number(num(talent.get('attack_speed'), 0 if shared else 16)), MyMaxStacks=str(max(1, math.floor(num(talent.get('max_stack_cnt'), 1))))+'U',
            MyHealthRatio=number(num(trait.get('hp_ratio'))), MyHealScale=number(num(trait.get('heal_scale'))), MySharedGuard=boolean(shared), MyGuard=boolean(guard))
        mods = dict(atkPct=num(bb.get('atk'))); healing = True
        if not guard: mods = {}; has_attack = True; kind = 'CHARGES' if num(skill.get('maxChargeTime'), 1) > 1 else 'INSTANT'; heal_scale = number(num(bb.get('heal_scale'), 1)); targets = str(max(1, math.floor(num(bb.get('max_target'), 2))))+'U'
    elif name == 'Glady':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        hidden = {}
        for t in raw.get('talents', []):
            if not t.get('name'): hidden.update(t.get('bb') or {})
        mode = 'RIP' if selected == 'skchr_glady_1' else 'GRASP' if selected == 'skchr_glady_2' else 'TORNADO'
        force = num(bb.get('force'), 1) if mode == 'RIP' else num(bb.get('attack@force'), 1) if mode == 'GRASP' else num(bb.get('force'), num(bb.get('attack@force')))
        rules = record('GladyKit', MySkill='GladySkillKind::'+mode, MyForce=number(force),
            MyFarRadius=number(num(hidden.get('skill@range_radius'), num(hidden.get('attack@range_radius')))), MyFarForce=number(num(hidden.get('skill@delta_force'), num(hidden.get('attack@delta_force')))),
            MyDragDamage=number(num(trait.get('value'))), MyDragDistance=number(max(1e-6, num(trait.get('dist'), 1))), MyInterval=number(max(0.1, num(bb.get('interval'), 1.5))),
            MyScale=number(num(bb.get('atk_scale'), 0.85)), MyMoveMultiplier=number(max(0, 1+num(bb.get('move_speed'), -0.5))),
            MyRegenRatio=number(num(talent.get('hp_recovery_per_sec_by_max_hp_ratio'), 0.025)), MySeaReduction=number(num(talent.get('damage_resistance'), 0.25)),
            MyMassLimit=number(num(t1.get('value'), 3)), MyMassScale=number(num(t1.get('atk_scale'), 1.3)))
        mods = {}
        if mode == 'RIP': has_attack = True; kind = 'CHARGES' if num(skill.get('maxChargeTime'), 1) > 1 else 'INSTANT'; scale = number(num(bb.get('atk_scale'), 1.5))
        elif mode == 'GRASP':
            has_attack = True; mods = dict(batPct=max(-0.9, delta/bat)); grid = skill.get('rangeGrid')
            targets = str(max(1, math.floor(num(bb.get('attack@max_target'), 2))))+'U'; scale = number(num(bb.get('attack@atk_scale'), 1.35))
    elif name == 'Cathy':
        forge = selected == 'skchr_cathy_1'
        mods = dict(atkPct=num(bb.get('s1_atk')), defPct=num(bb.get('s1_def'))) if forge else dict(hpPct=num(bb.get('max_hp')), defPct=num(bb.get('def')))
        rules = record('CathyKit', MyForgeModifiers=generic_modifiers(tables, mods) if forge else '{}', MyForge=boolean(forge))
        if forge: kind = 'PASSIVE'
        else: has_attack = True; no_attack = True
    elif name == 'Mizuki':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        hidden = {}
        for t in raw.get('talents', []):
            if not t.get('name'): hidden.update(t.get('bb') or {})
        mode = 'AWAKEN' if selected == 'skchr_mizuki_1' else 'MIRROR' if selected == 'skchr_mizuki_3' else 'BIND'
        rules = record('MizukiKit', MySkill='MizukiSkillKind::'+mode, MyTargets=str(max(1, math.floor(num(talent.get('attack@max_target'), 1))))+'U',
            MyExtraTargets=str(max(0, math.floor(num(bb.get('attack@max_target'), 1))))+'U', MyTalentScale=number(num(talent.get('attack@mizuki_t_1.atk_scale'), 0.5)),
            MyAwakenScale=number(num(bb.get('talent_scale'), 2)), MyStatusDuration=number(num(bb.get('attack@stun'), 0.7) if mode == 'MIRROR' else num(bb.get('attack@unmovable'), 0.8)),
            MySelfLoss=number(num(bb.get('attack@hp_ratio'), 0.15)), MyHealthThreshold=number(num(t1.get('hp_ratio'), 0.5)), MyAttack=number(num(t1.get('atk'), 0.1)),
            MySlow=number(max(0, 1+num(hidden.get('move_speed')))) if num(hidden.get('move_speed')) else '{}')
        if mode == 'AWAKEN': mods = {}; has_attack = True; kind = 'CHARGES' if num(skill.get('maxChargeTime'), 1) > 1 else 'INSTANT'; scale = number(num(bb.get('atk_scale'), 2))
        else:
            mods = dict(atkPct=num(bb.get('atk')))
            if mode == 'BIND': mods['batPct'] = max(-0.9, delta/bat)
            else: grid = skill.get('rangeGrid')
    elif name == 'Aroma':
        landing = selected == 'skchr_aroma_2'
        rules = record('AromaKit', MyFirstScale=number(num(talent.get('damage_scale'), 1.1)), MyLevitate=number(num(talent.get('levitate_duration'), 2.5)),
            MyDistanceScale=number(num(trait.get('damage_scale'))), MyMinDistance=number(num(trait.get('min_dist'))), MyMaxDistance=number(num(trait.get('max_dist'), 4)),
            MyFlyingScale=number(num(bb.get('atk_scale_to_fly'), 0.55)), MyLandingScale=number(num(bb.get('attack@atk_scale_when_fly_finish'), 0.65)), MyLanding=boolean(landing))
        if landing: mods = dict(atkPct=num(bb.get('atk')))
        else: mods = {}; has_attack = True; kind = 'CHARGES' if num(skill.get('maxChargeTime'), 1) > 1 else 'INSTANT'; scale = number(num(bb.get('atk_scale'), 1.5))
    elif name == 'Ines':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        mode = 'BLEED' if selected == 'skchr_ines_1' else 'RECALL' if selected == 'skchr_ines_3' else 'STEALTH'
        maximum = numeric(talent.get('steal_atk_max'))
        rules = record('InesKit', MySkill='InesSkillKind::'+mode, MyDp=number(num(bb.get('cost'), 2 if mode == 'BLEED' else 1)),
            MyBleedScale=number(num(bb.get('bleed_atk_scale'), 0.4)), MyBleedDuration=number(num(bb.get('bleed_duration'), 3)),
            MyStealSpeed=number(num(bb.get('attack@steal_atk_speed'), 5)), MyMaxSpeed=number(num(bb.get('attack@steal_atk_speed_max'), 50)),
            MyBind=number(num(talent.get('duration'), 5)), MyStealAttack=number(num(talent.get('steal_atk'), 90)),
            MyMaxAttack=number(maximum) if maximum is not None else 'std::numeric_limits<double>::infinity()', MyMoveMultiplier=number(max(0, 1+num(t1.get('move_speed'), -0.3))),
            MyFirstRedeploy=number(num(trait.get('respawn_time'))), MyAttack=number(num(bb.get('atk'))), MyRecallRadius=number(num(bb.get('projectile_range'), 1.4)),
            MyRecallScale=number(num(bb.get('atk_scale'), 1.1)), MyRecallTargets=str(max(1, math.floor(num(bb.get('max_target'), 4))))+'U')
        mods = {}; has_attack = mode == 'BLEED'
        if mode == 'BLEED': kind = 'CHARGES' if num(skill.get('maxChargeTime'), 1) > 1 else 'INSTANT'
        elif mode == 'STEALTH': mods = dict(atkPct=num(bb.get('atk'))); grid = skill.get('rangeGrid')
        else: trigger = 'SkillTrigger::NEVER'; duration = number(num(skill.get('duration'), 11))
    elif name == 'Rosesa':
        rules = record('RosesaKit', MyHealingScale=number(num(talent.get('heal_scale'), 1.15)), MyDamageScale=number(num(bb.get('attack@damage_scale'), 0.8)),
            MyDelayDuration=number(num(bb.get('attack@final_duration'), 5)), MyDelayInterval=number(max(0.1, num(bb.get('attack@interval'), 1))))
        permanent('rosesa:t1', dict(atkPct=num(talent.get('atk'), -0.05)), True)
        if default: mods = dict(batPct=max(-0.9, delta/bat)); healing = True
        module = raw.get('module') or {}
        if module.get('active'):
            rec = next((m for m in raw.get('modules', []) if m.get('uniEquipId') == module.get('id')), {})
            base_grid = next((t.get('rangeGrid') for t in rec.get('talentChanges', []) if t.get('rangeGrid')), None)
    elif name in ('Kroos2', 'Bpipe'):
        volley = name == 'Bpipe'
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        progressive = not volley and selected == 'skchr_kroos2_2'
        rules = record('PrecisionKit', MyProbability=number(num(talent.get('prob'), 0.25 if volley else 0.2)), MyScale=number(num(talent.get('atk_scale'), 1.3 if volley else 1.5)),
            MyStun=number(0 if volley else num(talent.get('stun'), 0.2)), MyVolleyExtra=boolean(volley), MyProgressive=boolean(progressive),
            MyUpgradeAfter=number(num(bb.get('attack@max_stack_count'), 40)), MyLowHealthRatio=number(num(trait.get('hp_ratio')) if volley else 0),
            MyLowHealthScale=number(num(trait.get('atk_scale'), 1) if volley else 1), MyInitialSp=number(num(t1.get('sp'), 6) if volley else 0),
            MyCamouflage=boolean(not volley and selected == 'skchr_kroos2_1'))
        mods = {}; has_attack = True
        if not volley:
            mods = dict(batPct=max(-0.9, delta/bat)) if progressive else dict(atkPct=num(bb.get('atk')))
            hits = '2U'
        elif selected == 'skcom_quickattack[3]': mods = dict(atkPct=num(bb.get('atk')), aspd=num(bb.get('attack_speed')))
        elif selected == 'skchr_bpipe_3':
            mods = dict(atkPct=num(bb.get('atk')), defPct=num(bb.get('def')), blockCnt=num(bb.get('block_cnt'), 1), batPct=max(-0.9, delta/bat)); hits = '3U'
        else:
            kind = 'CHARGES' if num(skill.get('maxChargeTime'), 1) > 1 else 'INSTANT'
            scale = number(num(bb.get('atk_scale'), 1.45)); hits = '2U'
    elif name == 'Beewax':
        rules = record('BeewaxKit', MyToken=quote(skill.get('overrideTokenKey') or next(iter(raw.get('tokens') or []), 'token_10011_beewax_oblisk')),
            MyKeepDefense=number(num(trait.get('soil_e_002[buff].def'))), MyKeepResistance=number(num(trait.get('soil_e_002[buff].magic_resistance'))),
            MyRegenRatio=number(num(talent.get('hp_recovery_per_sec_by_max_hp_ratio'), 0.04)), MyBurstScale=number(num(bb.get('atk_scale'), 2)),
            MyStun=number(num(bb.get('stun'), 1)), MyLifetime=number(num(skill.get('duration'), 20)), MySummon=boolean(default))
        if default: mods = {}
    elif name == 'Rmixer':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        mode = 'RELOAD' if selected == 'skchr_rmixer_1' else 'GUARD' if selected == 'skchr_rmixer_2' else 'COUNTER'
        module = raw.get('module') or {}
        rules = record('RmixerKit', MySkill='RmixerSkillKind::'+mode, MyReload=number(num(bb.get('charge'), 1) if mode == 'RELOAD' else num(bb.get('ammo'))),
            MyGuardCost=number(max(1, math.floor(num(bb.get('ammo_cost'), 30)))), MyCounterTargets=str(max(1, math.floor(num(bb.get('attack@max_target'), 3))))+'U',
            MyCounterRatio=number(num(bb.get('base_attack_time'), 0.6)), MyDefense=number(num(talent.get('def'))), MyAttackSpeed=number(num(talent.get('attack_speed'))),
            MyStackDuration=number(num(talent.get('duration'), 10)), MyStacks=str(max(1, math.ceil(num(talent.get('max_stack_cnt'), 3))))+'U',
            MyShieldInterval=number(num(t1.get('interval'), 8)), MyShieldRatio=number(num(t1.get('shield'), 0.15)),
            MyReveal=boolean(bool(module.get('active')) and module.get('id') == 'uniequip_002_rmixer'))
        if num(trait.get('ability_range_forward_extend')) > 0:
            permanent('module:range', dict(rangeExtend=num(trait.get('ability_range_forward_extend'), 1)))
        has_attack = True
        if mode == 'RELOAD':
            mods = {}; kind = 'CHARGES' if num(skill.get('maxChargeTime'), 1) > 1 else 'INSTANT'
            scale = number(num(bb.get('atk_scale'), 1.3)); hits = '3U'
        else:
            kind = 'AMMO'; ammo = num(bb.get('attack@trigger_time'), 42 if mode == 'GUARD' else 30)
            mods = dict(atkPct=num(bb.get('atk')), defPct=num(bb.get('def')))
            if mode == 'COUNTER': mods['hpPct'] = num(bb.get('max_hp')); grid = skill.get('rangeGrid'); no_attack = True
    elif name == 'Mostma':
        t1 = next((t.get('bb') or {} for t in raw.get('talents', []) if t.get('index') == 1), {})
        mode = 'ATTACK' if selected == 'skcom_atk_up[3]' else 'TIME_LOCK' if selected == 'skchr_mostma_2' else 'RIPPLE'
        rules = record('MostmaKit', MySkill='MostmaSkillKind::'+mode, MySpRecovery=number(num(talent.get('sp_recovery_per_sec'), 0.4)),
            MyMoveSpeed=number(num(t1.get('move_speed'), -0.15)), MySkillSlowScale=number(num(bb.get('talent_scale'), 3) if mode == 'RIPPLE' else 1),
            MyDamageScale=number(num(bb.get('atk_scale'), 1)), MyForce=number(num(bb.get('attack@force'), num(bb.get('force')))))
        mods = {} if mode == 'TIME_LOCK' else dict(atkPct=num(bb.get('atk')))
        if mode == 'RIPPLE': has_attack = True; grid = skill.get('rangeGrid')
        module = raw.get('module') or {}
        if module.get('active'):
            rec = next((m for m in raw.get('modules', []) if m.get('uniEquipId') == module.get('id')), {})
            base_grid = next((t.get('rangeGrid') for t in rec.get('talentChanges', []) if t.get('rangeGrid')), None)
    override = 'nullptr'
    if mods is not None:
        spec = record('GenericSkillRecord', MyKind='SkillKind::'+kind, MyDuration=duration, MyAmmo=number(ammo), MyActivateOnDeploy=boolean(on_deploy), MyTrigger=trigger, MyModifiers=generic_modifiers(tables, mods),
            MyRange=tables.grid(grid), MyRangeExtend=str(range_extend), MyMaxTargets=targets, MyHasAttack=boolean(has_attack), MyDamageType=damage_type, MyAttackScale=scale, MyHealScale=heal_scale, MyHits=hits, MyNoAttack=boolean(no_attack))
        override = tables.array('GenericSkillRecord', [spec])+'.data()'
    kit = record('OperatorKitRecord', MyRules=rules, MySkill=override, MyPriority=priority, MyChainNoFalloff=boolean(name == 'Leizi'), MySluggish=sluggish, MyHealing=boolean(healing),
        MyBaseSluggish=base_sluggish, MyBasePriority=base_priority, MySkillNoHeal=boolean(no_heal), MyTalents=tables.array('GenericTalentRecord', permanent_talents), MyBaseRange=tables.grid(base_grid))
    return tables.array('OperatorKitRecord', [kit])+'.data()'
