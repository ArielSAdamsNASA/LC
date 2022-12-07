/************************************************************************
 * NASA Docket No. GSC-18,921-1, and identified as “CFS Limit Checker
 * Application version 2.2.1”
 *
 * Copyright (c) 2021 United States Government as represented by the
 * Administrator of the National Aeronautics and Space Administration.
 * All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ************************************************************************/

/**
 * @file
 *   CFS Limit Checker (LC) command handling routines
 */

/************************************************************************
** Includes
*************************************************************************/
#include "lc_app.h"
#include "lc_cmds.h"
#include "lc_msgids.h"
#include "lc_events.h"
#include "lc_version.h"
#include "lc_action.h"
#include "lc_watch.h"
#include "lc_platform_cfg.h"
#include "lc_utils.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Process a command pipe message                                  */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
int32 LC_AppPipe(const CFE_SB_Buffer_t *BufPtr)
{
    int32             Status      = CFE_SUCCESS;
    CFE_SB_MsgId_t    MessageID   = CFE_SB_INVALID_MSG_ID;
    CFE_MSG_FcnCode_t CommandCode = 0;

    CFE_MSG_GetMsgId(&BufPtr->Msg, &MessageID);

    switch (CFE_SB_MsgIdToValue(MessageID))
    {
        /*
        ** Sample actionpoints request
        */
        case LC_SAMPLE_AP_MID:
            LC_SampleAPReq(BufPtr);
            break;

        /*
        ** Housekeeping telemetry request
        ** (only routine that can return a critical error indicator)
        */
        case LC_SEND_HK_MID:
            Status = LC_HousekeepingReq((CFE_MSG_CommandHeader_t *)BufPtr);
            break;

        /*
        ** LC application commands...
        */
        case LC_CMD_MID:

            CFE_MSG_GetFcnCode(&BufPtr->Msg, &CommandCode);
            switch (CommandCode)
            {
                case LC_NOOP_CC:
                    LC_NoopCmd(BufPtr);
                    break;

                case LC_RESET_CC:
                    LC_ResetCmd(BufPtr);
                    break;

                case LC_SET_LC_STATE_CC:
                    LC_SetLCStateCmd(BufPtr);
                    break;

                case LC_SET_AP_STATE_CC:
                    LC_SetAPStateCmd(BufPtr);
                    break;

                case LC_SET_AP_PERMOFF_CC:
                    LC_SetAPPermOffCmd(BufPtr);
                    break;

                case LC_RESET_AP_STATS_CC:
                    LC_ResetAPStatsCmd(BufPtr);
                    break;

                case LC_RESET_WP_STATS_CC:
                    LC_ResetWPStatsCmd(BufPtr);
                    break;

                default:
                    CFE_EVS_SendEvent(LC_CC_ERR_EID, CFE_EVS_EventType_ERROR,
                                      "Invalid command code: ID = 0x%08lX, CC = %d",
                                      (unsigned long)CFE_SB_MsgIdToValue(MessageID), CommandCode);

                    LC_AppData.CmdErrCount++;
                    break;

            } /* end CommandCode switch */
            break;

        /*
        ** All other message ID's should be monitor
        ** packets
        */
        default:
            LC_CheckMsgForWPs(MessageID, BufPtr);
            break;

    } /* end MessageID switch */

    return Status;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Sample Actionpoints Request                                     */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void LC_SampleAPReq(const CFE_SB_Buffer_t *BufPtr)
{
    LC_SampleAP_t *LC_SampleAP    = (LC_SampleAP_t *)BufPtr;
    size_t         ExpectedLength = sizeof(LC_SampleAP_t);
    uint16         WatchIndex;
    bool           ValidSampleCmd = false;

    /*
    ** Verify message packet length
    */
    if (LC_VerifyMsgLength(&BufPtr->Msg, ExpectedLength))
    {
        /*
        ** Ignore AP sample requests if disabled at the application level
        */
        if (LC_AppData.CurrentLCState != LC_STATE_DISABLED)
        {
            /*
            ** Range check the actionpoint array index arguments
            */
            if ((LC_SampleAP->StartIndex == LC_ALL_ACTIONPOINTS) && (LC_SampleAP->EndIndex == LC_ALL_ACTIONPOINTS))
            {
                /*
                ** Allow special "sample all" heritage values
                */
                LC_SampleAPs(0, LC_MAX_ACTIONPOINTS - 1);
                ValidSampleCmd = true;
            }
            else if ((LC_SampleAP->StartIndex <= LC_SampleAP->EndIndex) &&
                     (LC_SampleAP->EndIndex < LC_MAX_ACTIONPOINTS))
            {
                /*
                ** Start is less or equal to end, and end is within the array
                */
                LC_SampleAPs(LC_SampleAP->StartIndex, LC_SampleAP->EndIndex);
                ValidSampleCmd = true;
            }
            else
            {
                /*
                ** At least one actionpoint array index is out of range
                */
                CFE_EVS_SendEvent(LC_APSAMPLE_APNUM_ERR_EID, CFE_EVS_EventType_ERROR,
                                  "Sample AP error: invalid AP number, start = %d, end = %d", LC_SampleAP->StartIndex,
                                  LC_SampleAP->EndIndex);
            }

            /*
            ** Optionally update the age of watchpoint results
            */
            if ((LC_SampleAP->UpdateAge != 0) && (ValidSampleCmd))
            {
                for (WatchIndex = 0; WatchIndex < LC_MAX_WATCHPOINTS; WatchIndex++)
                {
                    if (LC_OperData.WRTPtr[WatchIndex].CountdownToStale != 0)
                    {
                        LC_OperData.WRTPtr[WatchIndex].CountdownToStale--;

                        if (LC_OperData.WRTPtr[WatchIndex].CountdownToStale == 0)
                        {
                            LC_OperData.WRTPtr[WatchIndex].WatchResult = LC_WATCH_STALE;
                        }
                    }
                }
            }
        }
    }

    return;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Housekeeping request                                            */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
int32 LC_HousekeepingReq(const CFE_MSG_CommandHeader_t *MsgPtr)
{
    int32  Result;
    size_t ExpectedLength = sizeof(LC_NoArgsCmd_t);
    uint16 TableIndex;
    uint16 HKIndex;
    uint8  ByteData;

    /*
    ** Verify message packet length
    */
    if (LC_VerifyMsgLength((CFE_MSG_Message_t *)MsgPtr, ExpectedLength))
    {
        /*
        ** Update HK variables
        */
        LC_OperData.HkPacket.Payload.CmdCount            = LC_AppData.CmdCount;
        LC_OperData.HkPacket.Payload.CmdErrCount         = LC_AppData.CmdErrCount;
        LC_OperData.HkPacket.Payload.APSampleCount       = LC_AppData.APSampleCount;
        LC_OperData.HkPacket.Payload.MonitoredMsgCount   = LC_AppData.MonitoredMsgCount;
        LC_OperData.HkPacket.Payload.RTSExecCount        = LC_AppData.RTSExecCount;
        LC_OperData.HkPacket.Payload.PassiveRTSExecCount = LC_AppData.PassiveRTSExecCount;
        LC_OperData.HkPacket.Payload.CurrentLCState      = LC_AppData.CurrentLCState;
        LC_OperData.HkPacket.Payload.WPsInUse            = LC_OperData.WatchpointCount;

        /*
        ** Clear out the active actionpoint count, it will get
        ** recomputed below
        */
        LC_OperData.HkPacket.Payload.ActiveAPs = 0;

        /*
        ** Update packed watch results
        ** (4 watch results in one 8-bit byte)
        */
        for (TableIndex = 0; TableIndex < LC_MAX_WATCHPOINTS; TableIndex += 4)
        {
            HKIndex = TableIndex / 4;

            /*
            ** Pack in first result
            */
            switch (LC_OperData.WRTPtr[TableIndex + 3].WatchResult)
            {
                case LC_WATCH_STALE:
                    ByteData = LC_HKWR_STALE << 6;
                    break;

                case LC_WATCH_FALSE:
                    ByteData = LC_HKWR_FALSE << 6;
                    break;

                case LC_WATCH_TRUE:
                    ByteData = LC_HKWR_TRUE << 6;
                    break;

                /*
                ** We should never get an undefined watch result,
                ** but we'll set an error result if we do
                */
                case LC_WATCH_ERROR:
                default:
                    ByteData = LC_HKWR_ERROR << 6;
                    break;
            }

            /*
            ** Pack in second result
            */
            switch (LC_OperData.WRTPtr[TableIndex + 2].WatchResult)
            {
                case LC_WATCH_STALE:
                    ByteData = (ByteData | (LC_HKWR_STALE << 4));
                    break;

                case LC_WATCH_FALSE:
                    ByteData = (ByteData | (LC_HKWR_FALSE << 4));
                    break;

                case LC_WATCH_TRUE:
                    ByteData = (ByteData | (LC_HKWR_TRUE << 4));
                    break;

                case LC_WATCH_ERROR:
                default:
                    ByteData = (ByteData | (LC_HKWR_ERROR << 4));
                    break;
            }

            /*
            ** Pack in third result
            */
            switch (LC_OperData.WRTPtr[TableIndex + 1].WatchResult)
            {
                case LC_WATCH_STALE:
                    ByteData = (ByteData | (LC_HKWR_STALE << 2));
                    break;

                case LC_WATCH_FALSE:
                    ByteData = (ByteData | (LC_HKWR_FALSE << 2));
                    break;

                case LC_WATCH_TRUE:
                    ByteData = (ByteData | (LC_HKWR_TRUE << 2));
                    break;

                case LC_WATCH_ERROR:
                default:
                    ByteData = (ByteData | (LC_HKWR_ERROR << 2));
                    break;
            }

            /*
            ** Pack in fourth and last result
            */
            switch (LC_OperData.WRTPtr[TableIndex].WatchResult)
            {
                case LC_WATCH_STALE:
                    ByteData = (ByteData | LC_HKWR_STALE);
                    break;

                case LC_WATCH_FALSE:
                    ByteData = (ByteData | LC_HKWR_FALSE);
                    break;

                case LC_WATCH_TRUE:
                    ByteData = (ByteData | LC_HKWR_TRUE);
                    break;

                case LC_WATCH_ERROR:
                default:
                    ByteData = (ByteData | LC_HKWR_ERROR);
                    break;
            }

            /*
            ** Update houskeeping watch results array
            */
            LC_OperData.HkPacket.Payload.WPResults[HKIndex] = ByteData;

        } /* end watch results for loop */

        /*
        ** Update packed action results
        ** (2 action state/result pairs (4 bits each) in one 8-bit byte)
        */
        for (TableIndex = 0; TableIndex < LC_MAX_ACTIONPOINTS; TableIndex += 2)
        {
            HKIndex = TableIndex / 2;

            /*
            ** Pack in first actionpoint, current state
            */
            switch (LC_OperData.ARTPtr[TableIndex + 1].CurrentState)
            {
                case LC_APSTATE_ACTION_NOT_USED:
                    ByteData = LC_HKAR_STATE_NOT_USED << 6;
                    break;

                case LC_APSTATE_ACTIVE:
                    ByteData = LC_HKAR_STATE_ACTIVE << 6;
                    LC_OperData.HkPacket.Payload.ActiveAPs++;
                    break;

                case LC_APSTATE_PASSIVE:
                    ByteData = LC_HKAR_STATE_PASSIVE << 6;
                    break;

                case LC_APSTATE_DISABLED:
                    ByteData = LC_HKAR_STATE_DISABLED << 6;
                    break;

                /*
                ** Permanantly disabled actionpoints get reported
                ** as unused. We should never get an undefined
                ** action state, but we'll set to not used if we do.
                */
                case LC_APSTATE_PERMOFF:
                default:
                    ByteData = LC_HKAR_STATE_NOT_USED << 6;
                    break;
            }

            /*
            ** Pack in first actionpoint, action result
            */
            switch (LC_OperData.ARTPtr[TableIndex + 1].ActionResult)
            {
                case LC_ACTION_STALE:
                    ByteData = (ByteData | (LC_HKAR_STALE << 4));
                    break;

                case LC_ACTION_PASS:
                    ByteData = (ByteData | (LC_HKAR_PASS << 4));
                    break;

                case LC_ACTION_FAIL:
                    ByteData = (ByteData | (LC_HKAR_FAIL << 4));
                    break;

                /*
                ** We should never get an undefined action result,
                ** but we'll set an error result if we do
                */
                case LC_ACTION_ERROR:
                default:
                    ByteData = (ByteData | (LC_HKAR_ERROR << 4));
                    break;
            }

            /*
            ** Pack in second actionpoint, current state
            */
            switch (LC_OperData.ARTPtr[TableIndex].CurrentState)
            {
                case LC_APSTATE_ACTION_NOT_USED:
                    ByteData = (ByteData | (LC_HKAR_STATE_NOT_USED << 2));
                    break;

                case LC_APSTATE_ACTIVE:
                    ByteData = (ByteData | (LC_HKAR_STATE_ACTIVE << 2));
                    LC_OperData.HkPacket.Payload.ActiveAPs++;
                    break;

                case LC_APSTATE_PASSIVE:
                    ByteData = (ByteData | (LC_HKAR_STATE_PASSIVE << 2));
                    break;

                case LC_APSTATE_DISABLED:
                    ByteData = (ByteData | (LC_HKAR_STATE_DISABLED << 2));
                    break;

                case LC_APSTATE_PERMOFF:
                default:
                    ByteData = (ByteData | (LC_HKAR_STATE_NOT_USED << 2));
                    break;
            }

            /*
            ** Pack in second actionpoint, action result
            */
            switch (LC_OperData.ARTPtr[TableIndex].ActionResult)
            {
                case LC_ACTION_STALE:
                    ByteData = (ByteData | LC_HKAR_STALE);
                    break;

                case LC_ACTION_PASS:
                    ByteData = (ByteData | LC_HKAR_PASS);
                    break;

                case LC_ACTION_FAIL:
                    ByteData = (ByteData | LC_HKAR_FAIL);
                    break;

                case LC_ACTION_ERROR:
                default:
                    ByteData = (ByteData | LC_HKAR_ERROR);
                    break;
            }

            /*
            ** Update houskeeping action results array
            */
            LC_OperData.HkPacket.Payload.APResults[HKIndex] = ByteData;

        } /* end action results for loop */

        /*
        ** Timestamp and send housekeeping packet
        */
        CFE_SB_TimeStampMsg(CFE_MSG_PTR(&LC_OperData.HkPacket.Payload));
        CFE_SB_TransmitMsg(CFE_MSG_PTR(&LC_OperData.HkPacket.Payload), true);

    } /* end LC_VerifyMsgLength if */

    Result = LC_PerformMaintenance();

    return Result;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Noop command                                                    */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void LC_NoopCmd(const CFE_SB_Buffer_t *BufPtr)
{
    size_t ExpectedLength = sizeof(LC_NoArgsCmd_t);

    /*
    ** Verify message packet length
    */
    if (LC_VerifyMsgLength(&BufPtr->Msg, ExpectedLength))
    {
        LC_AppData.CmdCount++;

        CFE_EVS_SendEvent(LC_NOOP_INF_EID, CFE_EVS_EventType_INFORMATION, "No-op command: Version %d.%d.%d.%d",
                          LC_MAJOR_VERSION, LC_MINOR_VERSION, LC_REVISION, LC_MISSION_REV);
    }

    return;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Reset counters command                                          */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void LC_ResetCmd(const CFE_SB_Buffer_t *BufPtr)
{
    size_t ExpectedLength = sizeof(LC_NoArgsCmd_t);

    /*
    ** Verify message packet length
    */
    if (LC_VerifyMsgLength(&BufPtr->Msg, ExpectedLength))
    {
        LC_ResetCounters();

        CFE_EVS_SendEvent(LC_RESET_DBG_EID, CFE_EVS_EventType_DEBUG, "Reset counters command");
    }

    return;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Reset housekeeping counters                                     */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void LC_ResetCounters(void)
{
    LC_AppData.CmdCount    = 0;
    LC_AppData.CmdErrCount = 0;

    LC_AppData.APSampleCount       = 0;
    LC_AppData.MonitoredMsgCount   = 0;
    LC_AppData.RTSExecCount        = 0;
    LC_AppData.PassiveRTSExecCount = 0;

    return;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Set LC state command                                            */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void LC_SetLCStateCmd(const CFE_SB_Buffer_t *BufPtr)
{
    size_t           ExpectedLength = sizeof(LC_SetLCState_t);
    LC_SetLCState_t *CmdPtr;

    /*
    ** Verify message packet length
    */
    if (LC_VerifyMsgLength(&BufPtr->Msg, ExpectedLength))
    {
        CmdPtr = ((LC_SetLCState_t *)BufPtr);

        switch (CmdPtr->NewLCState)
        {
            case LC_STATE_ACTIVE:
            case LC_STATE_PASSIVE:
            case LC_STATE_DISABLED:
                LC_AppData.CurrentLCState = CmdPtr->NewLCState;
                LC_AppData.CmdCount++;

                CFE_EVS_SendEvent(LC_LCSTATE_INF_EID, CFE_EVS_EventType_INFORMATION,
                                  "Set LC state command: new state = %d", CmdPtr->NewLCState);
                break;

            default:
                CFE_EVS_SendEvent(LC_LCSTATE_ERR_EID, CFE_EVS_EventType_ERROR, "Set LC state error: invalid state = %d",
                                  CmdPtr->NewLCState);

                LC_AppData.CmdErrCount++;
                break;
        }
    }

    return;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Set actionpoint state command                                   */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void LC_SetAPStateCmd(const CFE_SB_Buffer_t *BufPtr)
{
    size_t           ExpectedLength = sizeof(LC_SetAPState_t);
    LC_SetAPState_t *CmdPtr;
    uint32           TableIndex;
    uint8            CurrentAPState;
    bool             ValidState = true;
    bool             CmdSuccess = false;

    /*
    ** Verify message packet length
    */
    if (LC_VerifyMsgLength(&BufPtr->Msg, ExpectedLength))
    {
        CmdPtr = ((LC_SetAPState_t *)BufPtr);

        /*
        ** Do a sanity check on the new actionpoint state
        ** specified.
        */
        switch (CmdPtr->NewAPState)
        {
            case LC_APSTATE_ACTIVE:
            case LC_APSTATE_PASSIVE:
            case LC_APSTATE_DISABLED:
                break;

            default:
                ValidState = false;
                CFE_EVS_SendEvent(LC_APSTATE_NEW_ERR_EID, CFE_EVS_EventType_ERROR,
                                  "Set AP state error: AP = %d, Invalid new state = %d", CmdPtr->APNumber,
                                  CmdPtr->NewAPState);

                LC_AppData.CmdErrCount++;
                break;
        }

        /*
        ** Do the rest based on the actionpoint ID we were given
        */
        if (ValidState == true)
        {
            if ((CmdPtr->APNumber) == LC_ALL_ACTIONPOINTS)
            {
                /*
                ** Set all actionpoints to the new state except those that are not
                ** used or set permanently off
                */
                for (TableIndex = 0; TableIndex < LC_MAX_ACTIONPOINTS; TableIndex++)
                {
                    CurrentAPState = LC_OperData.ARTPtr[TableIndex].CurrentState;

                    if ((CurrentAPState != LC_APSTATE_ACTION_NOT_USED) && (CurrentAPState != LC_APSTATE_PERMOFF))
                    {
                        LC_OperData.ARTPtr[TableIndex].CurrentState = CmdPtr->NewAPState;
                    }
                }

                /*
                ** Set flag that we succeeded
                */
                CmdSuccess = true;
            }
            else
            {
                if ((CmdPtr->APNumber) < LC_MAX_ACTIONPOINTS)
                {
                    TableIndex     = CmdPtr->APNumber;
                    CurrentAPState = LC_OperData.ARTPtr[TableIndex].CurrentState;

                    if ((CurrentAPState != LC_APSTATE_ACTION_NOT_USED) && (CurrentAPState != LC_APSTATE_PERMOFF))
                    {
                        /*
                        ** Update state for single actionpoint specified
                        */
                        LC_OperData.ARTPtr[TableIndex].CurrentState = CmdPtr->NewAPState;

                        CmdSuccess = true;
                    }
                    else
                    {
                        /*
                        ** Actionpoints that are not used or set permanently
                        ** off can only be changed by a table load
                        */
                        CFE_EVS_SendEvent(LC_APSTATE_CURR_ERR_EID, CFE_EVS_EventType_ERROR,
                                          "Set AP state error: AP = %d, Invalid current AP state = %d",
                                          CmdPtr->APNumber, CurrentAPState);

                        LC_AppData.CmdErrCount++;
                    }
                }
                else
                {
                    /*
                    **  Actionpoint number is out of range
                    **  (it's zero based, since it's a table index)
                    */
                    CFE_EVS_SendEvent(LC_APSTATE_APNUM_ERR_EID, CFE_EVS_EventType_ERROR,
                                      "Set AP state error: Invalid AP number = %d", CmdPtr->APNumber);

                    LC_AppData.CmdErrCount++;
                }
            }

            /*
            ** Update the command counter and send out event if command
            ** executed
            */
            if (CmdSuccess == true)
            {
                LC_AppData.CmdCount++;

                CFE_EVS_SendEvent(LC_APSTATE_INF_EID, CFE_EVS_EventType_INFORMATION,
                                  "Set AP state command: AP = %d, New state = %d", CmdPtr->APNumber,
                                  CmdPtr->NewAPState);
            }

        } /* end ValidState if */

    } /* end LC_VerifyMsgLength if */

    return;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Set actionpoint permanently off command                         */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void LC_SetAPPermOffCmd(const CFE_SB_Buffer_t *BufPtr)
{
    size_t             ExpectedLength = sizeof(LC_SetAPPermOff_t);
    LC_SetAPPermOff_t *CmdPtr;
    uint32             TableIndex;
    uint8              CurrentAPState;

    /*
    ** Verify message packet length
    */
    if (LC_VerifyMsgLength(&BufPtr->Msg, ExpectedLength))
    {
        CmdPtr = ((LC_SetAPPermOff_t *)BufPtr);

        if (((CmdPtr->APNumber) == LC_ALL_ACTIONPOINTS) || ((CmdPtr->APNumber) >= LC_MAX_ACTIONPOINTS))
        {
            /*
            **  Invalid actionpoint number
            **  (This command can't be invoked for all actionpoints)
            */
            CFE_EVS_SendEvent(LC_APOFF_APNUM_ERR_EID, CFE_EVS_EventType_ERROR,
                              "Set AP perm off error: Invalid AP number = %d", CmdPtr->APNumber);

            LC_AppData.CmdErrCount++;
        }
        else
        {
            TableIndex     = CmdPtr->APNumber;
            CurrentAPState = LC_OperData.ARTPtr[TableIndex].CurrentState;

            if (CurrentAPState != LC_APSTATE_DISABLED)
            {
                /*
                ** Actionpoints can only be turned permanently off if
                ** they are currently disabled
                */
                CFE_EVS_SendEvent(LC_APOFF_CURR_ERR_EID, CFE_EVS_EventType_ERROR,
                                  "Set AP perm off error, AP NOT Disabled: AP = %d, Current state = %d",
                                  CmdPtr->APNumber, CurrentAPState);

                LC_AppData.CmdErrCount++;
            }
            else
            {
                /*
                ** Update state for actionpoint specified
                */
                LC_OperData.ARTPtr[TableIndex].CurrentState = LC_APSTATE_PERMOFF;

                LC_AppData.CmdCount++;

                CFE_EVS_SendEvent(LC_APOFF_INF_EID, CFE_EVS_EventType_INFORMATION,
                                  "Set AP permanently off command: AP = %d", CmdPtr->APNumber);
            }

        } /* end CmdPtr -> APNumber else */

    } /* end LC_VerifyMsgLength if */

    return;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Reset actionpoint statistics command                            */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void LC_ResetAPStatsCmd(const CFE_SB_Buffer_t *BufPtr)
{
    size_t             ExpectedLength = sizeof(LC_ResetAPStats_t);
    LC_ResetAPStats_t *CmdPtr         = (LC_ResetAPStats_t *)BufPtr;
    bool               CmdSuccess     = false;

    /* verify message packet length */
    if (LC_VerifyMsgLength(&BufPtr->Msg, ExpectedLength))
    {
        /* arg may be one or all AP's */
        if (CmdPtr->APNumber == LC_ALL_ACTIONPOINTS)
        {
            LC_ResetResultsAP(0, LC_MAX_ACTIONPOINTS - 1, true);
            CmdSuccess = true;
        }
        else if (CmdPtr->APNumber < LC_MAX_ACTIONPOINTS)
        {
            LC_ResetResultsAP(CmdPtr->APNumber, CmdPtr->APNumber, true);
            CmdSuccess = true;
        }
        else
        {
            /* arg is out of range (zero based table index) */
            LC_AppData.CmdErrCount++;

            CFE_EVS_SendEvent(LC_APSTATS_APNUM_ERR_EID, CFE_EVS_EventType_ERROR,
                              "Reset AP stats error: invalid AP number = %d", CmdPtr->APNumber);
        }

        if (CmdSuccess == true)
        {
            LC_AppData.CmdCount++;

            CFE_EVS_SendEvent(LC_APSTATS_INF_EID, CFE_EVS_EventType_INFORMATION, "Reset AP stats command: AP = %d",
                              CmdPtr->APNumber);
        }
    }

    return;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Reset selected AP statistics (utility function)                 */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void LC_ResetResultsAP(uint32 StartIndex, uint32 EndIndex, bool ResetStatsCmd)
{
    uint32 TableIndex;

    /* reset selected entries in actionpoint results table */
    for (TableIndex = StartIndex; TableIndex <= EndIndex; TableIndex++)
    {
        if (!ResetStatsCmd)
        {
            /* reset AP stats command does not modify AP state or most recent test result */
            LC_OperData.ARTPtr[TableIndex].ActionResult = LC_ACTION_STALE;
            LC_OperData.ARTPtr[TableIndex].CurrentState = LC_OperData.ADTPtr[TableIndex].DefaultState;
        }

        LC_OperData.ARTPtr[TableIndex].PassiveAPCount  = 0;
        LC_OperData.ARTPtr[TableIndex].FailToPassCount = 0;
        LC_OperData.ARTPtr[TableIndex].PassToFailCount = 0;

        LC_OperData.ARTPtr[TableIndex].ConsecutiveFailCount    = 0;
        LC_OperData.ARTPtr[TableIndex].CumulativeFailCount     = 0;
        LC_OperData.ARTPtr[TableIndex].CumulativeRTSExecCount  = 0;
        LC_OperData.ARTPtr[TableIndex].CumulativeEventMsgsSent = 0;
    }

    return;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Reset watchpoint statistics command                             */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void LC_ResetWPStatsCmd(const CFE_SB_Buffer_t *BufPtr)
{
    size_t             ExpectedLength = sizeof(LC_ResetWPStats_t);
    LC_ResetWPStats_Payload_t *CmdPtr         = &((LC_ResetWPStats_t *)BufPtr)->Payload;
    bool               CmdSuccess     = false;

    /* verify message packet length */
    if (LC_VerifyMsgLength(&BufPtr->Msg, ExpectedLength))
    {
        /* arg may be one or all WP's */
        if (CmdPtr->WPNumber == LC_ALL_WATCHPOINTS)
        {
            LC_ResetResultsWP(0, LC_MAX_WATCHPOINTS - 1, true);
            CmdSuccess = true;
        }
        else if (CmdPtr->WPNumber < LC_MAX_WATCHPOINTS)
        {
            LC_ResetResultsWP(CmdPtr->WPNumber, CmdPtr->WPNumber, true);
            CmdSuccess = true;
        }
        else
        {
            /* arg is out of range (zero based table index) */
            LC_AppData.CmdErrCount++;

            CFE_EVS_SendEvent(LC_WPSTATS_WPNUM_ERR_EID, CFE_EVS_EventType_ERROR,
                              "Reset WP stats error: invalid WP number = %d", CmdPtr->WPNumber);
        }

        if (CmdSuccess == true)
        {
            LC_AppData.CmdCount++;

            CFE_EVS_SendEvent(LC_WPSTATS_INF_EID, CFE_EVS_EventType_INFORMATION, "Reset WP stats command: WP = %d",
                              CmdPtr->WPNumber);
        }
    }

    return;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Reset selected WP statistics (utility function)                 */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void LC_ResetResultsWP(uint32 StartIndex, uint32 EndIndex, bool ResetStatsCmd)
{
    uint32 TableIndex;

    /* reset selected entries in watchoint results table */
    for (TableIndex = StartIndex; TableIndex <= EndIndex; TableIndex++)
    {
        if (!ResetStatsCmd)
        {
            /* reset WP stats command does not modify most recent test result */
            LC_OperData.WRTPtr[TableIndex].WatchResult      = LC_WATCH_STALE;
            LC_OperData.WRTPtr[TableIndex].CountdownToStale = 0;
        }

        LC_OperData.WRTPtr[TableIndex].EvaluationCount      = 0;
        LC_OperData.WRTPtr[TableIndex].FalseToTrueCount     = 0;
        LC_OperData.WRTPtr[TableIndex].ConsecutiveTrueCount = 0;
        LC_OperData.WRTPtr[TableIndex].CumulativeTrueCount  = 0;

        LC_OperData.WRTPtr[TableIndex].LastFalseToTrue.Value                = 0;
        LC_OperData.WRTPtr[TableIndex].LastFalseToTrue.Timestamp.Seconds    = 0;
        LC_OperData.WRTPtr[TableIndex].LastFalseToTrue.Timestamp.Subseconds = 0;

        LC_OperData.WRTPtr[TableIndex].LastTrueToFalse.Value                = 0;
        LC_OperData.WRTPtr[TableIndex].LastTrueToFalse.Timestamp.Seconds    = 0;
        LC_OperData.WRTPtr[TableIndex].LastTrueToFalse.Timestamp.Subseconds = 0;
    }

    return;
}
