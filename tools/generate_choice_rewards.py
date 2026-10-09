"""Compile the prep-side semantics of content/choices.js; preserve action and alias insertion order."""
import math
from generate_combat import record, quote, boolean


def build_choice_rewards(raw, tables):
    effects, choices = raw['effects'], raw['choices']
    cards = {c['effectId']: c for family in ['bounty', 'tactic'] for c in choices['cards'][family]}
    prep = {
        'global_special_choice_gain_equip': 'ITEM_POOL',
        'global_special_choice_bond_addlayer': 'LAYERS',
        'global_special_choice_gain_coin': 'FUNDS',
        'global_special_choice_refresh_free': 'FREE_REFRESH',
        'single_special_choice_gloden_equip_chess': 'UPGRADE_ITEM',
        'single_special_choice_gloden_char_chess': 'UPGRADE_CHESS',
        'single_special_choice_gain_bond_chess': 'BOND_CHESS',
    }
    gates = {
        'global_special_choice_all_activated': 'ALWAYS',
        'global_special_choice_prep_finish_bench_at_least': 'BENCH_AT_LEAST',
        'global_special_choice_prep_finish_bench_at_most': 'BENCH_AT_MOST',
        'global_special_choice_prep_finish_same_row_at_least': 'SAME_ROW',
    }
    bounty_keys = ['add_enemy_kill_gain_coin', 'add_enemy_selfbattle_win_gain_coin', 'next_battle_add_enemy_win_gain_coin']

    def num(value, default=0):
        try:
            value = float(value)
            return value if math.isfinite(value) else default
        except (TypeError, ValueError):
            return default

    def integer(value, default=0):
        return math.trunc(num(value, default))

    strings = lambda values: tables.array('std::string_view', [quote(v) for v in values])
    rows = []
    for key, effect in sorted(effects.items()):
        if effect.get('effectType') not in ['ENEMY_GAIN', 'BUFF_GAIN'] and key not in cards:
            continue
        buffs = [b for b in effect.get('buffs', []) if b]
        if effect.get('effectType') == 'ENEMY_GAIN':
            b = next((b for name in bounty_keys for b in buffs if b.get('key') == name and isinstance((b.get('bbStr') or {}).get('enemy_id'), str)), None)
            if b:
                bb, bs = b.get('bb') or {}, b.get('bbStr') or {}
                rounds = 1 if b['key'] == bounty_keys[2] else max(1, min(99, integer(bb.get('round'), 1)))
                enemy = bs['enemy_id']
                count, coins, perfect = max(1, min(20, integer(bb.get('count'), 1))), max(0, integer(bb.get('coin'), integer(effect.get('enemyPrice')))), b['key'] != bounty_keys[0]
                # bountyOf sets multiRound for every duration > 1; addBounty uses the project's two-battle override.
                rounds = 2 if rounds > 1 else rounds
            else:
                card = cards.get(key, {})
                enemy = card.get('enemyKey', '')
                rounds = card.get('rounds', 1)
                rounds = 2 if card.get('multiRound') or rounds >= 90 else max(1, min(99, integer(rounds, 1)))
                count, coins, perfect = max(1, min(20, integer(card.get('count'), 1))), max(0, integer(card.get('coin'))), card.get('payout') == 'perfect'
            rows.append(record('ChoiceRewardRule', MyId=quote(key), MyKind='ChoiceCardKind::BOUNTY', MyEnemyId=quote(enemy if enemy in raw['enemies'] else ''),
                MyEnemyCount=str(count), MyCoins=str(coins), MyBattles=str(rounds), MyPerfect=boolean(perfect)))
            continue
        actions, devices = [], {}
        gate_kind, gate_count, found_gate, battle = 'ALWAYS', 0, False, False
        for buff in buffs:
            name = buff.get('key', '')
            p = {**(buff.get('bb') or {}), **(buff.get('bbStr') or {})}
            if name in prep:
                bonds = [v.strip() for v in str(p.get('bond_list') or '').split(',') if v.strip()] if prep[name] == 'LAYERS' else [p['bond']] if prep[name] == 'BOND_CHESS' and isinstance(p.get('bond'), str) else []
                actions.append(record('ChoiceRewardAction', MyKind='ChoiceRewardKind::'+prep[name], MyCount=str(max(0, integer(p.get('count'), 1))),
                    MyPool=quote(p.get('pool') if isinstance(p.get('pool'), str) else ''), MyBonds=strings(bonds)))
            if name in gates and name != 'global_special_choice_all_activated' and not found_gate:
                gate_kind = gates[name]
                gate_count = max(1 if gate_kind == 'SAME_ROW' else 0, integer(p.get('count'), 3 if gate_kind == 'SAME_ROW' else 0))
                found_gate = True
            if name == 'auto_chess_change_map':
                for alias, value in p.items():
                    if '#' in alias:
                        devices[alias] = num(value) != 0
            if name and name not in prep and name not in gates:
                battle = True
        rows.append(record('ChoiceRewardRule', MyId=quote(key), MyKind='ChoiceCardKind::TACTIC',
            MyActions=tables.array('ChoiceRewardAction', actions),
            MyDevices=tables.array('ChoiceDeviceSetting', [record('ChoiceDeviceSetting', MyAlias=quote(k), MyActive=boolean(v)) for k, v in devices.items()]),
            MyGate=record('ChoiceGate', MyKind='ChoiceGateKind::'+gate_kind, MyCount=str(gate_count)), MyBattleEffect=boolean(battle)))
    return tables.array('ChoiceRewardRule', rows)
