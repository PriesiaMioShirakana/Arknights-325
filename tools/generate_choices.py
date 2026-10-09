"""Generate constexpr choice tables; the runtime implements the original draw order and RNG consumption."""
import argparse
import hashlib
import json
import re
from pathlib import Path
from generate_choice_rewards import build_choice_rewards
from generate_choice_battle import build_choice_battle
from generate_preparation_items import build_preparation_items
from generate_combat import Tables, number, quote, record, boolean


def main():
    p=argparse.ArgumentParser();p.add_argument('--data',type=Path,required=True);p.add_argument('--out',type=Path,required=True);args=p.parse_args()
    raw={k:json.loads((args.data/(k+'.json')).read_text(encoding='utf-8')) for k in ['choices','items','enemies','effects']}
    choices=raw['choices'];effects=raw['effects'];tables=Tables();rows=[];bounty_ids={};item_ids={};tactic_ids={}
    strings=lambda xs:tables.array('std::string_view',[quote(x) for x in xs])
    ints=lambda xs:tables.array('int',[str(x) for x in xs])
    indices=lambda xs:tables.array('std::size_t',[str(x) for x in xs])
    unsigned=lambda xs:tables.array('unsigned',[str(x) for x in xs])
    weights=lambda xs:tables.array('ChoiceWeight',[record('ChoiceWeight',MyValue=str(k),MyWeight=number(w)) for k,w in xs])
    kind=lambda k:'BountyDraftKind::'+(k or 'none').upper()
    def bounty_text(text,c):
        text=text or ''
        if not(c.get('multiRound') or c.get('rounds',0)>=90):return text
        text=re.sub(r'(?:之后的|后续的?)<@ba\.vdown>每场</>作战','接下来<@ba.vup>两场作战</>',text)
        return re.sub(r'(?:之后的|后续的?)每场作战','接下来两场作战',text)
    for c in choices['cards']['bounty']:
        bounty_ids[c['effectId']]=len(rows)
        rounds=2 if c.get('multiRound') or c.get('rounds',0)>=90 else max(1,min(99,c.get('rounds',1)))
        rows.append(record('ChoiceCardRecord',MyKind='ChoiceCardKind::BOUNTY',MyId=quote(c['effectId']),MyName=quote(c.get('name') or ''),
            MyDescription=quote(bounty_text(c.get('desc'),c)),MyRichDescription=quote(bounty_text((effects.get(c['effectId']) or {}).get('descRaw'),c)),
            MyTier=str(c.get('tier',1)),MyEnemyId=quote(c.get('enemyKey') or ''),MyCount=str(c.get('count',1)),MyCoins=str(c.get('coin',0)),MyBattles=str(rounds),MyPerfect=boolean(c.get('payout')=='perfect')))
    for c in choices['cards']['tactic']:
        tactic_ids[c['effectId']]=len(rows)
        rows.append(record('ChoiceCardRecord',MyKind='ChoiceCardKind::TACTIC',MyId=quote(c['effectId']),MyName=quote(c.get('name') or ''),
            MyDescription=quote(c.get('desc') or ''),MyTeam=boolean(c.get('team',False)),MyTacticKind=quote(c.get('kind') or '')))
    for key,c in sorted(raw['items'].items()):
        item_ids[key]=len(rows)
        rows.append(record('ChoiceCardRecord',MyKind='ChoiceCardKind::ITEM',MyId=quote(key),MyName=quote(c.get('name') or key),MyDescription=quote(c.get('desc') or ''),MyTier=str(c.get('tier',1))))
    cards=tables.array('ChoiceCardRecord',rows)
    bounties=tables.array('BountyDraftEntry',[record('BountyDraftEntry',MyCard=str(bounty_ids[c['effectId']]),MyPool=kind(c.get('draftPool')),MySeries=str(c.get('series') or 0))
        for c in choices['cards']['bounty'] if c.get('draft',c.get('payout')!='perfect') and c.get('enemyKey') in raw['enemies']])
    shop=choices.get('shopDraft') or {};tactic=choices.get('tacticDraft') or {}
    tactics=[]
    for c in choices['cards']['tactic']:
        effect=effects.get(c['effectId'])
        if not effect:continue
        bonds=[]
        for b in filter(None,effect.get('buffs',[])):
            bs=b.get('bbStr') or {}
            if b['key']=='single_special_choice_gain_bond_chess' and isinstance(bs.get('bond'),str):bonds.append(bs['bond'])
            elif b['key']=='global_special_choice_bond_addlayer':bonds.extend(x.strip() for x in (bs.get('bond_list') or '').split(',') if x.strip())
            else:bonds=[];break
        tactics.append(record('TacticDraftEntry',MyCard=str(tactic_ids[c['effectId']]),MyStage=quote(c.get('stageId') or ''),MyTargetBonds=strings(bonds),MyWeight=number((tactic.get('weights') or {}).get(c['effectId'],1))))
    tactics=tables.array('TacticDraftEntry',tactics)
    tiers=[]
    for tier in range(1,7):
        pairs=[(item_ids[k],(shop.get('itemWeights') or {}).get(k,1)) for k,v in sorted(raw['items'].items())
            if not v.get('isGolden') and not v.get('hideInShop') and not v.get('shopExcluded') and v.get('itemType')=='EQUIP' and v.get('tier')==tier]
        tiers.append(weights(pairs))
    specs=[]
    for k,s in (choices.get('bountyDrafts') or {}).items():
        groups=[]
        for g in s.get('groups',[]):
            ws=g.get('weights') or []
            pairs=[(bounty_ids[c],max(0,(ws[i] if i<len(ws) else 1) or 1)) for i,c in enumerate(g.get('cards',[])) if c in bounty_ids]
            groups.append(record('BountyDraftGroup',MyCards=weights(pairs),MySeen=str(sum(type(x)is int for x in g.get('seen',[])) or 1),MyOpen=str(g.get('open',0))))
        groups=tables.array('BountyDraftGroup',groups);r=s.get('rule') or {}
        refs=lambda key:indices([bounty_ids[c] for c in r.get(key,[]) if c in bounty_ids])
        rule=record('BountyDraftRule',MySeries=ints(r.get('series',[])),MyTiers=ints(r.get('tiers',[])),MyPerSeries=str(r.get('perSeries',1)),MyPreferred=refs('prefer'),
            MySize=str(r.get('size',7)),MyOnePerSeries=ints(r.get('onePerSeries',[])),MyMaxSeries16=str(r.get('maxSeries16',2)),MyGiants=refs('giants'),MyCards=refs('cards'))
        specs.append(record('BountyDraftSpec',MyKind=kind(k),MyCount=str(s.get('count',6)),MySlots=str(s.get('slots',0)),MyPickSeen=boolean(s.get('pick')=='seen'),MyGroups=groups,MyRule=rule))
    slots=tables.array('ChoiceShopSlot',[record('ChoiceShopSlot',MyKinds=weights([(0 if k=='coin' else int(k),v) for k,v in slot.items()])) for slot in shop.get('slots',[])])
    families=['bounty','supply','shop','tactic'];schedules=[]
    for mode,s in sorted(choices.get('schedule',{}).items()):
        for round,c in sorted(s.get('rounds',{}).items(),key=lambda x:int(x[0])):
            events='std::array<std::span<const std::string_view>, 4>{'+','.join(strings((c.get('events') or {}).get(f,[])) for f in families)+'}'
            schedules.append(record('ChoiceSchedule',MyMode=quote(mode),MyRound=str(int(round)),MyFamilies=weights([(families.index(f['family']),f['weight']) for f in c.get('families',[])]),
                MyCount=str(c.get('cards',0)),MySupplyTiers='std::array<int, 2>{'+','.join(map(str,c.get('supplyTiers',[1,6])))+'}',MyBountyKind=kind(c.get('bountyDraft')),MyEvents=events))
    family_data='std::array<ChoiceFamilyRecord, 4>{'+','.join(record('ChoiceFamilyRecord',MyName=quote(choices['families'][f]['name']),MyDescription=quote(choices['families'][f].get('desc') or '')) for f in families)+'}'
    rules=record('ChoiceGenerationRules',MyCards=cards,MyBounties=bounties,MyTactics=tactics,MyItemsByTier='std::array<std::span<const ChoiceWeight>, 6>{'+','.join(tiers)+'}',
        MyCoinCard=str(item_ids[shop['coin']]) if shop.get('coin') in item_ids else '{}',MyBountySpecs=tables.array('BountyDraftSpec',specs),MyShopRounds=unsigned(shop.get('rounds',[])),MyShopSlots=slots,
        MyTacticRounds=unsigned(tactic.get('rounds',[])),MyTacticKinds=strings(tactic.get('kinds',[])),MyHasTacticSpec=boolean(bool(tactic)),MySchedules=tables.array('ChoiceSchedule',schedules),MyFamilies=family_data,
        MySoloCount=str(choices.get('format',{}).get('solo',{}).get('cards',3)),MyCoopCount=str(choices.get('format',{}).get('multi',{}).get('cards',6)))
    pools=[]
    for key,pool in sorted(choices.get('pools',{}).items()):
        if pool.get('kind') not in ['equip','chess']: raise ValueError('unknown content pool kind: '+key)
        weighted=tables.array('ContentPoolWeight',[record('ContentPoolWeight',MyId=quote(k),MyWeight=number(w)) for k,w in pool.get('weighted',[])])
        pools.append(record('ContentPoolRecord',MyId=quote(key),MyKind='PieceKind::'+('ITEM' if pool['kind']=='equip' else 'CHESS'),
            MyWeighted=weighted,MyItems=strings(pool.get('items',[])),MyTiers=ints(pool.get('tiers',[])),MyShopLevel=boolean(pool.get('maxTier')=='shopLevel'),
            MyMaxTier=str(pool.get('maxTier') if type(pool.get('maxTier')) is int else 6),MyMinTier=str(pool.get('minTier',1)),
            MyExactTier=str(pool['tier']) if 'tier' in pool else '{}',MyBond=quote(pool.get('bond') or ''),MyGolden=boolean(pool.get('golden',False))))
    content_pools=tables.array('ContentPoolRecord',pools)
    reward_rules=build_choice_rewards(raw,tables)
    battle_rules=build_choice_battle(raw,tables)
    item_rules=build_preparation_items(raw['items'],tables)
    hashes=[f'// {k}.json SHA-256 {hashlib.sha256((args.data/(k+".json")).read_bytes()).hexdigest()}' for k in raw]
    args.out.parent.mkdir(parents=True,exist_ok=True)
    args.out.write_text('\n'.join(['// Generated by generate_choices.py; do not edit.',*hashes,'#include <stronghold/adapters/reference_choices.hpp>',
        'namespace Stronghold { namespace {',*tables.lines,'constexpr auto Rules = '+rules+';','}',
        'const ChoiceGenerationRules& ReferenceChoices() noexcept { return Rules; }',
        'std::span<const ContentPoolRecord> ReferenceContentPools() noexcept { return '+content_pools+'; }',
        'std::span<const ChoiceRewardRule> ReferenceChoiceRewards() noexcept { return '+reward_rules+'; }',
        'std::span<const ChoiceBattleRule> ReferenceChoiceBattleRules() noexcept { return '+battle_rules+'; }',
        'std::span<const PreparationItemRule> ReferencePreparationItems() noexcept { return '+item_rules+'; }','}'])+'\n',encoding='utf-8')

if __name__=='__main__':main()
