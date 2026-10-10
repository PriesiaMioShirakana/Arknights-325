"""Compile the 25 SERVER_* trait handlers and discard battle-only traits explicitly."""
import re
from generate_combat import record, quote, number, boolean


def build_preparation_garrisons(raw, tables):
    events = {'SERVER_GAIN': 'GAIN', 'SERVER_PREP_START': 'ROUND_START', 'SERVER_PREP_FIN': 'PREP_END',
              'SERVER_CHESS_SOLD': 'SOLD', 'SERVER_PRICE': 'PRICE', 'SERVER_REFRESH_SHOP': 'REFRESH'}
    kinds = {
        'ADD_BOND', 'ADD_BOND_CHESS_ALL', 'ADD_BOND_METHOD', 'ADD_MULTIPLE_BOND',
        'ADD_BOND_ACTIVATED_MOST_LAYER', 'ADD_ACT_BOND_DIFF_LV_MOST_LAYER', 'ADD_BOND_IN_HAND',
        'ADD_BOND_POSITION', 'ADD_BOND_ROUND_COIN_COST', 'ADD_REFRESH_CNT_MULTIPLIER_BOND_LAYER',
        'GAIN_BOND_LAYER_BY_REFRESH_CNT', 'CHESS_PRICE', 'GAIN_EQUIP', 'GAIN_FREE_REFRESH_COUNT',
        'GAIN_RANDOM_EQUIP_CHESS_IN_POOL', 'POOL_EQUIP', 'POOL_CHAR', 'MOST_BOND', 'ONCE_GOLD',
        'ONCE_GOLD_WITH_BOND_CONDITION', 'SELL_CHESS_GAIN_SPECIAL_GOODS', 'TRIGGER_ANOTHER',
        'TRIGGER_FRONT_COUNT', 'FRONT_SAME_EFFECT_PREP_FIN', 'FRONT_SAME_EFFECT_PREP_START'}
    methods = {'': 'NONE', 'shoplv': 'SHOP_LEVEL', 'round_gain_char': 'ROUND_GAINED',
               'hand_count': 'HAND_COUNT', 'same_row': 'SAME_ROW', 'same_bond_diff_lv': 'BOND_TIERS'}
    split = lambda value: [s.strip() for s in str(value or '').split(',') if s.strip()]
    rows = []
    for ident, value in sorted(raw['garrisons'].items()):
        if value['eventType'] == 'IN_BATTLE': continue
        kind = value['effectKey'].removeprefix('SERVER_')
        if kind not in kinds or value['eventType'] not in events:
            raise ValueError('unknown preparation garrison: ' + ident)
        p, strings = value.get('bb', {}), value.get('bbStr', {})
        bonds = split(strings.get('bond'))
        # 录武官 lists the same 奇迹 award twice; the multiple-bond trait owns that grant.
        if kind == 'ADD_BOND':
            covered = set()
            for owner in value.get('owners', []):
                for gid in raw['chess'].get(owner, {}).get('garrisonIds', []):
                    other = raw['garrisons'].get(gid, {})
                    if other.get('effectType') == 'SERVER_ADD_MULTIPLE_BOND' and other.get('eventType') == value['eventType'] and other.get('desc') == value.get('desc'):
                        covered.update(split(other.get('bbStr', {}).get('bond')))
            bonds = [b for b in bonds if b not in covered]
        pool = raw['choices'].get('pools', {}).get(strings.get('pool'), {})
        golden = []
        if ident.endswith('_b') and len(pool.get('weighted', [])) == len(pool.get('goldenWeights', [])):
            golden = [record('ContentPoolWeight', MyId=quote(pair[0]), MyWeight=number(max(0, weight)))
                      for pair, weight in zip(pool.get('weighted', []), pool.get('goldenWeights', []))
                      if weight > 0 and pair[0] in raw['items']]
        desc = value.get('desc', '')
        rows.append(record('PreparationGarrisonRule', MyId=quote(ident), MyEvent='GarrisonEvent::' + events[value['eventType']],
            MyKind='GarrisonKind::' + kind, MyMethod='GarrisonMethod::' + methods[strings.get('add_method', '')],
            MyCount=number(p.get('count', 1)), MyMultiplier=number(p.get('multi', p.get('multiplier', 1))),
            MyLayer=number(p.get('layer', 0)), MyMaximum=number(p.get('max_layer', 0)),
            MyCheckCount=number(p.get('check_count', 0)), MyPrice=number(p.get('price', 0)),
            MyRefreshCount=number(p.get('refresh_cnt', 1)), MyBonds=tables.array('std::string_view', [quote(b) for b in bonds]),
            MyBondCounts=tables.array('double', [number(float(n)) for n in split(strings.get('count'))]),
            MyChess=quote(strings.get('chess', '')), MyPool=quote(strings.get('pool', '')),
            MyLevelPools='std::array<std::string_view, 6>{' + ','.join(quote(strings.get('pool'+str(i), strings.get('max_pool', ''))) for i in range(1, 7)) + '}',
            MyRounds=tables.array('int', [str(int(n)) for n in split(strings.get('round_list'))]),
            MyGoldenWeights=tables.array('ContentPoolWeight', golden), MyTrigger='GarrisonEvent::' + events[strings.get('event', 'SERVER_GAIN')],
            MyRequireActive=boolean('无需激活' not in desc and bool(re.search('已激活|激活且', desc))),
            MyBoardOnly=boolean(strings.get('conditionkey') == 'character_target_inboard'),
            MySameRow=boolean(strings.get('conditionkey') == 'character_same_row'),
            MyBehind=boolean(strings.get('dir') == 'behind'), MyFarthest=boolean(strings.get('scope') == 'farright')))
    invest = raw['bonds'].get('investShip', {})
    bb = invest.get('bb', {})
    threshold = next((x['layer'] for x in invest.get('layerMilestones', []) if x.get('layer', 0) > 0), bb.get('layer', 100))
    return record('PreparationGarrisonRules', MyEffects=tables.array('PreparationGarrisonRule', rows),
                  MyInvestRepeat=str(max(2, int(bb.get('count', 2)))), MyInvestLayer=number(threshold))
