
#include "Vanilla1121_functions.h"
#include "modernNameplateDistance.h"
#include "inSight.h"
#include "distanceBetween.h"
#include "timer.h"
#include "performanceProfiling.h"
#include "editCamera.h"

// Pointer is __thiscall with first parameter being THIS.
// Detoured functions use __fastcall with THIS as the first parameter
// and an ignored second parameter.
ADDNAMEPLATE p_addNameplate =
    reinterpret_cast<ADDNAMEPLATE>(0x6086e0);

ADDNAMEPLATE p_original_addNameplate = NULL;

RENDERWORLD p_renderWorld =
    reinterpret_cast<RENDERWORLD>(0x482D70);

RENDERWORLD p_original_renderWorld = NULL;

REMOVENAMEPLATE p_removeNameplate =
    reinterpret_cast<REMOVENAMEPLATE>(0x608A10);

// Nameplate options.
bool modernNameplateDistance = true;
bool prioritizeTargetNameplate = false;
bool deprioritizeTargetNameplate = false;
unsigned int deprioritizeTargetNameplateDelayMs = 3000;
bool prioritizeMarkedNameplate = false;
bool nameplateCombatFilter = false;
bool showInCombatNameplatesNearPlayer = false;
bool hideCritterNameplate = true;

static bool nameplatesHasMarkOnThem = false;
static uint64_t delayedHideTargetGUID = 0;
static DWORD targetSelectedTick = 0;

// Return values:
// -1 = error
//  0 = do not show nameplate
//  1 = show nameplate
static int shouldHaveNameplate(void* voidUnit)
{
    if (voidUnit == NULL ||
        (reinterpret_cast<intptr_t>(voidUnit) & 1) != 0)
    {
        return -1;
    }

    uint32_t unit = reinterpret_cast<uint32_t>(voidUnit);

    uint64_t guidUnderNameplate = vanilla1121_unitGUID(unit);
    uint64_t targetGUID = 0;
if (deprioritizeTargetNameplate ||
    prioritizeMarkedNameplate || prioritizeTargetNameplate)
{
    targetGUID = vanilla1121_unitGUID("target");
}

    if (deprioritizeTargetNameplate && targetGUID > 0)
{
    // Start or restart the timer when the target changes.
    if (targetGUID != delayedHideTargetGUID)
    {
        delayedHideTargetGUID = targetGUID;
        targetSelectedTick = GetTickCount();
    }

    // Hide the current target's nameplate after the configured delay.
    if (guidUnderNameplate == targetGUID &&
        (DWORD)(GetTickCount() - targetSelectedTick) >=
            deprioritizeTargetNameplateDelayMs)
    {
        return 0;
    }
}
else
{
    delayedHideTargetGUID = 0;
}

    // Existing target and marked-nameplate prioritization.
    if (prioritizeMarkedNameplate || prioritizeTargetNameplate)
    {
        if (targetGUID > 0 || nameplatesHasMarkOnThem)
        {
            if (prioritizeTargetNameplate &&
                prioritizeMarkedNameplate)
            {
                if (guidUnderNameplate == targetGUID ||
                    vanilla1121_getTargetMark(guidUnderNameplate) > 0)
                {
                    return 1;
                }
                else
                {
                    return 0;
                }
            }

            if (prioritizeTargetNameplate &&
                !prioritizeMarkedNameplate)
            {
                if (targetGUID > 0)
                {
                    if (guidUnderNameplate == targetGUID)
                    {
                        return 1;
                    }
                    else
                    {
                        return 0;
                    }
                }
            }

            if (!prioritizeTargetNameplate &&
                prioritizeMarkedNameplate)
            {
                if (nameplatesHasMarkOnThem)
                {
                    if (vanilla1121_getTargetMark(guidUnderNameplate) > 0)
                    {
                        return 1;
                    }
                    else
                    {
                        return 0;
                    }
                }
            }
        }
    }

    // Existing critter-nameplate filter.
    if (hideCritterNameplate)
    {
        bool typeCheck =
            (vanilla1121_objectType(unit) == OBJECT_TYPE_Unit ||
             vanilla1121_objectType(unit) == OBJECT_TYPE_Player) &&
            vanilla1121_unitCreatureType(unit) == 8;

        if (typeCheck)
        {
            bool inCombat = vanilla1121_unitInCombat(unit);

            if (!inCombat)
            {
                return 0;
            }
        }
    }

    // Existing combat filter.
    if (nameplateCombatFilter) {
    uint32_t unit = reinterpret_cast<uint32_t>(voidUnit);

    bool fullHP =
        vanilla1121_unitCurrentHP(unit) >=
        vanilla1121_unitMaximumHP(unit);

    bool inCombat = vanilla1121_unitInCombat(unit);

    if (fullHP && !inCombat) {
        return 0;
    }
}

    // Existing behavior: show nearby units in combat.
    const float nearPlayerDistance = 8.0f;

    if (showInCombatNameplatesNearPlayer &&
        vanilla1121_unitInCombat(unit))
    {
        float distance = UnitXP_distanceBetween(
            guidUnderNameplate,
            vanilla1121_unitGUID("player"),
            METER_RANGED
        );

        if (distance >= 0.0f &&
            distance < nearPlayerDistance)
        {
            return 1;
        }
    }

    // Existing line-of-sight and camera-distance checks.
    bool inSight = (camera_inSight(voidUnit) > 0);

    C3Vector pos0 = editCamera_translatedPosition();
    C3Vector pos1 = vanilla1121_unitPosition(unit);

    float distance = UnitXP_distanceBetween(pos0, pos1);

    const float cameraSeeThroughWallDistance = 10.0f;

    if (distance > cameraSeeThroughWallDistance &&
        inSight == false)
    {
        return 0;
    }

    return 1;
}


// Check whether any currently allocated nameplate has a target mark.
static void refreshMarkStatus()
{
    nameplatesHasMarkOnThem = false;

    // Linked list containing the nameplates.
    uint32_t nameplate_item =
        *reinterpret_cast<uint32_t*>(0xc4d92c);

    while (nameplate_item != 0 &&
           (nameplate_item & 1) == 0)
    {
        uint32_t next_item =
            *reinterpret_cast<uint32_t*>(nameplate_item + 0x4e0);

        uint64_t guidUnderNameplate =
            *reinterpret_cast<uint64_t*>(nameplate_item + 0x4e8);

        if (vanilla1121_getTargetMark(guidUnderNameplate) > 0)
        {
            nameplatesHasMarkOnThem = true;
            return;
        }

        nameplate_item = next_item;
    }
}


int __fastcall detoured_renderWorld(void* self, void* ignored)
{
    if (self && modernNameplateDistance)
    {
        if (prioritizeMarkedNameplate)
        {
            refreshMarkStatus();
        }

        // Walk the linked list of active nameplates.
        uint32_t nameplate_item =
            *reinterpret_cast<uint32_t*>(0xc4d92c);

        while (nameplate_item != 0 &&
               (nameplate_item & 1) == 0)
        {
            // Save the next node before potentially removing this one.
            uint32_t next_item =
                *reinterpret_cast<uint32_t*>(nameplate_item + 0x4e0);

            uint64_t guidUnderNameplate =
                *reinterpret_cast<uint64_t*>(nameplate_item + 0x4e8);

            uint32_t unitUnderNameplate =
                vanilla1121_getVisiableObject(guidUnderNameplate);

            if (unitUnderNameplate == 0 ||
                (unitUnderNameplate & 1) != 0)
            {
                nameplate_item = next_item;
                continue;
            }

            // Skip objects that are not units or players.
            int type = vanilla1121_objectType(unitUnderNameplate);

            if (type != OBJECT_TYPE_Unit &&
                type != OBJECT_TYPE_Player)
            {
                nameplate_item = next_item;
                continue;
            }

            if (shouldHaveNameplate(
                    reinterpret_cast<void*>(unitUnderNameplate)) == 0)
            {
                p_removeNameplate(unitUnderNameplate);
            }

            nameplate_item = next_item;
        }
    }

    // Existing timer callback.
    if (self)
    {
        perfSetSlotName(4, "timer callback");
        perfMarkStart(4);
        gTimer.execute();
        perfMarkEnd(4);
    }

    // Call the original RenderWorld function.
    perfSetSlotName(3, "original RenderWorld");
    perfMarkStart(3);

    int result = p_original_renderWorld(self);

    perfMarkEnd(3);

    return result;
}


void __fastcall detoured_addNameplate(
    void* self,
    void* ignored,
    void* unknown1,
    void* unknown2)
{
    if (self && modernNameplateDistance)
    {
        uint32_t unit = reinterpret_cast<uint32_t>(self);

        if (unit == 0 || (unit & 1) != 0)
        {
            return;
        }

        // Skip objects that are not units or players.
        int type = vanilla1121_objectType(unit);

        if (type == OBJECT_TYPE_Unit ||
            type == OBJECT_TYPE_Player)
        {
            if (shouldHaveNameplate(self) == 0)
            {
                return;
            }
        }
    }

    // Preserve the original nameplate creation call.
    return p_original_addNameplate(self, unknown1, unknown2);
}
