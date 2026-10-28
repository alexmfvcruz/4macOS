#pragma once
#include <cstdint>
struct C3Cloth {std::uint32_t kind,cloth,selection,mesh;};
// Exact original object selections; no generated replacement geometry.
static constexpr C3Cloth c3StaticCloths[]={{24478,24479,21136,2000726}};
static constexpr C3Cloth c3CharacterCloths[]={
    {21881,21892,22062,1088947},{21882,21893,22084,1005537},
    {21883,21894,22182,1057356},{21884,21895,22178,1008809},
    {21885,21896,22087,1009320}};
static constexpr std::uint32_t c3StaticObject=2000460,c3CharacterObject=1005220;
static constexpr std::uint32_t c3CharacterAction=21548,c3CharacterAnimation=23452;
static constexpr std::uint32_t c3FixedTick=500; // CTMath::GetTimeValue -> 2400 key-time units.
