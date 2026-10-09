#include <iomanip>
#include <iostream>
#include <stronghold/adapters/reference_ally.hpp>
#include <stronghold/simulation/battle.hpp>

namespace
{
	using namespace Stronghold;
	// 数据包含中文文本，显式转义 JSON 控制字符；不依赖系统 locale。
	void String(std::string_view _value)
	{
		constexpr std::string_view Hex = "0123456789abcdef";
		std::cout << '"';
		for (const unsigned char ch : _value)
		{
			if (ch == '"' || ch == '\\') std::cout << '\\' << static_cast<char>(ch);
			else if (ch < 32) std::cout << "\\u00" << Hex[ch >> 4] << Hex[ch & 15];
			else std::cout << static_cast<char>(ch);
		}
		std::cout << '"';
	}

	void Board(Blackboard _board)
	{
		std::cout << '{';
		bool first = true;
		for (const auto& entry : _board.MyEntries)
		{
			if (!first) std::cout << ',';
			first = false;
			String(entry.MyKey);
			std::cout << ':';
			if (const auto* number = std::get_if<double>(&entry.MyValue)) std::cout << *number;
			else String(std::get<std::string_view>(entry.MyValue));
			if (_board.Find(entry.MyKey) != &entry) throw std::runtime_error("blackboard index failed");
		}
		std::cout << '}';
	}

	void Grid(std::span<const RangeOffset> _grid)
	{
		std::cout << '[';
		for (std::size_t i = 0; i < _grid.size(); ++i)
		{
			if (i) std::cout << ',';
			std::cout << '[' << _grid[i].MyRow << ',' << _grid[i].MyColumn << ']';
		}
		std::cout << ']';
	}

	void Strings(std::span<const std::string_view> _strings)
	{
		std::cout << '[';
		for (std::size_t i = 0; i < _strings.size(); ++i) { if (i) std::cout << ','; String(_strings[i]); }
		std::cout << ']';
	}

	void Skill(const std::optional<AllySkillRecord>& _record)
	{
		if (!_record) { std::cout << "null"; return; }
		const auto& skill = *_record;
		std::cout << '['; String(skill.MyId); std::cout << ','; String(skill.MyName); std::cout << ',';
		if (skill.MyIndex < 0) std::cout << "null"; else std::cout << skill.MyIndex;
		std::cout << ','; String(skill.MyType); std::cout << ','; String(skill.MyDurationType);
		std::cout << ',' << skill.MyDuration << ',' << static_cast<unsigned>(skill.MySpType) << ',' << skill.MySpCost << ',' << skill.MyInitialSp << ',' << skill.MyMaxCharges << ',';
		if (skill.MyHasRange) Grid(skill.MyRange); else std::cout << "null";
		std::cout << ','; String(skill.MyTrigger); std::cout << ',';
		if (skill.MyHasTriggerRange) Grid(skill.MyTriggerRange); else std::cout << "null";
		std::cout << ',' << skill.MyTriggerAllies << ','; Board(skill.MyBlackboard); std::cout << ','; Board(skill.MyStrings); std::cout << ',';
		String(skill.MyDescription); std::cout << ']';
	}

	// 比较已经解析的基础配置；有状态职业仍由独立战斗用例验证。
	void Profile(const AttackProfile& _profile)
	{
		const auto& p = _profile;
		std::cout << '[' << static_cast<unsigned>(p.MyDamageType) << ',' << p.MyDisabled << ',' << p.MyHealing << ','
			<< p.MyCanHitFlying << ',' << p.MyBlockFlying << ',' << p.MyRanged << ',' << p.MyMaxTargets << ','
			<< static_cast<unsigned>(p.MyPriority) << ',' << p.MyProjectileSpeed << ',' << p.MyGroundOnly << ','
			<< p.MyNoHeal << ',' << p.MyHits << ',' << p.MyAllInRange << ',' << p.MySplashRadius << ',' << p.MySplashScale << ','
			<< p.MyChainCount << ',' << p.MyChainFalloff << ',' << p.MyChainSluggish << ',' << p.MyHealChainCount << ','
			<< p.MyHealChainFalloff << ',' << p.MyElementHealRatio << ',';
		String(!p.MyOnHitStatus ? "" : *p.MyOnHitStatus == CombatStatus::WEAKEN ? "weaken" : "sluggish");
		std::cout << ',' << p.MyOnHitApplication.MyDuration << ',';
		if (p.MyOnHitApplication.MyValue) std::cout << *p.MyOnHitApplication.MyValue; else std::cout << "null";
		std::cout << ',' << p.MyOnlyDuringSkill << ',' << p.MyHitAllBlocked << ',' << static_cast<unsigned>(p.MyScaling)
			<< ',' << p.MyConditionalScale << ',' << p.MyHealFarMultiplier << ',' << p.MyHealNearDistance << ',' << p.MyBoomerang << ',' << p.MyFortress << ']';
	}

	void Body(const AllyRecord& _body)
	{
		std::cout << '[';
		for (const auto s : {_body.MyId, _body.MyName, _body.MyCharacterId, _body.MyProfession, _body.MySubProfession, _body.MyPosition}) { String(s); std::cout << ','; }
		const auto& s = _body.MyStats;
		std::cout << '[' << s.MyMaxHealth << ',' << s.MyAttack << ',' << s.MyDefense << ',' << s.MyResistance << ',' << s.MyAttackSpeed << ',' << s.MyBaseAttackTime << ','
			<< s.MyMoveSpeed << ',' << s.MyBlockCount << ',' << s.MyTaunt << ',' << s.MyRedeploySeconds << ',' << s.MyDeploymentCost << ',' << s.MySpRecovery << ',' << s.MyHealthRegen << ',' << s.MyMass << "],";
		Grid(_body.MyRange); std::cout << ',';
		for (const auto field : {_body.MyDamageType, _body.MyAttackKind, _body.MyProjectile}) { String(field); std::cout << ','; }
		if (_body.MyCanHitFlying) std::cout << *_body.MyCanHitFlying; else std::cout << "null";
		std::cout << ','; String(_body.MyTargetPriority); std::cout << ','; String(_body.MyTraitText); std::cout << ','; Board(_body.MyTraitBlackboard); std::cout << ",[";
		bool first = true;
		for (const auto status : {CombatStatus::STUN, CombatStatus::SILENCE, CombatStatus::SLEEP, CombatStatus::FREEZE, CombatStatus::LEVITATE})
		{ if (!first) std::cout << ',';
			first = false; std::cout << ((_body.MyImmunities >> static_cast<unsigned>(status)) & 1); }
		std::cout << "],"; Skill(_body.MySkill); std::cout << ",[";
		for (std::size_t i = 0; i < _body.MyTalents.size(); ++i)
		{
			const auto& talent = _body.MyTalents[i]; if (i) std::cout << ',';
			std::cout << '['; String(talent.MyName); std::cout << ','; String(talent.MyDescription); std::cout << ',';
			Board(talent.MyBlackboard); std::cout << ','; Board(talent.MyStrings); std::cout << ',';
			if (talent.MyHasRange) Grid(talent.MyRange); else std::cout << "null";
			std::cout << ','; String(talent.MyTokenKey); std::cout << ']';
		}
		std::cout << "],"; Strings(_body.MyTokens); std::cout << ",["; first = true;
		for (const auto status : {CombatStatus::UNTARGETABLE, CombatStatus::NO_HEAL, CombatStatus::ISOLATED})
		{ if (!first) std::cout << ',';
			first = false; std::cout << ((_body.MyStartingFlags >> static_cast<unsigned>(status)) & 1); }
		std::cout << "],"; if (_body.MyWithdrawDuration) std::cout << *_body.MyWithdrawDuration; else std::cout << "null";
		std::cout << ','; Profile(_body.MyBaseAttack); std::cout << ',';
		if (_body.MyHasTraitFrontRange) Grid(_body.MyTraitFrontRange); else std::cout << "null";
		const auto& trait = _body.MyProfessionTraits;
		std::cout << ",[" << static_cast<unsigned>(trait.MyKind) << ',' << trait.MyAmmoMax << ',' << trait.MyFunnelInitial << ','
			<< trait.MyFunnelDelta << ',' << trait.MyFunnelMax << ',' << trait.MyStoreMax << ',' << trait.MyGuardDefense << ','
			<< trait.MyGuardResistance << ',' << trait.MyDodge << ',' << trait.MySelfHeal << ',' << trait.MyHealRatio << ','
			<< trait.MyDpOnKill << ',' << trait.MyHpDrain << ',' << trait.MyMerchantInterval << ',' << trait.MyMerchantCost << ','
			<< trait.MyRampMax << ',' << trait.MyRampTime << ',' << trait.MyRampInitial << ',' << trait.MyAuraRatio << ',' << trait.MyShockScale << ',' << trait.MyShockCount << ',' << trait.MyDollDuration << ',' << trait.MyDollHealthMultiplier << ',' << trait.MyDollNoAttack << ']';
		std::cout << ']';
	}
}

int main()
{
	try
	{
		std::cout << std::setprecision(17) << "[[";
		bool first = true;
		for (const auto& op : ReferenceOperators())
		{
			if (&ReferenceOperator(op.MyId) != &op) throw std::logic_error("operator index");
			if (&op.Loadout(-999, "invalid") != &op.Loadout()) throw std::logic_error("invalid choice fallback");
			if (!first) std::cout << ',';
			first = false;
			std::cout << '['; String(op.MyId); std::cout << ','; String(op.MyBaseId);
			std::cout << ',' << op.MyGolden << ',' << op.MyDiy << ',' << op.MyTier << ','; Strings(op.MyBonds); std::cout << ','; Strings(op.MyGarrisons); std::cout << ",[";
			for (std::size_t i = 0; i < op.MyLoadouts.size(); ++i)
			{
				const auto& loadout = op.MyLoadouts[i];
				if (&op.Loadout(loadout.MySkillIndex, loadout.MyModuleId) != &loadout) throw std::logic_error("loadout index");
				if (i) std::cout << ',';
				std::cout << '[' << loadout.MySkillIndex << ','; String(loadout.MyModuleId);
				std::cout << ',' << loadout.MySkillDefault << ',' << loadout.MyModuleDefault << ',';
				Body(loadout.MyBody); std::cout << ','; String(loadout.MyEquippedModuleId); std::cout << ',' << loadout.MyModuleLevel << ',' << loadout.MyModuleActive << ']';
				// 真实属性均可进入战斗值模型；行为配置显式注入，不能把这个检查误认为内容脚本验证。
				Battle battle(BattleInput{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "one", .MyUnits = {AllyDeployment{
					.MyPieceUid = 1, .MyDefinition = loadout.MyBody.MakeDefinition(AttackProfile{.MyDisabled = true}), .MyPosition = WorldPoint{.MyX = 5, .MyY = 9}}}}}, .MyAutoFinish = false});
				battle.Step();
				if (!battle.Unit(1).MyAlive || std::islessgreater(battle.Unit(1).MyHealth, loadout.MyBody.MyStats.MyMaxHealth)) throw std::logic_error("real operator body deployment");
			}
			std::cout << "],";
			const auto printSelection = [](const AllyLoadoutRecord& _lo)
			{
				std::cout << '[' << _lo.MySkillIndex << ','; String(_lo.MyModuleId); std::cout << ',';
				Body(_lo.MyBody); std::cout << ','; String(_lo.MyEquippedModuleId); std::cout << ',' << _lo.MyModuleLevel << ',' << _lo.MyModuleActive << ']';
			};
			if (op.MyStandIn)
			{
				if (&op.Loadout(-999, "invalid", true) != &*op.MyStandIn) throw std::logic_error("stand-in ignores loadout");
				printSelection(*op.MyStandIn);
			}
			else std::cout << "null";
			std::cout << ",[";
			for (std::size_t i = 0; i < op.MyDiyChoices.size(); ++i)
			{
				const auto& choice = op.MyDiyChoices[i];
				if (&op.Diy(choice.MyCharacterId, choice.MySkillIndex, choice.MyModuleId) != &choice) throw std::logic_error("DIY lookup");
				if (choice.MyPrototype && &op.Diy(choice.MyCharacterId) != &choice) throw std::logic_error("prototype defaults");
				if (i) std::cout << ',';
				std::cout << '['; String(choice.MyCharacterId); std::cout << ',' << choice.MySkillIndex << ','; String(choice.MyModuleId);
				std::cout << ',' << choice.MyPrototype << ','; Strings(choice.MyBonds); std::cout << ','; String(choice.MyTokenOwner); std::cout << ',';
				printSelection(choice.MyLoadout); std::cout << ']';
			}
			bool rejected = false;
			try { (void)op.Diy("unknown", -1, "invalid"); } catch (const std::invalid_argument&) { rejected = true; }
			if (!rejected) throw std::logic_error("invalid DIY choice accepted");
			std::cout << "]]";
		}
		std::cout << "],["; first = true;
		for (const auto& token : ReferenceTokens())
		{
			if (&ReferenceToken(token.MyId) != &token || &token.Variant("unknown") != &token.Variant()) throw std::logic_error("token fallback");
			if (!first) std::cout << ',';
			first = false;
			std::cout << '['; String(token.MyId); std::cout << ',' << token.MyPlaceable << ',' << token.MyOwnerRange << ',' << token.MyDeployLimit << ','; String(token.MyFallbackOwner); std::cout << ",[";
			for (std::size_t i = 0; i < token.MyVariants.size(); ++i)
			{
				const auto& variant = token.MyVariants[i];
				if (variant.MyOwnerId.find('@') == std::string_view::npos && &token.Variant(variant.MyOwnerId, variant.MySkillIndex, variant.MyModuleId) != &variant) throw std::logic_error("token variant index");
				if (i) std::cout << ',';
				std::cout << '['; String(variant.MyOwnerId); std::cout << ',' << variant.MySkillIndex << ','; String(variant.MyModuleId); std::cout << ',' << variant.MyDefault << ',';
				Body(variant.MyBody); std::cout << ',' << variant.MyCount << ',';
				if (variant.MyHasSources) Strings(variant.MySources); else std::cout << "null";
				std::cout << ']';
			}
			std::cout << "],[";
			bool firstQuery = true;
			for (const auto& op : ReferenceOperators())
				for (const auto& lo : op.MyLoadouts)
				{
					if (!firstQuery) std::cout << ',';
					firstQuery = false;
					std::cout << (&token.Variant(op.MyId, lo.MySkillIndex, lo.MyModuleId) - token.MyVariants.data());
				}
			std::cout << "],[";
			for (std::size_t i = 0; i < token.MyDiyVariants.size(); ++i)
			{
				const auto& variant = token.MyDiyVariants[i];
				if (i) std::cout << ',';
				std::cout << '['; String(variant.MyOwnerId); std::cout << ',' << variant.MySkillIndex << ','; String(variant.MyModuleId); std::cout << ',';
				Body(variant.MyBody); std::cout << ',' << variant.MyCount << ',';
				if (variant.MyHasSources) Strings(variant.MySources); else std::cout << "null";
				std::cout << ']';
			}
			// Every valid pick can ask for every token. Return an index, keeping this cross-product trace compact.
			std::cout << "],[";
			firstQuery = true;
			for (const auto& op : ReferenceOperators())
				for (const auto& choice : op.MyDiyChoices)
				{
					if (!firstQuery) std::cout << ',';
					firstQuery = false;
					const auto& variant = token.DiyVariant(choice);
					std::cout << (&variant == &token.MyUnowned ? -1 : &variant - token.MyDiyVariants.data());
				}
			std::cout << "],["; Body(token.MyUnowned.MyBody); std::cout << ',' << token.MyUnowned.MyCount << ",null]]";
		}
		std::cout << "]]";
	}
	catch (const std::exception& _error) { std::cerr << _error.what(); return 1; }
}
