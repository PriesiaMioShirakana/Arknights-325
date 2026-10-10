cmake_minimum_required(VERSION 3.25)
project(stronghold_backend VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(CTest)

add_library(
  stronghold_core
  "${PROJECT_SOURCE_DIR}/src/core/scheduler.cpp"
  "${PROJECT_SOURCE_DIR}/src/domain/catalog.cpp"
  "${PROJECT_SOURCE_DIR}/src/domain/bonds.cpp"
  "${PROJECT_SOURCE_DIR}/src/domain/pool.cpp"
  "${PROJECT_SOURCE_DIR}/src/preparation/preparation.cpp"
  "${PROJECT_SOURCE_DIR}/src/preparation/preparation_roster.cpp"
  "${PROJECT_SOURCE_DIR}/src/preparation/preparation_summons.cpp"
  "${PROJECT_SOURCE_DIR}/src/preparation/preparation_economy_effects.cpp"
  "${PROJECT_SOURCE_DIR}/src/inventory/inventory.cpp"
  "${PROJECT_SOURCE_DIR}/src/inventory/inventory_effects.cpp"
  "${PROJECT_SOURCE_DIR}/src/preparation/placement.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/battle/battle.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/battle/battle_facade.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/battle/battle_owners.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/waves/wave_plan.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/battle/battle_components.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/skills/skill.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/skills/builtin_skill.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/skills/skill_sequence.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/operator.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/units/unit.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/units/unit_effects.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/units/unit_health.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/field/environment.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/selectors/selector.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/builtin_operator.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/effects/effects.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/selectors/selectors.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/effects/operations.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/content/component_registry.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/units/battle_movement.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/combat/battle_combat.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/combat/battle_status.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/buffs/attributes.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/buffs/battle_buffs.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/buffs/buff_program.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/content/content_registry.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/battle/battle_content.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/bonds/battle_choices.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/bonds/battle_bands.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/field/battle_map_characters.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/bonds/battle_egir.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/garrisons/battle_garrisons.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/skills/battle_generic_skill.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_kits.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_standins.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_diy.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_diy_team.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_support.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_auras.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_zones.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_tactics.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_medics.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_sustain.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_deployment.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_guards.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_control.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_ranged.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_healing_guard.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_vanguard.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_arrows.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_wolves.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_ammo.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_precision.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_obelisk.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_shadows.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_deferral.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_target_marks.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_tornado.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_frost.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_recast.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_layers.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_links.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_charge.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_verdict.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_sleep.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_burning.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_anchor.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_candles.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_ember.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_overload.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_recovery.cpp"
    "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_painting.cpp"
    "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_substitute.cpp"
    "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_waiting.cpp"
    "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_gravity.cpp"
    "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_icicles.cpp"
    "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_soul.cpp"
    "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_radiance.cpp"
    "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_alchemy.cpp"
    "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_bombardment.cpp"
    "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_storm.cpp"
    "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_tremor.cpp"
    "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_gaps.cpp"
    "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_rain.cpp"
    "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_slashes.cpp"
    "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_firewall.cpp"
    "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_snow.cpp"
    "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_sun.cpp"
    "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_oaths.cpp"
    "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_linking.cpp"
    "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_mist.cpp"
    "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_requiem.cpp"
    "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_scorch.cpp"
    "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_tactical_gear.cpp"
    "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_tide.cpp"
    "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_delivery.cpp"
    "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_flying_drones.cpp"
    "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_manifold.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_bard.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_allies.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_merchant.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin/battle_operator_dolls.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/summons/battle_token_kits.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/summons/battle_token_shields.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/garrisons/battle_garrisons_events.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/garrisons/battle_garrisons_attributes.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/bonds/battle_yan.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/summons/battle_yanyou.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/bonds/battle_core_bonds.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/bonds/battle_core_bonds_events.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/bonds/battle_bonds.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/bonds/battle_bonds_events.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/bonds/battle_bonds_raid.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/buffs/battle_strongest.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/equipment/battle_equipment.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/equipment/battle_equipment_periodic.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/equipment/battle_equipment_signatures.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/equipment/battle_equipment_hammers.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/equipment/battle_equipment_lending.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/skills/battle_skills.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/combat/battle_elements.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/combat/battle_attacks.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/field/field.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/field/battle_pathing.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/units/battle_induced_movement.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/units/battle_displacement.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/units/battle_spawns.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/field/battle_tiles.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/field/battle_positions.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/combat/battle_stealth.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/field/battle_terrain.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/units/battle_devices.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/battle/battle_scheduled.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/battle/battle_boss.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/units/battle_professions.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/units/battle_doll.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/battle/battle_economy.cpp"
  "${PROJECT_SOURCE_DIR}/src/match/unite.cpp"
  "${PROJECT_SOURCE_DIR}/src/match/settlement.cpp"
  "${PROJECT_SOURCE_DIR}/src/generation/strategy_draft.cpp"
  "${PROJECT_SOURCE_DIR}/src/generation/special_draft.cpp"
  "${PROJECT_SOURCE_DIR}/src/generation/choice_generation.cpp"
  "${PROJECT_SOURCE_DIR}/src/domain/content_pools.cpp"
  "${PROJECT_SOURCE_DIR}/src/generation/choice_rewards.cpp"
  "${PROJECT_SOURCE_DIR}/src/preparation/preparation_items.cpp"
  "${PROJECT_SOURCE_DIR}/src/preparation/preparation_bands.cpp"
  "${PROJECT_SOURCE_DIR}/src/preparation/preparation_garrisons.cpp"
  "${PROJECT_SOURCE_DIR}/src/preparation/preparation_bonds.cpp"
  "${PROJECT_SOURCE_DIR}/src/preparation/preparation_arts.cpp"
  "${PROJECT_SOURCE_DIR}/src/preparation/preparation_item_hooks.cpp"
  "${PROJECT_SOURCE_DIR}/src/match/final_assault.cpp"
  "${PROJECT_SOURCE_DIR}/src/match/boss_battle_group.cpp"
  "${PROJECT_SOURCE_DIR}/src/simulation/units/battle_profession_effects.cpp"
  "${PROJECT_SOURCE_DIR}/include/stronghold/simulation/battle.hpp"
  "${PROJECT_SOURCE_DIR}/include/stronghold/simulation/combat_types.hpp"
  "${PROJECT_SOURCE_DIR}/include/stronghold/domain/board.hpp"
  "${PROJECT_SOURCE_DIR}/include/stronghold/core/random.hpp"
  "${PROJECT_SOURCE_DIR}/include/stronghold/core/scheduler.hpp"
  "${PROJECT_SOURCE_DIR}/include/stronghold/domain/catalog.hpp"
  "${PROJECT_SOURCE_DIR}/include/stronghold/domain/pool.hpp"
  "${PROJECT_SOURCE_DIR}/include/stronghold/domain/combat_math.hpp"
  "${PROJECT_SOURCE_DIR}/include/stronghold/domain/preparation.hpp"
)
add_library(Stronghold::Core ALIAS stronghold_core)
target_include_directories(stronghold_core
  PUBLIC "${PROJECT_SOURCE_DIR}/include"
  PRIVATE "${PROJECT_SOURCE_DIR}/src/simulation/units" "${PROJECT_SOURCE_DIR}/src/simulation/battle" "${PROJECT_SOURCE_DIR}/src/simulation/skills" "${PROJECT_SOURCE_DIR}/src/simulation/operators/builtin")
target_compile_features(stronghold_core PUBLIC cxx_std_23)

function(stronghold_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
  endif()
endfunction()
stronghold_warnings(stronghold_core)

# Consumers also compile inline numeric rules, so deterministic floating-point flags are public.
if(MSVC)
  target_compile_options(stronghold_core PUBLIC /fp:strict)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  target_compile_options(stronghold_core PUBLIC -fno-fast-math -ffp-contract=off)
endif()

# The reference is a build-time data/oracle input, never a JavaScript runtime dependency.
set(STRONGHOLD_REFERENCE_DIR "${PROJECT_SOURCE_DIR}/../Stronghold-Protocol" CACHE PATH "Original JS repository")
option(STRONGHOLD_BUILD_CATALOG "Compile the reference JSON economy catalog" ON)
if(STRONGHOLD_BUILD_CATALOG)
  find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
  set(catalog_source "${PROJECT_BINARY_DIR}/generated/reference_catalog.cpp")
  add_custom_command(OUTPUT "${catalog_source}"
    COMMAND "${Python3_EXECUTABLE}" -X utf8 "${PROJECT_SOURCE_DIR}/tools/generate_catalog.py"
      --data "${STRONGHOLD_REFERENCE_DIR}/data" --out "${catalog_source}"
    DEPENDS "${PROJECT_SOURCE_DIR}/tools/generate_catalog.py"
      "${STRONGHOLD_REFERENCE_DIR}/data/config.json"
      "${STRONGHOLD_REFERENCE_DIR}/data/chess.json"
      "${STRONGHOLD_REFERENCE_DIR}/data/items.json"
      "${STRONGHOLD_REFERENCE_DIR}/data/stages.json"
    VERBATIM)
  set(combat_source "${PROJECT_BINARY_DIR}/generated/reference_combat.cpp")
  add_custom_command(OUTPUT "${combat_source}"
    COMMAND "${Python3_EXECUTABLE}" -X utf8 "${PROJECT_SOURCE_DIR}/tools/generate_combat.py"
      --data "${STRONGHOLD_REFERENCE_DIR}/data" --out "${combat_source}"
    DEPENDS "${PROJECT_SOURCE_DIR}/tools/generate_combat.py"
      "${PROJECT_SOURCE_DIR}/tools/generate_stage.py"
      "${PROJECT_SOURCE_DIR}/tools/generate_wave.py"
      "${STRONGHOLD_REFERENCE_DIR}/data/waves.json"
      "${STRONGHOLD_REFERENCE_DIR}/data/stages.json"
      "${STRONGHOLD_REFERENCE_DIR}/data/enemies.json"
    VERBATIM)
  set(ally_source "${PROJECT_BINARY_DIR}/generated/reference_ally.cpp")
  set(ally_header "${PROJECT_BINARY_DIR}/generated/reference_ally_data.hpp")
  set(ally_shards)
  foreach(index RANGE 0 11)
    list(APPEND ally_shards "${PROJECT_BINARY_DIR}/generated/reference_ally_${index}.cpp")
  endforeach()
  add_custom_command(OUTPUT "${ally_source}" "${ally_header}" ${ally_shards}
    COMMAND "${Python3_EXECUTABLE}" -X utf8 "${PROJECT_SOURCE_DIR}/tools/generate_ally.py"
      --data "${STRONGHOLD_REFERENCE_DIR}/data" --out "${ally_source}" --shards 12
    DEPENDS "${PROJECT_SOURCE_DIR}/tools/generate_ally.py"
      "${PROJECT_SOURCE_DIR}/tools/generate_roster.py"
      "${PROJECT_SOURCE_DIR}/tools/generate_profession.py"
      "${PROJECT_SOURCE_DIR}/tools/generate_generic_skill.py"
      "${PROJECT_SOURCE_DIR}/tools/generate_operator_kits.py"
      "${PROJECT_SOURCE_DIR}/tools/generate_standin_kits.py"
      "${PROJECT_SOURCE_DIR}/tools/generate_diy_kits.py"
      "${STRONGHOLD_REFERENCE_DIR}/data/backups.json"
      "${PROJECT_SOURCE_DIR}/tools/generate_combat.py"
      "${STRONGHOLD_REFERENCE_DIR}/data/chess.json"
      "${STRONGHOLD_REFERENCE_DIR}/data/tokens.json"
    VERBATIM)
  set(wave_generation_source "${PROJECT_BINARY_DIR}/generated/reference_wave_generation.cpp")
  set(match_source "${PROJECT_BINARY_DIR}/generated/reference_match.cpp")
  set(bonds_source "${PROJECT_BINARY_DIR}/generated/reference_bonds.cpp")
  set(roster_source "${PROJECT_BINARY_DIR}/generated/reference_roster.cpp")
  add_custom_command(OUTPUT "${roster_source}"
    COMMAND "${Python3_EXECUTABLE}" -X utf8 "${PROJECT_SOURCE_DIR}/tools/generate_player_roster.py"
      --data "${STRONGHOLD_REFERENCE_DIR}/data" --out "${roster_source}"
    DEPENDS "${PROJECT_SOURCE_DIR}/tools/generate_player_roster.py" "${PROJECT_SOURCE_DIR}/tools/generate_combat.py"
      "${STRONGHOLD_REFERENCE_DIR}/data/chess.json" "${STRONGHOLD_REFERENCE_DIR}/data/backups.json"
      "${STRONGHOLD_REFERENCE_DIR}/data/tokens.json"
    VERBATIM)
  add_custom_command(OUTPUT "${bonds_source}"
    COMMAND "${Python3_EXECUTABLE}" -X utf8 "${PROJECT_SOURCE_DIR}/tools/generate_bonds.py"
      --data "${STRONGHOLD_REFERENCE_DIR}/data" --out "${bonds_source}"
    DEPENDS "${PROJECT_SOURCE_DIR}/tools/generate_battle_bonds.py" "${PROJECT_SOURCE_DIR}/tools/generate_bonds.py" "${PROJECT_SOURCE_DIR}/tools/generate_combat.py"
      "${PROJECT_SOURCE_DIR}/tools/generate_match.py" "${STRONGHOLD_REFERENCE_DIR}/data/bonds.json"
      "${STRONGHOLD_REFERENCE_DIR}/data/chess.json" "${STRONGHOLD_REFERENCE_DIR}/data/items.json"
    VERBATIM)
  set(choices_source "${PROJECT_BINARY_DIR}/generated/reference_choices.cpp")
  add_custom_command(OUTPUT "${choices_source}"
    COMMAND "${Python3_EXECUTABLE}" -X utf8 "${PROJECT_SOURCE_DIR}/tools/generate_choices.py"
      --data "${STRONGHOLD_REFERENCE_DIR}/data" --out "${choices_source}"
    DEPENDS "${PROJECT_SOURCE_DIR}/tools/generate_choices.py" "${PROJECT_SOURCE_DIR}/tools/generate_choice_rewards.py" "${PROJECT_SOURCE_DIR}/tools/generate_choice_battle.py" "${PROJECT_SOURCE_DIR}/tools/generate_preparation_items.py" "${PROJECT_SOURCE_DIR}/tools/generate_preparation_bands.py" "${PROJECT_SOURCE_DIR}/tools/generate_preparation_garrisons.py" "${PROJECT_SOURCE_DIR}/tools/generate_battle_garrisons.py" "${PROJECT_SOURCE_DIR}/tools/generate_preparation_bonds.py" "${PROJECT_SOURCE_DIR}/tools/generate_band_battle.py" "${PROJECT_SOURCE_DIR}/tools/generate_combat.py"
      "${PROJECT_SOURCE_DIR}/tools/generate_equipment.py"
      "${STRONGHOLD_REFERENCE_DIR}/data/choices.json" "${STRONGHOLD_REFERENCE_DIR}/data/items.json" "${STRONGHOLD_REFERENCE_DIR}/data/bands.json"
      "${STRONGHOLD_REFERENCE_DIR}/data/garrisons.json" "${STRONGHOLD_REFERENCE_DIR}/data/chess.json" "${STRONGHOLD_REFERENCE_DIR}/data/bonds.json"
      "${STRONGHOLD_REFERENCE_DIR}/data/enemies.json" "${STRONGHOLD_REFERENCE_DIR}/data/effects.json"
    VERBATIM)
  add_custom_command(OUTPUT "${match_source}"
    COMMAND "${Python3_EXECUTABLE}" -X utf8 "${PROJECT_SOURCE_DIR}/tools/generate_match.py"
      --data "${STRONGHOLD_REFERENCE_DIR}/data" --out "${match_source}"
    DEPENDS "${PROJECT_SOURCE_DIR}/tools/generate_match.py" "${PROJECT_SOURCE_DIR}/tools/generate_combat.py"
      "${STRONGHOLD_REFERENCE_DIR}/data/config.json" "${STRONGHOLD_REFERENCE_DIR}/data/bands.json"
      "${STRONGHOLD_REFERENCE_DIR}/data/bosses.json"
    VERBATIM)
  add_custom_command(OUTPUT "${wave_generation_source}"
    COMMAND "${Python3_EXECUTABLE}" -X utf8 "${PROJECT_SOURCE_DIR}/tools/generate_wave_generation.py"
      --data "${STRONGHOLD_REFERENCE_DIR}/data" --out "${wave_generation_source}"
    DEPENDS "${PROJECT_SOURCE_DIR}/tools/generate_wave_generation.py" "${PROJECT_SOURCE_DIR}/tools/generate_combat.py"
      "${STRONGHOLD_REFERENCE_DIR}/data/config.json" "${STRONGHOLD_REFERENCE_DIR}/data/factions.json"
      "${STRONGHOLD_REFERENCE_DIR}/data/enemies.json" "${STRONGHOLD_REFERENCE_DIR}/data/stages.json"
      "${STRONGHOLD_REFERENCE_DIR}/data/bosses.json" "${STRONGHOLD_REFERENCE_DIR}/data/bands.json"
    VERBATIM)
  add_library(stronghold_catalog "${wave_generation_source}" "${PROJECT_SOURCE_DIR}/src/generation/wave_generation.cpp" "${PROJECT_SOURCE_DIR}/src/generation/wave_extras.cpp" "${PROJECT_SOURCE_DIR}/src/generation/wave_ducks.cpp" "${catalog_source}" "${combat_source}" "${ally_source}" ${ally_shards} "${PROJECT_SOURCE_DIR}/src/adapters/reference_ally.cpp" "${PROJECT_SOURCE_DIR}/src/adapters/reference_stage.cpp" "${PROJECT_SOURCE_DIR}/src/adapters/reference_wave.cpp")
  # MSVC's debug constant evaluation can use substantial memory for roster tables.
  target_sources(stronghold_catalog PRIVATE "${match_source}" "${PROJECT_SOURCE_DIR}/src/adapters/reference_match_setup.cpp" "${PROJECT_SOURCE_DIR}/src/adapters/reference_map_characters.cpp"
    "${PROJECT_SOURCE_DIR}/src/adapters/reference_summons.cpp" "${PROJECT_SOURCE_DIR}/src/adapters/reference_generic_skill.cpp" "${PROJECT_SOURCE_DIR}/src/adapters/reference_operator_kit.cpp")
  target_sources(stronghold_catalog PRIVATE "${PROJECT_SOURCE_DIR}/src/adapters/reference_equipment.cpp")
  target_sources(stronghold_catalog PRIVATE "${PROJECT_SOURCE_DIR}/src/match/local_match.cpp" "${PROJECT_SOURCE_DIR}/src/match/local_match_battle.cpp")
  target_sources(stronghold_catalog PRIVATE "${roster_source}" "${PROJECT_SOURCE_DIR}/src/adapters/reference_roster.cpp")
  target_sources(stronghold_catalog PRIVATE "${choices_source}" "${bonds_source}" "${PROJECT_SOURCE_DIR}/src/adapters/reference_preparation.cpp" "${PROJECT_SOURCE_DIR}/src/adapters/reference_choice_effects.cpp")
  # Keep the data in separate source files and bound simultaneous compilers on Ninja/VS CMake.
  set_property(GLOBAL APPEND PROPERTY JOB_POOLS stronghold_data_compile=2)
  set_property(TARGET stronghold_catalog PROPERTY JOB_POOL_COMPILE stronghold_data_compile)
  stronghold_warnings(stronghold_catalog)
  target_link_libraries(stronghold_catalog PUBLIC Stronghold::Core)
  add_executable(stronghold_cli "${PROJECT_SOURCE_DIR}/apps/main.cpp")
  target_link_libraries(stronghold_cli PRIVATE stronghold_catalog)
  if(BUILD_TESTING)
    add_executable(stronghold_local_match_tests "${PROJECT_SOURCE_DIR}/tests/local_match_tests.cpp")
    target_link_libraries(stronghold_local_match_tests PRIVATE stronghold_catalog)
    stronghold_warnings(stronghold_local_match_tests)
    add_test(NAME local_match_core COMMAND stronghold_local_match_tests)
  endif()
  stronghold_warnings(stronghold_cli)
endif()

if(BUILD_TESTING)
  add_executable(stronghold_final_assault_battle_tests "${PROJECT_SOURCE_DIR}/tests/final_assault_battle_tests.cpp")
  target_link_libraries(stronghold_final_assault_battle_tests PRIVATE Stronghold::Core)
  stronghold_warnings(stronghold_final_assault_battle_tests)
  add_test(NAME final_assault_battle_core COMMAND stronghold_final_assault_battle_tests)
  add_executable(stronghold_tests "${PROJECT_SOURCE_DIR}/tests/core_tests.cpp")
  target_link_libraries(stronghold_tests PRIVATE Stronghold::Core)
  stronghold_warnings(stronghold_tests)
  add_test(NAME core COMMAND stronghold_tests)
  add_executable(stronghold_battle_tests "${PROJECT_SOURCE_DIR}/tests/battle_tests.cpp")
  target_link_libraries(stronghold_battle_tests PRIVATE Stronghold::Core)
  stronghold_warnings(stronghold_battle_tests)
  add_test(NAME battle_core COMMAND stronghold_battle_tests)
  add_executable(stronghold_effect_tests "${PROJECT_SOURCE_DIR}/tests/effects_tests.cpp")
  target_link_libraries(stronghold_effect_tests PRIVATE Stronghold::Core)
  stronghold_warnings(stronghold_effect_tests)
  add_test(NAME effects_core COMMAND stronghold_effect_tests)
  add_executable(stronghold_content_tests "${PROJECT_SOURCE_DIR}/tests/content_tests.cpp")
  target_link_libraries(stronghold_content_tests PRIVATE Stronghold::Core)
  stronghold_warnings(stronghold_content_tests)
  add_test(NAME content_core COMMAND stronghold_content_tests)
  add_executable(stronghold_preparation_garrisons_tests "${PROJECT_SOURCE_DIR}/tests/preparation_garrisons_tests.cpp")
  target_link_libraries(stronghold_preparation_garrisons_tests PRIVATE Stronghold::Core)
  stronghold_warnings(stronghold_preparation_garrisons_tests)
  add_test(NAME preparation_garrisons_core COMMAND stronghold_preparation_garrisons_tests)
  add_executable(stronghold_preparation_bonds_tests "${PROJECT_SOURCE_DIR}/tests/preparation_bonds_tests.cpp")
  target_link_libraries(stronghold_preparation_bonds_tests PRIVATE Stronghold::Core)
  stronghold_warnings(stronghold_preparation_bonds_tests)
  add_test(NAME preparation_bonds_core COMMAND stronghold_preparation_bonds_tests)
  add_executable(stronghold_preparation_bands_tests "${PROJECT_SOURCE_DIR}/tests/preparation_bands_tests.cpp")
  target_link_libraries(stronghold_preparation_bands_tests PRIVATE Stronghold::Core)
  stronghold_warnings(stronghold_preparation_bands_tests)
  add_test(NAME preparation_bands_core COMMAND stronghold_preparation_bands_tests)
  add_executable(stronghold_preparation_items_tests "${PROJECT_SOURCE_DIR}/tests/preparation_items_tests.cpp")
  target_link_libraries(stronghold_preparation_items_tests PRIVATE Stronghold::Core)
  stronghold_warnings(stronghold_preparation_items_tests)
  add_test(NAME preparation_items_core COMMAND stronghold_preparation_items_tests)
  add_executable(stronghold_skill_tests "${PROJECT_SOURCE_DIR}/tests/skill_tests.cpp")
  target_link_libraries(stronghold_skill_tests PRIVATE Stronghold::Core)
  stronghold_warnings(stronghold_skill_tests)
  add_test(NAME skill_core COMMAND stronghold_skill_tests)
  add_executable(stronghold_position_rules_tests "${PROJECT_SOURCE_DIR}/tests/position_rules_tests.cpp")
  target_link_libraries(stronghold_position_rules_tests PRIVATE Stronghold::Core)
  stronghold_warnings(stronghold_position_rules_tests)
  add_test(NAME position_rules_core COMMAND stronghold_position_rules_tests)
  add_executable(stronghold_operator_kits_tests "${PROJECT_SOURCE_DIR}/tests/operator_kits_tests.cpp")
  target_link_libraries(stronghold_operator_kits_tests PRIVATE Stronghold::Core)
  stronghold_warnings(stronghold_operator_kits_tests)
  add_test(NAME operator_kits_core COMMAND stronghold_operator_kits_tests)
  add_executable(stronghold_element_tests "${PROJECT_SOURCE_DIR}/tests/element_tests.cpp")
  target_link_libraries(stronghold_element_tests PRIVATE Stronghold::Core)
  stronghold_warnings(stronghold_element_tests)
  add_executable(stronghold_attack_tests "${PROJECT_SOURCE_DIR}/tests/attack_tests.cpp")
  target_link_libraries(stronghold_attack_tests PRIVATE Stronghold::Core)
  stronghold_warnings(stronghold_attack_tests)
  add_test(NAME attack_core COMMAND stronghold_attack_tests)
  add_executable(stronghold_field_probe "${PROJECT_SOURCE_DIR}/tests/field_probe.cpp")
  target_link_libraries(stronghold_field_probe PRIVATE Stronghold::Core)
  stronghold_warnings(stronghold_field_probe)
  foreach(probe IN ITEMS body stealth terrain turret cold_wind skill_trigger healing enemy_healing boss_pool carry profession_attack profession_state profession_effect profession_projectile tactical doll bounty unite settlement strategy_draft final_assault special_draft layer_gains)
    add_executable(stronghold_${probe}_probe "${PROJECT_SOURCE_DIR}/tests/${probe}_probe.cpp")
    target_link_libraries(stronghold_${probe}_probe PRIVATE Stronghold::Core)
    stronghold_warnings(stronghold_${probe}_probe)
  endforeach()
  add_executable(stronghold_spawn_tests "${PROJECT_SOURCE_DIR}/tests/spawn_tests.cpp")
  target_link_libraries(stronghold_spawn_tests PRIVATE Stronghold::Core)
  stronghold_warnings(stronghold_spawn_tests)
  add_test(NAME spawn_core COMMAND stronghold_spawn_tests)
  add_executable(stronghold_displacement_probe "${PROJECT_SOURCE_DIR}/tests/displacement_probe.cpp")
  target_link_libraries(stronghold_displacement_probe PRIVATE Stronghold::Core)
  stronghold_warnings(stronghold_displacement_probe)
  add_executable(stronghold_pathing_probe "${PROJECT_SOURCE_DIR}/tests/pathing_probe.cpp")
  target_link_libraries(stronghold_pathing_probe PRIVATE Stronghold::Core)
  stronghold_warnings(stronghold_pathing_probe)
  add_executable(stronghold_battle_probe "${PROJECT_SOURCE_DIR}/tests/battle_probe.cpp")
  target_link_libraries(stronghold_battle_probe PRIVATE Stronghold::Core)
  stronghold_warnings(stronghold_battle_probe)
  if(STRONGHOLD_BUILD_CATALOG)
    add_executable(stronghold_choice_battle_probe "${PROJECT_SOURCE_DIR}/tests/choice_battle_probe.cpp")
    target_link_libraries(stronghold_choice_battle_probe PRIVATE stronghold_catalog)
    stronghold_warnings(stronghold_choice_battle_probe)
    add_executable(stronghold_choice_rewards_probe "${PROJECT_SOURCE_DIR}/tests/choice_rewards_probe.cpp")
    target_link_libraries(stronghold_choice_rewards_probe PRIVATE stronghold_catalog)
    stronghold_warnings(stronghold_choice_rewards_probe)
    add_executable(stronghold_preparation_items_probe "${PROJECT_SOURCE_DIR}/tests/preparation_items_probe.cpp")
    target_link_libraries(stronghold_preparation_items_probe PRIVATE stronghold_catalog)
    stronghold_warnings(stronghold_preparation_items_probe)
    add_executable(stronghold_content_pools_probe "${PROJECT_SOURCE_DIR}/tests/content_pools_probe.cpp")
    target_link_libraries(stronghold_content_pools_probe PRIVATE stronghold_catalog)
    stronghold_warnings(stronghold_content_pools_probe)
    add_executable(stronghold_summons_probe "${PROJECT_SOURCE_DIR}/tests/summons_probe.cpp")
    target_link_libraries(stronghold_summons_probe PRIVATE stronghold_catalog)
    stronghold_warnings(stronghold_summons_probe)
    add_executable(stronghold_diy_stock_probe "${PROJECT_SOURCE_DIR}/tests/diy_stock_probe.cpp")
    target_link_libraries(stronghold_diy_stock_probe PRIVATE stronghold_catalog)
    stronghold_warnings(stronghold_diy_stock_probe)
    add_executable(stronghold_roster_probe "${PROJECT_SOURCE_DIR}/tests/roster_probe.cpp")
    target_link_libraries(stronghold_roster_probe PRIVATE stronghold_catalog)
    stronghold_warnings(stronghold_roster_probe)
    add_executable(stronghold_bonds_probe "${PROJECT_SOURCE_DIR}/tests/bonds_probe.cpp")
    target_link_libraries(stronghold_bonds_probe PRIVATE stronghold_catalog)
    stronghold_warnings(stronghold_bonds_probe)
    add_executable(stronghold_inventory_probe "${PROJECT_SOURCE_DIR}/tests/inventory_probe.cpp")
    target_link_libraries(stronghold_inventory_probe PRIVATE stronghold_catalog)
    stronghold_warnings(stronghold_inventory_probe)
    add_executable(stronghold_choice_generation_probe "${PROJECT_SOURCE_DIR}/tests/choice_generation_probe.cpp")
    target_link_libraries(stronghold_choice_generation_probe PRIVATE stronghold_catalog)
    stronghold_warnings(stronghold_choice_generation_probe)
    add_executable(stronghold_match_catalog_probe "${PROJECT_SOURCE_DIR}/tests/match_catalog_probe.cpp")
    target_link_libraries(stronghold_match_catalog_probe PRIVATE stronghold_catalog)
    stronghold_warnings(stronghold_match_catalog_probe)
    add_executable(stronghold_wave_generation_probe "${PROJECT_SOURCE_DIR}/tests/wave_generation_probe.cpp")
    target_link_libraries(stronghold_wave_generation_probe PRIVATE stronghold_catalog)
    stronghold_warnings(stronghold_wave_generation_probe)
    add_executable(stronghold_ally_catalog_probe "${PROJECT_SOURCE_DIR}/tests/ally_catalog_probe.cpp")
    target_link_libraries(stronghold_ally_catalog_probe PRIVATE stronghold_catalog)
    stronghold_warnings(stronghold_ally_catalog_probe)
    add_executable(stronghold_combat_catalog_probe "${PROJECT_SOURCE_DIR}/tests/combat_catalog_probe.cpp")
    target_link_libraries(stronghold_combat_catalog_probe PRIVATE stronghold_catalog)
    stronghold_warnings(stronghold_combat_catalog_probe)
    add_executable(stronghold_stage_catalog_probe "${PROJECT_SOURCE_DIR}/tests/stage_catalog_probe.cpp")
    target_link_libraries(stronghold_stage_catalog_probe PRIVATE stronghold_catalog)
    stronghold_warnings(stronghold_stage_catalog_probe)
    add_executable(stronghold_wave_catalog_probe "${PROJECT_SOURCE_DIR}/tests/wave_catalog_probe.cpp")
    target_link_libraries(stronghold_wave_catalog_probe PRIVATE stronghold_catalog)
    stronghold_warnings(stronghold_wave_catalog_probe)
    add_test(NAME catalog_smoke COMMAND stronghold_cli catalog)
    add_executable(stronghold_match_setup_probe "${PROJECT_SOURCE_DIR}/tests/match_setup_probe.cpp")
    target_link_libraries(stronghold_match_setup_probe PRIVATE stronghold_catalog)
    stronghold_warnings(stronghold_match_setup_probe)
    add_executable(stronghold_local_match_probe "${PROJECT_SOURCE_DIR}/tests/local_match_probe.cpp")
    target_link_libraries(stronghold_local_match_probe PRIVATE stronghold_catalog)
    stronghold_warnings(stronghold_local_match_probe)
    add_executable(stronghold_wave_ducks_probe "${PROJECT_SOURCE_DIR}/tests/wave_ducks_probe.cpp")
    target_link_libraries(stronghold_wave_ducks_probe PRIVATE stronghold_catalog)
    stronghold_warnings(stronghold_wave_ducks_probe)
    add_executable(stronghold_battle_bands_probe "${PROJECT_SOURCE_DIR}/tests/battle_bands_probe.cpp")
    target_link_libraries(stronghold_battle_bands_probe PRIVATE stronghold_catalog)
    stronghold_warnings(stronghold_battle_bands_probe)
    add_executable(stronghold_map_characters_probe "${PROJECT_SOURCE_DIR}/tests/map_characters_probe.cpp")
    target_link_libraries(stronghold_map_characters_probe PRIVATE stronghold_catalog)
    stronghold_warnings(stronghold_map_characters_probe)
    add_executable(stronghold_equipment_stats_probe "${PROJECT_SOURCE_DIR}/tests/equipment_stats_probe.cpp")
    target_link_libraries(stronghold_equipment_stats_probe PRIVATE stronghold_catalog)
    stronghold_warnings(stronghold_equipment_stats_probe)
    add_executable(stronghold_equipment_effects_probe "${PROJECT_SOURCE_DIR}/tests/equipment_effects_probe.cpp")
    target_link_libraries(stronghold_equipment_effects_probe PRIVATE stronghold_catalog)
    stronghold_warnings(stronghold_equipment_effects_probe)
    add_executable(stronghold_battle_garrisons_probe tests/battle_garrisons_probe.cpp)
    add_executable(stronghold_generic_skill_probe tests/generic_skill_probe.cpp)
    add_executable(stronghold_operator_kits_probe tests/operator_kits_probe.cpp)
    target_link_libraries(stronghold_operator_kits_probe PRIVATE stronghold_catalog)
    stronghold_warnings(stronghold_operator_kits_probe)
    target_link_libraries(stronghold_generic_skill_probe PRIVATE stronghold_catalog)
    stronghold_warnings(stronghold_generic_skill_probe)
    target_link_libraries(stronghold_battle_garrisons_probe PRIVATE stronghold_catalog)
    stronghold_warnings(stronghold_battle_garrisons_probe)
    add_executable(stronghold_yanyou_probe tests/yanyou_probe.cpp)
    target_link_libraries(stronghold_yanyou_probe PRIVATE stronghold_catalog)
    stronghold_warnings(stronghold_yanyou_probe)
    add_executable(stronghold_siracusa_probe tests/siracusa_probe.cpp)
    target_link_libraries(stronghold_siracusa_probe PRIVATE stronghold_catalog)
    stronghold_warnings(stronghold_siracusa_probe)
    add_executable(stronghold_egir_probe tests/egir_probe.cpp)
    target_link_libraries(stronghold_egir_probe PRIVATE stronghold_catalog)
    stronghold_warnings(stronghold_egir_probe)
    add_executable(stronghold_core_bonds_probe tests/core_bonds_probe.cpp)
    target_link_libraries(stronghold_core_bonds_probe PRIVATE stronghold_catalog)
    stronghold_warnings(stronghold_core_bonds_probe)
    add_executable(stronghold_battle_bonds_probe tests/battle_bonds_probe.cpp)
    target_link_libraries(stronghold_battle_bonds_probe PRIVATE stronghold_catalog)
    stronghold_warnings(stronghold_battle_bonds_probe)
    find_program(STRONGHOLD_NODE node)
    if(STRONGHOLD_NODE)
      add_test(NAME map_characters_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/map_characters_parity.mjs"
        --native $<TARGET_FILE:stronghold_map_characters_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
      add_test(NAME equipment_stats_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/equipment_stats_parity.mjs"
        --native $<TARGET_FILE:stronghold_equipment_stats_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
      add_test(NAME equipment_effects_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/equipment_effects_parity.mjs"
        --native $<TARGET_FILE:stronghold_equipment_effects_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
      add_test(NAME equipment_hammers_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/equipment_effects_parity.mjs" --hammers
        --native $<TARGET_FILE:stronghold_equipment_effects_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
      foreach(quality IN ITEMS a b)
        add_test(NAME equipment_lending_${quality}_javascript_parity COMMAND "${STRONGHOLD_NODE}"
          "${PROJECT_SOURCE_DIR}/tests/equipment_effects_parity.mjs" --lending --quality ${quality}
          --native $<TARGET_FILE:stronghold_equipment_effects_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
      endforeach()
      add_test(NAME preparation_bonds_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/preparation_bonds_parity.mjs"
        --native $<TARGET_FILE:stronghold_preparation_items_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
      add_test(NAME battle_garrisons_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/battle_garrisons_parity.mjs"
        --native $<TARGET_FILE:stronghold_battle_garrisons_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
      add_test(NAME generic_skill_catalog_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/generic_skill_parity.mjs" --catalog
        --native $<TARGET_FILE:stronghold_generic_skill_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
      add_test(NAME generic_skill_professions_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/generic_skill_parity.mjs" --professions --seed 1
        --native $<TARGET_FILE:stronghold_generic_skill_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
      foreach(seed IN ITEMS 1 42)
        add_test(NAME operator_kits_${seed}_javascript_parity COMMAND "${STRONGHOLD_NODE}"
          "${PROJECT_SOURCE_DIR}/tests/operator_kits_parity.mjs" --seed ${seed}
          --native $<TARGET_FILE:stronghold_operator_kits_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
        add_test(NAME generic_skill_${seed}_javascript_parity COMMAND "${STRONGHOLD_NODE}"
          "${PROJECT_SOURCE_DIR}/tests/generic_skill_parity.mjs" --seed ${seed}
          --native $<TARGET_FILE:stronghold_generic_skill_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
      endforeach()
      add_test(NAME yanyou_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/yanyou_parity.mjs"
        --native $<TARGET_FILE:stronghold_yanyou_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
      add_test(NAME siracusa_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/siracusa_parity.mjs"
        --native $<TARGET_FILE:stronghold_siracusa_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
      add_test(NAME egir_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/egir_parity.mjs"
        --native $<TARGET_FILE:stronghold_egir_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
      add_test(NAME core_bonds_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/core_bonds_parity.mjs"
        --native $<TARGET_FILE:stronghold_core_bonds_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
      add_test(NAME battle_bonds_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/battle_bonds_parity.mjs"
        --native $<TARGET_FILE:stronghold_battle_bonds_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
      add_test(NAME battle_bands_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/battle_bands_parity.mjs"
        --native $<TARGET_FILE:stronghold_battle_bands_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
      add_test(NAME local_match_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/local_match_parity.mjs"
        --native $<TARGET_FILE:stronghold_local_match_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
      add_test(NAME wave_ducks_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/wave_ducks_parity.mjs"
        --native $<TARGET_FILE:stronghold_wave_ducks_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
      add_test(NAME match_setup_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/match_setup_parity.mjs"
        --native $<TARGET_FILE:stronghold_match_setup_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
      add_test(NAME preparation_garrisons_lisa_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/preparation_garrisons_parity.mjs" --lisa
        --native $<TARGET_FILE:stronghold_preparation_items_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
      foreach(garrison_group RANGE 0 3)
        add_test(NAME preparation_bonds_garrisons_${garrison_group}_javascript_parity COMMAND "${STRONGHOLD_NODE}"
          "${PROJECT_SOURCE_DIR}/tests/preparation_garrisons_parity.mjs" --bonds --group "${garrison_group}"
          --native $<TARGET_FILE:stronghold_preparation_items_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
        add_test(NAME preparation_garrisons_${garrison_group}_javascript_parity COMMAND "${STRONGHOLD_NODE}"
          "${PROJECT_SOURCE_DIR}/tests/preparation_garrisons_parity.mjs" --group "${garrison_group}"
          --native $<TARGET_FILE:stronghold_preparation_items_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
      endforeach()
      foreach(band_group IN ITEMS 1 42 4294967295 mode_single_normal mode_multi_funny)
        add_test(NAME preparation_bands_${band_group}_javascript_parity COMMAND "${STRONGHOLD_NODE}"
          "${PROJECT_SOURCE_DIR}/tests/preparation_bands_parity.mjs" --group "${band_group}"
          --native $<TARGET_FILE:stronghold_preparation_items_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
      endforeach()
      add_test(NAME preparation_items_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/preparation_items_parity.mjs"
        --native $<TARGET_FILE:stronghold_preparation_items_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
      add_test(NAME preparation_arts_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/preparation_items_parity.mjs" --arts-only
        --native $<TARGET_FILE:stronghold_preparation_items_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
      add_test(NAME preparation_item_hooks_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/preparation_items_parity.mjs" --hooks-only
        --native $<TARGET_FILE:stronghold_preparation_items_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
      add_test(NAME choice_battle_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/choice_battle_parity.mjs"
        --native $<TARGET_FILE:stronghold_choice_battle_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
      add_test(NAME choice_rewards_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/choice_rewards_parity.mjs"
        --native $<TARGET_FILE:stronghold_choice_rewards_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
      add_test(NAME content_pools_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/content_pools_parity.mjs"
        --native $<TARGET_FILE:stronghold_content_pools_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
      add_test(NAME summons_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/summons_parity.mjs"
        --native $<TARGET_FILE:stronghold_summons_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
      add_test(NAME diy_stock_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/diy_stock_parity.mjs"
        --native $<TARGET_FILE:stronghold_diy_stock_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
      add_test(NAME roster_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/roster_parity.mjs"
        --native $<TARGET_FILE:stronghold_roster_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
      add_test(NAME bonds_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/bonds_parity.mjs"
        --native $<TARGET_FILE:stronghold_bonds_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
      add_test(NAME inventory_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/inventory_parity.mjs"
        --native $<TARGET_FILE:stronghold_inventory_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
      add_test(NAME choice_generation_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/choice_generation_parity.mjs"
        --native $<TARGET_FILE:stronghold_choice_generation_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
      add_test(NAME match_catalog_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/match_catalog_parity.mjs"
        --native $<TARGET_FILE:stronghold_match_catalog_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
      add_test(NAME wave_generation_javascript_parity
      COMMAND "${STRONGHOLD_NODE}" "${PROJECT_SOURCE_DIR}/tests/wave_generation_parity.mjs"
        --native $<TARGET_FILE:stronghold_wave_generation_probe> --reference "${STRONGHOLD_REFERENCE_DIR}")
      add_test(NAME ally_catalog_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/ally_catalog_parity.mjs"
        --reference "${STRONGHOLD_REFERENCE_DIR}" --native "$<TARGET_FILE:stronghold_ally_catalog_probe>")
      set_tests_properties(ally_catalog_javascript_parity PROPERTIES TIMEOUT 60)
      add_test(NAME wave_catalog_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/wave_catalog_parity.mjs"
        --reference "${STRONGHOLD_REFERENCE_DIR}" --native "$<TARGET_FILE:stronghold_wave_catalog_probe>")
      set_tests_properties(wave_catalog_javascript_parity PROPERTIES TIMEOUT 60)
      add_test(NAME stage_catalog_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/stage_catalog_parity.mjs"
        --reference "${STRONGHOLD_REFERENCE_DIR}" --native "$<TARGET_FILE:stronghold_stage_catalog_probe>")
      set_tests_properties(stage_catalog_javascript_parity PROPERTIES TIMEOUT 60)
      add_test(NAME combat_catalog_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/combat_catalog_parity.mjs"
        --reference "${STRONGHOLD_REFERENCE_DIR}" --native "$<TARGET_FILE:stronghold_combat_catalog_probe>")
      set_tests_properties(combat_catalog_javascript_parity PROPERTIES TIMEOUT 60)
      add_test(NAME javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/parity.mjs"
        --reference "${STRONGHOLD_REFERENCE_DIR}" --native "$<TARGET_FILE:stronghold_cli>")
      set_tests_properties(javascript_parity PROPERTIES TIMEOUT 60)
      add_test(NAME battle_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/battle_parity.mjs"
        --reference "${STRONGHOLD_REFERENCE_DIR}" --native "$<TARGET_FILE:stronghold_battle_probe>")
      set_tests_properties(battle_javascript_parity PROPERTIES TIMEOUT 60)
      add_test(NAME effects_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/effects_parity.mjs"
        --reference "${STRONGHOLD_REFERENCE_DIR}" --native "$<TARGET_FILE:stronghold_effect_tests>")
      set_tests_properties(effects_javascript_parity PROPERTIES TIMEOUT 60)
      add_test(NAME skill_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/skill_parity.mjs"
        --reference "${STRONGHOLD_REFERENCE_DIR}" --native "$<TARGET_FILE:stronghold_skill_tests>")
      set_tests_properties(skill_javascript_parity PROPERTIES TIMEOUT 60)
      add_test(NAME element_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/element_parity.mjs"
        --reference "${STRONGHOLD_REFERENCE_DIR}" --native "$<TARGET_FILE:stronghold_element_tests>")
      set_tests_properties(element_javascript_parity PROPERTIES TIMEOUT 60)
      add_test(NAME attack_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/attack_parity.mjs"
        --reference "${STRONGHOLD_REFERENCE_DIR}" --native "$<TARGET_FILE:stronghold_attack_tests>")
      set_tests_properties(attack_javascript_parity PROPERTIES TIMEOUT 60)
      add_test(NAME field_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/field_parity.mjs"
        --reference "${STRONGHOLD_REFERENCE_DIR}" --native "$<TARGET_FILE:stronghold_field_probe>")
      set_tests_properties(field_javascript_parity PROPERTIES TIMEOUT 60)
      add_test(NAME pathing_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/pathing_parity.mjs"
        --reference "${STRONGHOLD_REFERENCE_DIR}" --native "$<TARGET_FILE:stronghold_pathing_probe>")
      set_tests_properties(pathing_javascript_parity PROPERTIES TIMEOUT 60)
      add_test(NAME displaced_pathing_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/pathing_parity.mjs" --displaced
        --reference "${STRONGHOLD_REFERENCE_DIR}" --native "$<TARGET_FILE:stronghold_pathing_probe>")
      set_tests_properties(displaced_pathing_javascript_parity PROPERTIES TIMEOUT 60)
      foreach(probe IN ITEMS body stealth terrain turret cold_wind skill_trigger healing enemy_healing boss_pool carry profession_attack profession_state profession_effect profession_projectile tactical doll bounty unite settlement strategy_draft final_assault special_draft layer_gains)
        add_test(NAME ${probe}_javascript_parity COMMAND "${STRONGHOLD_NODE}"
          "${PROJECT_SOURCE_DIR}/tests/${probe}_parity.mjs"
          --reference "${STRONGHOLD_REFERENCE_DIR}" --native "$<TARGET_FILE:stronghold_${probe}_probe>")
        set_tests_properties(${probe}_javascript_parity PROPERTIES TIMEOUT 60)
      endforeach()
      add_test(NAME spawn_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/spawn_parity.mjs"
        --reference "${STRONGHOLD_REFERENCE_DIR}" --native "$<TARGET_FILE:stronghold_spawn_tests>")
      set_tests_properties(spawn_javascript_parity PROPERTIES TIMEOUT 60)
      add_test(NAME displacement_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/displacement_parity.mjs"
        --reference "${STRONGHOLD_REFERENCE_DIR}" --native "$<TARGET_FILE:stronghold_displacement_probe>")
      set_tests_properties(displacement_javascript_parity PROPERTIES TIMEOUT 60)
      add_test(NAME induced_movement_javascript_parity COMMAND "${STRONGHOLD_NODE}"
        "${PROJECT_SOURCE_DIR}/tests/pathing_parity.mjs" --induced
        --reference "${STRONGHOLD_REFERENCE_DIR}" --native "$<TARGET_FILE:stronghold_pathing_probe>")
      set_tests_properties(induced_movement_javascript_parity PROPERTIES TIMEOUT 60)
    endif()
  endif()
endif()
