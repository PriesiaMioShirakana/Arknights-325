"""Compile wave generation configuration; runtime never reads JSON or invokes the JS oracle."""
import argparse
import json
import struct
from pathlib import Path
from generate_combat import Tables, number, quote, record, boolean


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--data',required=True,type=Path);parser.add_argument('--out',required=True,type=Path);args=parser.parse_args()
    raw={k:json.loads((args.data/(k+'.json')).read_text(encoding='utf-8')) for k in ('config','factions','enemies','stages','bosses')}
    config=raw['config'];factions=raw['factions'];g=factions['generation'];tables=Tables()
    strings=lambda xs:tables.array('std::string_view',[quote(x) for x in xs])
    weighted=lambda xs:tables.array('WeightedId',[record('WeightedId',MyId=quote(k),MyWeight=number(v)) for k,v in xs])
    f32=lambda x:struct.unpack('<f',struct.pack('<f',x))[0]
    enemies=[]
    for key,e in sorted(raw['enemies'].items()):
        st=e.get('stats') or {};f=g.get('beFactors') or {}
        power=e.get('attrPower')
        if power is None:
            power=f32(f32(f32(st.get('atk',0)*f.get('atk',5))+f32(st.get('maxHp',0)*f.get('hp',1)))+f32(st.get('def',0)*f.get('def',3)))
            power=f32(power+f32(f.get('res',3)*st.get('res',0)))
        enemies.append(record('WaveEnemyParameters',MyId=quote(key),MyPower=number(power),MyFactor=number(e.get('beFactor') or 1),
            MyFlying=boolean(e.get('isFlyEnemy',st.get('motion')=='FLY')),MyRank='EnemyRank::'+e.get('rank','NORMAL'),MyTokenOnly=boolean(e.get('tokenOnly',False))))
    enemy_span=tables.array('WaveEnemyParameters',enemies)
    faction_span=tables.array('FactionDefinition',[record('FactionDefinition',MyType='FactionType::'+key,MyRandom=boolean(v.get('involveRandom',False)),
        MyCount=str(v['count'] if v.get('count',0)>0 else 3),MySort=str(v.get('sortId',9))) for key,v in sorted(factions['types'].items())])
    entry_span=tables.array('FactionEntry',[record('FactionEntry',MyEnemyId=quote(e['key']),MyType='FactionType::'+e['type'],MyWeight=number(e.get('weight',1)),
        MyFirstHalf=boolean(e['firstHalf']),MyFlying=boolean(e.get('fly',False)),
        MyNormal=strings([x if isinstance(x,str) else x['key'] for x in e.get('N',[])]),MyElite=strings([x if isinstance(x,str) else x['key'] for x in e.get('E',[])])) for _,e in sorted(factions['entries'].items())])
    placeholder_span=tables.array('WavePlaceholder',[record('WavePlaceholder',MyEnemyId=quote(k),MySlot='WaveSlot::'+v['slot']) for k,v in g['placeholders'].items()])
    rules=record('WaveGenerationRules',MyFactions=faction_span,MyEntries=entry_span,MyEnemies=enemy_span,MyPlaceholders=placeholder_span,
        MyFactionCount=str(g.get('specialEnemyNum',3)),MyRoundCount=str(g.get('maxLevelCnt',15)),MyFirstHalfLastRound=str(g.get('firstHalfMaxRound',7)),
        MyMinReplacement=str(g.get('minReplacedEnemyCount',1)),MyMaxReplacement=str(g.get('maxReplacedEnemyCount',5)),MyFillType='FactionType::'+g.get('fillType','SPECIAL'),
        MyTemplateSlots=tables.array('WavePlaceholder',[record('WavePlaceholder',MyEnemyId=quote(v),MySlot='WaveSlot::'+k) for k,v in factions['templateSlots'].items()]),
        MyUniteTemplates='std::array<std::string_view, 2>{'+','.join(quote((config.get('unite') or {}).get('templates',{}).get(str(i),d)) for i,d in [(1,'act1autochess_escaped_single'),(2,'act1autochess_escaped_multi')])+'}')
    modes=[]
    fallback=next(iter(sorted(k for k,v in raw['stages'].items() if v.get('active'))),'')
    for key,m in sorted(config['modes'].items()):
        rounds=[]
        for r,c in sorted(m['rounds'].items(),key=lambda x:int(x[0])):
            scale=(m.get('enemyScale') or {}).get(r) or {};supply=scale.get('supplyHp',1)
            boss_map=tables.array('BossTemplate',[record('BossTemplate',MyBossId=quote(k),MyTemplateId=quote(v)) for k,v in (c.get('bossTemplates') or {}).items()])
            rounds.append(record('WaveRoundRules',MyRound=str(int(r)),MyTemplateId=quote(c.get('template') or ''),MyBossTemplates=boss_map,
                MyTimeLimit=number((c.get('combatTimeLimit') or (m.get('combatTimeLimit') or {}).get(r) or 60)*(config.get('combatTimeScale') or 2)),
                MyScale=record('WaveScale',MyHealth=number(max(0.01,scale.get('hp',1))),MyAttack=number(max(0,scale.get('atk',1))),MySpeed=number(max(0.01,scale.get('speed',1))),
                    MySupplyHealth=number(supply) if supply>0 and supply!=1 else '{}')))
        stage_pairs=[(k,raw['stages'][k]['weight']) for k in m.get('stages',[]) if k in raw['stages'] and raw['stages'][k].get('active') is not False and raw['stages'][k].get('weight',0)>0]
        bosses=lambda kind:weighted([(k,v) for k,v in (m.get(kind) or {}).items() if k in raw['bosses'] and v>0])
        modes.append(record('WaveModeRules',MyId=quote(key),MyStages=weighted(stage_pairs),MyFallbackStage=quote(fallback),
            MyBosses=bosses('bossWeights'),MyHiddenBosses=bosses('hiddenBossWeights'),MyBossRound=str(m.get('bossRound') or m.get('lastRound') or 14),
            MyHiddenRound=str(m.get('hiddenRound') or 0),MyInactiveEnemies=strings(m.get('inactiveEnemyKeys') or []),MyRounds=tables.array('WaveRoundRules',rounds)))
    mode_span=tables.array('WaveModeRules',modes)
    args.out.parent.mkdir(parents=True,exist_ok=True)
    args.out.write_text('\n'.join(['// Generated from config/factions/enemies/stages/bosses JSON; edit generate_wave_generation.py.',
        '#include <stronghold/adapters/reference_wave_generation.hpp>','namespace Stronghold { namespace {',*tables.lines,'constexpr auto Rules = '+rules+';','}',
        'const WaveGenerationRules& ReferenceWaveGeneration() noexcept { return Rules; }',
        'std::span<const WaveModeRules> ReferenceWaveModes() noexcept { return '+mode_span+'; }',
        'const WaveModeRules& ReferenceWaveMode(std::string_view _id) { const auto modes = ReferenceWaveModes();',
        'const auto it = std::ranges::lower_bound(modes, _id, {}, &WaveModeRules::MyId);',
        'if (it == modes.end() || it->MyId != _id) throw std::out_of_range("unknown wave mode");','return *it; }','}'])+'\n',encoding='utf-8')

if __name__=='__main__':main()
