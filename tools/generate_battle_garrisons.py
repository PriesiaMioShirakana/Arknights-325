"""Compile IN_BATTLE trait dispatch, text conditions and blackboards into static rules."""
from generate_combat import record, quote, number, boolean


def build_battle_garrisons(raw, tables):
    kinds = {
        'ADD_BOND': 'GRANT', 'GAIN_BUFF': 'BASE_ATTRIBUTES', 'attr_common_global_buff': 'COMMON_ATTRIBUTES', 'NONE': 'NONE',
        'act1autochess_gar_event_useskill': 'SKILL_GAIN', 'act1autochess_gar_event_selfkillenemy': 'KILL_GAIN',
        'act1autochess_gar_event_selfdead': 'DEATH_GAIN', 'act1autochess_gar_event_consume_ammo': 'AMMO_GAIN',
        'act1autochess_gar_event_enemy_abflag_inrange': 'FREEZE_GAIN', 'act2autochess_gar_event_onstart': 'DEPLOY_GAIN',
        'act2autochess_gar_event_allyenemy_sleepstun_inrange': 'SLEEP_STUN_GAIN',
        'act1autochess_gar_event_addition_cnt': 'EXTRA_GAIN', 'act1autochess_gar_eff_attrByBond': 'ATTRIBUTES_BY_BOND',
        'act1autochess_gar_eff_respawnTimeByBond': 'REDEPLOY_BY_BOND',
        'act2autochess_gar_eff_attrByBond_add_onstart': 'DEPLOY_ATTRIBUTES',
        'act2autochess_gar_eff_ab_damageScaleByBond': 'STATUS_DAMAGE',
        'act1autochess_gar_eff_attack_enemy': 'TAG_DAMAGE', 'act1autochess_gar_eff_chaos': 'WEAKNESS'}
    rows = []
    for ident, value in sorted(raw['garrisons'].items()):
        if value['eventType'] != 'IN_BATTLE': continue
        kind = kinds[value['effectKey']]
        p, s, desc = value.get('bb', {}), value.get('bbStr', {}), value.get('desc', '')
        amount = p.get('bond_add_count', 1 if s.get('bond_add_type') in ('by_charlevel', 'by_charcount_samerow') else 0)
        if s.get('bond_add_type') == 'by_charcount_samerow': amount = p.get('bond_add_count_multi', amount)
        rows.append(record('BattleGarrisonRule', MyId=quote(ident), MyKind='BattleGarrisonKind::'+kind,
            MyBonds=tables.array('std::string_view', [quote(b.strip()) for b in s.get('bond_id', '').split(',') if b.strip()]),
            MyGrantedGarrison=quote(s.get('give_garrison_id', '')), MyRequiredBond=quote(s.get('check_bond_id', '')),
            MyEnemyTag=quote(s.get('check_tag', '')),
            MyBondTarget='GarrisonBondTarget::'+{'bond_self':'SELF','bond_actived_maxstack':'HIGHEST'}.get(s.get('bond_type'), 'LIST'),
            MyGainAmount='GarrisonGainAmount::'+{'by_charcount_samerow':'ROW_COUNT','by_charlevel':'TIER'}.get(s.get('bond_add_type'), 'FIXED'),
            MyCondition='GarrisonCondition::'+{'character_same_row':'ROW','character_same_col':'COLUMN'}.get(s.get('conditionkey'), 'NONE'),
            MyGrantTarget='GarrisonGrantTarget::'+('ALL' if '所有' in desc else 'ROW_RIGHT' if '同一行最右边' in desc else 'SELF_FRONT' if '自身和身前' in desc else 'FRONT'),
            MyAmmoScope='GarrisonAmmoScope::'+{'1-1':'FRONT','x-5':'ADJACENT'}.get(s.get('range_id'), 'SELF'),
            MyAmount=number(amount), MyMaximum=number(p.get('max_add_count_per_battle', 0)),
            MyCheckCount=number(p.get('check_count', 0)), MyEvery=number(max(1, p.get('consume_count' if kind=='AMMO_GAIN' else 'check_cnt', 1))),
            MyProbability=number(p.get('prob', 1)), MyExtra=number(p.get('extra_cnt', 0)), MyDivisor=number(max(1, p.get('divide_num', 1))),
            MyAttack=number(p.get('atk', 1 if kind=='TAG_DAMAGE' else 0)), MyHealth=number(p.get('max_hp', 0)), MyDefense=number(p.get('def', 0)),
            MyAttackSpeed=number(p.get('attack_speed', 0)), MyHpRegen=number(p.get('hp_recovery_per_sec', 0)),
            MySpRegen=number(p.get('sp_recovery_per_sec', 0)), MyRedeploy=number(p.get('respawn_time', 0)),
            MyDuration=number(p.get('duration', 0)), MyDamageScale=number(p.get('damage_scale_per_stack', 0)),
            MyAlliedKills=boolean('我方干员' in desc), MyDollSwap=boolean('替身' in desc),
            MyBound=boolean(bool(p.get('check_ab_flag'))), MySluggish=boolean(bool(p.get('check_sluggish')))))
    return record('BattleGarrisonRules', MyEffects=tables.array('BattleGarrisonRule', rows),
                  MyBondOrder=tables.array('std::string_view', [quote(b) for b in raw['bonds']]))
