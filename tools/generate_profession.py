"""Value-only part of sim/professions.js resolveProfile; stateful trait hooks are installed separately."""
import re
from generate_combat import record, number, boolean


def profile(raw, token=False):
    profession=(raw.get('profession') or ('TOKEN' if token else 'WARRIOR')).upper();sub=raw.get('subProfessionId') or raw.get('subProf') or ''
    trait=raw.get('trait') or {};text=trait if isinstance(trait,str) else trait.get('desc') or '';bb=trait.get('bb',{}) if isinstance(trait,dict) else {}
    num=lambda k,d:bb.get(k,d)
    ranged=profession in ('SNIPER','CASTER','MEDIC','SUPPORT')
    damage='arts' if profession in ('CASTER','SUPPORT') else 'heal' if profession=='MEDIC' else 'phys'
    projectile='arrow' if profession=='SNIPER' else 'bolt' if damage=='arts' else 'orb' if damage=='heal' else 'none'
    fly=ranged;disabled=False;only=False;all_range=False;ground=False;healing=profession=='MEDIC'
    priority='FLYING' if sub=='fastshot' else 'LOW_DEFENSE' if sub=='longrange' else 'DEFAULT'
    splash=0;scale=1;hits=2 if sub in ('sword','swordmaster') else 1
    chain_count=heal_chain=1;chain_falloff=0.15;heal_falloff=0.25;sluggish=element_heal=0;heal_targets=1
    scaling='NONE';conditional=far=1;status=None;status_duration=status_value=0
    if sub in ('aoesniper','bombarder','fortress'):
        splash={'aoesniper':1.1,'bombarder':0.9,'fortress':1.0}[sub];projectile='bomb'
        if sub!='aoesniper':ground=True;fly=False
    if sub=='splashcaster':splash=1.0 if raw.get('charId')=='char_253_greyy' else 1.1
    if sub=='funnel':projectile='drone'
    if sub=='loopshooter':projectile='boomerang'
    if sub in ('reaperrange','reaper','stalker'):all_range=True
    if sub=='phalanx':only=True
    if sub=='librator':only=True
    if sub=='bard':disabled=True;damage='heal';healing=False
    if sub=='incantationmedic':damage='arts';projectile='bolt';healing=False
    if sub=='craftsman':ranged=False;damage='phys';projectile='none';fly=False
    if sub=='shotprotector':ranged=True;fly=True;projectile='arrow'
    if sub=='hammer':splash=num('attack@ability_range_radius',1);scale=num('attack@atk_scale_2',0.5)
    if sub in ('lord','agent','hookmaster','skywalker'):fly=True
    if sub=='artsfghter':damage='arts'
    if sub=='alchemist':ranged=True;fly=True;projectile='lob'
    if sub=='traper':ranged=True;fly=False;projectile='none'
    if sub=='tactician':scaling='REINFORCEMENT';conditional=num('atk_scale',1.5)
    if sub=='hunter':scaling='HUNTER';conditional=num('atk_scale',1.2)
    if sub=='funnel':scaling='FUNNEL'
    if sub=='fastshot':scaling='FLYING';conditional=num('atk_scale',1)
    if sub=='instructor':scaling='UNBLOCKED';conditional=num('atk_scale',1.2)
    if sub=='lord':scaling='DISTANT';conditional=num('atk_scale',0.8)
    if sub=='reaperrange':scaling='FRONT';conditional=num('atk_scale',1.5)
    if sub=='healer':far=num('heal_scale',0.8)
    if sub=='wandermedic':element_heal=num('ep_heal_ratio',0.5)
    if sub=='ringhealer':heal_targets=3
    if sub=='chainhealer':
        count=re.search(r'在(\d+)个友方单位间跳跃',text);falloff=re.search(r'治疗量降低(\d+)%',text)
        heal_chain=num('attack@chain.max_target',int(count[1]) if count else 3)
        heal_falloff=1-num('attack@chain.atk_scale',1-(int(falloff[1])/100 if falloff else 0.25))
    if sub=='chain':
        count=re.search(r'在(\d+)个敌人间跳跃',text);falloff=re.search(r'伤害降低(\d+)%',text)
        chain_count=num('attack@max_target',int(count[1]) if count else 3)
        chain_falloff=1-num('attack@chain.atk_scale',1-(int(falloff[1])/100 if falloff else 0.15))
        sluggish=num('attack@sluggish',0.5)
    if sub=='slower':status='SLUGGISH';status_duration=num('sluggish',0.8)
    if sub=='underminer' and 'atk' in bb and 'duration' in bb:status='WEAKEN';status_duration=num('duration',2);status_value=abs(num('atk',0.1))
    # Authored attack fields override the branch's defaults in the same order as resolveProfile.
    if token:
        desc=raw.get('description',raw.get('desc',text)) or ''
        damage=raw.get('dmgType') or ('arts' if '法术' in desc else 'true' if '真实' in desc else 'heal' if '恢复' in desc or '治疗' in desc else 'phys')
    elif raw.get('dmgType'):damage=raw['dmgType']
    attack=(raw.get('attackKind') or '').lower()
    if attack in ('ranged','melee'):
        ranged=attack=='ranged';projectile=raw.get('projectile') or projectile
        fly=raw['canHitFly'] if raw.get('canHitFly') is not None else ranged or fly
    elif attack=='heal':ranged=True;damage='heal'
    elif attack=='none' and not disabled and not only:only=True
    if raw.get('targetPriority'):priority={'FLY_FIRST':'FLYING','LOW_DEF':'LOW_DEFENSE','RANGED':'RANGED','fly':'FLYING','lowestDef':'LOW_DEFENSE','lowDef':'LOW_DEFENSE','ranged':'RANGED','lowestHp':'LOWEST_HEALTH','highestHp':'HIGHEST_HEALTH','nearest':'NEAREST','farthest':'FARTHEST'}[raw['targetPriority']]
    if not token and raw.get('splashRadius',0)>0:splash=raw['splashRadius']
    if sub=='loopshooter' and ranged:projectile='boomerang'
    if token:
        if damage=='heal':healing=True
        if (raw.get('stats') or {}).get('atk',0)<=0:disabled=True
    if damage=='heal' and not healing and not disabled:healing=True
    if damage!='heal':healing=False
    if damage=='none':disabled=True
    if ground:fly=False
    if sub in ('phalanx','blastcaster'):
        all_range=True
        if ranged:projectile='beam'
    speed=0 if projectile in ('none','beam') else {'arrow':14,'bolt':11,'orb':10,'drone':16,'bomb':8,'lob':8,'boomerang':15,'droneBomb':5}.get(projectile,12)
    return record('AttackProfile',MyDamageType='DamageType::'+{'phys':'PHYSICAL','arts':'ARTS','true':'TRUE_DAMAGE','heal':'PHYSICAL','none':'PHYSICAL'}[damage],
        MyDisabled=boolean(disabled),MyHealing=boolean(healing),MyCanHitFlying=boolean(fly),MyBlockFlying=boolean(sub=='skywalker'),MyRanged=boolean(ranged),
        MyMaxTargets=str(int(max(1,heal_targets if healing else 1))),MyPriority='TargetPriority::'+priority,MyProjectileSpeed=number(speed),
        MyGroundOnly=boolean(ground),MyNoHeal=boolean(sub in ('unyield','musha','reaper')),MyHits=str(hits),MyAllInRange=boolean(all_range),
        MySplashRadius=number(splash),MySplashScale=number(scale),MyChainCount=str(int(max(1,chain_count))),MyChainFalloff=number(chain_falloff),MyChainSluggish=number(sluggish),
        MyHealChainCount=str(int(max(1,heal_chain if healing else 1))),MyHealChainFalloff=number(heal_falloff),MyElementHealRatio=number(element_heal if healing else 0),
        MyOnHitStatus='CombatStatus::'+status if status else '{}',MyOnHitApplication=record('StatusApplication',MyDuration=number(status_duration),MyValue=number(status_value) if status=='WEAKEN' else '{}'),
        MyOnlyDuringSkill=boolean(only),MyHitAllBlocked=boolean(sub in ('centurion','crusher','pusher')),MyScaling='AttackScaling::'+scaling,
        MyConditionalScale=number(conditional),MyHealFarMultiplier=number(far), MyBoomerang=boolean(sub=='loopshooter'), MyFortress=boolean(sub=='fortress'))


def traits(raw, doll_health=1):
    """Emit only branch-specific values; defaults stay constexpr in the C++ definition.

    This avoids repeating the whole definition thousands of times in the generated MSVC input.
    Stateful scripts not listed here are still outstanding, never silently mapped to another branch.
    """
    sub=raw.get('subProfessionId') or raw.get('subProf') or ''
    trait=raw.get('trait') or {};bb=trait.get('bb',{}) if isinstance(trait,dict) else {}
    kinds={'hunter','funnel','mystic','phalanx','bearer','stalker','musha','reaper','incantationmedic','charger','geek','merchant','librator','bard','loopshooter','bombarder','tactician','skywalker','dollkeeper'}
    if sub not in kinds:return '{}'
    fields={}
    if sub=='hunter':fields=dict(MyAmmoMax=number(bb.get('value',8)))
    if sub=='funnel':fields=dict(MyFunnelInitial=number(bb.get('init_atk_scale',0.2)),MyFunnelDelta=number(bb.get('delta_atk_scale',0.15)),MyFunnelMax=number(bb.get('max_atk_scale',1.1)))
    if sub=='mystic':fields=dict(MyStoreMax=str(max(0,int(bb.get('times',3)))))
    if sub=='phalanx':fields=dict(MyGuardDefense=number(bb.get('def',2)),MyGuardResistance=number(bb.get('magic_resistance',20)))
    if sub=='stalker':fields=dict(MyDodge=number(bb.get('prob',0.5)))
    if sub in ('musha','reaper'):fields=dict(MySelfHeal=number(bb.get('value',50)))
    if sub=='incantationmedic':fields=dict(MyHealRatio=number(bb.get('scale',0.5)))
    if sub=='charger':fields=dict(MyDpOnKill=number(bb.get('cost',1)))
    if sub=='geek':fields=dict(MyHpDrain=number(bb.get('hp_ratio',0.03)))
    if sub=='merchant':fields=dict(MyMerchantInterval=number(bb.get('interval',3)),MyMerchantCost=number(abs(bb.get('cost',-3))))
    if sub=='librator':fields=dict(MyRampMax=number(bb.get('atk',2)),MyRampTime=number(bb.get('max_stack_cnt',40)),MyRampInitial=number(bb.get('init_atk',0)))
    if sub=='bombarder':fields=dict(MyShockScale=number(bb.get('attack@append_atk_scale',0.5)),MyShockCount=str(max(1,int(bb.get('attack@times',2)-1))))
    if sub=='dollkeeper':fields=dict(MyDollDuration=number(bb.get('duration',20)),MyDollHealthMultiplier=number(doll_health),MyDollNoAttack=boolean(bb.get('dollNoAttack',False)))
    if sub=='bard':fields=dict(MyAuraRatio=number(bb.get('attack@atk_to_hp_recovery_ratio',0.1)))
    return record('ProfessionDefinition',MyKind='ProfessionTrait::'+('INCANTATION' if sub=='incantationmedic' else sub.upper()),**fields)
