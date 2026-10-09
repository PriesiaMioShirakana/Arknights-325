"""Static wave records, including branch actions and per-level enemy ability overrides.

The tables keep selection metadata. Weighted packs/slot substitution belong to the match layer,
not to spawnsFromTemplate's expansion. No template action is silently turned into an enemy.
"""
from copy import deepcopy
from generate_combat import boolean, enemy, number, quote, record


def point(pair):
    return record('WorldPoint', MyX=number(pair[1]), MyY=number(pair[0]))


def route(tables, raw):
    steps, patrol = [], []
    source = raw.get('steps') or raw.get('checkpoints', [])
    for cp in source:
        if isinstance(cp, list):
            kind, pos, duration = 'MOVE', cp, 0
        else:
            kind = cp.get('t', cp.get('type', 'MOVE')).upper()
            pos, duration = cp.get('p', cp.get('pos', [0, 0])), cp.get('s', cp.get('time', 0))
        if kind == 'PATROL':
            patrol.append(str(len(steps)))
            kind = 'MOVE'
        if kind not in {'MOVE', 'WAIT', 'DISAPPEAR', 'APPEAR'}:
            raise ValueError(f'unsupported route step {kind}')
        steps.append(record('RouteStep', MyKind='RouteStepKind::'+kind, MyPosition=point(pos), MyWaitSeconds=number(duration)))
    return record('WaveRoute', MyStart=point(raw['start']), MyEnd=point(raw['end']),
        MySteps=tables.array('RouteStep', steps), MyFlying=boolean(raw['motion'] == 'FLY'),
        MyPatrolSteps=tables.array('std::size_t', patrol), MySpawnRandom=point(raw.get('spawnRandom', [0, 0])))


def group(raw):
    return record('WaveSpawnGroup', MyTime=number(raw['time']), MyEnemyId=quote(raw['key']),
        MyCount=str(max(1, int(raw['count']))), MyInterval=number(raw['interval']), MyRoute=str(raw['routeIndex']),
        MySlot=quote(raw.get('slot', '')), MyTag='EnemySpawnTag::'+raw.get('tag', 'NONE').upper(),
        MyGroup=quote(raw.get('group', '')), MyPack=quote(raw.get('pack', '')), MyWeight=number(raw.get('weight', 1)),
        MyUnharmful=boolean(raw.get('unharmful', False)), MyAction='WaveAction::'+raw.get('action', 'SPAWN'))


def override(base, patch):
    """battle/spawns.js changes stats; enemies/helpers.js abOf merges talent keys, replaces skills."""
    result = deepcopy(base)
    result.setdefault('stats', {}).update(patch.get('stats', {}))
    for board in ['bb', 'bbStr']:
        result.setdefault('talents', {}).setdefault(board, {}).update(patch.get('talents', {}).get(board, {}))
    if 'skills' in patch:
        result['skills'] = patch['skills']
    return result


def wave(tables, key, raw, enemies):
    branches = []
    for branch_key, phases in sorted(raw['branches'].items()):
        branches.append(record('WaveBranch', MyId=quote(branch_key), MyPhases=tables.array('WaveBranchPhase', [
            record('WaveBranchPhase', MyActions=tables.array('WaveSpawnGroup', [group(x) for x in phase])) for phase in phases])))
    overrides = [enemy(tables, enemy_key, override(enemies[enemy_key], patch)) for enemy_key, patch in sorted(raw['overrides'].items())]
    devices = [record('WaveDevice', MyId=quote(d['key']), MyAlias=quote(d['alias']), MyPosition=point(d['pos']),
        MyFacing='Facing::'+d['dir'], MyHidden=boolean(d.get('hidden', False))) for d in raw['devices']]
    used = [record('WaveUse', MyModeId=quote(u['modeId']), MyRound=str(u['round']), MyBossId=quote(u.get('bossId') or '')) for u in raw['usedBy']]
    return record('WaveRecord', MyId=quote(key), MyKind='WaveKind::'+raw['kind'].upper(), MySolo=boolean(raw['solo']),
        MyBossId=quote(raw.get('bossId') or ''), MyMaxPlayTime=number(raw['maxPlayTime']),
        MyInitialDp=number(raw['dp']['init']), MyDpPerSecond=number(raw['dp']['perSec']), MyMaxDp=number(raw['dp']['max']),
        MyCharacterLimit=str(raw['characterLimit']), MyMoveMultiplier=number(raw['moveMultiplier']), MyMusic=quote(raw['bgm']),
        MyRoutes=tables.array('WaveRoute', [route(tables, r) for r in raw['routes']]),
        MyExtraRoutes=tables.array('WaveRoute', [route(tables, r) for r in raw['extraRoutes']]),
        MySpawns=tables.array('WaveSpawnGroup', [group(s) for s in raw['spawns']]),
        MyBranches=tables.array('WaveBranch', branches), MyEnemyOverrides=tables.array('EnemyRecord', overrides),
        MyDevices=tables.array('WaveDevice', devices), MyTotalCount=str(raw['totalCount']),
        MySlotCounts=tables.blackboard(raw['slotCounts']), MyUsedBy=tables.array('WaveUse', used))
