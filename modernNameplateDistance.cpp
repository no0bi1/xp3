
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

// Per-frame update of the nameplate frame object (vtable slot 14 of the
// nameplate frame class at 0x81de50). Takes dt as a float, ret 4.
NAMEPLATE_UPDATE p_nameplateUpdate =
    reinterpret_cast<NAMEPLATE_UPDATE>(0x7cb950);

NAMEPLATE_UPDATE p_original_nameplateUpdate = NULL;

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

// Fade state for the delayed target nameplate.
// fadingGUID == 0 means no fade is active.
static uint64_t fadingGUID = 0;
static uint32_t fadingAlpha = 255; // 0-255

// Apply the target-nameplate delay only while the player is in combat
// and at least two attackable units that are themselves in combat are present.
static bool shouldApplyTargetNameplateDelay()
{
    uint32_t player = vanilla1121_getVisiableObject(
        vanilla1121_unitGUID("player"));

    if (player == 0 || (player & 1) != 0 ||
        !vanilla1121_unitInCombat(player))
    {
        return false;
    }

    unsigned int enemyCount = 0;

    // Count attackable units in the visible-object manager rather than only
    // active nameplates. This remains stable after the target plate is removed.
    uint32_t objects = *reinterpret_cast<uint32_t*>(0xb41414);
    if (objects == 0 || (objects & 1) != 0)
    {
        return false;
    }

    uint32_t i = *reinterpret_cast<uint32_t*>(objects + 0xac);
    while (i != 0 && (i & 1) == 0)
    {
        int type = vanilla1121_objectType(i);

        if ((type == OBJECT_TYPE_Unit &&
             vanilla1121_unitIsControlledByPlayer(i) == 0) ||
            type == OBJECT_TYPE_Player)
        {
            if (vanilla1121_unitCanBeAttacked(i) == 1 &&
                vanilla1121_unitIsDead(i) == 0 &&
                vanilla1121_unitInCombat(i))
            {
                ++enemyCount;
                if (enemyCount > 1)
                {
                    return true;
                }
            }
        }

        i = *reinterpret_cast<uint32_t*>(
            *reinterpret_cast<int32_t*>(objects + 0xa4) + i + 4);
    }
    return enemyCount > 1;
}

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

    // The hide option always wins for the selected target. Only the
    // multi-enemy combat case gets a grace period before hiding it.
    if (deprioritizeTargetNameplate &&
        targetGUID > 0 &&
        guidUnderNameplate == targetGUID)
    {
        if (shouldApplyTargetNameplateDelay())
        {
            // Start/restart the timer when the target changes or when the
            // multi-enemy combat condition becomes active.
            if (targetGUID != delayedHideTargetGUID)
            {
                delayedHideTargetGUID = targetGUID;
                targetSelectedTick = GetTickCount();
            }

            DWORD elapsed = GetTickCount() - targetSelectedTick;

            // Keep the target plate only until the configured delay expires.
            if (elapsed >= deprioritizeTargetNameplateDelayMs)
            {
                fadingGUID = 0;
                return 0;
            }

            // Fade across the whole configured delay:
            // full alpha at the start, zero alpha as the delay expires.
            float fade = 1.0f - static_cast<float>(elapsed) /
                static_cast<float>(deprioritizeTargetNameplateDelayMs);
            if (fade < 0.0f) fade = 0.0f;
            if (fade > 1.0f) fade = 1.0f;

            fadingGUID = targetGUID;
            fadingAlpha = static_cast<uint32_t>(fade * 255.0f);

            // Keep the target nameplate visible until the delay expires.
            return 1;
        }

        // Outside multi-enemy combat, hide the target immediately.
        delayedHideTargetGUID = 0;
        fadingGUID = 0;
        return 0;
    }

    // Do not carry elapsed delay time into a later target/combat state.
    if (!deprioritizeTargetNameplate || targetGUID == 0 ||
        targetGUID != delayedHideTargetGUID ||
        !shouldApplyTargetNameplateDelay())
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
    // Reset the fade each frame. shouldHaveNameplate() below sets it again
    // while the delayed target nameplate is still fading.
    fadingGUID = 0;

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


// Runs the engine's own per-frame nameplate update, then overrides alpha for
// the delayed target nameplate. The engine resets alpha to 0xFF/0x7F whenever
// it differs, so the override has to be applied after the original every frame.
void __fastcall detoured_nameplateUpdate(void* self, void* ignored, float dt)
{
    p_original_nameplateUpdate(self, dt);

    if (self && fadingGUID != 0 && fadingAlpha < 255)
    {
        uint32_t frame = reinterpret_cast<uint32_t>(self);
        if ((frame & 1) != 0)
        {
            return;
        }

        uint64_t guid = *reinterpret_cast<uint64_t*>(frame + 0x4e8);
        if (guid == fadingGUID)
        {
            // Vtable slot 10 is SetAlpha(uint8), __thiscall.
            typedef void(__thiscall* SETALPHA)(void*, uint32_t);
            void** vtable = *reinterpret_cast<void***>(self);
            auto setAlpha = reinterpret_cast<SETALPHA>(vtable[10]);
            setAlpha(self, fadingAlpha);
        }
    }
}
