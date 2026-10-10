"""Compile the preparation-only strategy handlers; retain source grouping/order."""
from generate_combat import record, quote, boolean


def build_preparation_bands(bands, tables):
    keys = dict(zip([
        'prep_finish_char_bond_add_layer', 'band_first_self_refresh_present_char',
        'band_shop_refresh_copy_max_lv_char', 'preparation_start_gain_chess_from_round',
        'preparation_start_gain_chess_every_n_round', 'give_coin_in_round',
        'prep_start_gain_chess_from_pool_in_round', 'band_coin_cost_gain_random_char_by_shop_level',
        'up_shop_next_refresh_must_present_bond_char', 'gain_bond_char_per_round',
        'first_buy_in_round_char_price_change', 'band_cost_coin_reach_cnt_gain_chess_from_pool',
        'round_start_bond_check_gain_layer', 'up_shop_add_special_goods', 'coin_carry_over',
        'first_sell_char_chess_exchange_char_chess_in_shop', 'round_start_gain_char_chess_in_shop_every_n_round',
        'round_start_gain_coin_by_bond_char_chess_buy', 'refresh_shop_count_gain_coin_bond_char_chess',
        'preparation_start_add_special_goods_every_n_round'], [
        'TIER_LAYERS', 'REFRESH_BOND', 'COPY_SHOP', 'ROUND_GIFT', 'PERIODIC_GIFT', 'INCOME',
        'ROUND_POOL', 'SPEND_CHESS', 'SPECIAL_REFRESH', 'PERIODIC_BOND', 'FIRST_DISCOUNT',
        'SPEND_POOL', 'ACTIVE_LAYERS', 'LEVEL_OFFER', 'INTEREST', 'SELL_EXCHANGE',
        'SHOP_GIFT', 'BUY_PENDING', 'REFRESH_GIFT', 'PERIODIC_OFFER']))
    keys['round_start_activate_char_chess_effect_in_board'] = 'TRIGGER_GAIN'
    other = {'env_gbuff_new_with_verify', 'auto_chess_change_map',
             'round_start_all_player_change_enemy_2', 'round_start_activate_char_chess_effect_in_board'}
    rows = []
    for ident, band in sorted(bands.items()):
        groups = {}
        for buff in band.get('buffs', []):
            key = buff['key']
            if key not in keys:
                if key not in other:
                    raise ValueError('unclassified strategy effect: ' + key)
                continue
            groups.setdefault(keys[key], []).append({**buff.get('bb', {}), **buff.get('bbStr', {})})
        effects = []
        for kind, params in groups.items():
            if kind not in ('ROUND_GIFT', 'PERIODIC_GIFT', 'INCOME', 'ROUND_POOL'):
                params = params[:1]
            for p in params:
                fields = dict(MyKind='PreparationBandKind::' + kind)
                mappings = dict(MyCount=('count', 1), MyRound=('round', 0), MyPeriod=('preround', 1))
                if kind == 'TIER_LAYERS': mappings['MyCount'] = ('layer', 0)
                if kind == 'INCOME': mappings['MyCount'] = ('coin', 0)
                if kind in ('SPEND_CHESS', 'SPEND_POOL'): mappings['MyThreshold'] = ('coin_cnt', 0)
                if kind == 'FIRST_DISCOUNT': mappings['MyCount'] = ('price', 1)
                if kind == 'ACTIVE_LAYERS':
                    mappings.update(MyCount=('count1', 0), MyAlternate=('count2', 0), MyThreshold=('factioncount', 1))
                if kind == 'LEVEL_OFFER': mappings['MyCount'] = ('count', 3)
                if kind == 'INTEREST':
                    mappings.update(MyCount=('interest', 0), MyThreshold=('capital', 0), MyMaximum=('max', 2**63-1))
                if kind in ('BUY_PENDING', 'REFRESH_GIFT'): mappings['MyMaximum'] = ('max_count', 2**63-1)
                if kind == 'REFRESH_GIFT': mappings['MyThreshold'] = ('refresh_count', 0)
                if kind == 'PERIODIC_OFFER': mappings.update(MyCount=('refresh_cnt', 2), MyAlternate=('choice_cnt', 1))
                for field, (key, default) in mappings.items():
                    value = int(p.get(key, default))
                    if value < 0 or value > 2**63-1: raise ValueError('invalid strategy parameter')
                    fields[field] = str(value)
                fields.update(MyChess=quote(p.get('chess', p.get('chess_id', ''))), MyBond=quote(p.get('bond', '')),
                              MyPool=quote(p.get('pool', '')),
                              MyLevels=tables.array('int', [str(int(x.strip())) for x in p.get('lvlist', '').split(',') if x.strip()]))
                # C++ designated initializers must follow declaration order.
                order = ('MyKind', 'MyCount', 'MyMaximum', 'MyThreshold', 'MyAlternate', 'MyRound', 'MyPeriod',
                         'MyChess', 'MyBond', 'MyPool', 'MyLevels')
                effects.append(record('PreparationBandEffect', **{k: fields[k] for k in order if k in fields}))
        rows.append(record('PreparationBandRule', MyId=quote(ident),
                           MyEffects=tables.array('PreparationBandEffect', effects), MyKeepFunds=boolean(ident == 'band_cannot')))
    return tables.array('PreparationBandRule', rows)
