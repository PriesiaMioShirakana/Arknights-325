"""Compile the pinned stage legends and device data, matching grid.js/devices.js.

Device overrides remain runtime input. Raw map tiles are never changed for a specific player here.
"""
import math
import re
from generate_combat import record, quote, number, boolean


def stage(tables, key, raw):
    tiles = []
    terrain_names = {'tile_mire':'MIRE', 'tile_smog':'SMOG', 'tile_deepsea':'DEEPSEA', 'tile_deepwater':'DEEPSEA', 'tile_infection':'INFECTION'}
    if len(raw['rows']) != 19 or any(len(row) != 21 for row in raw['rows']):
        raise ValueError('invalid stage dimensions: '+key)
    for row in raw['rows']:
        for glyph in row:
            tile = raw['tiles'][glyph]
            tile_key = tile['tileKey']
            build = 'NONE' if tile_key == 'tile_deepsea' else tile['buildable']
            tiles.append(record('FieldTile', MyWalkable=boolean(tile['groundPassable']), MyFlyable=boolean(tile['flyPassable']),
                MyLow=boolean(tile['height']=='LOW'), MyBuild='FieldBuild::'+build, MyGoal=boolean(tile_key=='tile_end'),
                MyTerrain='FieldTerrain::'+terrain_names.get(tile_key, 'NONE')))
    devices = []
    special = raw.get('special') or {}
    for device in raw['devices']:
        st = device.get('stats') or {}
        skill = device.get('skill') or {}
        bb = skill.get('bb', {})
        point = device['pos']
        flow = '{}'
        if device['role'] == 'blower':
            bb = skill.get('bb', special.get('blower', {}).get('bb', {}))
            x,y = {'UP':(0,1),'RIGHT':(1,0),'DOWN':(0,-1),'LEFT':(-1,0)}[device.get('dir', 'UP')]
            flow = record('Airflow', MyX=str(x), MyY=str(y),
                MyAllyEqualAttack=number(bb.get('blower_s_character[equal].atk',0)),
                MyAllyOppositeAttack=number(bb.get('blower_s_character[opposite].atk',0)),
                MyAllyVerticalAttack=number(bb.get('blower_s_character[vertical].atk',0)),
                MyEnemyEqualSpeed=number(bb.get('blower_s_enemy[equal].move_speed',0)),
                MyEnemyOppositeSpeed=number(bb.get('blower_s_enemy[opposite].move_speed',0)))
        duration = re.search(r'持续(\d+(?:\.\d+)?)秒', skill.get('desc') or skill.get('description') or '')
        devices.append(record('StageDeviceRecord', MyId=quote(device['key']), MyAlias=quote(device.get('alias') or ''),
            MyRole=quote(device['role']), MyPosition=record('FieldPoint', MyRow=str(point[0]), MyColumn=str(point[1])),
            MyActive=boolean(device.get('active', not device.get('hidden',False))),
            MyStats=record('CombatStats', MyMaxHealth=number(st.get('maxHp',100)), MyAttack=number(st.get('atk',0)),
                MyDefense=number(st.get('def',0)), MyResistance=number(st.get('res',0)), MyAttackSpeed=number(st.get('aspd',100)),
                MyBaseAttackTime=number(st.get('bat',1)), MyBlockCount=str(st.get('blockCnt',0))),
            MyRange=tables.array('FieldPoint', [record('FieldPoint', MyRow=str(r), MyColumn=str(c)) for r,c in device.get('rangeTiles') or []]),
            MySkill=tables.blackboard(bb), MyAirflow=flow, MyFragilityDuration=number(duration.group(1) if duration else 0)))
    def device_skill(role):
        return next((d.get('skill',{}).get('bb',{}) for d in raw['devices'] if d['role']==role), {})
    def aspd(value):
        return value*100 if abs(value)<1 else value
    mire=special.get('mire',{}); mdev=device_skill('mireController')
    deep=special.get('deepsea',{}).get('bb',device_skill('tideController'))
    infection=special.get('infection',{}).get('bb',{})
    rules=record('TerrainRules', MyMireInterval=number(mire.get('intervalSec',1) if mire.get('intervalSec',1)>0 else 1),
        MyMireAttackSpeed=number(aspd(mire.get('aspdPerStack',mdev.get('attack_speed',-.05)))),
        MyMireMovePerStack=number(mire.get('moveMulPerStack',mdev.get('move_speed',-.05))),
        MyMireMaxStacks=str(max(1,math.floor(mire.get('maxStacks',mdev.get('max_stack_cnt',10))))),
        MyMireHeavyMass=number(mire.get('heavyWeight',mdev.get('value',3))),
        MyDeepseaDamage=number(deep.get('sea_drown[enemy].damage',40)),
        MyDeepseaAttackSpeed=number(aspd(deep.get('sea_drown[enemy].attack_speed',-.6))),
        MyDeepseaMoveMultiplier=number(deep.get('sea_drown[enemy].move_speed',.6)),
        MyInfectionDamage=number(infection.get('damage',70)), MyInfectionAttackPercent=number(infection.get('atk',.2)),
        MyInfectionAttackSpeed=number(aspd(infection.get('attack_speed',20))),
        MyInfectionDuration=number(infection.get('duration',300) if infection.get('duration',300)>0 else 300))
    return record('StageRecord', MyId=quote(key), MyName=quote(raw['name']),
        MyTiles=tables.array('FieldTile',tiles), MyRules=rules, MyDevices=tables.array('StageDeviceRecord',devices))
