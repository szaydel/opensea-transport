//
// Do NOT modify or remove this copyright and license
//
// Copyright (c) 2012-2023 Seagate Technology LLC and/or its Affiliates, All Rights Reserved
//
// This software is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// ****************************************************************************************** *****************************************************************************

// \file usb_hacks.h
// \brief Set of functions to check or make modifications to commands to work on USB bridges that don't always follow SCSI/SAT specs.

#pragma once

#include "common_public.h"

#if defined (__cplusplus)
extern "C"
{
#endif

    //-----------------------------------------------------------------------------
    //
    //  set_ATA_Passthrough_Type_By_Trial_And_Error(tDevice* device)
    //
    //! \brief   Description:  Attempts to figure out the ATA passthrough method of external (USB and IEEE1394) products by issueing identify commands with different passthrough types until success is found
    //
    //  Entry:
    //!   \param[in] device = file descriptor
    //!
    //  Exit:
    //!   \return VOID
    //
    //-----------------------------------------------------------------------------
    OPENSEA_TRANSPORT_API bool set_ATA_Passthrough_Type_By_Trial_And_Error(tDevice* device);

#if defined (__cplusplus)
}
#endif
