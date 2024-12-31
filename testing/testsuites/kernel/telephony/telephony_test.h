/****************************************************************************
 * apps/testing/testsuites/kernel/telephony/telephony_test.h
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 *The ASF licenses this file to you under the Apache License, Version 2.0
 *(the "License"); you may not use this file except in compliance with
 *the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 *WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 *implied.  See the License for the specific language governing
 *permissions and limitations under the License.
 *
 ****************************************************************************/

#ifndef TELEPHONY_TEST_H_
#define TELEPHONY_TEST_H_

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <tapi.h>
#include <unistd.h>
#include <uv.h>

#define context tapi_test_context

/* Generic Callback Event */

#define EVENT_MODEM_LIST_QUERY_DONE 0x1001
#define EVENT_RADIO_STATE_SET_DONE 0x1002
#define EVENT_RAT_MODE_SET_DONE 0x1003
#define EVENT_MODEM_ACTIVITY_INFO_QUERY_DONE 0x1004
#define EVENT_MODEM_ENABLE_DONE 0x1005
#define EVENT_MODEM_STATUS_QUERY_DONE 0x1006
#define EVENT_OEM_RIL_REQUEST_RAW_DONE 0x1007
#define EVENT_OEM_RIL_REQUEST_STRINGS_DONE 0x1008

/* Data Callback Event */

#define EVENT_APN_LOADED_DONE 0x1009
#define EVENT_APN_ADD_DONE 0x1010
#define EVENT_APN_EDIT_DONE 0x101A
#define EVENT_APN_REMOVAL_DONE 0x101B
#define EVENT_APN_RESTORE_DONE 0x101C
#define EVENT_DATA_ALLOWED_DONE 0x101D
#define EVENT_DATA_CALL_LIST_QUERY_DONE 0x101E
#define EVENT_REQUEST_SCREEN_STATE_DONE 0x101F

/* SIM Callback Event */

#define EVENT_CHANGE_SIM_PIN_DONE 0x21
#define EVENT_ENTER_SIM_PIN_DONE 0x22
#define EVENT_RESET_SIM_PIN_DONE 0x23
#define EVENT_LOCK_SIM_PIN_DONE 0x24
#define EVENT_UNLOCK_SIM_PIN_DONE 0x25
#define EVENT_OPEN_LOGICAL_CHANNEL_DONE 0x26
#define EVENT_CLOSE_LOGICAL_CHANNEL_DONE 0x27
#define EVENT_TRANSMIT_APDU_LOGICAL_CHANNEL_DONE 0x28
#define EVENT_TRANSMIT_APDU_BASIC_CHANNEL_DONE 0x29
#define EVENT_UICC_ENABLEMENT_SET_DONE 0x2A

/* Network Callback Event */

#define EVENT_NETWORK_SCAN_DONE 0x31
#define EVENT_REGISTER_AUTO_DONE 0x32
#define EVENT_REGISTER_MANUAL_DONE 0x33
#define EVENT_QUERY_REGISTRATION_INFO_DONE 0x34
#define EVENT_QUERY_SERVING_CELL_DONE 0x35
#define EVENT_QUERY_NEIGHBOURING_CELL_DONE 0x36
#define EVENT_NETWORK_SET_CELL_INFO_LIST_RATE_DONE 0x37

/* SS Callback Event */

#define EVENT_REQUEST_CALL_BARRING_DONE 0x41
#define EVENT_CALL_BARRING_PASSWD_CHANGE_DONE 0x42
#define EVENT_DISABLE_ALL_CALL_BARRINGS_DONE 0x43
#define EVENT_DISABLE_ALL_INCOMING_DONE 0x44
#define EVENT_DISABLE_ALL_OUTGOING_DONE 0x45
#define EVENT_REQUEST_CALL_FORWARDING_DONE 0x46
#define EVENT_DISABLE_CALL_FORWARDING_DONE 0x47
#define EVENT_CANCEL_USSD_DONE 0x48
#define EVENT_REQUEST_CALL_WAITING_DONE 0x49
#define EVENT_SEND_USSD_DONE 0x4A
#define EVENT_INITIATE_SERVICE_DONE 0x4B
#define EVENT_ENABLE_FDN_DONE 0x4C
#define EVENT_QUERY_FDN_DONE 0x4D
#define EVENT_REQUEST_CLIR_DONE 0x4E
#define EVENT_QUERY_ALL_CALL_BARRING_DONE 0x4F
#define EVENT_QUERY_ALL_CALL_FORWARDING_DONE 0x50
#define EVENT_QUERY_ALL_CALL_SETTING_DONE 0x51
#define EVENT_QUERY_CALL_FORWARDING_DONE 0x52
#define EVENT_QUERY_CALL_WAITING_DONE 0x53

/* PhoneBook Callback Event */

#define EVENT_LOAD_ADN_ENTRIES_DONE 0x61
#define EVENT_LOAD_FDN_ENTRIES_DONE 0x62
#define EVENT_INSERT_FDN_ENTRIES_DONE 0x63
#define EVENT_UPDATE_FDN_ENTRIES_DONE 0x64
#define EVENT_DELETE_FDN_ENTRIES_DONE 0x65

/* Call CallBack Event */

#define EVENT_REQUEST_DIAL_DONE 0x71

/* Sms CallBack Event */

#define EVENT_SEND_MESSAGE_DONE 0x81
#define EVENT_SEND_DATA_MESSAGE_DONE 0x82

#define TIMEOUT 15
#define INVALID_VALUE -1

tapi_context get_tapi_ctx(void);
int judge(void);
void judge_data_init(void);

struct judge_type
{
    int flag;
    int expect;
    int result;
};

#endif /* TELEPHONY_TEST_H_ */
