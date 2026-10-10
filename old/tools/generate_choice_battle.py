"""Compile choices.js battle plans to immutable C++ tables; runtime input owns selected plans."""
import math
from generate_combat import record, quote, number


def build_choice_battle(raw, tables):
    def num(value, default=0):
        try:
            value = float(value)
            return value if math.isfinite(value) else default
        except (TypeError, ValueError):
            return default

    def merge(target, source):
        for key, value in source.items():
            target[key] = target.get(key, 1) * value if key.endswith('MULTIPLIER') else target.get(key, 0) + value

    def op_mods(p):
        mods = {}
        names = {'magic_resist_penetrate_fixed': 'RESISTANCE_IGNORE_FLAT', 'magic_resist_penetrate': 'RESISTANCE_IGNORE_PERCENT',
            'def_penetrate': 'DEFENSE_IGNORE_PERCENT', 'def_penetrate_fixed': 'DEFENSE_IGNORE_FLAT',
            'atk': 'ATTACK_PERCENT', 'def': 'DEFENSE_PERCENT', 'max_hp': 'HEALTH_PERCENT', 'attack_speed': 'ATTACK_SPEED', 'damage_scale': 'DAMAGE_DEALT_MULTIPLIER'}
        for key, raw in p.items():
            v = num(raw)
            if key not in names or v == 0:
                continue
            if key == 'attack_speed' and abs(v) < 1:
                v *= 100
            merge(mods, {names[key]: 1 + v if key == 'damage_scale' else v})
        return mods

    def enemy_mods(p, mul):
        names = {'atk': 'ATTACK', 'def': 'DEFENSE', 'max_hp': 'HEALTH', 'magic_resistance': 'RESISTANCE', 'move_speed': 'MOVE'}
        mods = {}
        for key, raw in p.items():
            v = num(raw, math.nan)
            if not math.isfinite(v):
                continue
            if key == 'attack_speed':
                v = (v - 1) * 100 if mul else v
                if v != 0:
                    merge(mods, {'ATTACK_SPEED': v})
            elif key in names and ((v > 0 and v != 1) if mul else v != 0):
                merge(mods, {names[key] + ('_MULTIPLIER' if mul else '_FLAT'): v})
        return mods

    def changes(mods):
        return tables.array('AttributeChange', [record('AttributeChange', MyAttribute='Attribute::'+k, MyValue=number(v)) for k, v in mods.items()])

    gates = {'global_special_choice_prep_finish_bench_at_least': 'BENCH_AT_LEAST',
        'global_special_choice_prep_finish_bench_at_most': 'BENCH_AT_MOST', 'global_special_choice_prep_finish_same_row_at_least': 'SAME_ROW'}
    card_ids = {c['effectId'] for family in ['bounty', 'tactic'] for c in raw['choices']['cards'][family]}
    rows = []
    for key, effect in sorted(raw['effects'].items()):
        if effect.get('effectType') not in ['ENEMY_GAIN', 'BUFF_GAIN'] and key not in card_ids:
            continue
        mods, flawless, enemies, heal = {}, None, [], 0
        gate_kind, gate_count = 'ALWAYS', 0
        for buff in filter(None, effect.get('buffs', [])):
            name = buff.get('key', '')
            p = {**(buff.get('bb') or {}), **(buff.get('bbStr') or {})}
            if name in gates and gate_kind == 'ALWAYS':
                gate_kind = gates[name]
                gate_count = max(1 if gate_kind == 'SAME_ROW' else 0, math.trunc(num(p.get('count'), 3 if gate_kind == 'SAME_ROW' else 0)))
            if name in ['env_gbuff_new', 'env_gbuff_new_with_verify']:
                if p.get('key') == 'act1autochess_debuff_3':
                    heal += max(0, num(p.get('value')))
                elif p.get('key') == 'act1autochess_debuff_9':
                    flawless = op_mods({'atk': p.get('atk'), 'def': p.get('def')})
                else:
                    merge(mods, op_mods(p))
            elif name == 'char_respawntime_mul':
                value = num(p.get('scale'), math.nan)
                if math.isfinite(value) and value >= 0:
                    merge(mods, {'REDEPLOY_MULTIPLIER': value})
            elif name in ['enemy_attribute_mul', 'enemy_attribute_add']:
                enemy = enemy_mods(p, name.endswith('_mul'))
                rank = (p.get('enemy_level_type') or 'ANY').upper()
                if rank not in ['ANY', 'NORMAL', 'ELITE', 'BOSS']:
                    raise ValueError('unknown enemy rank: ' + rank)
                if enemy:
                    enemies.append(record('ChoiceEnemyRule', MyRank='ChoiceEnemyRank::'+rank, MyModifiers=changes(enemy)))
        if heal > 0 or flawless is not None or mods or enemies:
            rows.append(record('ChoiceBattleRule', MyId=quote(key), MyGate=record('ChoiceGate', MyKind='ChoiceGateKind::'+gate_kind, MyCount=str(gate_count)),
                MySelfHeal=number(heal), MyOperatorModifiers=changes(mods), MyFullHealthModifiers=changes(flawless or {}), MyEnemies=tables.array('ChoiceEnemyRule', enemies)))
    return tables.array('ChoiceBattleRule', rows)
