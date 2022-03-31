/*
 *
 *    Copyright (c) 2022 Project CHIP Authors
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/**
 *    @file
 *          A 'Fail Safe Context' SHALL be created on the receiver, to track fail-safe
 *          state information while the fail-safe is armed.
 */

#pragma once

#include <platform/internal/CHIPDeviceLayerInternal.h>

namespace chip {
namespace DeviceLayer {

class FailSafeContext
{
public:
    // ===== Members for internal use by other Device Layer components.

    /**
     * @brief
     *  Only a single fail-safe timer is started on the device, if this function is called again
     *  when the fail-safe timer is currently armed, the currently-running fail-safe timer will
     *  first be cancelled, then the fail-safe timer will be re-armed.
     */
    CHIP_ERROR ArmFailSafe(FabricIndex accessingFabricIndex, System::Clock::Timeout expiryLength);
    CHIP_ERROR DisarmFailSafe();
    CHIP_ERROR SetAddNocCommandInvoked(FabricIndex nocFabricIndex);
    CHIP_ERROR SetUpdateNocCommandInvoked();

    inline bool IsFailSafeArmed(FabricIndex accessingFabricIndex) const
    {
        return mFailSafeArmed && MatchesFabricIndex(accessingFabricIndex);
    }

    // Returns true if the fail-safe is in a state where commands that require an armed
    // fail-safe can no longer execute, but a new fail-safe can't be armed yet.
    inline bool IsFailSafeBusy() const { return mFailSafeBusy; }

    inline bool IsFailSafeArmed() const { return mFailSafeArmed; }

    inline void SetFailSafeBusy(bool val) { mFailSafeBusy = val; }

    inline bool MatchesFabricIndex(FabricIndex accessingFabricIndex) const
    {
        VerifyOrDie(mFailSafeArmed);
        return (accessingFabricIndex == mFabricIndex);
    }

    inline bool NocCommandHasBeenInvoked() const { return mAddNocCommandHasBeenInvoked || mUpdateNocCommandHasBeenInvoked; }
    inline bool AddNocCommandHasBeenInvoked() { return mAddNocCommandHasBeenInvoked; }
    inline bool UpdateNocCommandHasBeenInvoked() { return mUpdateNocCommandHasBeenInvoked; }

    inline FabricIndex GetFabricIndex() const
    {
        VerifyOrDie(mFailSafeArmed);
        return mFabricIndex;
    }

    static CHIP_ERROR LoadFromStorage(FabricIndex & fabricIndex, bool & addNocCommandInvoked, bool & updateNocCommandInvoked);
    static CHIP_ERROR DeleteFromStorage();

private:
    // ===== Private members reserved for use by this class only.

    bool mFailSafeArmed                  = false;
    bool mFailSafeBusy                   = false;
    bool mAddNocCommandHasBeenInvoked    = false;
    bool mUpdateNocCommandHasBeenInvoked = false;
    FabricIndex mFabricIndex             = kUndefinedFabricIndex;

    // TODO:: Track the state of what was mutated during fail-safe.

    static constexpr size_t FailSafeContextTLVMaxSize()
    {
        return TLV::EstimateStructOverhead(sizeof(FabricIndex), sizeof(bool), sizeof(bool));
    }

    /**
     * @brief
     *  The callback function to be called when "fail-safe timer" expires.
     */
    static void HandleArmFailSafeTimer(System::Layer * layer, void * aAppState);

    /**
     * @brief
     *  The callback function to be called asynchronously after various cleanup work has completed
     *  to actually disarm the fail-safe.
     */
    static void HandleDisarmFailSafe(intptr_t arg);

    void FailSafeTimerExpired();
    CHIP_ERROR CommitToStorage();
};

} // namespace DeviceLayer
} // namespace chip
