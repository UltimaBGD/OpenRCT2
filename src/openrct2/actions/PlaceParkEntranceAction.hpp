/*****************************************************************************
 * Copyright (c) 2014-2019 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include "../Cheats.h"
#include "../OpenRCT2.h"
#include "../core/MemoryStream.h"
#include "../localisation/StringIds.h"
#include "../management/Finance.h"
#include "../world/Entrance.h"
#include "../world/Footpath.h"
#include "../world/MapAnimation.h"
#include "../world/Park.h"
#include "../world/Sprite.h"
#include "../world/Surface.h"
#include "GameAction.h"

DEFINE_GAME_ACTION(PlaceParkEntranceAction, GAME_COMMAND_PLACE_PARK_ENTRANCE, GameActionResult)
{
private:
    CoordsXYZD _loc;

public:
    PlaceParkEntranceAction()
    {
    }
    PlaceParkEntranceAction(CoordsXYZD location)
        : _loc(location)
    {
    }

    uint16_t GetActionFlags() const override
    {
        return GameActionBase::GetActionFlags() | GA_FLAGS::EDITOR_ONLY;
    }

    void Serialise(DataSerialiser & stream) override
    {
        GameAction::Serialise(stream);

        stream << DS_TAG(_loc);
    }

    GameActionResult::Ptr Query() const override
    {
        if (!(gScreenFlags & SCREEN_FLAGS_EDITOR) && !gCheatsSandboxMode)
        {
            return std::make_unique<GameActionResult>(
                GA_ERROR::NOT_IN_EDITOR_MODE, STR_CANT_BUILD_PARK_ENTRANCE_HERE, STR_NONE);
        }

        auto res = std::make_unique<GameActionResult>();
        res->ExpenditureType = RCT_EXPENDITURE_TYPE_LAND_PURCHASE;
        res->Position = { _loc.x, _loc.y, _loc.z };

        if (!map_check_free_elements_and_reorganise(3))
        {
            return std::make_unique<GameActionResult>(GA_ERROR::NO_FREE_ELEMENTS, STR_CANT_BUILD_PARK_ENTRANCE_HERE, STR_NONE);
        }

        if (_loc.x <= 32 || _loc.y <= 32 || _loc.x >= (gMapSizeUnits - 32) || _loc.y >= (gMapSizeUnits - 32))
        {
            return std::make_unique<GameActionResult>(
                GA_ERROR::INVALID_PARAMETERS, STR_CANT_BUILD_PARK_ENTRANCE_HERE, STR_TOO_CLOSE_TO_EDGE_OF_MAP);
        }

        if (gParkEntrances.size() >= MAX_PARK_ENTRANCES)
        {
            return std::make_unique<GameActionResult>(
                GA_ERROR::INVALID_PARAMETERS, STR_CANT_BUILD_PARK_ENTRANCE_HERE, STR_ERR_TOO_MANY_PARK_ENTRANCES);
        }

        int8_t zLow = _loc.z / 8;
        int8_t zHigh = zLow + 12;
        LocationXY16 entranceLoc = { (int16_t)_loc.x, (int16_t)_loc.y };
        for (uint8_t index = 0; index < 3; index++)
        {
            if (index == 1)
            {
                entranceLoc.x += CoordsDirectionDelta[(_loc.direction - 1) & 0x3].x;
                entranceLoc.y += CoordsDirectionDelta[(_loc.direction - 1) & 0x3].y;
            }
            else if (index == 2)
            {
                entranceLoc.x += CoordsDirectionDelta[(_loc.direction + 1) & 0x3].x * 2;
                entranceLoc.y += CoordsDirectionDelta[(_loc.direction + 1) & 0x3].y * 2;
            }

            if (!map_can_construct_at(entranceLoc.x, entranceLoc.y, zLow, zHigh, { 0b1111, 0 }))
            {
                return std::make_unique<GameActionResult>(
                    GA_ERROR::NO_CLEARANCE, STR_CANT_BUILD_PARK_ENTRANCE_HERE, gGameCommandErrorText, gCommonFormatArgs);
            }

            // Check that entrance element does not already exist at this location
            EntranceElement* entranceElement = map_get_park_entrance_element_at(entranceLoc.x, entranceLoc.y, zLow, false);
            if (entranceElement != nullptr)
            {
                return std::make_unique<GameActionResult>(
                    GA_ERROR::ITEM_ALREADY_PLACED, STR_CANT_BUILD_PARK_ENTRANCE_HERE, STR_NONE);
            }
        }

        return res;
    }

    GameActionResult::Ptr Execute() const override
    {
        auto res = std::make_unique<GameActionResult>();
        res->ExpenditureType = RCT_EXPENDITURE_TYPE_LAND_PURCHASE;
        res->Position = CoordsXYZ{ _loc.x, _loc.y, _loc.z };

        uint32_t flags = GetFlags();

        CoordsXYZD parkEntrance;
        parkEntrance = _loc;

        gParkEntrances.push_back(parkEntrance);

        int8_t zLow = _loc.z / 8;
        int8_t zHigh = zLow + 12;
        CoordsXY entranceLoc = { _loc.x, _loc.y };
        for (uint8_t index = 0; index < 3; index++)
        {
            if (index == 1)
            {
                entranceLoc.x += CoordsDirectionDelta[(_loc.direction - 1) & 0x3].x;
                entranceLoc.y += CoordsDirectionDelta[(_loc.direction - 1) & 0x3].y;
            }
            else if (index == 2)
            {
                entranceLoc.x += CoordsDirectionDelta[(_loc.direction + 1) & 0x3].x * 2;
                entranceLoc.y += CoordsDirectionDelta[(_loc.direction + 1) & 0x3].y * 2;
            }

            if (!(flags & GAME_COMMAND_FLAG_GHOST))
            {
                SurfaceElement* surfaceElement = map_get_surface_element_at(entranceLoc);
                surfaceElement->SetOwnership(OWNERSHIP_UNOWNED);
            }

            TileElement* newElement = tile_element_insert({ entranceLoc.x / 32, entranceLoc.y / 32, zLow }, 0b1111);
            Guard::Assert(newElement != nullptr);
            newElement->SetType(TILE_ELEMENT_TYPE_ENTRANCE);
            auto entranceElement = newElement->AsEntrance();
            if (entranceElement == nullptr)
            {
                Guard::Assert(false);
                return nullptr;
            }
            entranceElement->clearance_height = zHigh;

            if (flags & GAME_COMMAND_FLAG_GHOST)
            {
                newElement->SetGhost(true);
            }

            entranceElement->SetDirection(_loc.direction);
            entranceElement->SetSequenceIndex(index);
            entranceElement->SetEntranceType(ENTRANCE_TYPE_PARK_ENTRANCE);
            entranceElement->SetPathType(gFootpathSelectedId);

            if (!(flags & GAME_COMMAND_FLAG_GHOST))
            {
                footpath_connect_edges(entranceLoc.x, entranceLoc.y, newElement, 1);
            }

            update_park_fences(entranceLoc);
            update_park_fences({ entranceLoc.x - 32, entranceLoc.y });
            update_park_fences({ entranceLoc.x + 32, entranceLoc.y });
            update_park_fences({ entranceLoc.x, entranceLoc.y - 32 });
            update_park_fences({ entranceLoc.x, entranceLoc.y + 32 });

            map_invalidate_tile(entranceLoc.x, entranceLoc.y, newElement->base_height * 8, newElement->clearance_height * 8);

            if (index == 0)
            {
                map_animation_create(MAP_ANIMATION_TYPE_PARK_ENTRANCE, entranceLoc.x, entranceLoc.y, zLow);
            }
        }

        return res;
    }
};
