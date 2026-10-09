"""Compile the ten intrinsic strategy battle behaviours; map characters use the token adapter."""
from generate_combat import record, quote, number, boolean


def build_band_battle(bands, tables):
    keys = dict(zip([
        'act1autochess_band2_buff', 'act1autochess_band28_buff', 'act1autochess_band13_buff',
        'act1autochess_band16_buff', 'act1autochess_band17_buff', 'act1autochess_band18_buff',
        'act1autochess_band15_buff', 'act1autochess_band19_buff', 'act2autochess_band12_buff',
        'act2autochess_band3_buff',
    ], ['ACTIVE_BOND_STATS', 'REVIVE', 'DEATH_LAYERS', 'DEPLOY_COOLDOWN', 'FRONT_SHIELD',
        'SKILL_END_SP', 'DEATH_ATTACK', 'WEAKNESS', 'SAME_NAME_ATTACK', 'ELITE_STATS']))
    rows = []
    for ident, band in sorted(bands.items()):
        for buff in band.get('buffs', []):
            p = {**buff.get('bb', {}), **buff.get('bbStr', {})}
            key = p.get('key')
            if key not in keys:
                if key: raise ValueError(f'Unported strategy battle key: {key}')
                continue
            kind = keys[key]
            value = (max(0, 1 + p.get('respawn_time', 0)) if kind == 'DEPLOY_COOLDOWN' else
                p.get('prob', 0) if kind == 'FRONT_SHIELD' else p.get('sp', 0) if kind == 'SKILL_END_SP' else
                max(1, int(p.get('value', 1))) if kind == 'DEATH_LAYERS' else p.get('atk_per_cnt', 0) if kind == 'ELITE_STATS' else p.get('atk', 0))
            maximum = max(0, int(p.get('max_respawn_cnt', 3))) if kind == 'REVIVE' else max(1, int(p.get('max_stack_cnt', 10))) if kind == 'DEATH_ATTACK' else 20 if kind == 'DEPLOY_COOLDOWN' else 0
            steps = []
            for i in range(1, 10):
                if f'value_{i}' not in p: break
                steps.append(record('BandStatStep', MyCount=str(int(p[f'value_{i}'])), MyAttack=number(p.get(f'atk_{i}', 0)), MyHealth=number(p.get(f'max_hp_{i}', 0))))
            params = record('BandBattleParameters', MyKind='BandBattleKind::'+kind, MyValue=number(value),
                MyHealth=number(p.get('max_hp_per_cnt', 0)), MyMaximum=str(maximum),
                MyByTier=boolean(p.get('bond_add_type', 'by_charlevel') == 'by_charlevel'),
                MySteps='std::array<BandStatStep, 9>{'+','.join(steps)+'}', MyStepCount=str(len(steps)))
            rows.append(record('BandBattleRule', MyId=quote(ident), MyBond=quote(p.get('bond_id', '')), MyParameters=params))
    return tables.array('BandBattleRule', rows)


def build_map_character_bands(bands, tables):
    rows = []
    for buff in bands['band_amedic']['buffs']:
        if buff.get('key') != 'auto_chess_change_map': continue
        p = {**buff.get('bb', {}), **buff.get('bbStr', {})}
        cond = p.get('common_condition', '')
        if not cond.endswith(('_ge', '_le')): raise ValueError('unknown map character condition: '+cond)
        chars = list(dict.fromkeys(k.split('#')[0] for k, v in p.items() if '#' in k and float(v) > 0))
        rows.append(record('MapCharacterBandRule', MyMinimumElites=str(int(p['cnt']) if cond.endswith('_ge') else 0),
            MyMaximumElites=str(int(p['cnt'])) if cond.endswith('_le') else 'std::numeric_limits<unsigned>::max()',
            MyCharacters=tables.array('std::string_view', [quote(c) for c in chars])))
    if len(rows) != 2: raise ValueError('expected both map character variants')
    return tables.array('MapCharacterBandRule', rows)
