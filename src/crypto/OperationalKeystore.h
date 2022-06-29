/*
 *    Copyright (c) 2022 Project CHIP Authors
 *    All rights reserved.
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

#pragma once

#include <crypto/CHIPCryptoPAL.h>
#include <lib/core/CHIPError.h>
#include <lib/core/DataModelTypes.h>
#include <lib/support/Span.h>

namespace chip {
namespace Crypto {

class OperationalKeystore
{
public:
    virtual ~OperationalKeystore() {}

    // ==== API designed for commisionables to support fail-safe (although can be used by controllers) ====

    /**
     * @brief Returns true if a pending operational key exists from a previous
     *        `NewOpKeypairForFabric` before any `CommitOpKeypairForFabric` or
     *        `RevertOpKeypairForFabric`. This returns true even if the key is
     *        NOT ACTIVE (i.e after `NewOpKeypairForFabric` but before
     *        `ActivateOpKeypairForFabric`).
     */
    virtual bool HasPendingOpKeypair() const = 0;

    /**
     * @brief Returns whether a usable operational key exists for the given fabric.
     *
     * Returns true even if the key is not persisted, such as if `ActivateOpKeypairForFabric`
     * had been successfully called for a given fabric. Only returns true if a key
     * is presently usable such that `SignWithOpKeypair` would succeed for the fabric. Therefore
     * if there was no previously persisted key and `NewOpKeypairForFabric` had been called
     * but not `ActivateOpKeypairForFabric`, there is only an inactive key, and this would return false.
     *
     * @param fabricIndex - FabricIndex for which availability of keypair will be checked.
     * @return true if there an active operational keypair for the given FabricIndex, false otherwise.
     */
    virtual bool HasOpKeypairForFabric(FabricIndex fabricIndex) const = 0;

    /**
     * @brief This initializes a new keypair for the given fabric and generates a CSR for it,
     *        so that it can be passed in a CSRResponse.
     *
     * The keypair is temporary and becomes usable for `SignWithOpKeypair` only after either
     * `ActivateOpKeypairForFabric` is called. It is destroyed if
     * `RevertPendingKeypair` is called before `CommitOpKeypairForFabric`.
     *  If a pending keypair already existed for the given `fabricIndex`, it is replaced by this call.
     *
     *  Only one pending operational keypair is supported at a time.
     *
     * @param fabricIndex - FabricIndex for which a new keypair must be made available
     * @param outCertificateSigningRequest - Buffer to contain the CSR. Must be at least `kMAX_CSR_Length` large.
     *
     * @retval CHIP_NO_ERROR on success
     * @retval CHIP_ERROR_BUFFER_TOO_SMALL if `outCertificateSigningRequest` buffer is too small
     * @retval CHIP_ERROR_INCORRECT_STATE if the key store is not properly initialized.
     * @retval CHIP_ERROR_INVALID_FABRIC_INDEX if there is already a pending keypair for another `fabricIndex` value
     *                                         or if fabricIndex is an invalid value.
     * @retval CHIP_ERROR_NOT_IMPLEMENTED if only `SignWithOpKeypair` is supported
     * @retval other CHIP_ERROR value on internal crypto engine errors
     */
    virtual CHIP_ERROR NewOpKeypairForFabric(FabricIndex fabricIndex, MutableByteSpan & outCertificateSigningRequest) = 0;

    /**
     * @brief Temporarily activates the operational keypair last generated with `NewOpKeypairForFabric`, so
     *        that `SignWithOpKeypair` starts using it, but only if it matches the public key associated
     *        with the last NewOpKeypairForFabric.
     *
     * This is to be used by AddNOC and UpdateNOC so that a prior key generated by NewOpKeypairForFabric
     * can be used for CASE while not committing it yet to permanent storage to remain after fail-safe.
     *
     * @param fabricIndex - FabricIndex for which to activate the keypair, used for security cross-checking
     * @param nocPublicKey - Subject public key associated with an incoming NOC
     *
     * @retval CHIP_NO_ERROR on success
     * @retval CHIP_ERROR_INCORRECT_STATE if the key store is not properly initialized.
     * @retval CHIP_ERROR_INVALID_FABRIC_INDEX if there is no operational keypair for `fabricIndex` from a previous
     *                                         matching `NewOpKeypairForFabric`.
     * @retval CHIP_ERROR_INVALID_PUBLIC_KEY if `nocPublicKey` does not match the public key associated with the
     *                                       key pair from last `NewOpKeypairForFabric`.
     * @retval CHIP_ERROR_NOT_IMPLEMENTED if only `SignWithOpKeypair` is supported
     * @retval other CHIP_ERROR value on internal storage or crypto engine errors
     */
    virtual CHIP_ERROR ActivateOpKeypairForFabric(FabricIndex fabricIndex, const Crypto::P256PublicKey & nocPublicKey) = 0;

    /**
     * @brief Permanently commit the operational keypair last generated with `NewOpKeypairForFabric`,
     *        replacing a prior one previously committed, if any, so that `SignWithOpKeypair` for the
     *        given `FabricIndex` permanently uses the key that was pending.
     *
     * This is to be used when CommissioningComplete is successfully received
     *
     * @param fabricIndex - FabricIndex for which to commit the keypair, used for security cross-checking
     *
     * @retval CHIP_NO_ERROR on success
     * @retval CHIP_ERROR_INCORRECT_STATE if the key store is not properly initialized,
     *                                    or ActivateOpKeypairForFabric not yet called
     * @retval CHIP_ERROR_INVALID_FABRIC_INDEX if there is no pending operational keypair for `fabricIndex`
     * @retval CHIP_ERROR_NOT_IMPLEMENTED if only `SignWithOpKeypair` is supported
     * @retval other CHIP_ERROR value on internal storage or crypto engine errors
     */
    virtual CHIP_ERROR CommitOpKeypairForFabric(FabricIndex fabricIndex) = 0;

    /**
     * @brief Permanently remove the keypair associated with a fabric
     *
     * This is to be used for fail-safe handling and RemoveFabric.  Removes both the
     * pending operational keypair for the fabricIndex (if any) and the committed one (if any).
     *
     * @param fabricIndex - FabricIndex for which to remove the keypair
     *
     * @retval CHIP_NO_ERROR on success
     * @retval CHIP_ERROR_INCORRECT_STATE if the key store is not properly initialized.
     * @retval CHIP_ERROR_INVALID_FABRIC_INDEX if there is no operational keypair for `fabricIndex`
     * @retval CHIP_ERROR_NOT_IMPLEMENTED if only `SignWithOpKeypair` is supported
     * @retval other CHIP_ERROR value on internal storage errors
     */
    virtual CHIP_ERROR RemoveOpKeypairForFabric(FabricIndex fabricIndex) = 0;

    /**
     * @brief Permanently release the operational keypair last generated with `NewOpKeypairForFabric`,
     *        such that `SignWithOpKeypair` uses the previously committed key (if one existed).
     *
     * This is to be used when a fail-safe expires prior to CommissioningComplete.
     *
     * This method cannot error-out and must always succeed, even on a no-op. This should
     * be safe to do given that `CommitOpKeypairForFabric` must succeed to make a new operational
     * keypair usable.
     */
    virtual void RevertPendingKeypair() = 0;

    // ==== Primary operation required: signature
    /**
     * @brief Sign a message with a fabric's currently-active operational keypair.
     *
     * If a Keypair was successfully made temporarily active for the given `fabricIndex` with `ActivateOpKeypairForFabric`,
     * then that is the keypair whose private key is used. Otherwise, the last committed private key
     * is used, if one exists
     *
     * @param fabricIndex - FabricIndex whose operational keypair will be used to sign the `message`
     * @param message - Message to sign with the currently active operational keypair
     * @param outSignature - Buffer to contain the signature
     *
     * @retval CHIP_NO_ERROR on success
     * @retval CHIP_ERROR_INCORRECT_STATE if the key store is not properly initialized.
     * @retval CHIP_ERROR_INVALID_FABRIC_INDEX if no active key is found for the given `fabricIndex` or if
     *                                         `fabricIndex` is invalid.
     * @retval other CHIP_ERROR value on internal crypto engine errors
     *
     */
    virtual CHIP_ERROR SignWithOpKeypair(FabricIndex fabricIndex, const ByteSpan & message,
                                         Crypto::P256ECDSASignature & outSignature) const = 0;

    /**
     * @brief Create an ephemeral keypair for use in session establishment.
     *
     * The caller must Initialize() the P256Keypair if needed. It is not done by this method.
     *
     * This method MUST ONLY be used for CASESession ephemeral keys.
     *
     * NOTE: The stack will allocate as many of these as there are CASE sessions which
     *       can be concurrently in the process of establishment. Implementations must
     *       support more than one such keypair, or be implemented to match the limitations
     *       enforced by a given product on its concurrent CASE session establishment limits.
     *
     * WARNING: The return value MUST be released by `ReleaseEphemeralKeypair`. This is because
     *          Matter CHIPMem.h does not properly support UniquePtr in a way that would
     *          safely allow classes derived from Crypto::P256Keypair to be released properly.
     *
     * @return a pointer to a P256Keypair (or derived class thereof), which may evaluate to nullptr
     *         if running out of memory.
     */
    virtual Crypto::P256Keypair * AllocateEphemeralKeypairForCASE() = 0;

    /**
     * @brief Release an ephemeral keypair previously provided by `AllocateEphemeralKeypairForCASE()`
     */
    virtual void ReleaseEphemeralKeypair(Crypto::P256Keypair * keypair) = 0;
};

} // namespace Crypto
} // namespace chip
