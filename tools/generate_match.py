"""Compile immutable match configuration, mirroring GameData's mode/band/boss fallbacks."""
import argparse
import hashlib
import json
import math
from pathlib import Path
from generate_combat import Tables, number, quote, record, boolean


def finite(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)


def positive(value, fallback):
    return value if finite(value) and value > 0 else fallback


def integer(value, fallback, minimum=1):
    return value if type(value) is int and value >= minimum else fallback


def scale(value):
    value = value or {}
    fields = {}
    for key, member in [('solo','MySolo'),('coop','MyCoop'),('aliveFull','MyFullTeam')]:
        if finite(value.get(key)): fields[member] = number(value[key])
    for key, member in [('perPlayer','MyPerPlayer'),('aliveScaling','MyAliveScaling')]:
        if type(value.get(key)) is bool: fields[member] = boolean(value[key])
    return record('BossHealthScale', **fields)


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--data',required=True,type=Path);parser.add_argument('--out',required=True,type=Path);args=parser.parse_args()
    raw={k:json.loads((args.data/(k+'.json')).read_text(encoding='utf-8')) for k in ['config','bands','bosses']}
    config=raw['config'];bands=raw['bands'];bosses=raw['bosses'];tables=Tables()
    strings=lambda xs:tables.array('std::string_view',[quote(x) for x in xs])
    timer=lambda k,d:positive((config.get('timers') or {}).get(k),d)
    clock=positive(config.get('combatTimeScale'),2)
    hc=config.get('hiddenCore') or {};bd=config.get('bandDraft') or {};economy=config.get('economy') or {}
    default_band=bd.get('timeoutBandId') if bd.get('timeoutBandId') in bands else economy.get('defaultBandId','band_bldsk')
    modes=[]
    for key,m in sorted(config['modes'].items()):
        solo=m.get('type')=='SINGLE' or key.startswith('mode_single_');difficulty=m.get('difficulty') or key.split('_')[-1].upper()
        last=integer(m.get('lastRound'),9 if key=='mode_single_funny' else 14);boss_round=integer(m.get('bossRound'),last);hidden=integer(m.get('hiddenRound'),0)
        rounds=[]
        for r,c in sorted((m.get('rounds') or {}).items(),key=lambda x:int(x[0])):
            prep=positive(c.get('prepTime'),None);countdown=positive(c.get('levelMaxPlayTime'),None)
            duration=positive(c.get('combatTimeLimit'),positive((m.get('combatTimeLimit') or {}).get(r),60))*clock
            rounds.append(record('MatchRoundRules',MyRound=str(int(r)),MyPreparationSeconds=number(prep) if prep is not None else '{}',
                MyCombatGameSeconds=number(duration),MyBossCountdownSeconds=number(countdown) if countdown is not None else '{}'))
        strategies=[]
        for bid,b in sorted(bands.items(),key=lambda x:(x[1].get('sortId',99),x[0])):
            if isinstance(b.get('modeTypeList'),list) and ('SINGLE' if solo else 'MULTI') not in b['modeTypeList']:continue
            strategies.append(record('StrategyRecord',MyId=quote(bid),MyStartingLife=str(integer(b.get('totalHp'),integer(economy.get('defaultStartLp'),28))),
                MyEffectId=quote(b.get('effectId') or ''),MyBonds=strings(b.get('bondIds') or [])))
        health=[]
        for bid,b in sorted(bosses.items()):
            bp=b.get('bloodPoint') or {};base=bp.get(difficulty)
            if not finite(base):base=next((v for v in bp.values() if finite(v)),500000)
            health.append(record('BossHealthRecord',MyId=quote(bid),MyBaseHealth=number(base)))
        final=record('FinalAssaultRules',MySolo=boolean(solo),MyHasHiddenRound=boolean(hidden>0),MyHiddenDifficultyAllowed=boolean(difficulty in hc.get('difficulties',['NORMAL','HARD','ABYSS'])),
            MyHiddenSoloLayers=number(hc.get('single',350)),MyHiddenCoopLayers=number(hc.get('multi',1200)),MyHiddenMinimumLife=number(hc.get('minTeamLpExclusive',1)),
            MyGameSecondsPerRealSecond=number(clock),MyOvertimeAfterRealSeconds=number(max(0,config.get('bossOvertimeAfter',150))),MyOvertimeDrainPerRealSecond=number(max(0,config.get('bossOvertimeDrainPerSec',1))))
        modes.append(record('MatchRules',MyId=quote(key),MySolo=boolean(solo),MyDifficulty=quote(difficulty),MyLastRound=str(last),MyBossRound=str(boss_round),MyHiddenRound=str(hidden),
            MySpecialRounds=tables.array('unsigned',[str(v) for v in m.get('spRounds',[]) if type(v) is int]),MyRounds=tables.array('MatchRoundRules',rounds),
            MyStrategies=tables.array('StrategyRecord',strategies),MyBosses=tables.array('BossHealthRecord',health),MyActiveBonds=strings(m.get('activeBondIds') or []),
            MyInactiveBonds=strings(m.get('inactiveBondIds') or []),MyBossScale=scale(m.get('bossHpScale')),MyGlobalBossScale=scale(config.get('bossHpScale')),
            MyFinalAssault=final,MySpecialDraft=record('SpecialDraftRules',MySolo=boolean(solo),MyFirstTurnSeconds=number(timer('spFirst',30)),MyTurnSeconds=number(timer('spTurn',16))),
            MyInformationSeconds=number(timer('infoCheck',25)),MyBattleCheckSeconds=number(timer('battleCheck',3)),MyStrategyTurnSeconds=number(timer('bandTurn',30)),
            MyStrategySkips=str(integer(bd.get('skipsPerPlayer'),1,0)),MyDefaultStrategy=quote(default_band),MyLifeCapPerRound=str(integer(config.get('lpCapPerRound'),10))))
    span=tables.array('MatchRules',modes)
    fingerprints=[f'// {k}.json SHA-256 {hashlib.sha256((args.data/(k+".json")).read_bytes()).hexdigest()}' for k in raw]
    args.out.parent.mkdir(parents=True,exist_ok=True)
    args.out.write_text('\n'.join(['// Generated by generate_match.py; do not edit the constexpr output.',*fingerprints,
        '#include <stronghold/adapters/reference_match.hpp>','namespace Stronghold { namespace {',*tables.lines,'}',
        'std::span<const MatchRules> ReferenceMatchModes() noexcept { return '+span+'; }',
        'const MatchRules& ReferenceMatchMode(std::string_view _id) { const auto modes = ReferenceMatchModes();',
        'const auto found = std::ranges::lower_bound(modes, _id, {}, &MatchRules::MyId);',
        'if (found == modes.end() || found->MyId != _id) throw std::out_of_range("unknown match mode");',
        'return *found; }','}'])+'\n',encoding='utf-8')

if __name__=='__main__':main()
