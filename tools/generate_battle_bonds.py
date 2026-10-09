"""Compile implemented core battle blackboards, including Narant's alternate Sargon effect."""
from generate_combat import record, number, boolean
import math
import re


def prd_constant(probability):
    if not probability > 0: return 0.0
    if probability >= 1: return 1.0

    def rate(step):
        expectation, remaining = 0.0, 1.0
        for attempt in range(1, math.ceil(1 / step) + 2):
            if not remaining > 0: break
            chance = min(1.0, attempt * step)
            expectation += attempt * remaining * chance
            remaining *= 1 - chance
        return 1 / expectation if expectation > 0 else 1.0

    low, high = 0.0, probability
    for _ in range(50):
        mid = (low + high) / 2
        if rate(mid) < probability: low = mid
        else: high = mid
    return (low + high) / 2


def core_bond_effects(bonds):
    kinds = [('yanShip', 'YAN'), ('sargonShip', 'SARGON'), ('victoriaShip', 'VICTORIA'), ('kjeragShip', 'KJERAG'),
        ('lateranoShip', 'LATERANO'), ('egirShip', 'EGIR'), ('siracusaShip', 'SIRACUSA'), ('kazimierzShip', 'KAZIMIERZ')]
    fields = [('MyPowerCount', 'power_bond_char_cnt'), ('MyAttack', 'base_atk'), ('MyGoldenAttack', 'atk_golden_equip'),
        ('MyAttackMaximum', 'max_atk_for_consume'), ('MyAttackMaximumPerLayer', 'max_atk_when_born_per_stack'),
        ('MyDuration', 'base_time'), ('MyDurationPerLayer', 'time_per_stack'), ('MyAttackSpeed', 'base_attack_speed'),
        ('MyMaxStacks', 'max_buff_stack_cnt'), ('MyDamageMultiplier', 'base_damage_scale'), ('MyDamagePerLayer', 'damage_scale_per_stack'),
        ('MyColdMultiplier', 'base_ex_damage_scale'), ('MyColdPerLayer', 'ex_damage_scale_per_stack'),
        ('MyInterval', 'damage_interval'), ('MyAmmoPercent', 'base_ammo_percent'), ('MyAmmoPerLayer', 'ammo_percent_per_stack'),
        ('MyRadius', 'range_radius'), ('MyDamageAttackScale', 'damage_atk_scale'), ('MyUnblockedAttackScale', 'pure_atk_scale'),
        ('MyStun', 'stun'), ('MyLendDuration', 'base_power_time'), ('MyLendDurationPerLayer', 'power_time_per_stack'), ('MyLendMaximumTier', 'filter_item_level'), ('MyHealth', 'base_max_hp'), ('MyHealthPerLayer', 'max_hp_per_stack'), ('MyDevourDamage', 'damage_value'), ('MyReviveMaximum', 'max_free_respawn_cnt'),
        ('MyAttackSpeedPerLayer', 'attack_speed_per_stack'), ('MyBaseDamage', 'base_damage'), ('MyStealthTail', 'end_duration'),
        ('MyPrdStep', 'prob'), ('MyFear', 'fear'), ('MyAttackPerLayer', 'atk_per_stack'), ('MyExCount', 'ex_bond_char_cnt'),
        ('MySummonAttackMultiplier', 'atk'), ('MySummonDamageResistance', 'damage_resistance')]
    output = []
    for identity, kind in kinds:
        for buff in bonds[identity].get('buffs', []):
            if buff.get('key') != 'env_gbuff_new':
                continue
            bb = buff.get('bb', {})
            keys = dict(fields)
            if kind == 'VICTORIA': keys['MyAttack'] = 'atk_normal_equip'
            if kind == 'KJERAG':
                keys.update(MyInterval='bond_eff_kjerag[storm].interval', MyDuration='bond_eff_kjerag[storm].base_time',
                    MyDurationPerLayer='bond_eff_kjerag[storm].time_per_stack')
            if kind == 'LATERANO': keys['MyAttack'] = 'atk_per_consume'
            if kind == 'SIRACUSA': keys.update(MyDuration='base_duration', MyDurationPerLayer='duration_per_stack', MyDamagePerLayer='damage_per_stack')
            if kind == 'KAZIMIERZ': keys.update(MyAttack='atk_when_born', MyAttackMaximum='base_max_atk_when_born')
            integers = {'MyPowerCount', 'MyMaxStacks', 'MyLendMaximumTier', 'MyReviveMaximum', 'MyExCount'}
            values = {field: str(int(bb[keys[field]])) if field in integers else number(prd_constant(bb[keys[field]]) if field == 'MyPrdStep' else bb[keys[field]]) for field, _ in fields if keys[field] in bb}
            if kind == 'YAN':
                share = re.search(r'总和的\s*(\d+(?:\.\d+)?)\s*%', bonds[identity].get('effectDesc', bonds[identity].get('desc', '')))
                ratio = float(share[1]) / 100 if share else 0.3
                values['MySummonShare'] = number(ratio if 0 < ratio <= 10 else 0.3)
            output.append(record('CoreBondEffect', MyKind='CoreBondKind::' + kind,
                MyParameters=record('CoreBondParameters', **values), MyShareEquipment=boolean(buff.get('bbStr', {}).get('valid_in_band') == 'band_narant')))
    return output
