"""Compile pinned battle data to constexpr tables; the game has no JSON/Node runtime dependency.

Keep field names/defaults aligned with server/sim/simdata.js normalizeEnemy and battle/spawns.js.
Generation is deterministic, accepts no executable source from the JSON, and rejects invalid numbers.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path


def quote(value):
    return json.dumps(str(value), ensure_ascii=False)


def number(value):
    value = float(value)
    if not math.isfinite(value):
        raise ValueError('non-finite combat data')
    return repr(value)


def boolean(value):
    return 'true' if value else 'false'


def record(kind, **fields):
    return kind + '{' + ', '.join('.' + key + ' = ' + value for key, value in fields.items()) + '}'


class Tables:
    """Each referenced span has static storage; empty spans need no zero-length extension array."""
    def __init__(self):
        self.lines = []
        self.sequence = 0

    def array(self, kind, rows):
        if not rows:
            return '{}'
        self.sequence += 1
        name = 'Data' + str(self.sequence)
        self.lines.append(f'constexpr std::array<{kind}, {len(rows)}> {name}' + '{{\n' + ',\n'.join(rows) + '\n}};')
        return name

    def blackboard(self, values):
        rows = []
        for key, value in sorted((values or {}).items()):
            data = 'std::string_view{' + quote(value) + '}' if isinstance(value, str) else number(value)
            rows.append(record('BlackboardEntry', MyKey=quote(key), MyValue=data))
        return record('Blackboard', MyEntries=self.array('BlackboardEntry', rows))


def enemy(tables, key, raw):
    st = raw.get('stats', raw)
    # Generated JSON already uses the normalized spellings. Keep runtime creation's enemy-only defaults.
    stats = record('CombatStats', MyMaxHealth=number(max(1, st.get('maxHp', 1000))),
        MyAttack=number(max(0, st.get('atk', 0))), MyDefense=number(max(0, st.get('def', 0))),
        MyResistance=number(max(0, st.get('res', 0))), MyAttackSpeed=number(st.get('aspd', 100) or 100),
        MyBaseAttackTime=number(max(0.1, st.get('bat', 2))), MyMoveSpeed=number(max(0, st.get('moveSpeed', 1))),
        MyBlockCount='0', MyTaunt=number(st.get('tauntLevel', 0)), MyRedeploySeconds='0', MyDeploymentCost='0',
        MySpRecovery='0', MyHealthRegen=number(st.get('hpRecoveryPerSec', 0)), MyMass=number(st.get('massLevel', 1)),
        MyElementalResistance=number(st.get('elementDmgRes', 0)), MyElementResistance=number(st.get('elementRes', 0)))
    damage = st.get('dmgType', 'phys')
    animation = raw.get('attackAnim') or {}
    dur = animation.get('dur', 0)
    hit = animation.get('hit')
    attack = record('AttackProfile', MyDamageType='DamageType::' + {'phys':'PHYSICAL', 'arts':'ARTS', 'none':'PHYSICAL', 'true':'TRUE_DAMAGE', 'heal':'ARTS'}[damage],
        MyDisabled=boolean(damage == 'none' or raw.get('applyWay') == 'NONE'), MyHealing=boolean(damage == 'heal'), MyRanged=boolean(raw.get('applyWay') == 'RANGED'),
        MyAttackWhileMoving=boolean(raw.get('attackMoves', False)), MyEnemyRange=number(max(0, st.get('rangeRadius', 0))),
        MyAnimationDuration=number(dur), MyAnimationHit=number(min(dur, max(0, hit))) if hit is not None and dur > 0 else '{}')
    area = raw.get('hitArea')
    body = record('HitArea', MyWidth=number(area['w']), MyHeight=number(area['h']),
        MyOffsetX=number(area.get('dx', 0)), MyOffsetY=number(area.get('dy', 0))) if area else '{}'
    immunity_names = {'stun':'STUN', 'silence':'SILENCE', 'sleep':'SLEEP', 'frozen':'FREEZE', 'levitate':'LEVITATE',
        'feared':'FEAR', 'attract':'ATTRACT', 'disarmedCombat':'DISARM', 'palsy':'PALSY'}
    immune = [k for k,v in st.get('immunities', {}).items() if v] + st.get('otherImmunities', [])
    flags = ' | '.join(f'(std::uint64_t{{1}} << static_cast<unsigned>(CombatStatus::{immunity_names[k]}))' for k in sorted(set(immune))) or '0'
    skills = [record('EnemySkillRecord', MyId=quote(s['prefabKey']), MyPriority=number(s['priority']),
        MyCooldown=number(s['cooldown']), MyInitialCooldown=number(s['initCooldown']), MySpCost=number(s['spCost']),
        MyBlackboard=tables.blackboard(s.get('bb')), MyStrings=tables.blackboard(s.get('bbStr'))) for s in raw.get('skills', [])]
    sp = raw.get('sp') or {}
    sp_kind = {'INCREASE_WHEN_ATTACK':'ATTACK', 'INCREASE_WITH_TIME':'TIME', 'INCREASE_WHEN_TAKEN_DAMAGE':'HURT'}.get(sp.get('type'), 'NONE')
    return record('EnemyRecord', MyId=quote(key), MyName=quote(raw.get('name', key)), MyStats=stats, MyAttack=attack,
        MyRank='EnemyRank::'+raw.get('rank', 'NORMAL'), MyFlying=boolean(st.get('motion') == 'FLY'),
        MyBlockWeight=str(max(1, int(st.get('blockCnt', 1)))), MyLifeCost=str(int(st.get('lpr', 1))),
        MyCounted=boolean(not raw.get('notCountInTotal', False)), MyStaticBody=boolean(raw.get('staticBody', False)),
        MyHitArea=body, MyImmunities=flags, MyTalent=tables.blackboard(raw.get('talents', {}).get('bb')),
        MyTalentStrings=tables.blackboard(raw.get('talents', {}).get('bbStr')), MySkills=tables.array('EnemySkillRecord', skills),
        MyAbilities=tables.array('EnemyAbilityRecord', [record('EnemyAbilityRecord', MyText=quote(a['text']), MyFormat=quote(a['format'])) for a in raw.get('abilities', [])]),
        MyTags=tables.array('std::string_view', [quote(v) for v in raw.get('tags', [])]),
        MySp=record('EnemySpRecord', MyType='SpType::'+sp_kind, MyMaximum=number(sp.get('maxSp', 0)),
            MyInitial=number(sp.get('initSp', 0)), MyIncrement=number(sp.get('increment', 0))))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--data', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path)
    args = parser.parse_args()
    source = (args.data / 'enemies.json').read_bytes()
    data = json.loads(source)
    tables = Tables()
    enemies = tables.array('EnemyRecord', [enemy(tables, key, value) for key, value in sorted(data.items())])
    from generate_stage import stage
    stage_source = (args.data / 'stages.json').read_bytes()
    stages = tables.array('StageRecord', [stage(tables, key, value) for key, value in sorted(json.loads(stage_source).items())])
    from generate_wave import wave
    wave_source = (args.data / 'waves.json').read_bytes()
    waves = tables.array('WaveRecord', [wave(tables, key, value, data) for key, value in sorted(json.loads(wave_source).items())])
    lines = ['// Generated from data/enemies.json, stages.json and waves.json. Edit tools/generate_combat.py, not this file.',
        '#include <stronghold/adapters/reference_stage.hpp>', '#include <stronghold/adapters/reference_wave.hpp>', 'namespace Stronghold { namespace {', *tables.lines, '}',
        f'std::span<const EnemyRecord> ReferenceEnemies() noexcept {{ return {enemies}; }}',
        'const EnemyRecord& ReferenceEnemy(std::string_view _id) {',
        'const auto records = ReferenceEnemies();',
        'const auto found = std::ranges::lower_bound(records, _id, {}, &EnemyRecord::MyId);',
        'if (found == records.end() || found->MyId != _id) throw std::out_of_range("unknown enemy");',
        'return *found; }',
        f'std::span<const StageRecord> ReferenceBattleStages() noexcept {{ return {stages}; }}',
        'const StageRecord& ReferenceBattleStage(std::string_view _id) {',
        'const auto records = ReferenceBattleStages();',
        'const auto found = std::ranges::lower_bound(records, _id, {}, &StageRecord::MyId);',
        'if (found == records.end() || found->MyId != _id) throw std::out_of_range("unknown stage");',
        'return *found; }',
        f'std::span<const WaveRecord> ReferenceWaves() noexcept {{ return {waves}; }}',
        'const WaveRecord& ReferenceWave(std::string_view _id) {',
        'const auto records = ReferenceWaves();',
        'const auto found = std::ranges::lower_bound(records, _id, {}, &WaveRecord::MyId);',
        'if (found == records.end() || found->MyId != _id) throw std::out_of_range("unknown wave");',
        'return *found; }',
        f'std::string_view ReferenceCombatFingerprint() noexcept {{ return "{hashlib.sha256(source + stage_source + wave_source).hexdigest()}"; }}', '}']
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text('\n'.join(lines) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
