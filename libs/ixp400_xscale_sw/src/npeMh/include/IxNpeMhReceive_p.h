/**
 * @file IxNpeMhReceive_p.h
 *
 * @author Intel Corporation
 * @date 18 Jan 2002
 *
 * @brief This file contains the private API for the Receive module.
 *
 * 
 * @par
 * IXP400 SW Release version 2.1
 * 
 * -- Copyright Notice --
 * 
 * @par
 * Copyright (c) 2001-2005, Intel Corporation.
 * All rights reserved.
 * 
 * @par
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * 
 * @par
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * 
 * @par
 * -- End of Copyright Notice --
*/

/**
 * @defgroup IxNpeMhReceive_p IxNpeMhReceive_p
 *
 * @brief The private API for the Receive module.
 * 
 * @{
 */

#ifndef IXNPEMHRECEIVE_P_H
#define IXNPEMHRECEIVE_P_H

#include "IxNpeMh.h"
#include "ixp_osal/IxOsalTypes.h"

/*
 * #defines for function return types, etc.
 */

/*
 * Prototypes for interface functions.
 */

/**
 * @fn void ixNpeMhReceiveInitialize (void)
 *
 * @brief This function registers an internal ISR to handle the NPEs'
 * "outFIFO not empty" interrupts and receive messages from the NPEs when
 * they become available.
 *
 * @return No return value.
 */

void ixNpeMhReceiveInitialize (void);

/**
 * @fn void ixNpeMhReceiveUninitialize (void)
 *
 * @brief This function unregisters the internal ISR to handle the NPEs'
 * "outFIFO not empty" interrupt.
 *
 * @return No return value.
 */

void ixNpeMhReceiveUninitialize (void);


/**
 * @fn void ixNpeMhReceiveMessagesReceive (
           IxNpeMhNpeId npeId)
 *
 * @brief This function reads messages from a particular NPE's outFIFO
 * until the outFIFO is empty, and for each message looks first for an
 * unsolicited callback, then a solicited callback, to pass the message
 * back to the client.  If no callback can be found the message is
 * discarded and an error reported. This function will return TIMEOUT 
 * status if NPE hang / halt.
 *
 * @param IxNpeMhNpeId npeId (in) - The ID of the NPE to receive
 * messages from.
 *
 * @return The function returns a status indicating success, failure or timeout.
 */

IX_STATUS ixNpeMhReceiveMessagesReceive (
    IxNpeMhNpeId npeId);

/**
 * @fn void ixNpeMhReceiveShow (
           IxNpeMhNpeId npeId)
 *
 * @brief This function will display the current state of the Receive
 * module.
 *
 * @param IxNpeMhNpeId npeId (in) - The ID of the NPE to display state
 * information for.
 *
 * @return No return status.
 */

void ixNpeMhReceiveShow (
    IxNpeMhNpeId npeId);

/**
 * @fn void ixNpeMhReceiveShowReset (
           IxNpeMhNpeId npeId)
 *
 * @brief This function will reset the current state of the Receive
 * module.
 *
 * @param IxNpeMhNpeId npeId (in) - The ID of the NPE to reset state
 * information for.
 *
 * @return No return status.
 */

void ixNpeMhReceiveShowReset (
    IxNpeMhNpeId npeId);

#endif /* IXNPEMHRECEIVE_P_H */

/**
 * @} defgroup IxNpeMhReceive_p
 */
