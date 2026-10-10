# 战斗组件接口

本轮实现尚未运行生成器、构建或测试。历史回归结果不代表此次重构已验证；完整内容迁移仍按 `migration-progress.md` 的待办继续。

## 技能与干员

技能、干员、选择器／效果、注册表和出怪计划的公开接口分别位于 `skill.hpp`、`operator.hpp`、`effects.hpp`、`component_registry.hpp` 和 `wave_plan.hpp`；按需包含相应头文件。

`SkillDefinition` 保存技力费用、回复方式、触发条件、范围、属性修正、弹药和持续时间。`SkillBase` 执行共同状态机，外部通过 `battle.Skill(id)` 调用 `Start`、`End`、`GainSp`、`SetSpTotal`、`AddAmmo`、`Extend`、`Range` 和 `SetTrigger`。持续技能与普通技力回复保持原有顺序；收尾效果先执行，再清除技能属性与范围。

派生技能可覆盖 `CanActivate`、`BeforeStart`、`OnStart`、`AfterStart`、`OnEnding`、`OnEnd`、`OnTick`。内置技能对象按值存放；原有专属启动规则暂由私有 `BuiltinSkill` 连接。`MyStartEffects`、`MyTickEffects`、`MyEndingEffects` 和 `MyEndEffects` 可以直接引用共享效果程序。

`OperatorBase` 提供状态、定义、持有者、技能、攻击、部署、退场和移动接口，通过 `battle.Operator(id)` 获取。注册的派生干员可覆盖部署、受伤、死亡、时间推进及通用事件钩子。已有干员的专属状态与规则仍在私有实现内，未把全部角色机械转换成独立派生类。

由战场取得的组件地址保持稳定；动态召唤和移动外层 `Battle` 不会改变这些组件的战场引用。注册工厂接收的是内部稳定的战场视图，不应复制、移动或替换回调收到的 `Battle`。引用在拥有该战场的对象析构后失效。

## 选择目标与执行操作

`EffectProgram` 包含有序的 `EffectStep`；每步用一个 `SelectorDefinition` 选取目标，然后对每个目标按顺序执行 `EffectOperation`。内置操作通过 `std::variant` 静态分派，自定义选择器和操作只在对应自定义分支调用注册实例。

选择器提供地形、敌人、友军、同一持有者的单位、队友单位、玩家、持有者、队友，以及自身和当前事件参与者等目标来源。单位、玩家和地形目标采用不同标签，操作只接受对应类别。玩家选择器默认排除虚拟玩家，持有者选择器仍能选中单位的虚拟持有者；也可显式打开 `MyIncludeVirtualPlayers`。

范围可以是全场、来源射程、绝对格掩码或圆形半径；来源射程及友军坐标遵循开局的初始／实时位置设置。敌人范围判定包含身体覆盖的格子。选择器支持存活、隐藏、飞行、可选中、生命比例、职业、国家和单位类别条件，以及稳定顺序、射程格顺序、攻击优先级等排序。

共用操作包含伤害、治疗、生命流失、技力、状态、Buff、护盾、元素损伤、DP／金币／盟约层数、地形障碍、推拉和技能弹药／时间／充能。`EventOperation` 还能在伤害、治疗、状态、技力及层数的提交前钩子修改数值或取消事件；应使用单个事件参与者或持有者目标，避免对同一事件重复结算。延迟任务不保留原事件指针，提交后的结果事件也不能倒改既有结算。`EffectAmount` 可以组合固定值、来源攻击／防御／最大生命、目标最大生命及事件数值；`MyPerSecond` 使用当前上下文的时间增量。

```cpp
#include <stronghold/simulation/battle.hpp>
#include <stronghold/simulation/effects.hpp>

Stronghold::EffectProgram burst{
    .MySteps = {
        Stronghold::EffectStep{
            .MySelector = {
                .MyKind = Stronghold::SelectorKind::ENEMIES,
                .MyRange = Stronghold::SelectorRange::SOURCE},
            .MyOperations = {
                Stronghold::DamageOperation{
                    .MyAmount = {.MySourceAttack = 1.8},
                    .MyDamage = {
                        .MyType = Stronghold::DamageType::ARTS,
                        .MyTags = static_cast<Stronghold::DamageTags>(Stronghold::DamageTag::SKILL),
                        .MyIsSkill = true}}}}}};
```

把程序交给 `EffectExecutor::Execute(battle, context, burst)`，或设置到 `SkillDefinition::MyStartEffects`。相同程序也能挂到盟约事件机制、模组的 `CombatDefinition::MyMechanisms` 或 Buff 生命周期。程序指针、单位定义中的机制 span、借用的字符串与范围表均须覆盖战场生命周期。

## Buff、事件机制与注册

Buff 保留已有叠层、刷新、独立寿命和强者覆盖规则。新增 `MyApplyProgram`、`MyTickProgram`、`MyExpireProgram`、`MyRemoveProgram`，与旧的持续效果数组共用操作执行器；Buff 接受者作为上下文的事件单位和显式目标传入。

`MechanismDefinition` 将事件、来源／持有者／全局作用域、可选目标条件、触发间隔、延迟与效果程序组合起来。`MyDuringSkill` 限制到技能活动期，`MyRequiresSource` 限制来源在场；标为 `MyGarrison` 的机制遵循来源退场后的战斗特质开关。授予接受者的特质保留仍由既有独立开关管理。机制执行期间不重复进入自身，延迟任务保存目标 ID，并检查来源部署版本和技能激活版本。

通过 `battle.AttachMechanism(unit, definition, playerId)` 或注册引用安装机制，返回的稳定句柄可交给 `RemoveMechanism`；取消也阻止尚未执行的延迟任务。`BATTLE_END` 机制可以执行结算资源操作，战斗伤害等接口继续遵循原有结束状态限制。

`ComponentRegistry` 提供 `RegisterSkill<T>`、`RegisterOperator<T>`、`RegisterSelector<T>`、`RegisterOperation<T>`、`RegisterBuff`、`RegisterMechanism`。注册后 `Seal`，通过 `BattleInput::MyComponentRegistry` 借给战场；注册表须覆盖战场生命周期且不得移动。引用分类型校验，自定义技能和干员每单位独立实例化，自定义选择器和操作每战场实例化一次。现有 `ContentRegistry` 的干员、敌人、Buff、盟约和盟约效果注册接口继续保留。

## 持有者与未来出怪模式

`BattlePlayerKind` 区分真实参与者和虚拟玩家。友军单位必须归属到某个持有者；无指定持有者的装置、独立召唤物使用系统创建的环境虚拟玩家。宿主也可在 `BattlePlayerInput` 中提供虚拟玩家与初始单位。`MyPrincipalId` 可将虚拟玩家的贡献关联到一个真实参与者，队伍随该参与者继承，同一持有者判定仍区分两者。

`Players()` 返回真实参与者，`Owners()` 返回全部持有者，`UnitOwner()` 返回指定单位的实际持有者。真实玩家与虚拟玩家分别出现在 `BattleResult::MyPlayers` 和 `MyVirtualPlayers` 中；虚拟玩家不自动回复 DP，也不参与首领玩家初始化或半场漏怪归属。

敌人默认归属系统敌方虚拟玩家。`EnemySpawn::MyUnitOwnerId` 可指定实际持有者，原字段 `MyOwnerId` 继续指定漏怪／击杀结算玩家，单位状态分别保存为 `MyOwner` 和 `MyResponsiblePlayer`。友军选择器据实际持有者筛选；旧盟约、战略和寒风中按出生半场作用的敌人条件继续使用结算玩家。

未来敌方玩家负责出怪规划。`BattlePlayerRole::WAVE_PLANNER` 与 `EnemySpawn::MyPlannerId` 保存规划者身份，`WavePlan` / `WaveSpawnOrder` 表达敌人定义、路线、数量、间隔和首次出现时间，`ExpandWavePlan` 展开为现有出生事件。规划者不直接操纵敌人行动，也不参与防守方的初始单位、地图角色生成、联防半场人数、自动 DP 回复和首领玩家初始化。回合提交、允许的敌人目录、出怪预算、路线许可和完整敌方玩法尚待相应比赛模式实现。

## 源码布局

公开接口位于 `include/stronghold/simulation`。`src/simulation` 按 `skills`、`operators`、`selectors`、`effects`、`content`、`buffs`、`bonds`、`equipment`、`garrisons`、`summons`、`combat`、`units`、`field`、`battle`、`waves` 分目录。适配器、准备阶段、库存、内容生成、比赛和核心调度分别位于 `src/adapters`、`preparation`、`inventory`、`generation`、`match`、`core`。`battle_core.hpp` 为库内私有实现，调用方使用公开 `Battle`、技能、干员及效果接口。
