"""Build-time adapter for the pinned reference JSON. No third-party packages required."""
import argparse
import hashlib
import json
from pathlib import Path


def board_tiles(stage, field):
    """Static buildDeployMap: map legend plus initially active devices, in board reading order."""
    tiles = {}
    for row in range(12, 8, -1):
        for column in range(2, 11):
            sr = row if field == 'normal' else row - 7
            sc = 20 - column if field == 'bossR' else column
            line = stage.get('rows', [])[sr]
            tile = stage.get('tiles', {}).get(line[sc], {})
            height, buildable = tile.get('height'), tile.get('buildable')
            terrain = 'BLOCKED'
            if height == 'LOW' and buildable in ('ALL', 'MELEE'):
                terrain = 'GROUND'
            elif (height == 'HIGH' and buildable in ('ALL', 'RANGED')) or (height == 'LOW' and buildable == 'RANGED'):
                terrain = 'HIGH'
            tiles[row, column] = terrain
    for device in stage.get('devices', []):
        if not device.get('active', not device.get('hidden', False)):
            continue
        row, column = device['pos']
        if field != 'normal':
            row += 7
        if field == 'bossR':
            column = 20 - column
        role = device.get('role')
        terrain = {'crate': 'BLOCKED', 'mound': 'BLOCKED', 'platform': 'HIGH', 'waterPlatform': 'GROUND'}.get(role)
        if (row, column) in tiles and terrain:
            tiles[row, column] = terrain
    return '{{' + ','.join('Terrain::' + value for value in tiles.values()) + '}}'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    raw = {name: (args.data / (name + '.json')).read_bytes() for name in ('config', 'chess', 'items', 'stages')}
    config, chess, items = (json.loads(raw[name]) for name in ('config', 'chess', 'items'))
    economy = config['economy']
    quote = lambda value: json.dumps(value, ensure_ascii=True)
    boolean = lambda value: 'true' if value else 'false'
    rows = []
    # Preserve hidden records and DIY templates as data, but never put them in the ordinary shop.
    for kind, records in (('CHESS', chess), ('ITEM', items)):
        for identity, record in sorted(records.items()):
            tier = record['tier']
            golden = record.get('isGolden', False)
            base = record.get('baseId') or identity
            target = record.get('upgradeChessId') or record.get('goldenId') or ''
            merge = 0
            if not golden and target in records and records[target].get('isGolden'):
                if kind == 'CHESS':
                    merge = record.get('upgradeNum') or economy.get('mergeCountOverrides', {}).get(identity, economy['mergeCount'])
                elif record.get('mergeable'):
                    merge = record.get('upgradeNum') or economy['itemMergeCount']
            if kind == 'CHESS':
                eligible = record.get('visible') and not golden and not record.get('isDiy') and not record.get('isHidden')
                copies = economy.get('poolCopiesOverrides', {}).get(base, economy['poolCopies'][str(tier)])
                price = record.get('price', economy['chessPrice'][str(tier)]['golden' if golden else 'normal'])
                sell = record.get('sellPrice', 1)
            else:
                eligible = not golden and not record.get('hideInShop') and not record.get('shopExcluded') and record.get('itemType') == 'EQUIP'
                copies, price, sell = 0, record.get('price', 2), 0
            trait = record.get('traitBase') or record.get('trait') or {}
            widened = record.get('position') == 'MELEE' and '\u53ef\u4ee5\u653e\u7f6e\u4e8e\u8fdc\u7a0b\u4f4d' in trait.get('desc', '')
            placement = 'HIGH_ONLY' if record.get('rangedTilesOnly') else ('MELEE' if str(record.get('position', '')).upper() == 'MELEE' and not widened else 'ANY')
            values = [quote(identity), quote(base), quote(target), 'PieceKind::' + kind,
                str(tier), str(price), str(sell), str(copies), str(merge), boolean(golden), boolean(eligible),
                'PlacementClass::' + placement, 'ItemUse::' + ('ART' if record.get('itemType') == 'MAGIC' else 'CONSUME_ON_EQUIP' if str(record.get('kind', '')).startswith('consume_on_equip') else 'EQUIPMENT')]
            values.append(boolean(record.get('isDiy', False)))
            fields = ['MyId', 'MyBaseId', 'MyGoldenId', 'MyKind', 'MyTier', 'MyPrice', 'MySellPrice', 'MyPoolCopies', 'MyMergeCount', 'MyGolden', 'MyShopEligible', 'MyPlacement', 'MyItemUse', 'MyRequiresSelection']
            rows.append('Definition{' + ', '.join('.' + name + ' = ' + value for name, value in zip(fields, values)) + '}')
    lines = ['// Generated from reference JSON. Edit the generator, not this file.',
             '#include <stronghold/adapters/reference_catalog.hpp>',
             'namespace Stronghold {',
             'Catalog ReferenceCatalog() {',
             'std::vector<Definition> definitions{' + ',\n'.join(rows) + '};',
             'std::map<std::string, EconomyRules, std::less<>> modes;']
    reward = economy['rewardOffer']
    for identity, mode in sorted(config['modes'].items()):
        lines.append('{ EconomyRules rules;')
        fields = {'MyIncome': '{' + ','.join(map(str, economy['income'])) + '}',
                  'MyIncomeCap': economy['incomeCap'], 'MyRefreshPrice': economy['refreshPrice'],
                  'MyGoldenCopies': economy['goldenCopies'], 'MyMaxLevel': mode['maxShopLevel'],
                  'MyUpgrades': '{' + ','.join(map(str, mode['upgradePrices'])) + '}',
                  'MyHandSize': economy['benchSize'], 'MyTemporarySize': economy.get('tempSize', 5), 'MyEquipmentPerChess': economy.get('equipPerChess', 2), 'MyDeployCap': economy['deployCap'], 'MyRewardCount': reward['count'],
                  'MyRewardTierOffset': reward['tierOffset'], 'MyRewardMaxTier': reward['maxTier'],
                  'MyRewardPrice': reward['price'], 'MyMaxArtsPerRound': economy.get('maxArtsPerRound', 2)}
        for name, value in fields.items():
            lines.append(f'rules.{name} = {value};')
        for level, layout in mode['shopSlots'].items():
            lines.append(f'rules.MyLayouts[{int(level)-1}] = {{{layout["chess"]}, {layout["item"]}}};')
        lines.append(f'modes.emplace({quote(identity)}, std::move(rules)); }}')
    fingerprint = hashlib.sha256(b''.join(raw.values())).hexdigest()
    lines += ['return Catalog(std::move(definitions), std::move(modes));', '}',
              'const std::map<std::string, StageBoards, std::less<>>& ReferenceStages() {',
              'static const std::map<std::string, StageBoards, std::less<>> Stages{']
    for identity, stage in sorted(json.loads(raw['stages']).items()):
        layouts = ','.join(board_tiles(stage, field) for field in ('normal', 'bossL', 'bossR'))
        lines.append('{' + quote(identity) + ', {' + layouts + '}},')
    lines += ['}; return Stages; }', f'std::string_view ReferenceDataFingerprint() noexcept {{ return "{fingerprint}"; }}', '}']
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text('\n'.join(lines) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
