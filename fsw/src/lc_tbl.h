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
 *   Specification for the CFS Limit Checker (LC) table related data
 *   structures
 *
 * @note
 *   Constants and enumerated types related to these table structures
 *   are defined in lc_tbldefs.h.
 */
#ifndef LC_TBL_H
#define LC_TBL_H

/*************************************************************************
 * Includes
 *************************************************************************/
#include "cfe.h"
#include "lc_platform_cfg.h"
#include "lc_tbldefs.h"

/*************************************************************************
 * Type Definitions
 *************************************************************************/

/**
 * \brief Alignment union
 *
 * A union type provides a way to have many different data types occupy
 * the same memory and legally alias each other.
 *
 * This is used to store the watch data points, as they may be 8, 16, or 32
 * bits and this is defined in the table / not known until runtime.
 */
typedef union
{
    uint32 Unsigned32;
    int32  Signed32;
    float  Float32;
    uint16 Unsigned16;
    int16  Signed16;
    uint8  Unsigned8;
    int8   Signed8;
    uint8  RawByte[4];
} LC_MultiType_t;

/**
 *  \brief Watchpoint Definition Table (WDT) Entry
 */
typedef struct
{
    uint8          DataType;        /**< \brief Watchpoint Data Type (enumerated)     */
    uint8          OperatorID;      /**< \brief Comparison type (enumerated)          */
    CFE_SB_MsgId_t MessageID;       /**< \brief Message ID for the message containing
                                                the watchpoint                        */
    uint32 WatchpointOffset;        /**< \brief Byte offset from the beginning of
                                                the message (including any headers)
                                                to the watchpoint                     */
    uint32 BitMask;                 /**< \brief Value to be masked with watchpoint
                                                data prior to comparison              */
    LC_MultiType_t ComparisonValue; /**< \brief Value watchpoint data is compared
                                                against                               */
    uint32 ResultAgeWhenStale;      /**< \brief Number of LC Sample Actionpoint
                                                commands that must be processed after
                                                comparison before result goes stale   */
    uint32 CustomFuncArgument;      /**< \brief Data passed to the custom function
                                                when Operator_ID is set to
                                                #LC_OPER_CUSTOM                       */
} LC_WDTEntry_t;

/**
 *  \brief Actionpoint Definition Table (ADT) Entry
 */
typedef struct
{
    uint8 DefaultState;       /**< \brief Default state for this AP (enumerated)
                                          States are defined in lc_msgdefs.h        */
    uint8 MaxPassiveEvents;   /**< \brief Max number of events before filter
                                          - RTS not started because AP is passive   */
    uint8 MaxPassFailEvents;  /**< \brief Max number of events before filter
                                          - AP result transition from pass to fail  */
    uint8 MaxFailPassEvents;  /**< \brief Max number of events before filter
                                          - AP result transition from fail to pass  */
    uint16 RTSId;             /**< \brief RTS to request if this AP fails           */
    uint16 MaxFailsBeforeRTS; /**< \brief How may consecutive failures before
                                          an RTS request is issued                  */

    uint16 RPNEquation[LC_MAX_RPN_EQU_SIZE]; /**< \brief Reverse Polish Equation that
                                                         specifies when this actionpoint
                                                         should fail                     */

    uint16 EventType; /**< \brief Event type used for event msg if AP fails:
                                  #CFE_EVS_EventType_INFORMATION,
                                  #CFE_EVS_EventType_DEBUG,
                                  #CFE_EVS_EventType_ERROR,
                                  or #CFE_EVS_EventType_CRITICAL      */

    uint16 EventID; /**< \brief Event ID used for event msg if AP fails
                                See lc_events.h for those already in use  */

    char EventText[LC_MAX_ACTION_TEXT]; /**< \brief Text used for the event msg when
                                                    this AP fails                   */
} LC_ADTEntry_t;

/**
 *  \brief Watchpoint Transition Structure
 */
typedef struct
{
    uint32 Value;   /**< \brief Watchpoint value at comparison that caused
                                the transition                               */
    uint8 DataType; /**< \brief Same as Watchpoint Data Type (enumerated)    */

    uint8 Padding[3]; /**< \brief Structure padding */

    CFE_TIME_SysTime_t Timestamp; /**< \brief Timstamp when the transition was detected    */
} LC_WRTTransition_t;

/**
 *  \brief Watchpoint Results Table (WRT) Entry
 */
typedef struct
{
    uint8 WatchResult; /**< \brief Result for the last evaluation of this
                                   watchpoint (enumerated)                */

    uint8 Padding[3]; /**< \brief Structure padding */

    uint32 CountdownToStale;            /**< \brief Number of LC Sample Actionpoint
                                                    commands still to be processed
                                                    before WatchResult becomes stale        */
    uint32 EvaluationCount;             /**< \brief How many times this watchpoint has
                                                    been evaluated                         */
    uint32 FalseToTrueCount;            /**< \brief How many times this watchpoint has
                                                    transitioned from FALSE to TRUE        */
    uint32 ConsecutiveTrueCount;        /**< \brief Number of consecutive times this
                                                    watchpoint has evaluated to TRUE       */
    uint32 CumulativeTrueCount;         /**< \brief Total number of times this watchpoint
                                                    has evaluated to TRUE                  */
    LC_WRTTransition_t LastFalseToTrue; /**< \brief Last transition from FALSE to TRUE     */
    LC_WRTTransition_t LastTrueToFalse; /**< \brief Last transition from TRUE to FALSE     */
} LC_WRTEntry_t;

/**
 *  \brief Actionpoint Results Table (ART) Entry
 */
typedef struct
{
    uint8 ActionResult; /**< \brief Result for the last sample of this
                                    actionpoint                            */
    uint8 CurrentState; /**< \brief Current state of this actionpoint      */

    uint16 Padding; /**< \brief Structure padding */

    uint32 PassiveAPCount;          /**< \brief Total number of times RTS not invoked
                                                because this AP was passive            */
    uint32 FailToPassCount;         /**< \brief How many times this actionpoint has
                                                transitioned from Fail to Pass         */
    uint32 PassToFailCount;         /**< \brief How many times this actionpoint has
                                                transitioned from Pass to Fail         */
    uint32 ConsecutiveFailCount;    /**< \brief Number of consecutive times this
                                                actionpoint has evaluated to Fail      */
    uint32 CumulativeFailCount;     /**< \brief Total number of times this actionpoint
                                                has evaluated to Fail                  */
    uint32 CumulativeRTSExecCount;  /**< \brief Total number of times an RTS request
                                                has been sent for this actionpoint     */
    uint32 CumulativeEventMsgsSent; /**< \brief Total number of event messages sent    */
} LC_ARTEntry_t;

#endif
