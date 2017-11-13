//
// Do NOT modify or remove this copyright and license
//
// Copyright (c) 2012 - 2017 Seagate Technology LLC and/or its Affiliates, All Rights Reserved
//
// This software is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// ******************************************************************************************
// 
// \file nec_legacy_helper.c   Implementation for NEC Legacy USB Pass-through CDBs

#include "nec_legacy_helper.h"
#include "scsi_helper.h"
#include "scsi_helper_func.h"
#include "ata_helper_func.h"

int build_NEC_Legacy_CDB(uint8_t cdb[16], ataPassthroughCommand *ataCommandOptions)
{
    int ret = SUCCESS;
    memset(cdb, 0, CDB_LEN_16);
    cdb[OPERATION_CODE] = NEC_WRITE_OPCODE;
    cdb[1] = NEC_WRAPPER_SIGNATURE;
    cdb[2] = ataCommandOptions->tfr.CommandStatus;
    cdb[3] = ataCommandOptions->tfr.DeviceHead;
    if (ataCommandOptions->commandType == ATA_CMD_TYPE_EXTENDED_TASKFILE)
    {
        //Ext LBA or cylinders
        cdb[4] = ataCommandOptions->tfr.LbaHi48;
        cdb[5] = ataCommandOptions->tfr.LbaMid48;
        cdb[6] = ataCommandOptions->tfr.LbaLow48;
        //Ext Sector count
        cdb[10] = ataCommandOptions->tfr.SectorCount48;
    }
    cdb[7] = ataCommandOptions->tfr.LbaHi;
    cdb[8] = ataCommandOptions->tfr.LbaMid;
    cdb[9] = ataCommandOptions->tfr.LbaLow;
    cdb[11] = ataCommandOptions->tfr.SectorCount;
    cdb[12] = ataCommandOptions->tfr.ErrorFeature;
    if (ataCommandOptions->multipleCount > 0)
    {
        cdb[13] |= NEC_MULTIPLE_BIT;
        //set the multiple count in the "number of long data bytes"? I guess...-TJE
        cdb[14] |= M_Nibble0(ataCommandOptions->multipleCount);
    }
    //set the protocol
    switch (ataCommandOptions->commadProtocol)
    {
    case ATA_PROTOCOL_PIO:
        break;
    case ATA_PROTOCOL_DEV_DIAG:
        cdb[13] |= BIT2;
        break;
    case ATA_PROTOCOL_DMA:
    case ATA_PROTOCOL_UDMA:
        cdb[13] |= BIT3;
        break;
    case ATA_PROTOCOL_DMA_QUE:
        cdb[13] |= BIT3 | BIT2;
        break;
    case ATA_PROTOCOL_PACKET:
    case ATA_PROTOCOL_PACKET_DMA:
        cdb[13] |= BIT4;
        break;
    case ATA_PROTOCOL_HARD_RESET:
        cdb[13] |= BIT4 | BIT2;
        break;
    default:
        return NOT_SUPPORTED;
    }
    //set the data direction
    switch (ataCommandOptions->commandDirection)
    {
    case XFER_DATA_IN://01b
        cdb[13] |= BIT0;
        break;
    case XFER_DATA_OUT://10b
        cdb[13] |= BIT1;
        break;
    case XFER_NO_DATA://00b
        break;
    default:
        return NOT_SUPPORTED;
    }
    //set the sector count option (add force flag or something for this later if we ever find out what it means since the documentation doesn't make much sense - TJE)
    return ret;
}

int get_RTFRs_From_NEC_Legacy(tDevice *device, ataPassthroughCommand *ataCommandOptions, int commandRet)
{
    int ret = SUCCESS;
    if (commandRet == OS_PASSTHROUGH_FAILURE)
    {
        return commandRet;
    }
    uint8_t cdb[CDB_LEN_16] = { 0 };
    uint8_t returnData[11] = { 0 };
    uint8_t senseData[SPC3_SENSE_LEN] = { 0 };
    cdb[OPERATION_CODE] = NEC_READ_OPCODE;
    cdb[1] = NEC_WRAPPER_SIGNATURE;
    //send the command
    ret = scsi_Send_Cdb(device, cdb, CDB_LEN_16, returnData, 11, XFER_DATA_IN, senseData, SPC3_SENSE_LEN, 0);
    //now get the RTFRs
    if (ret == SUCCESS)
    {
        ataCommandOptions->rtfr.status = returnData[0];
        ataCommandOptions->rtfr.device = returnData[1];
        ataCommandOptions->rtfr.lbaHiExt = returnData[2];
        ataCommandOptions->rtfr.lbaMidExt = returnData[3];
        ataCommandOptions->rtfr.lbaLowExt = returnData[4];
        ataCommandOptions->rtfr.lbaHi = returnData[5];
        ataCommandOptions->rtfr.lbaMid = returnData[6];
        ataCommandOptions->rtfr.lbaLow = returnData[7];
        ataCommandOptions->rtfr.secCntExt = returnData[8];
        ataCommandOptions->rtfr.secCnt = returnData[9];
        ataCommandOptions->rtfr.error = returnData[10];
    }
    return ret;
}

int send_NEC_Legacy_Passthrough_Command(tDevice *device, ataPassthroughCommand *ataCommandOptions)
{
    int ret = UNKNOWN;
    uint8_t necCDB[CDB_LEN_16] = { 0 };
    uint8_t *senseData = NULL;//only allocate if the pointer in the ataCommandOptions is NULL
    bool localSenseData = false;
    if (!ataCommandOptions->ptrSenseData)
    {
        senseData = (uint8_t*)calloc(SPC3_SENSE_LEN, sizeof(uint8_t));
        if (!senseData)
        {
            return MEMORY_FAILURE;
        }
        localSenseData = true;
        ataCommandOptions->ptrSenseData = senseData;
        ataCommandOptions->senseDataSize = SPC3_SENSE_LEN;
    }
    //build the command
    ret = build_NEC_Legacy_CDB(necCDB, ataCommandOptions);
    if (ret == SUCCESS)
    {
        //print verbose tfr info
        print_Verbose_ATA_Command_Information(ataCommandOptions);
        //send it
        ret = scsi_Send_Cdb(device, necCDB, CDB_LEN_16, ataCommandOptions->ptrData, ataCommandOptions->dataSize, ataCommandOptions->commandDirection, ataCommandOptions->ptrSenseData, ataCommandOptions->senseDataSize, 0);
        //get the RTFRs
        ret = get_RTFRs_From_NEC_Legacy(device, ataCommandOptions, ret);
        //print RTFRs
        print_Verbose_ATA_Command_Result_Information(ataCommandOptions);
        //set return code
        //Based on the RTFRs or sense data, generate a return value
        if (ataCommandOptions->rtfr.status == (ATA_STATUS_BIT_READY | ATA_STATUS_BIT_SEEK_COMPLETE))
        {
            ret = SUCCESS;
        }
        else if (ataCommandOptions->rtfr.status == ATA_STATUS_BIT_BUSY)
        {
            ret = IN_PROGRESS;
        }
        else if (ataCommandOptions->rtfr.status == 0 && ret == SUCCESS)//the IO was successful, however we didn't fill in any tfrs...This should allow us one more chance to dummy up values
        {
            ret = SUCCESS;
            ataCommandOptions->rtfr.status = ATA_STATUS_BIT_READY | ATA_STATUS_BIT_SEEK_COMPLETE;//just make sure we set passing status for anything that cares to check rtfrs
        }
        else if (ret != NOT_SUPPORTED && ret != IN_PROGRESS)
        {
            ret = FAILURE;
        }
    }
    //before we get rid of the sense data, copy it back to the last command sense data
    memset(device->drive_info.lastCommandSenseData, 0, SPC3_SENSE_LEN);//clear before copying over data
    memcpy(&device->drive_info.lastCommandSenseData[0], &ataCommandOptions->ptrSenseData, M_Min(SPC3_SENSE_LEN, ataCommandOptions->senseDataSize));
    memcpy(&device->drive_info.lastCommandRTFRs, &ataCommandOptions->rtfr, sizeof(ataReturnTFRs));
    safe_Free(senseData);
    if (localSenseData)
    {
        ataCommandOptions->ptrSenseData = NULL;
        ataCommandOptions->senseDataSize = 0;
    }
    return ret;
}