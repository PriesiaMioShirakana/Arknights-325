#ifndef STRONGHOLD_ADAPTERS_REFERENCE_WAVE_GENERATION_HPP
#define STRONGHOLD_ADAPTERS_REFERENCE_WAVE_GENERATION_HPP
#include <stronghold/domain/wave_generation.hpp>
namespace Stronghold
{
	// 静态配置无所有权分配；调用返回的引用可贯穿整个对局。
	[[nodiscard]] const WaveGenerationRules& ReferenceWaveGeneration() noexcept;
	[[nodiscard]] std::span<const WaveModeRules> ReferenceWaveModes() noexcept;
	[[nodiscard]] const WaveModeRules& ReferenceWaveMode(std::string_view _id);
}
#endif
