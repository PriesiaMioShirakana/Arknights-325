"""Compile item attributes and fixed battle behaviours from the pinned reference blackboards."""
from generate_combat import record, quote, number


def build_equipment_stats(items, tables, by_item=None):
    templates = {'attr_common_global_buff', 'act1autochess_equip_acarm056_global_buff',
        'act1autochess_equip_acarm050_global_buff', 'magic_penetrate_global_buff', 'act1autochess_equip_acarm054_global_buff'}
    attributes = {'atk': 'ATTACK_PERCENT', 'def': 'DEFENSE_PERCENT', 'max_hp': 'HEALTH_PERCENT',
        'attack_speed': 'ATTACK_SPEED', 'magic_resistance': 'RESISTANCE_FLAT', 'respawn_time': 'REDEPLOY_MULTIPLIER',
        'sp_recovery_per_sec': 'SP_RECOVERY_FLAT', 'magic_resist_penetrate': 'RESISTANCE_IGNORE_PERCENT', 'taunt_level': 'TAUNT'}
    rows = []
    for ident, item in sorted(items.items()):
        if item.get('itemType') != 'EQUIP': continue
        for buff in item.get('buffs', []):
            p = {**buff.get('bb', {}), **buff.get('bbStr', {})}
            key = p.get('key')
            if key not in templates: continue
            mods = []
            for name, attribute in attributes.items():
                value = p.get(name, 0)
                if not value: continue
                if name == 'max_hp': value = max(-.99, value)
                elif name == 'respawn_time': value = max(0, 1 + value)
                mods.append(record('AttributeChange', MyAttribute='Attribute::'+attribute, MyValue=number(value)))
            if mods:
                rows.append(record('EquipmentStatRule', MyItem=quote(ident), MyBuff=quote(key), MyModifiers=tables.array('AttributeChange', mods)))
                if by_item is not None: by_item.setdefault(ident, []).append(rows[-1])
    return tables.array('EquipmentStatRule', rows)


def build_equipment_effects(items, tables, by_item=None):
    # Each mapping names the upstream item's own blackboard template. Static attrs above remain independent.
    mapping = {
        '2_03': ('HAMMER_TREMBLE', 'act1autochess_equip_acarm045_global_buff', {'MyProbability': ('prob', 0), 'MyDuration': ('disarmed_duration', 2)}),
        '3_09': ('HAMMER_UNDYING', 'act1autochess_equip_acarm043_global_buff', {'MyDuration': ('undeadable_duration', 8)}),
        '3_10': ('HAMMER_SPEED', 'act1autochess_equip_acarm044_global_buff', {'MyValue': ('attack_speed', 0)}),
        '4_09': ('HAMMER_BURN', 'act1autochess_equip_acarm042_global_buff', {'MyValue': ('damage_scale', 0)}),
        '6_05': ('STEAM_HEART', 'act1autochess_equip_acarm079_global_buff', {'MyValue': ('damage_scale', 0), 'MyExtra': ('undeadable_duration', 8), 'MyProbability': ('prob', 0), 'MyDuration': ('disarmed_duration', 2), 'MyThreshold': ('attack_speed', 0)}),
        '1_05': ('SOLVENT', 'periodic_damage', {'MyValue': ('damage', 0)}),
        '3_01': ('SIDE_ATTACK_SPEED', 'act2autochess_equip_acarm121_ability', {'MyValue': ('attack_speed', 0)}),
        '3_08': ('ARCANE_SILENCE', 'silence_attachment', {'MyDuration': ('silence', 0)}),
        '3_02': ('DISTANCE_DAMAGE', 'act1autochess_equip_acarm063_global_buff', {'MyValue': ('damage_scale', 1), 'MyThreshold': ('radius', 3)}),
        '3_04': ('SKILL_ATTACK_STACK', 'act1vautochess_equip_acarm024_global_buff', {'MyValue': ('atk', 0), 'MyProbability': ('prob', 1), 'MyMaximum': ('atk_buff_cnt', 10)}),
        '3_05': ('DEPLOY_BOND_SP', 'act1autochess_equip_acarm051_global_buff', {'MyValue': ('sp_each_person', 0)}),
        '3_06': ('FRONT_HEALTH', 'act1autochess_equip_acarm049_global_buff', {'MyValue': ('ex_max_hp', 0), 'MyExtra': ('init_max_hp', 0)}),
        '3_11': ('DEPLOY_STUN', 'act1autochess_equip_acarm064_global_buff', {'MyValue': ('stun', 0), 'MyDuration': ('duration', 0)}),
        '4_03': ('ATTACK_SPEED_STACK', 'act1vautochess_equip_acarm037_global_buff', {'MyValue': ('attack_speed', 0), 'MyMaximum': ('max_buff_cnt', 60)}),
        '4_04': ('FIRST_DAMAGE_STEALTH', 'act2autochess_equip_acarm055_global_buff', {'MyDuration': ('duration', 0)}),
        '4_05': ('BLOCK_DAMAGE', 'act1autochess_equip_acarm060_global_buff', {'MyValue': ('damage_scale', 1)}),
        '4_06': ('ATTACK_SELF_HEAL', 'act1vautochess_equip_acarm025_global_buff', {'MyValue': ('hp_ratio', 0)}),
        '4_07': ('DECAY_DAMAGE', 'act2autochess_equip_acarm120_global_buff', {'MyValue': ('damage_scale', 1), 'MyExtra': ('damage_scale_minus', 0), 'MyDuration': ('interval', 15), 'MyInterval': ('ex_interval', .5)}),
        '4_08': ('AMMO_REFILL', 'act1autochess_equip_acarm057_global_buff', {'MyValue': ('ammo_percent', 0), 'MyProbability': ('prob', 0), 'MyMaximum': ('max_trigger_cnt', 3)}),
        '4_10': ('HEALTH_CONTROL_IMMUNITY', 'act1autochess_equip_acarm059_global_buff', {'MyThreshold': ('hp_ratio', 1)}),
        '4_11': ('HEAL_SHIELD', 'act1autochess_equip_acarm061_global_buff', {'MyProbability': ('prob', 0), 'MyMaximum': ('max_stack_cnt', 1)}),
        '4_12': ('REVIVE', 'act1autochess_equip_acarm068_global_buff', {'MyMaximum': ('max_respawn_cnt', 1)}),
        '5_01': ('ATTACK_PALSY', 'act1autochess_equip_acarm058_global_buff', {'MyProbability': ('prob', 0)}),
        '5_02': ('ATTACK_COLD', 'act1vautochess_equip_acarm003_global_buff', {'MyProbability': ('prob', 0), 'MyDuration': ('cold', 0)}),
        '5_03': ('WEAKNESS', '', {}),
        '5_09': ('PARTNER_HEAL', 'act2autochess_equip_acarm118_global_buff', {'MyValue': ('hp_recovery_per_sec_by_max_hp_ratio', 0)}),
        '6_01': ('PARTNER_TRUE_DAMAGE', 'act2autochess_equip_acarm117_global_buff', {'MyValue': ('atk_scale', 0)}),
        '6_02': ('EXTRA_BULLET', 'act1autochess_equip_acarm076_global_buff', {'MyValue': ('atk_scale_1', 0), 'MyExtra': ('atk_scale_2', 0), 'MyProbability': ('prob', 0)}),
        '6_03': ('GAINED_ATTACK_SPEED', 'act1autochess_equip_acarm077_global_buff', {'MyValue': ('attack_speed', 0), 'MyMaximum': ('max_cnt', 3)}),
        '6_06': ('COLD_DAMAGE', 'act2autochess_equip_acarm102_ability', {'MyValue': ('atk_scale', 0), 'MyExtra': ('atk_scale_ex', 0), 'MyInterval': ('interval', 1)}),
        '6_07': ('SKILL_COMPASS', 'act1autochess_equip_acarm103_global_buff', {'MyValue': ('init_sp', 0), 'MyExtra': ('sp', 0), 'MyThreshold': ('addition_sp', 0)}),
        '6_10': ('KNIGHT_CREED', 'act2autochess_equip_acarm119_global_buff', {'MyValue': ('atk', 0), 'MyDuration': ('duration', 20)}),
        '6_11': ('STEALTH_CHARGE', 'act2autochess_equip_acarm122_global_buff', {'MyValue': ('atk_per_sec', 0), 'MyExtra': ('max_atk', 1), 'MyThreshold': ('atk_scale', 0)}),
        '6_04': ('TRENCH_COUNTER', 'act2autochess_equip_acarm078_global_buff', {'MyValue': ('atk_scale', 0), 'MyInterval': ('lock_duration', .5)}),
    }
    rows = []
    for ident, item in sorted(items.items()):
        bare = ident.removesuffix('_a').removesuffix('_b')
        entry = mapping.get(bare.removeprefix('chess_item_').removesuffix('_e'))
        if not entry or item.get('itemType') != 'EQUIP': continue
        start = len(rows)
        kind, key, fields = entry
        params = next(({**b.get('bb', {}), **b.get('bbStr', {})} for b in item.get('buffs', []) if b.get('bbStr', {}).get('key') == key), None)
        if key and params is None: raise ValueError('missing equipment params: '+ident)
        if kind == 'TRENCH_COUNTER':
            flat = next((b['bb']['value'] for b in item['buffs'] if b.get('bbStr', {}).get('key') == 'halfidle_block_fixed_damage'), 0)
            rows.append(record('EquipmentEffectRule', MyItem=quote(ident), MyParameters=record('EquipmentParameters',
                MyKind='EquipmentEffectKind::FLAT_DAMAGE_REDUCTION', MyValue=number(flat))))
        values = {'MyKind': 'EquipmentEffectKind::'+kind}
        if kind == 'KNIGHT_CREED':
            aura = next((b['bb'] for b in item['buffs'] if b.get('bbStr', {}).get('key') == 'act2autochess_equip_acarm119_ability'), {})
            values['MyExtra'] = number(aura.get('attack_speed', 1))
            values['MyThreshold'] = number(aura.get('move_speed', 1))
        for field, (name, fallback) in fields.items():
            value = (params or {}).get(name, fallback)
            if field == 'MyMaximum':
                value = max(1 if kind in ['SKILL_ATTACK_STACK', 'ATTACK_SPEED_STACK', 'HEAL_SHIELD'] else 0, int(value))
                values[field] = str(value)
            else:
                values[field] = number(max(.1, value) if field == 'MyInterval' else value)
        order = ['MyKind', 'MyValue', 'MyExtra', 'MyProbability', 'MyDuration', 'MyInterval', 'MyThreshold', 'MyMaximum']
        rows.append(record('EquipmentEffectRule', MyItem=quote(ident), MyParameters=record('EquipmentParameters', **{k:values[k] for k in order if k in values}),
            MyPartner=quote((params or {}).get('equip_chess_id', ''))))
        if by_item is not None: by_item[ident] = rows[start:]
    return tables.array('EquipmentEffectRule', rows)


def build_equipment_templates(items, tables, stats, effects):
    rows = []
    for ident in sorted(stats.keys() | effects.keys()):
        rows.append(record('EquipmentTemplate', MyId=quote(ident), MyTier=str(items[ident]['tier']),
            MyStats=tables.array('EquipmentStatRule', stats.get(ident, [])),
            MyEffects=tables.array('EquipmentEffectRule', effects.get(ident, []))))
    return tables.array('EquipmentTemplate', rows)
