
#pragma once

#include "../../BlockEntities/BeehiveEntity.h"
#include "../../BlockInfo.h"
#include "../../Blocks/BlockComparator.h"





namespace RedstoneComparatorHandler
{
	static unsigned char GetFrontPowerLevel(NIBBLETYPE a_Meta, unsigned char a_HighestSidePowerLevel, unsigned char a_HighestRearPowerLevel)
	{
		if (cBlockComparatorHandler::IsInSubtractionMode(a_Meta))
		{
			// Subtraction mode
			return static_cast<unsigned char>(std::max(static_cast<char>(a_HighestRearPowerLevel) - a_HighestSidePowerLevel, 0));
		}
		else
		{
			// Comparison mode
			return (a_HighestRearPowerLevel < a_HighestSidePowerLevel) ? 0 : a_HighestRearPowerLevel;
		}
	}

	static PowerLevel GetPowerDeliveredToPosition(const cChunk & a_Chunk, Vector3i a_Position, BLOCKTYPE a_BlockType, Vector3i a_QueryPosition, BLOCKTYPE a_QueryBlockType, bool IsLinked)
	{
		UNUSED(a_QueryPosition);
		UNUSED(a_QueryBlockType);

		const auto Meta = a_Chunk.GetMeta(a_Position);
		return (
			(cBlockComparatorHandler::GetFrontCoordinate(a_Position, Meta & 0x3) == a_QueryPosition) ?
			DataForChunk(a_Chunk).GetCachedPowerData(a_Position) : 0
		);
	}

	static PowerLevel GetBeeHomePower(cChunk & a_Chunk, Vector3i a_Position)
	{
		const auto * const Hive = static_cast<cBeehiveEntity *>(a_Chunk.GetBlockEntityRel(a_Position));
		return (Hive != nullptr) ? static_cast<PowerLevel>(Hive->GetHoneyLevel()) : 0;
	}

	static unsigned char GetPowerLevel(cChunk & a_Chunk, Vector3i Position, BLOCKTYPE BlockType, NIBBLETYPE Meta)
	{
		UInt8 SignalStrength = 0;
		auto RearCoordinate = cBlockComparatorHandler::GetRearCoordinate(Position, Meta & 0x3);
		const auto RearOffset = RearCoordinate - Position;

		auto RearChunk = a_Chunk.GetRelNeighborChunkAdjustCoords(RearCoordinate);
		if ((RearChunk == nullptr) || !RearChunk->IsValid())
		{
			return SignalStrength;
		}

		const auto RearType = RearChunk->GetBlock(RearCoordinate);
		if (RearType == E_BLOCK_BEEHIVE)
		{
			return GetBeeHomePower(*RearChunk, RearCoordinate);
		}

		RearChunk->DoWithBlockEntityAt(RearCoordinate, [&](cBlockEntity & a_BlockEntity)
		{
			// Skip BlockEntities that don't have slots
			auto BlockEntityWithItems = dynamic_cast<cBlockEntityWithItems *>(&a_BlockEntity);
			if (BlockEntityWithItems == nullptr)
			{
				return false;
			}

			// TODO: handle double chests

			auto & Contents = BlockEntityWithItems->GetContents();
			float Fullness = 0;  // Is a floating-point type to allow later calculation to produce a non-truncated value

			for (int Slot = 0; Slot != Contents.GetNumSlots(); ++Slot)
			{
				Fullness += static_cast<float>(Contents.GetSlot(Slot).m_ItemCount) / Contents.GetSlot(Slot).GetMaxStackSize();
			}

			SignalStrength = (Fullness < 0.001 /* container empty? */) ? 0 : static_cast<UInt8>(1 + (Fullness / Contents.GetNumSlots()) * 14);
			return false;
		});

		const auto RearPower = std::max(
			SignalStrength,
			RedstoneHandler::GetPowerDeliveredToPosition(
				*RearChunk, RearCoordinate, RearType,
				cIncrementalRedstoneSimulatorChunkData::RebaseRelativePosition(a_Chunk, *RearChunk, Position), BlockType, false
			)
		);

		// Comparators can read a bee home's analog output through one opaque full block.
		if ((RearPower < 15) && !cBlockInfo::IsTransparent(RearType) && cBlockInfo::FullyOccupiesVoxel(RearType))
		{
			auto Source = RearCoordinate + RearOffset;
			const auto SourceChunk = RearChunk->GetRelNeighborChunkAdjustCoords(Source);
			if ((SourceChunk != nullptr) && SourceChunk->IsValid() && (SourceChunk->GetBlock(Source) == E_BLOCK_BEEHIVE))
			{
				// A fully powered conductor takes precedence over the analog signal behind it.
				ForEachSourceCallback Input(a_Chunk, Position, BlockType);
				Input(Position + RearOffset);
				return (Input.Power == 15) ? Input.Power : GetBeeHomePower(*SourceChunk, Source);
			}
		}
		return RearPower;
	}

	static void Update(cChunk & a_Chunk, cChunk & CurrentlyTicking, Vector3i a_Position, BLOCKTYPE a_BlockType, NIBBLETYPE a_Meta, const PowerLevel Power)
	{
		// Note that Power here contains the maximum * side * power level, as specified by GetValidSourcePositions
		// LOGD("Evaluating ALU the comparator (%d %d %d)", a_Position.x, a_Position.y, a_Position.z);

		auto & Data = DataForChunk(a_Chunk);
		auto DelayInfo = Data.GetMechanismDelayInfo(a_Position);

		// Delay is used here to prevent an infinite loop (#3168)
		if (DelayInfo == nullptr)
		{
			const auto RearPower = GetPowerLevel(a_Chunk, a_Position, a_BlockType, a_Meta);
			const auto FrontPower = GetFrontPowerLevel(a_Meta, Power, RearPower);
			const auto PreviousFrontPower = Data.GetCachedPowerData(a_Position);
			const bool ShouldUpdate = (FrontPower != PreviousFrontPower);  // "Business logic" (:P) - determined by side and rear power levels

			if (ShouldUpdate)
			{
				Data.m_MechanismDelays[a_Position] = std::make_pair(1, bool());
			}

			return;
		}

		int DelayTicks;
		std::tie(DelayTicks, std::ignore) = *DelayInfo;

		if (DelayTicks != 0)
		{
			return;
		}

		const auto RearPower = GetPowerLevel(a_Chunk, a_Position, a_BlockType, a_Meta);
		const auto FrontPower = GetFrontPowerLevel(a_Meta, Power, RearPower);
		const NIBBLETYPE NewMeta = (FrontPower > 0) ? (a_Meta | 0x08u) : (a_Meta & 0x07u);

		// Don't care about the previous power level so return value ignored
		Data.ExchangeUpdateOncePowerData(a_Position, FrontPower);

		a_Chunk.SetMeta(a_Position, NewMeta);
		Data.m_MechanismDelays.erase(a_Position);

		// Assume that an update (to front power) is needed:
		UpdateAdjustedRelative(a_Chunk, CurrentlyTicking, a_Position, cBlockComparatorHandler::GetFrontCoordinate(a_Position, a_Meta & 0x3) - a_Position);
	}

	static void ForValidSourcePositions(const cChunk & a_Chunk, Vector3i a_Position, BLOCKTYPE a_BlockType, NIBBLETYPE a_Meta, ForEachSourceCallback & Callback)
	{
		UNUSED(a_Chunk);
		UNUSED(a_BlockType);

		Callback(cBlockComparatorHandler::GetSideCoordinate(a_Position, a_Meta & 0x3, false));
		Callback(cBlockComparatorHandler::GetSideCoordinate(a_Position, a_Meta & 0x3, true));
	}
};
