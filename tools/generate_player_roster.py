"""Compile preparation metadata from protocol.js, player/diy.js and GameData.placeableTokens.

Separate from combat bodies: preparation grants use deployLimit, not a skill's summon count.
Tables keep source order where it decides which conflicting DIY slot is accepted first.
"""
import argparse
import json
import re
from pathlib import Path
from generate_combat import Tables, record, quote, boolean


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    chess = json.loads((args.data / 'chess.json').read_bytes())
    backups = json.loads((args.data / 'backups.json').read_bytes())
    tokens = {**backups['tokens'], **json.loads((args.data / 'tokens.json').read_bytes())}
    tables = Tables()
    strings = lambda values: tables.array('std::string_view', [quote(v) for v in values])

    def skills_of(raw):
        values = raw.get('skills') or [raw.get('skill') or {}]
        return sorted({s['index'] for s in values if isinstance(s.get('index'), int) and 0 <= s['index'] <= 9})

    rules = []
    for key, raw in sorted(chess.items()):
        if raw['isGolden']: continue
        golden = chess.get(raw.get('goldenId'))
        skills = skills_of(raw)
        elite_skills = skills_of(golden) if golden else []
        if elite_skills: skills = [s for s in skills if s in elite_skills]
        default_skill = next((s['index'] for s in raw.get('skills', []) if s.get('isDefault') and s.get('index') in range(10)), (raw.get('skill') or {}).get('index', -1))
        if default_skill not in skills and skills: default_skill = skills[0]
        modules, default_module = [], ''
        if golden:
            modules = list(dict.fromkeys(m['uniEquipId'] for m in golden.get('modules', []) if m.get('uniEquipId') and m['uniEquipId'] != 'none'))
            mod = golden.get('module') or {}
            if 'modules' not in golden and mod.get('active') and mod.get('id'): modules = [mod['id']]
            default_module = next((m['uniEquipId'] for m in golden.get('modules', []) if m.get('isDefault') and m.get('uniEquipId')), None)
            if default_module is None and mod.get('active') and mod.get('id') in modules: default_module = mod['id']
            default_module = default_module or 'none'
            modules.append('none')
        backup = raw.get('backup') or {}
        ownable = not raw['isDiy'] and raw.get('chessType') == 'NORMAL' and bool(backup.get('charId')) and backup['charId'] != raw.get('charId') and raw['baseId'] == key
        selectable = raw.get('visible') is not False and not raw.get('isHidden') and not raw['isDiy'] and raw['baseId'] == key
        rules.append(record('RosterRule', MyId=quote(key), MyGoldenId=quote((golden or {}).get('chessId', '')), MySelectable=boolean(selectable), MyOwnable=boolean(ownable),
            MySkills=tables.array('int', [str(s) for s in skills]), MyDefaultSkill=str(default_skill), MyModules=strings(modules), MyDefaultModule=quote(default_module)))
    rules_table = tables.array('RosterRule', rules)
    slots_table = tables.array('DiySlotRule', [record('DiySlotRule', MyId=quote(k), MyGoldenId=quote(v['goldenId']), MyTier=str(v['tier']), MyShopLevel=str(v.get('shopLevel') or v['tier'])) for k, v in backups['diy']['slots'].items()])
    excluded = sorted({m['uniEquipId'] for u in backups['units'].values() for f in u['forms'].values() for m in f.get('modules', []) if re.match(r'^(?:ISW|SO|RA)-', m.get('typeName', ''))})
    excluded_table = strings(excluded)

    def count(value, fallback=1):
        return min(value, 9) if isinstance(value, int) and value > 0 else fallback

    def available(sources): return 'talent' in sources or 'skill' in sources

    supplies = []
    for key, raw in sorted(tokens.items()):
        if raw.get('kind') != 'summon' or raw.get('placeable') is not True: continue
        variants = []
        for owner, variant in raw.get('variants', {}).items():
            total = count(variant.get('stats', {}).get('deployLimit'), count(raw.get('deployLimit')))
            skills = [record('TokenSkillSource', MySkill=str(int(si)), MyAvailable=boolean(available(alt['sources']))) for si, alt in variant.get('bySkill', {}).items() if isinstance(alt.get('sources'), list)]
            modules = [record('TokenModuleCount', MyModule=quote(mid), MyCount=str(count(mod.get('stats', {}).get('deployLimit'), total))) for mid, mod in variant.get('byModule', {}).items()]
            variants.append(record('TokenSupplyVariant', MyOwner=quote(owner), MyAvailable=boolean(available(variant.get('sources', []))), MyCount=str(total),
                MySkills=tables.array('TokenSkillSource', skills), MyModules=tables.array('TokenModuleCount', modules)))
        supplies.append(record('TokenSupplyRule', MyId=quote(key), MyOwnerRange=boolean(raw.get('ownerRange', False)), MyOwnerRangeOutside=boolean(raw.get('ownerRangeOutside', False)), MyCount=str(count(raw.get('deployLimit'))), MyVariants=tables.array('TokenSupplyVariant', variants)))
    supplies_table = tables.array('TokenSupplyRule', supplies)
    lines = ['// Generated preparation roster metadata; edit tools/generate_player_roster.py.', '#include <array>', '#include <stronghold/adapters/reference_roster.hpp>', 'namespace Stronghold { namespace {', *tables.lines, '}',
        f'std::span<const RosterRule> ReferenceRosterRules() noexcept {{ return {rules_table}; }}',
        f'std::span<const DiySlotRule> ReferenceDiySlots() noexcept {{ return {slots_table}; }}',
        f'std::span<const std::string_view> ReferenceExcludedDiyModules() noexcept {{ return {excluded_table}; }}',
        f'std::span<const TokenSupplyRule> ReferenceTokenSupplies() noexcept {{ return {supplies_table}; }}', '}']
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text('\n'.join(lines) + '\n', encoding='utf-8')


if __name__ == '__main__': main()
