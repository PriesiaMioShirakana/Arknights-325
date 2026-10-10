#include "battle_core.hpp"
#include <stronghold/simulation/component_registry.hpp>
#include <tuple>

namespace Stronghold
{
	double EffectExecutor::Amount(const Battle& _battle, const EffectContext& _context, const EffectTarget& _target, const EffectAmount& _amount)
	{
		double amount = _amount.MyFlat + _amount.MyEventAmount * _context.MyEventAmount;
		if (_context.MySource)
		{
			const auto& stats = _battle.Unit(_context.MySource).MyStats;
			amount += _amount.MySourceAttack * stats.MyAttack + _amount.MySourceDefense * stats.MyDefense + _amount.MySourceMaxHealth * stats.MyMaxHealth;
		}
		if (_target.MyKind == EffectTargetKind::UNIT) amount += _amount.MyTargetMaxHealth * _battle.Unit(_target.MyUnit).MyStats.MyMaxHealth;
		return amount;
	}

	void EffectExecutor::Apply(Battle& _battle, const EffectContext& _context, const EffectTarget& _target, const EffectOperation& _operation)
	{
		auto& core = *_battle._MyView;
		std::visit([&](const auto& _effect)
		{
			using Effect = std::remove_cvref_t<decltype(_effect)>;
			if constexpr (std::is_same_v<Effect, CustomOperation>) core.CustomOperationHandler(_effect.MyReference).Apply(_battle, _context, _target);
			else if constexpr (std::is_same_v<Effect, ObstacleOperation>)
			{
				if (_target.MyKind == EffectTargetKind::TERRAIN) _battle.SetObstacle(_target.MyTile.MyRow, _target.MyTile.MyColumn, _effect.MyEnabled, _effect.MyKind);
			}
			else if constexpr (std::is_same_v<Effect, PlayerOperation>)
			{
				if (_target.MyKind != EffectTargetKind::PLAYER) return;
				const auto& player = core._MyPlayers.at(_target.MyPlayer);
				const auto amount = Amount(_battle, _context, _target, _effect.MyAmount);
				switch (_effect.MyKind)
				{
				case PlayerOperationKind::DP: (void)_battle.AddDp(player.MyPlayerId, amount); break;
				case PlayerOperationKind::COINS: (void)_battle.AddCoins(player.MyPlayerId, amount); break;
				case PlayerOperationKind::BOND_LAYERS: (void)_battle.AddBondLayers(player.MyPlayerId, _effect.MyBond, amount, {.MySource = _context.MySource}); break;
				}
			}
			else
			{
				if (_target.MyKind != EffectTargetKind::UNIT) return;
				const auto id = _target.MyUnit;
				if constexpr (std::is_same_v<Effect, DamageOperation>)
				{
					auto damage = _effect.MyDamage; damage.MyAmount += Amount(_battle, _context, _target, _effect.MyAmount);
					(void)_battle.DealDamage(_context.MySource, id, damage);
				}
				else if constexpr (std::is_same_v<Effect, HealOperation>) (void)_battle.Heal(_context.MySource, id, Amount(_battle, _context, _target, _effect.MyAmount), _effect.MyOptions);
				else if constexpr (std::is_same_v<Effect, HealthLossOperation>) (void)_battle.LoseHealth(_context.MySource, id, Amount(_battle, _context, _target, _effect.MyAmount), _effect.MySilent);
				else if constexpr (std::is_same_v<Effect, SpOperation>)
				{
					const auto amount = Amount(_battle, _context, _target, _effect.MyAmount);
					if (_effect.MySetTotal) _battle.SetSpTotal(id, amount); else (void)_battle.GainSp(id, amount, _effect.MyReason);
				}
				else if constexpr (std::is_same_v<Effect, StatusOperation>)
				{
					if (_effect.MyRemove) (void)_battle.RemoveStatus(id, _effect.MyStatus);
					else { auto application = _effect.MyApplication; application.MySource = _context.MySource; (void)_battle.ApplyStatus(id, _effect.MyStatus, application); }
				}
				else if constexpr (std::is_same_v<Effect, BuffOperation>)
				{
					auto definition = _effect.MyPreset.MyKind == ComponentKind::BUFF ? core.Registry().Buff(_effect.MyPreset) : _effect.MyDefinition;
					if (_effect.MyRemove) (void)_battle.RemoveBuff(id, definition.MyKey);
					else { definition.MySource = _context.MySource; (void)_battle.AddBuff(id, std::move(definition)); }
				}
				else if constexpr (std::is_same_v<Effect, ShieldOperation>)
				{
					auto shield = _effect.MyShield; shield.MyHealth += Amount(_battle, _context, _target, _effect.MyAmount); _battle.AddShield(id, shield);
				}
				else if constexpr (std::is_same_v<Effect, ElementOperation>)
				{
					auto hit = _effect.MyHit; hit.MyAmount += Amount(_battle, _context, _target, _effect.MyAmount); (void)_battle.DealElement(_context.MySource, id, hit);
				}
				else if constexpr (std::is_same_v<Effect, PushOperation>)
				{
					if (!_context.MySource) return;
					if (_effect.MyPull) (void)_battle.PullToFront(id, _context.MySource, _effect.MyForce);
					else (void)_battle.Push(id, _effect.MyForce, {.MyFrom = _battle.Unit(_context.MySource).MyPosition, .MyEffect = _effect.MyEffect});
				}
				else if constexpr (std::is_same_v<Effect, SkillOperation>)
				{
					_battle.AddSkillAmmo(id, _effect.MyAmmo); _battle.ExtendSkill(id, _effect.MyDuration); _battle.AddSkillCharge(id, _effect.MyCharges);
				}
			}
		}, _operation);
	}

}
