"""Generate immutable bond membership and tier metadata; behavior lives in domain/bonds.cpp."""
import argparse
import hashlib
import json
from pathlib import Path
from generate_combat import Tables, number, quote, record, boolean
from generate_match import finite
from generate_battle_bonds import core_bond_effects


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--data', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path)
    args = parser.parse_args()
    raw = {name: (args.data / (name + '.json')).read_bytes() for name in ['bonds', 'chess', 'items']}
    bonds, chess, items = (json.loads(raw[name]) for name in raw)
    tables = Tables()
    strings = lambda values: tables.array('std::string_view', [quote(value) for value in values])
    rules = []
    for identity, bond in sorted(bonds.items(), key=lambda item: (item[1].get('identifier', 99), item[0])):
        values = bond.get('thresholds')
        thresholds = sorted(value for value in values if finite(value)) if isinstance(values, list) and values else [bond.get('activeCount') if finite(bond.get('activeCount')) else 2]
        mode = 'BOARD_ALL_ELITES' if bond.get('countMode') == 'BOARD_ALL_CHESS' or bond.get('thresholdTemplate') == 'count_threshold_upward_golden' else 'BOARD_AND_HAND' if bond.get('countMode') == 'BOARD_AND_DECK' else 'BOARD'
        rules.append(record('BondRule', MyId=quote(identity), MyCountMode='BondCountMode::' + mode,
            MyThresholds=tables.array('double', [number(value) for value in thresholds]), MyCore=boolean(bond.get('isCore', False)),
            MyDownward=boolean(bond.get('thresholdTemplate') == 'count_threshold_downward'),
            MyMaximum=number(bond['maxCount']) if finite(bond.get('maxCount')) else '{}',
            MyCanDisable=boolean(finite(bond.get('weight')) and bond['weight'] > 0)))
    roster = [record('BondRosterRecord', MyId=quote(identity), MyBaseId=quote(value.get('baseId') or identity),
        MyGolden=boolean(value.get('isGolden', False)), MyBonds=strings(value.get('bonds') or [])) for identity, value in sorted(chess.items())]
    equipment = [record('BondItemRecord', MyId=quote(identity), MyItem=record('BondItem', MyCanGiveBond=boolean(value.get('canGiveBond', False)),
        MyGrantedBond=quote(value.get('giveBondId') or ''))) for identity, value in sorted(items.items())]
    addon = []
    kinds = ['PRECISE', 'SWIFT', 'SKILLFUL', 'ARCANE', 'STEADFAST', 'DEPUTY', 'RAID', 'INDOMITABLE', 'ASSIST', 'SOLO', 'ELITE']
    identities = ['preciShip', 'swiftShip', 'skillfulShip', 'arcaneShip', 'steadShip', 'deputShip', 'raidShip', 'indomShip', 'emptyShip', 'soloShip', 'suntShip']
    fields = [('MyAttack', 'base_atk'), ('MyAttackPerLayer', 'atk_per_stack'), ('MyHealth', 'base_max_hp'),
        ('MyHealthPerLayer', 'max_hp_per_stack'), ('MyDefense', 'base_def'), ('MyDefensePerLayer', 'def_per_stack'),
        ('MyAttackSpeed', 'base_attack_speed'), ('MyAttackSpeedPerLayer', 'attack_speed_per_stack'),
        ('MyDefenseIgnore', 'power_def_penetrate'), ('MyResistanceIgnore', 'power_magic_resist_penetrate'),
        ('MyProbability', 'base_prob'), ('MyProbabilityPerLayer', 'prob_per_stack'), ('MyMilestone', 'power_bond_stack_cnt'),
        ('MySp', 'normal_sp'), ('MyExtraSp', 'power_sp'), ('MyDamageMultiplier', 'base_damage_scale'),
        ('MyDamagePerLayer', 'damage_scale_per_stack'), ('MyLowHealthMultiplier', 'power_weak_scale'),
        ('MyHealthThreshold', 'hp_ratio'), ('MyDuration', 'weak_duration'), ('MyResistance', 'damage_resistance'),
        ('MyCooldown', 'cd_duration'), ('MyThornDamage', 'base_damage_value'), ('MyThornPerLayer', 'damage_value_per_stack'),
        ('MyFragile', 'damage_scale'), ('MyRedeployDelta', 'respawn_time'), ('MyMilestoneAttackSpeed', 'power_attack_speed'),
        ('MyIdleTime', 'no_attack_duration'), ('MyEliteDamageMultiplier', 'damage_scale_extra'), ('MySpCostMultiplier', 'sp_ratio')]
    for identity, kind in zip(identities, kinds):
        bb = next((buff.get('bb', {}) for buff in bonds[identity].get('buffs', []) if buff.get('key') == 'env_gbuff_new'), {})
        keys = dict(fields)
        if kind == 'SOLO': keys.update(MyAttack='atk', MyHealth='max_hp', MySp='sp')
        if kind == 'ELITE': keys['MyAttack'] = 'power_atk'
        if kind == 'INDOMITABLE': keys['MySp'] = 'sp'
        if kind == 'ASSIST': keys['MyDamageMultiplier'] = 'damage_scale_normal'
        if kind == 'STEADFAST': keys['MyDuration'] = 'weak[limit]'
        parameters = {field: number(bb[keys[field]]) for field, _ in fields if keys[field] in bb}
        addon.append(record('AddonBondEffect', MyKind='AddonBondKind::' + kind, MyParameters=record('AddonBondParameters', **parameters)))
    spans = [(name, typ, tables.array(typ, values)) for name, typ, values in [
        ('ReferenceCoreBondEffects', 'CoreBondEffect', core_bond_effects(bonds)), ('ReferenceAddonBondEffects', 'AddonBondEffect', addon), ('ReferenceBondRules', 'BondRule', rules), ('ReferenceBondRoster', 'BondRosterRecord', roster), ('ReferenceBondItems', 'BondItemRecord', equipment)]]
    lines = ['// Generated by generate_bonds.py; edit the generator, not this static data.',
        *[f'// {name}.json SHA-256 {hashlib.sha256(value).hexdigest()}' for name, value in raw.items()],
        '#include <array>', '#include <stronghold/adapters/reference_bonds.hpp>', 'namespace Stronghold { namespace {', *tables.lines, '}']
    lines.extend('std::span<const ' + typ + '> ' + name + '() noexcept { return ' + span + '; }' for name, typ, span in spans)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text('\n'.join([*lines, '}']) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
