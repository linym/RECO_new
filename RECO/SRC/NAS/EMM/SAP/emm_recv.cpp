/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under 
 * the Apache License, Version 2.0  (the "License"); you may not use this file
 * except in compliance with the License.  
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

/*****************************************************************************

  Source      emm_recv.c

  Version     0.1

  Date        2013/01/30

  Product     NAS stack

  Subsystem   EPS Mobility Management

  Author      Frederic Maurel

  Description Defines functions executed at the EMMAS Service Access
        Point upon receiving EPS Mobility Management messages
        from the Access Stratum sublayer.

*****************************************************************************/
#include <pthread.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include "bstrlib.h"

#include "3gpp_24.007.h"
#include "emm_recv.h"
#include "commonDef.h"
#include "common_defs.h"
#include "log.h"
#include "emm_msgDef.h"
#include "emm_cause.h"
#include "emm_proc.h"
#include "mme_config.h"
#include "nas_config.h"


/****************************************************************************/
/****************  E X T E R N A L    D E F I N I T I O N S  ****************/
/****************************************************************************/

/****************************************************************************/
/*******************  L O C A L    D E F I N I T I O N S  *******************/
/****************************************************************************/

/****************************************************************************/
/******************  E X P O R T E D    F U N C T I O N S  ******************/
/****************************************************************************/

/****************************************************************************/
/*********************  L O C A L    F U N C T I O N S  *********************/
/****************************************************************************/

/*
   --------------------------------------------------------------------------
   Functions executed by both the UE and the MME upon receiving EMM messages
   --------------------------------------------------------------------------
*/
/****************************************************************************
 **                                                                        **
 ** Name:    emm_recv_status()                                         **
 **                                                                        **
 ** Description: Processes EMM status message                              **
 **                                                                        **
 ** Inputs:  ue_id:      UE lower layer identifier                  **
 **          msg:       The received EMM message                   **
 **      Others:    None                                       **
 **                                                                        **
 ** Outputs:     emm_cause: EMM cause code                             **
 **      Return:    RETURNok, RETURNerror                      **
 **      Others:    None                                       **
 **                                                                        **
 ***************************************************************************/
int
emm_recv_status (
  mme_ue_s1ap_id_t ue_id,
  emm_status_msg * msg,
  int *emm_cause,
  const nas_message_decode_status_t * status)
{
  OAILOG_FUNC_IN (LOG_NAS_EMM);
  int                                     rc = RETURNok;

  OAILOG_INFO (LOG_NAS_EMM, "EMMAS-SAP - Received EMM Status message (cause=%d)\n", msg->emmcause);
  /*
   * Message checking
   */
  *emm_cause = EMM_CAUSE_SUCCESS;
  /*
   * Message processing
   */
  rc = emm_proc_status_ind (ue_id, msg->emmcause);
  OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
}

/*
   --------------------------------------------------------------------------
   Functions executed by the MME upon receiving EMM message from the UE
   --------------------------------------------------------------------------
*/
/****************************************************************************
 **                                                                        **
 ** Name:    emm_recv_attach_request()                                 **
 **                                                                        **
 ** Description: Processes Attach Request message                          **
 **                                                                        **
 ** Inputs:  ue_id:      UE lower layer identifier                  **
 **      msg:       The received EMM message                   **
 **      Others:    None                                       **
 **                                                                        **
 ** Outputs:     emm_cause: EMM cause code                             **
 **      Return:    RETURNok, RETURNerror                      **
 **      Others:    None                                       **
 **                                                                        **
 ***************************************************************************/
int
emm_recv_attach_request (
  const enb_s1ap_id_key_t enb_ue_s1ap_id_key,
  const mme_ue_s1ap_id_t ue_id,
  const tai_t              * const originating_tai,
  const ecgi_t             * const originating_ecgi,
  const attach_request_msg * const msg,
  const bool                       is_initial,
  int * const emm_cause,
  const nas_message_decode_status_t  * decode_status)
{
  int                                     rc = RETURNok;
  emm_proc_attach_type_t                  type = EMM_ATTACH_TYPE_RESERVED;

  OAILOG_FUNC_IN (LOG_NAS_EMM);
  OAILOG_INFO (LOG_NAS_EMM, "EMMAS-SAP - Received Attach Request message\n");
  /*
   * Message checking
   */
  if (msg->uenetworkcapability.spare != 0b000) {
    /*
     * Spare bits shall be coded as zero
     */
    *emm_cause = EMM_CAUSE_PROTOCOL_ERROR;
    OAILOG_WARNING (LOG_NAS_EMM, "EMMAS-SAP - [%08x] - Non zero spare bits is suspicious\n", ue_id);
  }

  /*
   * Handle message checking error
   */
  if (*emm_cause != EMM_CAUSE_SUCCESS) {
    /*
     * Requirement MME24.301R10_5.5.1.2.7_b Protocol error
     */
    rc = emm_proc_attach_reject (ue_id, *emm_cause);
    *emm_cause = EMM_CAUSE_SUCCESS;
    OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
  }

  /*
   * Message processing
   */
  /*
   * Get the EPS attach type
   */
  if (msg->epsattachtype == EPS_ATTACH_TYPE_EPS) {
    type = EMM_ATTACH_TYPE_EPS;
    OAILOG_TRACE (LOG_NAS_EMM, "EMMAS-SAP - get attach type EPS_ATTACH_TYPE_EPS\n");
  } else if (msg->epsattachtype == EPS_ATTACH_TYPE_COMBINED_EPS_IMSI) {
    type = EMM_ATTACH_TYPE_COMBINED_EPS_IMSI;
    OAILOG_TRACE (LOG_NAS_EMM, "EMMAS-SAP - get attach type EPS_ATTACH_TYPE_COMBINED_EPS_IMSI\n");
  } else if (msg->epsattachtype == EPS_ATTACH_TYPE_EMERGENCY) {
    type = EMM_ATTACH_TYPE_EMERGENCY;
    OAILOG_TRACE (LOG_NAS_EMM, "EMMAS-SAP - get attach type EPS_ATTACH_TYPE_EMERGENCY\n");
  } else if (msg->epsattachtype == EPS_ATTACH_TYPE_RESERVED) {
    type = EMM_ATTACH_TYPE_RESERVED;
    OAILOG_TRACE (LOG_NAS_EMM, "EMMAS-SAP - get attach type EMM_ATTACH_TYPE_RESERVED\n");
  } else {
    /*
     * Requirement MME24.301R10_9.9.3.11_1
     * All other values shall be interpreted as "EPS attach"
     */
    type = EMM_ATTACH_TYPE_EPS;
    OAILOG_TRACE (LOG_NAS_EMM, "EMMAS-SAP - get attach type forced to EMM_ATTACH_TYPE_EPS\n");
  }

  /*
   * Get the EPS mobile identity
   */
  guti_t                                  guti;
  guti.gummei.plmn = {0};
  guti.gummei.mme_gid = 0;
  guti.gummei.mme_code = 0;
  guti.m_tmsi = INVALID_M_TMSI;

  guti_t                                 *p_guti = NULL;
  imsi_t                                  imsi = {0},
                                         *p_imsi = NULL;
  imei_t                                  imei = {0},
                                         *p_imei = NULL;

  if (msg->oldgutiorimsi.guti.typeofidentity == EPS_MOBILE_IDENTITY_GUTI) {
    OAILOG_TRACE (LOG_NAS_EMM, "EMMAS-SAP - get GUTI\n");
    /*
     * Get the GUTI
     */
    p_guti = &guti;
    guti.gummei.plmn.mcc_digit1 = msg->oldgutiorimsi.guti.mcc_digit1;
    guti.gummei.plmn.mcc_digit2 = msg->oldgutiorimsi.guti.mcc_digit2;
    guti.gummei.plmn.mcc_digit3 = msg->oldgutiorimsi.guti.mcc_digit3;
    guti.gummei.plmn.mnc_digit1 = msg->oldgutiorimsi.guti.mnc_digit1;
    guti.gummei.plmn.mnc_digit2 = msg->oldgutiorimsi.guti.mnc_digit2;
    guti.gummei.plmn.mnc_digit3 = msg->oldgutiorimsi.guti.mnc_digit3;
    guti.gummei.mme_gid = msg->oldgutiorimsi.guti.mme_group_id;
    guti.gummei.mme_code = msg->oldgutiorimsi.guti.mme_code;
    guti.m_tmsi = msg->oldgutiorimsi.guti.m_tmsi;
  } else if (msg->oldgutiorimsi.imsi.typeofidentity == EPS_MOBILE_IDENTITY_IMSI) {
    OAILOG_TRACE (LOG_NAS_EMM, "EMMAS-SAP - get IMSI\n");
    /*
     * Get the IMSI
     */
    p_imsi = &imsi;
    imsi.u.num.digit1 = msg->oldgutiorimsi.imsi.identity_digit1;
    imsi.u.num.digit2 = msg->oldgutiorimsi.imsi.identity_digit2;
    imsi.u.num.digit3 = msg->oldgutiorimsi.imsi.identity_digit3;
    imsi.u.num.digit4 = msg->oldgutiorimsi.imsi.identity_digit4;
    imsi.u.num.digit5 = msg->oldgutiorimsi.imsi.identity_digit5;
    imsi.u.num.digit6 = msg->oldgutiorimsi.imsi.identity_digit6;
    imsi.u.num.digit7 = msg->oldgutiorimsi.imsi.identity_digit7;
    imsi.u.num.digit8 = msg->oldgutiorimsi.imsi.identity_digit8;
    imsi.u.num.digit9 = msg->oldgutiorimsi.imsi.identity_digit9;
    imsi.u.num.digit10 = msg->oldgutiorimsi.imsi.identity_digit10;
    imsi.u.num.digit11 = msg->oldgutiorimsi.imsi.identity_digit11;
    imsi.u.num.digit12 = msg->oldgutiorimsi.imsi.identity_digit12;
    imsi.u.num.digit13 = msg->oldgutiorimsi.imsi.identity_digit13;
    imsi.u.num.digit14 = msg->oldgutiorimsi.imsi.identity_digit14;
    imsi.u.num.digit15 = msg->oldgutiorimsi.imsi.identity_digit15;
    imsi.u.num.parity  = 0x0f;
    for (int index=0; index < IMSI_BCD8_SIZE; index++) {
      if (imsi.u.value[index] == 0xff) {
        imsi.length = index;
        break;
      } else  if ((imsi.u.value[index] & 0x0f) == 0x0f) {
        imsi.length = index + 1;
        break;
      }
    }
  } else if (msg->oldgutiorimsi.imei.typeofidentity == EPS_MOBILE_IDENTITY_IMEI) {
    OAILOG_TRACE (LOG_NAS_EMM, "EMMAS-SAP - get IMEI\n");
    /*
     * Get the IMEI
     */
    p_imei = &imei;
    imei.u.num.tac1 = msg->oldgutiorimsi.imei.identity_digit1;
    imei.u.num.tac2 = msg->oldgutiorimsi.imei.identity_digit2;
    imei.u.num.tac3 = msg->oldgutiorimsi.imei.identity_digit3;
    imei.u.num.tac4 = msg->oldgutiorimsi.imei.identity_digit4;
    imei.u.num.tac5 = msg->oldgutiorimsi.imei.identity_digit5;
    imei.u.num.tac6 = msg->oldgutiorimsi.imei.identity_digit6;
    imei.u.num.tac7 = msg->oldgutiorimsi.imei.identity_digit7;
    imei.u.num.tac8 = msg->oldgutiorimsi.imei.identity_digit8;
    imei.u.num.snr1 = msg->oldgutiorimsi.imei.identity_digit9;
    imei.u.num.snr2 = msg->oldgutiorimsi.imei.identity_digit10;
    imei.u.num.snr3 = msg->oldgutiorimsi.imei.identity_digit11;
    imei.u.num.snr4 = msg->oldgutiorimsi.imei.identity_digit12;
    imei.u.num.snr5 = msg->oldgutiorimsi.imei.identity_digit13;
    imei.u.num.snr6 = msg->oldgutiorimsi.imei.identity_digit14;
    imei.u.num.cdsd = msg->oldgutiorimsi.imei.identity_digit15;
    imei.u.num.parity = msg->oldgutiorimsi.imei.oddeven;
  }

  /*
   * TODO: Get the UE network capabilities
   */
  /*
   * Get the Last visited registered TAI
   */
  tai_t                                   last_visited_registered_tai = {0},
                                         *p_last_visited_registered_tai = NULL;

  if (msg->presencemask & ATTACH_REQUEST_LAST_VISITED_REGISTERED_TAI_PRESENT) {
    OAILOG_TRACE (LOG_NAS_EMM, "EMMAS-SAP - get LAST VISITED REGISTERED TAI\n");
    p_last_visited_registered_tai = &last_visited_registered_tai;
    last_visited_registered_tai.mcc_digit1 = msg->lastvisitedregisteredtai.mcc_digit1;
    last_visited_registered_tai.mcc_digit2 = msg->lastvisitedregisteredtai.mcc_digit2;
    last_visited_registered_tai.mcc_digit3 = msg->lastvisitedregisteredtai.mcc_digit3;
    last_visited_registered_tai.mnc_digit1 = msg->lastvisitedregisteredtai.mnc_digit1;
    last_visited_registered_tai.mnc_digit2 = msg->lastvisitedregisteredtai.mnc_digit2;
    last_visited_registered_tai.mnc_digit3 = msg->lastvisitedregisteredtai.mnc_digit3;
    last_visited_registered_tai.tac = msg->lastvisitedregisteredtai.tac;
  }

  /*
   * Execute the requested UE attach procedure
   */
  rc = emm_proc_attach_request (enb_ue_s1ap_id_key, ue_id, is_initial, type,
                                msg->naskeysetidentifier.tsc != NAS_KEY_SET_IDENTIFIER_MAPPED,
                                msg->naskeysetidentifier.naskeysetidentifier,
                                msg->oldgutitype != GUTI_MAPPED, p_guti, p_imsi, p_imei,
                                p_last_visited_registered_tai,originating_tai,originating_ecgi,
                                &msg->uenetworkcapability,
                                (msg->presencemask & ATTACH_REQUEST_MS_NETWORK_CAPABILITY_PRESENT) ? &msg->msnetworkcapability: NULL,
                                msg->esmmessagecontainer,
                                decode_status);
  OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
}

/****************************************************************************
 **                                                                        **
 ** Name:    emm_recv_attach_complete()                                **
 **                                                                        **
 ** Description: Processes Attach Complete message                         **
 **                                                                        **
 ** Inputs:  ue_id:      UE lower layer identifier                  **
 **      msg:       The received EMM message                   **
 **      Others:    None                                       **
 **                                                                        **
 ** Outputs:     emm_cause: EMM cause code                             **
 **      Return:    RETURNok, RETURNerror                      **
 **      Others:    None                                       **
 **                                                                        **
 ***************************************************************************/
int
emm_recv_attach_complete (
  mme_ue_s1ap_id_t ue_id,
  const attach_complete_msg * msg,
  int *emm_cause,
  const nas_message_decode_status_t * status)
{
  OAILOG_FUNC_IN (LOG_NAS_EMM);
  int                                     rc;

  OAILOG_INFO (LOG_NAS_EMM, "EMMAS-SAP - Received Attach Complete message\n");
  /*
   * Execute the attach procedure completion
   */
  rc = emm_proc_attach_complete (ue_id, msg->esmmessagecontainer);
  OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
}

/****************************************************************************
 **                                                                        **
 ** Name:    emm_recv_detach_request()                                 **
 **                                                                        **
 ** Description: Processes Detach Request message                          **
 **                                                                        **
 ** Inputs:  ue_id:      UE lower layer identifier                  **
 **      msg:       The received EMM message                   **
 **      Others:    None                                       **
 **                                                                        **
 ** Outputs:     emm_cause: EMM cause code                             **
 **      Return:    RETURNok, RETURNerror                      **
 **      Others:    None                                       **
 **                                                                        **
 ***************************************************************************/
int
emm_recv_detach_request (
  mme_ue_s1ap_id_t ue_id,
  const detach_request_msg * msg,
  int *emm_cause,
  const nas_message_decode_status_t * status)
{
  OAILOG_FUNC_IN (LOG_NAS_EMM);
  int                                     rc  = RETURNok;

  OAILOG_INFO (LOG_NAS_EMM, "EMMAS-SAP - Received Detach Request message\n");
  /*
   * Message processing
   */
  /*
   * Get the detach type
   */
  emm_proc_detach_type_t                  type = EMM_DETACH_TYPE_RESERVED;

  if (msg->detachtype.typeofdetach == DETACH_TYPE_EPS) {
    type = EMM_DETACH_TYPE_EPS;
  } else if (msg->detachtype.typeofdetach == DETACH_TYPE_IMSI) {
    type = EMM_DETACH_TYPE_IMSI;
  } else if (msg->detachtype.typeofdetach == DETACH_TYPE_EPS_IMSI) {
    type = EMM_DETACH_TYPE_EPS_IMSI;
  } else if (msg->detachtype.typeofdetach == DETACH_TYPE_RESERVED_1) {
    type = EMM_DETACH_TYPE_RESERVED;
  } else if (msg->detachtype.typeofdetach == DETACH_TYPE_RESERVED_2) {
    type = EMM_DETACH_TYPE_RESERVED;
  } else {
    /*
     * All other values are interpreted as "combined EPS/IMSI detach"
     */
    type = static_cast<emm_proc_detach_type_t>(DETACH_TYPE_EPS_IMSI);
  }

  /*
   * Get the EPS mobile identity
   */
  guti_t                                  guti;
  guti.gummei.plmn = {0};
  guti.gummei.mme_gid = 0;
  guti.gummei.mme_code = 0;
  guti.m_tmsi = INVALID_M_TMSI;
  guti_t                                 *p_guti = NULL;
  imsi_t                                  imsi = {0},
                                         *p_imsi = NULL;
  imei_t                                  imei = {0},
                                         *p_imei = NULL;

  if (msg->gutiorimsi.guti.typeofidentity == EPS_MOBILE_IDENTITY_GUTI) {
    /*
     * Get the GUTI
     */
    p_guti = &guti;
    guti.gummei.plmn.mcc_digit1 = msg->gutiorimsi.guti.mcc_digit1;
    guti.gummei.plmn.mcc_digit2 = msg->gutiorimsi.guti.mcc_digit2;
    guti.gummei.plmn.mcc_digit3 = msg->gutiorimsi.guti.mcc_digit3;
    guti.gummei.plmn.mnc_digit1 = msg->gutiorimsi.guti.mnc_digit1;
    guti.gummei.plmn.mnc_digit2 = msg->gutiorimsi.guti.mnc_digit2;
    guti.gummei.plmn.mnc_digit3 = msg->gutiorimsi.guti.mnc_digit3;
    guti.gummei.mme_gid = msg->gutiorimsi.guti.mme_group_id;
    guti.gummei.mme_code = msg->gutiorimsi.guti.mme_code;
    guti.m_tmsi = msg->gutiorimsi.guti.m_tmsi;
  } else if (msg->gutiorimsi.imsi.typeofidentity == EPS_MOBILE_IDENTITY_IMSI) {
    /*
     * Get the IMSI
     */
    p_imsi = &imsi;
    imsi.u.num.digit1 = msg->gutiorimsi.imsi.identity_digit1;
    imsi.u.num.digit2 = msg->gutiorimsi.imsi.identity_digit2;
    imsi.u.num.digit3 = msg->gutiorimsi.imsi.identity_digit3;
    imsi.u.num.digit4 = msg->gutiorimsi.imsi.identity_digit4;
    imsi.u.num.digit5 = msg->gutiorimsi.imsi.identity_digit5;
    imsi.u.num.digit6 = msg->gutiorimsi.imsi.identity_digit6;
    imsi.u.num.digit7 = msg->gutiorimsi.imsi.identity_digit7;
    imsi.u.num.digit8 = msg->gutiorimsi.imsi.identity_digit8;
    imsi.u.num.digit9 = msg->gutiorimsi.imsi.identity_digit9;
    imsi.u.num.digit10 = msg->gutiorimsi.imsi.identity_digit10;
    imsi.u.num.digit11 = msg->gutiorimsi.imsi.identity_digit11;
    imsi.u.num.digit12 = msg->gutiorimsi.imsi.identity_digit12;
    imsi.u.num.digit13 = msg->gutiorimsi.imsi.identity_digit13;
    imsi.u.num.digit14 = msg->gutiorimsi.imsi.identity_digit14;
    imsi.u.num.digit15 = msg->gutiorimsi.imsi.identity_digit15;
    imsi.u.num.parity  = 0x0f;
    for (int index=0; index < IMSI_BCD8_SIZE; index++) {
      if (imsi.u.value[index] == 0xff) {
        imsi.length = index;
        break;
      } else  if ((imsi.u.value[index] & 0x0f) == 0x0f) {
        imsi.length = index + 1;
        break;
      }
    }
  } else if (msg->gutiorimsi.imei.typeofidentity == EPS_MOBILE_IDENTITY_IMEI) {
    /*
     * Get the IMEI
     */
    p_imei = &imei;
    imei.u.num.tac1 = msg->gutiorimsi.imei.identity_digit1;
    imei.u.num.tac2 = msg->gutiorimsi.imei.identity_digit2;
    imei.u.num.tac3 = msg->gutiorimsi.imei.identity_digit3;
    imei.u.num.tac4 = msg->gutiorimsi.imei.identity_digit4;
    imei.u.num.tac5 = msg->gutiorimsi.imei.identity_digit5;
    imei.u.num.tac6 = msg->gutiorimsi.imei.identity_digit6;
    imei.u.num.tac7 = msg->gutiorimsi.imei.identity_digit7;
    imei.u.num.tac8 = msg->gutiorimsi.imei.identity_digit8;
    imei.u.num.snr1 = msg->gutiorimsi.imei.identity_digit9;
    imei.u.num.snr2 = msg->gutiorimsi.imei.identity_digit10;
    imei.u.num.snr3 = msg->gutiorimsi.imei.identity_digit11;
    imei.u.num.snr4 = msg->gutiorimsi.imei.identity_digit12;
    imei.u.num.snr5 = msg->gutiorimsi.imei.identity_digit13;
    imei.u.num.snr6 = msg->gutiorimsi.imei.identity_digit14;
    imei.u.num.cdsd = msg->gutiorimsi.imei.identity_digit15;
    imei.u.num.parity = msg->gutiorimsi.imei.oddeven;
  }

  /*
   * Execute the UE initiated detach procedure completion by the network
   */
  rc = emm_proc_detach_request (ue_id, type, msg->detachtype.switchoff != DETACH_TYPE_NORMAL_DETACH,
      msg->naskeysetidentifier.tsc != NAS_KEY_SET_IDENTIFIER_MAPPED,
      msg->naskeysetidentifier.naskeysetidentifier, p_guti, p_imsi, p_imei);
  OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
}

/****************************************************************************
 **                                                                        **
 ** Name:        emm_recv_tracking_area_update_request()                   **
 **                                                                        **
 ** Description: Processes Tracking Area Update Request message            **
 **                                                                        **
 ** Inputs:      ue_id:          UE lower layer identifier                  **
 **              msg:           The received EMM message                   **
 **              Others:        None                                       **
 **                                                                        **
 ** Outputs:     emm_cause:     EMM cause code                             **
 **              Return:        RETURNok, RETURNerror                      **
 **              Others:        None                                       **
 **                                                                        **
 ***************************************************************************/
int
emm_recv_tracking_area_update_request (
  const mme_ue_s1ap_id_t ue_id,
  tracking_area_update_request_msg * const msg,
  int *emm_cause,
  const nas_message_decode_status_t  * decode_status)
{
  int                                     rc = RETURNok;

  OAILOG_FUNC_IN (LOG_NAS_EMM);
  OAILOG_INFO (LOG_NAS_EMM, "EMMAS-SAP - Received Tracking Area Update Request message, Security context %s Integrity protected %s MAC matched %s Ciphered %s\n",
      (decode_status->security_context_available)?"yes":"no",
      (decode_status->integrity_protected_message)?"yes":"no",
      (decode_status->mac_matched)?"yes":"no",
      (decode_status->ciphered_message)?"yes":"no");

  if (nas_config.force_reject_tau) {
    // LW: Not completely implemented; send a Received Tracking Area Update Reject to induce a Attach Request from UE!
    rc = emm_proc_tracking_area_update_reject (ue_id, EMM_CAUSE_IMPLICITLY_DETACHED);
  } else {
    rc = emm_proc_tracking_area_update_request(ue_id, msg, emm_cause, decode_status);
  }

  OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
}

/****************************************************************************
 **                                                                        **
 ** Name:        emm_recv_service_request()                                **
 **                                                                        **
 ** Description: Processes Service Request message                         **
 **                                                                        **
 ** Inputs:      ue_id:          UE lower layer identifier                  **
 **              msg:           The received EMM message                   **
 **              Others:        None                                       **
 **                                                                        **
 ** Outputs:     emm_cause:     EMM cause code                             **
 **              Return:        RETURNok, RETURNerror                      **
 **              Others:        None                                       **
 **                                                                        **
 ***************************************************************************/
int
emm_recv_service_request (
  mme_ue_s1ap_id_t ue_id,
  enb_ue_s1ap_id_t enb_ue_s1ap_id,
  const service_request_msg * msg,
  int *emm_cause,
  const nas_message_decode_status_t  * decode_status)
{
  int                                     rc = RETURNok;

  OAILOG_FUNC_IN (LOG_NAS_EMM);
  OAILOG_INFO (LOG_NAS_EMM, "EMMAS-SAP - Received Service Request message, Security context %s Integrity protected %s MAC matched %s Ciphered %s\n",
      (decode_status->security_context_available)?"yes":"no",
      (decode_status->integrity_protected_message)?"yes":"no",
      (decode_status->mac_matched)?"yes":"no",
      (decode_status->ciphered_message)?"yes":"no");

  if (nas_config.force_reject_sr) {
    /*
     * Service request procedure not implemented, send a Service Reject to induce a Attach Request from UE!
     */
    // EMM causes for triggering an attach in the UE can be "UE identity cannot be derived by the network": EMM_CAUSE_UE_IDENTITY_CANT_BE_DERIVED_BY_NW,
    // "Implicitly detached": EMM_CAUSE_IMPLICITLY_DETACHED,
    rc = emm_proc_service_reject (ue_id, enb_ue_s1ap_id, EMM_CAUSE_UE_IDENTITY_CANT_BE_DERIVED_BY_NW);
  }
  OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
}

/****************************************************************************
 **                                                                        **
 ** Name:    emm_recv_identity_response()                              **
 **                                                                        **
 ** Description: Processes Identity Response message                       **
 **                                                                        **
 ** Inputs:  ue_id:      UE lower layer identifier                  **
 **      msg:       The received EMM message                   **
 **      Others:    None                                       **
 **                                                                        **
 ** Outputs:     emm_cause: EMM cause code                             **
 **      Return:    RETURNok, RETURNerror                      **
 **      Others:    None                                       **
 **                                                                        **
 ***************************************************************************/
int
emm_recv_identity_response (
  mme_ue_s1ap_id_t ue_id,
  identity_response_msg * msg,
  int *emm_cause,
  const nas_message_decode_status_t * status)
{
  OAILOG_FUNC_IN (LOG_NAS_EMM);
  int                                     rc = RETURNok;

  OAILOG_INFO (LOG_NAS_EMM, "EMMAS-SAP - Received Identity Response message\n");
  /*
   * Message processing
   */
  /*
   * Get the mobile identity
   */
  imsi_t                                  imsi = {0},
                                         *p_imsi = NULL;
  imei_t                                  imei = {0},
                                         *p_imei = NULL;
  imeisv_t                                imeisv = {0},
                                         *p_imeisv = NULL;
  tmsi_t                                  tmsi = 0,
                                         *p_tmsi = NULL;

  if (msg->mobileidentity.imsi.typeofidentity == MOBILE_IDENTITY_IMSI) {
    /*
     * Get the IMSI
     */
    p_imsi = &imsi;
    imsi.u.num.digit1 = msg->mobileidentity.imsi.digit1;
    imsi.u.num.digit2 = msg->mobileidentity.imsi.digit2;
    imsi.u.num.digit3 = msg->mobileidentity.imsi.digit3;
    imsi.u.num.digit4 = msg->mobileidentity.imsi.digit4;
    imsi.u.num.digit5 = msg->mobileidentity.imsi.digit5;
    imsi.u.num.digit6 = msg->mobileidentity.imsi.digit6;
    imsi.u.num.digit7 = msg->mobileidentity.imsi.digit7;
    imsi.u.num.digit8 = msg->mobileidentity.imsi.digit8;
    imsi.u.num.digit9 = msg->mobileidentity.imsi.digit9;
    imsi.u.num.digit10 = msg->mobileidentity.imsi.digit10;
    imsi.u.num.digit11 = msg->mobileidentity.imsi.digit11;
    imsi.u.num.digit12 = msg->mobileidentity.imsi.digit12;
    imsi.u.num.digit13 = msg->mobileidentity.imsi.digit13;
    imsi.u.num.digit14 = msg->mobileidentity.imsi.digit14;
    imsi.u.num.digit15 = msg->mobileidentity.imsi.digit15;
    imsi.u.num.parity  = 0x0f;

    for (int index=0; index < IMSI_BCD8_SIZE; index++) {
      if (imsi.u.value[index] == 0xff) {
        imsi.length = index;
        break;
      } else  if ((imsi.u.value[index] & 0x0f) == 0x0f) {
        imsi.length = index + 1;
        break;
      }
    }
  } else if (msg->mobileidentity.imei.typeofidentity == MOBILE_IDENTITY_IMEI) {
    /*
     * Get the IMEI
     */
    p_imei = &imei;
    imei.u.num.tac1 = msg->mobileidentity.imei.tac1;
    imei.u.num.tac2 = msg->mobileidentity.imei.tac2;
    imei.u.num.tac3 = msg->mobileidentity.imei.tac3;
    imei.u.num.tac4 = msg->mobileidentity.imei.tac4;
    imei.u.num.tac5 = msg->mobileidentity.imei.tac5;
    imei.u.num.tac6 = msg->mobileidentity.imei.tac6;
    imei.u.num.tac7 = msg->mobileidentity.imei.tac7;
    imei.u.num.tac8 = msg->mobileidentity.imei.tac8;
    imei.u.num.snr1 = msg->mobileidentity.imei.snr1;
    imei.u.num.snr2 = msg->mobileidentity.imei.snr2;
    imei.u.num.snr3 = msg->mobileidentity.imei.snr3;
    imei.u.num.snr4 = msg->mobileidentity.imei.snr4;
    imei.u.num.snr5 = msg->mobileidentity.imei.snr5;
    imei.u.num.snr6 = msg->mobileidentity.imei.snr6;
    imei.u.num.cdsd = msg->mobileidentity.imei.cdsd;
    imei.u.num.parity = msg->mobileidentity.imei.oddeven;
  } else if (msg->mobileidentity.imeisv.typeofidentity == MOBILE_IDENTITY_IMEISV) {
    /*
     * Get the IMEISV
     */
    p_imeisv = &imeisv;
    imeisv.u.num.tac1 = msg->mobileidentity.imeisv.tac1;
    imeisv.u.num.tac2 = msg->mobileidentity.imeisv.tac2;
    imeisv.u.num.tac3 = msg->mobileidentity.imeisv.tac3;
    imeisv.u.num.tac4 = msg->mobileidentity.imeisv.tac4;
    imeisv.u.num.tac5 = msg->mobileidentity.imeisv.tac5;
    imeisv.u.num.tac6 = msg->mobileidentity.imeisv.tac6;
    imeisv.u.num.tac7 = msg->mobileidentity.imeisv.tac7;
    imeisv.u.num.tac8 = msg->mobileidentity.imeisv.tac8;
    imeisv.u.num.snr1 = msg->mobileidentity.imeisv.snr1;
    imeisv.u.num.snr2 = msg->mobileidentity.imeisv.snr2;
    imeisv.u.num.snr3 = msg->mobileidentity.imeisv.snr3;
    imeisv.u.num.snr4 = msg->mobileidentity.imeisv.snr4;
    imeisv.u.num.snr5 = msg->mobileidentity.imeisv.snr5;
    imeisv.u.num.snr6 = msg->mobileidentity.imeisv.snr6;
    imeisv.u.num.svn1 = msg->mobileidentity.imeisv.svn1;
    imeisv.u.num.svn2 = msg->mobileidentity.imeisv.svn2;
    imeisv.u.num.parity = msg->mobileidentity.imeisv.oddeven;
  } else if (msg->mobileidentity.tmsi.typeofidentity == MOBILE_IDENTITY_TMSI) {
    /*
     * Get the TMSI
     */
    p_tmsi = &tmsi;
    tmsi  = ((tmsi_t)msg->mobileidentity.tmsi.tmsi[0]) << 24;
    tmsi |= (((tmsi_t)msg->mobileidentity.tmsi.tmsi[1]) << 16);
    tmsi |= (((tmsi_t)msg->mobileidentity.tmsi.tmsi[2]) << 8);
    tmsi |= ((tmsi_t)msg->mobileidentity.tmsi.tmsi[3]);
  }

  /*
   * Execute the identification completion procedure
   */
  rc = emm_proc_identification_complete (ue_id, p_imsi, p_imei, p_imeisv, (uint32_t *) (p_tmsi));
  OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
}

/****************************************************************************
 **                                                                        **
 ** Name:    emm_recv_authentication_response()                        **
 **                                                                        **
 ** Description: Processes Authentication Response message                 **
 **                                                                        **
 ** Inputs:  ue_id:      UE lower layer identifier                  **
 **      msg:       The received EMM message                   **
 **      Others:    None                                       **
 **                                                                        **
 ** Outputs:     emm_cause: EMM cause code                             **
 **      Return:    RETURNok, RETURNerror                      **
 **      Others:    None                                       **
 **                                                                        **
 ***************************************************************************/
int
emm_recv_authentication_response (
  mme_ue_s1ap_id_t ue_id,
  authentication_response_msg * msg,
  int *emm_cause,
  const nas_message_decode_status_t * status)
{
  OAILOG_FUNC_IN (LOG_NAS_EMM);
  int                                     rc = RETURNok;

  OAILOG_INFO (LOG_NAS_EMM, "EMMAS-SAP - Received Authentication Response message\n");

  /*
   * Message checking
   */
  if ((NULL == msg->authenticationresponseparameter) || (!blength(msg->authenticationresponseparameter))) {
    /*
     * RES parameter shall not be null
     */
    *emm_cause = EMM_CAUSE_INVALID_MANDATORY_INFO;
  }

  /*
   * Handle message checking error
   */
  if (*emm_cause != EMM_CAUSE_SUCCESS) {
    OAILOG_FUNC_RETURN (LOG_NAS_EMM, RETURNerror);
  }

  /*
   * Message processing
   */
  /*
   * Execute the authentication completion procedure
   */
  rc = emm_proc_authentication_complete (ue_id, EMM_CAUSE_SUCCESS, msg->authenticationresponseparameter);
  OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
}

/****************************************************************************
 **                                                                        **
 ** Name:    emm_recv_authentication_failure()                         **
 **                                                                        **
 ** Description: Processes Authentication Failure message                  **
 **                                                                        **
 ** Inputs:  ue_id:      UE lower layer identifier                  **
 **      msg:       The received EMM message                   **
 **      Others:    None                                       **
 **                                                                        **
 ** Outputs:     emm_cause: EMM cause code                             **
 **      Return:    RETURNok, RETURNerror                      **
 **      Others:    None                                       **
 **                                                                        **
 ***************************************************************************/
int
emm_recv_authentication_failure (
  mme_ue_s1ap_id_t ue_id,
  authentication_failure_msg * msg,
  int *emm_cause,
  const nas_message_decode_status_t * status)
{
  OAILOG_FUNC_IN (LOG_NAS_EMM);
  int                                     rc = RETURNok;

  OAILOG_INFO (LOG_NAS_EMM, "EMMAS-SAP - Received Authentication Failure message\n");

  /*
   * Message checking
   */
  if (msg->emmcause == EMM_CAUSE_SUCCESS) {
    *emm_cause = EMM_CAUSE_INVALID_MANDATORY_INFO;
  } else if ((msg->emmcause == EMM_CAUSE_SYNCH_FAILURE) && !(msg->presencemask & AUTHENTICATION_FAILURE_AUTHENTICATION_FAILURE_PARAMETER_PRESENT)) {
    /*
     * AUTS parameter shall be present in case of "synch failure"
     */
    *emm_cause = EMM_CAUSE_INVALID_MANDATORY_INFO;
  }

  /*
   * Handle message checking error
   */
  if (*emm_cause != EMM_CAUSE_SUCCESS) {
    OAILOG_FUNC_RETURN (LOG_NAS_EMM, RETURNerror);
  }

  /*
   * Message processing
   */
  /*
   * Execute the authentication completion procedure
   */
  rc = emm_proc_authentication_failure (ue_id, msg->emmcause, msg->authenticationfailureparameter);
  OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
}

/****************************************************************************
 **                                                                        **
 ** Name:    emm_recv_security_mode_complete()                         **
 **                                                                        **
 ** Description: Processes Security Mode Complete message                  **
 **                                                                        **
 ** Inputs:  ue_id:      UE lower layer identifier                  **
 **      msg:       The received EMM message                   **
 **      Others:    None                                       **
 **                                                                        **
 ** Outputs:     emm_cause: EMM cause code                             **
 **      Return:    RETURNok, RETURNerror                      **
 **      Others:    None                                       **
 **                                                                        **
 ***************************************************************************/
int
emm_recv_security_mode_complete (
  mme_ue_s1ap_id_t ue_id,
  security_mode_complete_msg * msg,
  int *emm_cause,
  const nas_message_decode_status_t * status)
{
  OAILOG_FUNC_IN (LOG_NAS_EMM);
  int                                     rc = RETURNok;

  OAILOG_INFO (LOG_NAS_EMM, "EMMAS-SAP - Received Security Mode Complete message\n");
  /*
   * Message processing
   */
  /*
   * Execute the NAS security mode control completion procedure
   */
  if (msg->presencemask & SECURITY_MODE_COMMAND_IMEISV_REQUEST_PRESENT) {
    rc = emm_proc_security_mode_complete (ue_id, &msg->imeisv.imeisv);
  } else {
    rc = emm_proc_security_mode_complete (ue_id, NULL);
  }
  OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
}

/****************************************************************************
 **                                                                        **
 ** Name:    emm_recv_security_mode_reject()                           **
 **                                                                        **
 ** Description: Processes Security Mode Reject message                    **
 **                                                                        **
 ** Inputs:  ue_id:      UE lower layer identifier                  **
 **      msg:       The received EMM message                   **
 **      Others:    None                                       **
 **                                                                        **
 ** Outputs:     emm_cause: EMM cause code                             **
 **      Return:    RETURNok, RETURNerror                      **
 **      Others:    None                                       **
 **                                                                        **
 ***************************************************************************/
int
emm_recv_security_mode_reject (
  mme_ue_s1ap_id_t ue_id,
  security_mode_reject_msg * msg,
  int *emm_cause,
  const nas_message_decode_status_t * status)
{
  OAILOG_FUNC_IN (LOG_NAS_EMM);
  int                                     rc = RETURNok;

  OAILOG_INFO (LOG_NAS_EMM, "EMMAS-SAP - Received Security Mode Reject message " "(cause=%d)\n", msg->emmcause);

  /*
   * Message checking
   */
  if (msg->emmcause == EMM_CAUSE_SUCCESS) {
    *emm_cause = EMM_CAUSE_INVALID_MANDATORY_INFO;
  }

  /*
   * Handle message checking error
   */
  if (*emm_cause != EMM_CAUSE_SUCCESS) {
    OAILOG_FUNC_RETURN (LOG_NAS_EMM, RETURNerror);
  }

  /*
   * Message processing
   */
  /*
   * Execute the NAS security mode commend not accepted by the UE
   */
  rc = emm_proc_security_mode_reject (ue_id);
  OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
}
