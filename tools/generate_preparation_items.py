"""Compile builtinMeta.js and items/meta.js item parameters to immutable, ordered C++ records."""
import math
from generate_combat import record, quote, boolean


def build_preparation_items(raw, tables):
    # The concrete normal/golden record selects its own handler. Doctor projection is deliberately two kinds.
    simple = {
        'equip_destory_gain_random_coin': ('COINS', None, 0),
        'use_equip_reward_char_chess_bond_layer': ('LAYERS', 'layer', 3),
        'use_equip_reward_random_char_chess_in_shop': ('SHOP_CHESS', 'count', 1),
        'gain_coin_when_round_start': ('ROUND_COINS', 'count', 1),
        'use_equip_reward_char_chess_with_same_bond': ('SAME_BOND_CHESS', 'count', 1),
        'use_equip_gain_coin_when_next_round_start': ('NEXT_ROUND_COINS', 'count', 2),
        'equip_destory_deployment_cnt_change': ('DEPLOY_CAP', 'count', 9),
        'use_equip_upgrade_char': ('PROMOTE', None, 0),
        'equip_round_start_upgrade_char': ('PROMOTE_NEXT_ROUND', None, 0),
        'use_equip_reward_char_chess': ('MIMIC', None, 0),
        'use_equip_reward_special_goods_char_chess': ('OFFER_SAME_BOND', 'refresh_cnt', 3),
        'use_equip_recruit_new_char_and_give_char_to_player_most_bond': ('BEACON', 'refresh_cnt', 2),
        'sell_char_count_gain_equip_owner_bond': ('SELL_BONUS', 'count', 8),
        'char_chess_transformation_equip': ('TRANSFORM', None, 0),
        'trap_copy_front_char': ('COPY_ART', None, 0),
        'trap_disney_special': ('DESTROY_PASS', 'count', 1),
        'equip_with_another_gain_coin_when_gain_char': ('CAULDRON', 'count', 2),
    }
    from_buff = {'SHOP_CHESS', 'OFFER_SAME_BOND', 'DESTROY_PASS', 'CAULDRON'}
    at_least_one = {'SHOP_CHESS', 'SAME_BOND_CHESS', 'OFFER_SAME_BOND', 'BEACON', 'SELL_BONUS'}

    def integer(value, default=0):
        return math.trunc(value) if isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value) else default

    rows = []
    for key, item in sorted(raw.items()):
        effects = []
        for buff in filter(None, item.get('buffs', [])):
            name = buff.get('key')
            if name == 'trap_create_self_choice':
                if key not in ['chess_item_6_01_m', 'chess_item_6_03_m']:
                    raise ValueError('unrecognized personal bounty item: ' + key)
                effects.append(record('PreparationItemEffect', MyKind='PreparationItemKind::'+('BAND_BOUNTY' if key == 'chess_item_6_01_m' else 'TRAINING_BOUNTY')))
                continue
            if name not in simple:
                continue
            kind, field, default = simple[name]
            params = {**(buff.get('bb') or {}), **(buff.get('bbStr') or {})} if kind in from_buff else item.get('params') or {}
            count = integer(params.get(field), default) if field else 0
            if kind in at_least_one:
                count = max(1, count)
            minimum = integer(params.get('min'), 1) if kind == 'COINS' else 0
            maximum = max(minimum, integer(params.get('max'), minimum)) if kind == 'COINS' else max(0, integer(params.get('max'), 3)) if kind == 'CAULDRON' else 0
            bond = (params.get('bond') or 'yanShip') if kind == 'CAULDRON' else ''
            others = [v.strip().removesuffix('_a').removesuffix('_b') for v in str(params.get('other_equip') or '').split(',') if v.strip()] if kind == 'CAULDRON' else []
            effects.append(record('PreparationItemEffect', MyKind='PreparationItemKind::'+kind, MyCount=str(count), MyMinimum=str(minimum), MyMaximum=str(maximum),
                MyBond=quote(bond), MyOtherItems=tables.array('std::string_view', [quote(v) for v in others])))
        use = 'ART' if item.get('itemType') == 'MAGIC' else 'CONSUME_ON_EQUIP' if str(item.get('kind', '')).startswith('consume_on_equip') else 'EQUIPMENT'
        grid = item.get('rangeGrid') or [[0, 0]]
        rows.append(record('PreparationItemRule', MyId=quote(key), MyUse='ItemUse::'+use, MyEffects=tables.array('PreparationItemEffect', effects),
            MyRange=tables.array('RangeOffset', [record('RangeOffset', MyRow=str(r), MyColumn=str(c)) for r, c in grid]),
            MyBond=record('BondItem', MyCanGiveBond=boolean(item.get('canGiveBond', False)), MyGrantedBond=quote(item.get('giveBondId') or ''))))
    return tables.array('PreparationItemRule', rows)
