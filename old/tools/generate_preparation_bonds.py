"""Compile preparation-side bond effects in the original dispatch order."""
from generate_combat import number, quote, record


def build_preparation_bonds(bonds, tables):
    keys = {
        'bond_activated_add_layer': 'PREP_LAYERS',
        'bond_layer_gain_coin': 'COIN_MILESTONE',
        'bond_multi_layer_char_goods_price_bond_discount': 'DISCOUNT',
        'bond_refresh_shop_next_free': 'REFRESH_CHANCE',
        'bond_layer_added_reward_equip': 'ITEM_MILESTONE',
    }
    rows = []
    for ident, bond in sorted(bonds.items(), key=lambda pair: (pair[1].get('identifier', 99), pair[0])):
        for buff in bond.get('buffs', []):
            if buff['key'] not in keys: continue
            kind = keys[buff['key']]
            p = {**buff.get('bb', {}), **buff.get('bbStr', {})}
            step, count, high_count, high_step = int(p.get('layer', 0)), int(p.get('count', 0)), 0, 0
            if kind == 'PREP_LAYERS': count, high_count = int(p.get('layer', 0)), int(p.get('more_layer', p.get('layer', 0)))
            if kind == 'DISCOUNT': step, high_step, count = int(p.get('layer1', 0)), int(p.get('layer2', 0)), int(p.get('discount', 0))
            rows.append(record('PreparationBondEffect', MyBond=quote(ident), MyKind='PreparationBondKind::' + kind,
                MyStep=str(step), MyCount=str(count), MyHighCount=str(high_count), MyHighStep=str(high_step),
                MyBaseChance=number(p.get('baseprob', 0)), MyChancePerLayer=number(p.get('prob', 0)),
                MyPool=quote(p.get('pool', '')), MyDiscountBond=quote(p.get('bond', ident))))
    return tables.array('PreparationBondEffect', rows)
