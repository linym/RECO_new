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
  Source      TrackingAreaUpdate.c

  Version     0.1

  Date        2013/05/07

  Product     NAS stack

  Subsystem   EPS Mobility Management

  Author      Frederic Maurel

  Description Defines the tracking area update EMM procedure executed by the
        Non-Access Stratum.

        The tracking area updating procedure is always initiated by the
        UE and is used to update the registration of the actual tracking
        area of a UE in the network, to periodically notify the availa-
        bility of the UE to the network, for MME load balancing, to up-
        date certain UE specific parameters in the network.

*****************************************************************************/
#include <pthread.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "dynamic_memory_check.h"
#include "assertions.h"
#include "log.h"
#include "msc.h"
#include "nas_timer.h"
#include "3gpp_requirements_24.301.h"
#include "common_types.h"
#include "common_defs.h"
#include "3gpp_24.007.h"
#include "3gpp_24.008.h"
#include "3gpp_29.274.h"
#include "mme_app_ue_context.hpp"
#include "emm_proc.h"
#include "common_defs.h"
#include "emm_data.h"
#include "emm_sap.h"
#include "emm_cause.h"
#include "mme_config.h"
#include "mme_app_defs.h"

#include "nas_config.h"

/****************************************************************************/
/****************  E X T E R N A L    D E F I N I T I O N S  ****************/
/****************************************************************************/

/****************************************************************************/
/*******************  L O C A L    D E F I N I T I O N S  *******************/
/****************************************************************************/

/*
   --------------------------------------------------------------------------
     Internal data handled by the tracking area update procedure in the MME
   --------------------------------------------------------------------------
*/


int emm_proc_tracking_area_update_request (
  const mme_ue_s1ap_id_t ue_id,
  tracking_area_update_request_msg * const msg,
  int *emm_cause,
  const nas_message_decode_status_t  * decode_status);

static int _emm_tracking_area_update (emm_context_t * ctx);
static int _emm_tracking_area_update_security (emm_context_t * ctx);
static int _emm_tracking_area_update_reject (emm_context_t * ctx);
static int _emm_tracking_area_update_accept (emm_context_t * ctx, tau_data_t * data);
static int _emm_tracking_area_update_abort (emm_context_t * ctx);


/****************************************************************************/
/******************  E X P O R T E D    F U N C T I O N S  ******************/
/****************************************************************************/

//------------------------------------------------------------------------------
int emm_proc_tracking_area_update_request (
  const mme_ue_s1ap_id_t ue_id,
  tracking_area_update_request_msg *const msg,
  int *emm_cause,
  const nas_message_decode_status_t  * decode_status)
{
  OAILOG_FUNC_IN (LOG_NAS_EMM);
  int                                     rc = RETURNerror;
  ue_mm_context_t                        *ue_mm_context = NULL;
  emm_context_t                          *emm_ctx = NULL;

  /*
   * Get the UE's EMM context if it exists
   */

  ue_mm_context = mme_ue_context_exists_mme_ue_s1ap_id (&mme_app_desc.mme_ue_contexts, ue_id);
  if (ue_mm_context) {
    emm_ctx = &ue_mm_context->emm_context;
  }

  // May be the MME APP module did not find the context, but if we have the GUTI, we may find it
  if (! ue_mm_context) {
    if (EPS_MOBILE_IDENTITY_GUTI == msg->oldguti.guti.typeofidentity) {
      guti_t guti;
      guti.m_tmsi = msg->oldguti.guti.m_tmsi;
      guti.gummei.mme_gid = msg->oldguti.guti.mme_group_id;
      guti.gummei.mme_code = msg->oldguti.guti.mme_code;
      guti.gummei.plmn.mcc_digit1 = msg->oldguti.guti.mcc_digit1;
      guti.gummei.plmn.mcc_digit2 = msg->oldguti.guti.mcc_digit2;
      guti.gummei.plmn.mcc_digit3 = msg->oldguti.guti.mcc_digit3;
      guti.gummei.plmn.mnc_digit1 = msg->oldguti.guti.mnc_digit1;
      guti.gummei.plmn.mnc_digit2 = msg->oldguti.guti.mnc_digit2;
      guti.gummei.plmn.mnc_digit3 = msg->oldguti.guti.mnc_digit3;

      ue_mm_context = mme_ue_context_exists_guti (&mme_app_desc.mme_ue_contexts, &guti);

      if ( ue_mm_context) {
        emm_ctx = &ue_mm_context->emm_context;
        //TODO
        // ...anyway now for simplicity we reject the TAU (else have to re-do integrity checking on NAS msg)
        rc = emm_proc_tracking_area_update_reject (ue_id, EMM_CAUSE_IMPLICITLY_DETACHED);
        OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
      } else {
        // NO S10
        rc = emm_proc_tracking_area_update_reject (ue_id, EMM_CAUSE_IMPLICITLY_DETACHED);
        OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
      }
    }
  }
  /*
   * Requirements MME24.301R10_5.5.3.2.4_2
   */
  if (msg->presencemask & TRACKING_AREA_UPDATE_REQUEST_UE_NETWORK_CAPABILITY_IEI) {
    if ( emm_ctx) {
      memcpy(&emm_ctx->tau_ue_network_capability, &msg->uenetworkcapability, sizeof(emm_ctx->tau_ue_network_capability));
    }
  }
  if (msg->presencemask & TRACKING_AREA_UPDATE_REQUEST_MS_NETWORK_CAPABILITY_IEI) {
    if ( emm_ctx) {
      memcpy(&emm_ctx->tau_ms_network_capability, &msg->msnetworkcapability, sizeof(emm_ctx->tau_ms_network_capability));
    }
  }
  /*
   * Requirements MME24.301R10_5.5.3.2.4_3
   */
  if (msg->presencemask & TRACKING_AREA_UPDATE_REQUEST_UE_RADIO_CAPABILITY_INFORMATION_UPDATE_NEEDED_IEI) {
    if ( emm_ctx) {
      if (0 != msg->ueradiocapabilityinformationupdateneeded) {
        // delete the stored UE radio capability information if any
//#pragma message  "TODO: Actually UE radio capability information is not stored in EPC"
      }
    }
  }
  /*
   * Requirements MME24.301R10_5.5.3.2.4_4
   */
  if (msg->presencemask & TRACKING_AREA_UPDATE_REQUEST_DRX_PARAMETER_IEI) {
    if ( emm_ctx) {
      emm_ctx_set_current_drx_parameter(emm_ctx, &msg->drxparameter);
    }
  }
  /*
   * Requirement MME24.301R10_5.5.3.2.4_5a
   */
  if (msg->presencemask & TRACKING_AREA_UPDATE_REQUEST_EPS_BEARER_CONTEXT_STATUS_IEI) {
    if ( emm_ctx) {
      emm_ctx_set_eps_bearer_context_status(emm_ctx, &msg->epsbearercontextstatus);
//#pragma message  "TODO Requirement MME24.301R10_5.5.3.2.4_5a: TAU Request: Deactivate EPS bearers if necessary (S11 Modify Bearer Request)"
    }
  }
  /*
   * Requirement MME24.301R10_5.5.3.2.4_6
   */
  if ( EPS_UPDATE_TYPE_PERIODIC_UPDATING == msg->epsupdatetype.eps_update_type_value) {
/*
 * MME24.301R10_5.5.3.2.4_6 Normal and periodic tracking area updating procedure accepted by the network UE - EPS update type
 * If the EPS update type IE included in the TRACKING AREA UPDATE REQUEST message indicates "periodic updating", and the UE was
 *  previously successfully attached for EPS and non-EPS services, subject to operator policies the MME should allocate a TAI
 *  list that does not span more than one location area.
 */
    //TODO

  }
  /*
   * Requirement MME24.301R10_5.5.3.2.4_12
   * MME24.301R10_5.5.3.2.4_12 Normal and periodic tracking area updating procedure accepted by the network UE - active flag
   * If the "active" flag is included in the TRACKING AREA UPDATE REQUEST message, the MME shall re-establish
   * the radio and S1 bearers for all active EPS bearer contexts..
   */
  if ( 0 < msg->epsupdatetype.active_flag) {
//#pragma message  "TODO Requirement MME24.301R10_5.5.3.2.4_12: Request: re-establish the radio and S1 bearers for all active EPS bearer contexts (S11 Modify Bearer Request)"

  } else {
    /*
     * Requirement MME24.301R10_5.5.3.2.4_13
     * MME24.301R10_5.5.3.2.4_13 Normal and periodic tracking area updating procedure accepted by the network UE - not active flag
     * If the "active" flag is not included in the TRACKING AREA UPDATE REQUEST message, the MME may also re-establish the radio and
     *  S1 bearers for all active EPS bearer contexts due to downlink pending data or downlink pending signalling.
     */
//#pragma message  "TODO Requirement MME24.301R10_5.5.3.2.4_13: Request: re-establish the radio and S1 bearers for all active EPS bearer contexts (S11 Modify Bearer Request)"
  }

  /*
   * MME24.301R10_5.5.3.2.3_3 TAU procedure: EMM common procedure initiation - emergency bearer
   * The MME may be configured to skip the authentication procedure even if no EPS security context is available and
   * proceed directly to the execution of the security mode control procedure as specified in subclause 5.4.3, during a
   * tracking area updating procedure for a UE that has only a PDN connection for emergency bearer services.
   */
  if ((! IS_EMM_CTXT_VALID_SECURITY(emm_ctx)) &&
      (_emm_data.conf.eps_network_feature_support & EPS_NETWORK_FEATURE_SUPPORT_EMERGENCY_BEARER_SERVICES_IN_S1_MODE_SUPPORTED) &&
      (emm_ctx->is_emergency)) {
    if ( IS_EMM_CTXT_PRESENT_UE_NETWORK_CAPABILITY(emm_ctx)) {
      // overwrite previous values
      emm_ctx_set_valid_ue_nw_cap(emm_ctx, &emm_ctx->tau_ue_network_capability);
    }
    rc = emm_proc_security_mode_control (ue_mm_context->mme_ue_s1ap_id,
      (ksi_t)0,        // TODO: eksi != 0
      _emm_tracking_area_update,
      _emm_tracking_area_update_reject,
      _emm_tracking_area_update_reject);
    OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
  } /*else {
     *
     * MME24.301R10_5.5.3.2.3_4 TAU procedure: EMM common procedure initiation – no authentication procedure
     * The MME shall not initiate an EMM authentication procedure before completion of the tracking area updating
     * procedure, if the following conditions apply:
     * a) the UE initiated the tracking area updating procedure after handover or inter-system handover to S1 mode;
     * b) the target cell is a shared network cell; and
     *   -the UE has provided its GUTI in the Old GUTI IE or the Additional GUTI IE in the TRACKING AREA
     *   UPDATE REQUEST message, and the PLMN identity included in the GUTI is different from the selected
     *   PLMN identity of the target cell; or
     *   -the UE has mapped the P-TMSI and RAI into the Old GUTI IE and not included an Additional GUTI IE in
     *   the TRACKING AREA UPDATE REQUEST message, and the PLMN identity included in the RAI is
     *   different from the selected PLMN identity of the target cell.
     }
     */
  else {
    /*
     * MME24.301R10_5.5.3.2.3_2 TAU procedure: EMM common procedure initiation
     * During the tracking area updating procedure, the MME may initiate EMM common procedures, e.g. the EMM
     * authentication and security mode control procedures.
     */
    bool authentication_requested = false;
    authentication_requested = ((0 < decode_status->integrity_protected_message) && (0 == decode_status->mac_matched)) ||
        (0 == decode_status->integrity_protected_message) || (0 == decode_status->security_context_available);
    if (authentication_requested) {
      int                                     eksi = 0;
      int                                     vindex = 0;

      if (emm_ctx->_security.eksi !=  KSI_NO_KEY_AVAILABLE) {
        REQUIREMENT_3GPP_24_301(R10_5_4_2_4__2);
        eksi = (emm_ctx->_security.eksi + 1) % (EKSI_MAX_VALUE + 1);
      }
      for (vindex = 0; vindex < MAX_EPS_AUTH_VECTORS; vindex++) {
        if (IS_EMM_CTXT_PRESENT_AUTH_VECTOR(emm_ctx, vindex)) {
          break;
        }
      }
      AssertFatal(IS_EMM_CTXT_PRESENT_AUTH_VECTOR(emm_ctx, vindex), "TODO No valid vector, should not happen");
      emm_ctx_set_security_vector_index(emm_ctx, vindex);

      rc = emm_proc_authentication (emm_ctx, ue_mm_context->mme_ue_s1ap_id, eksi,
          emm_ctx->_vector[vindex].rand,
          emm_ctx->_vector[vindex].autn,
          _emm_tracking_area_update_security,
          _emm_tracking_area_update_reject,
          _emm_tracking_area_update_reject);
      OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
    } else {
      if ( IS_EMM_CTXT_PRESENT_UE_NETWORK_CAPABILITY(emm_ctx)) {
        if (memcmp(&emm_ctx->_ue_network_capability, &emm_ctx->tau_ue_network_capability, sizeof(emm_ctx->_ue_network_capability))) {
          emm_ctx_set_valid_ue_nw_cap(emm_ctx, &emm_ctx->tau_ue_network_capability);
        }
      }
      // keep things simple at the beginning
      //if (network_capability_have_changed)
      {
        rc = emm_proc_security_mode_control (ue_mm_context->mme_ue_s1ap_id,
            0,        // TODO: eksi != 0
            _emm_tracking_area_update,
            _emm_tracking_area_update_reject,
            _emm_tracking_area_update_reject);
        OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
      } //else {
        // TAU Accept
      //}

    }
    OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
  }

  rc = emm_proc_tracking_area_update_reject (ue_id, EMM_CAUSE_ILLEGAL_UE);
  OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
}
/****************************************************************************
 **                                                                        **
 ** Name:        emm_proc_tracking_area_update_reject()                    **
 **                                                                        **
 ** Description:                                                           **
 **                                                                        **
 ** Inputs:  ue_id:              UE lower layer identifier                  **
 **                  emm_cause: EMM cause code to be reported              **
 **                  Others:    None                                       **
 **                                                                        **
 ** Outputs:     None                                                      **
 **                  Return:    RETURNok, RETURNerror                      **
 **                  Others:    _emm_data                                  **
 **                                                                        **
 ***************************************************************************/
int emm_proc_tracking_area_update_reject (
  const mme_ue_s1ap_id_t ue_id,
  const emm_cause_t emm_cause)
{
  OAILOG_FUNC_IN (LOG_NAS_EMM);
  int                                     rc = RETURNerror;

  /*
   * Create temporary UE context
   */
  ue_mm_context_t                      ue_mm_context;

  memset (&ue_mm_context, 0, sizeof (ue_mm_context_t));
  ue_mm_context.emm_context.is_dynamic = false;
  ue_mm_context.mme_ue_s1ap_id = ue_id;
  /*
   * Update the EMM cause code
   */
  if (ue_id > 0)
  {
    ue_mm_context.emm_context.emm_cause = emm_cause;
  } else {
    ue_mm_context.emm_context.emm_cause = EMM_CAUSE_ILLEGAL_UE;
  }

  /*
   * Do not accept attach request with protocol error
   */
  rc = _emm_tracking_area_update_reject (&ue_mm_context.emm_context);
  OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
}

/****************************************************************************/
/*********************  L O C A L    F U N C T I O N S  *********************/
/****************************************************************************/

static int _emm_tracking_area_update (emm_context_t * emm_context)
{
  OAILOG_FUNC_IN (LOG_NAS_EMM);
  int                                     rc = RETURNerror;

  if (emm_context) {


    if (emm_context->specific_proc) {
      emm_proc_specific_cleanup(&emm_context->specific_proc);
    }
    emm_context->specific_proc = (emm_specific_procedure_data_t *) calloc (1, sizeof (*emm_context->specific_proc));

    if (emm_context->specific_proc ) {
      /*
       * Setup ongoing EMM procedure callback functions
       */
      AssertFatal(0, "TODO Code commented");
      rc = emm_proc_specific_initialize (emm_context, EMM_SPECIFIC_PROC_TYPE_TAU, emm_context->specific_proc, NULL, NULL, _emm_tracking_area_update_abort);

      if (rc != RETURNok) {
        OAILOG_WARNING (LOG_NAS_EMM, "Failed to initialize EMM callback functions");
        OAILOG_FUNC_RETURN (LOG_NAS_EMM, RETURNerror);
      }

      tau_data_t                             *data = (tau_data_t *) calloc (1, sizeof (tau_data_t));
      /*
       * Set the UE identifier
       */
      data->ue_id = PARENT_STRUCT(emm_context, ue_mm_context_t, emm_context)->mme_ue_s1ap_id;
      /*
       * Reset the retransmission counter
       */
      data->retransmission_count = 0;
      rc = RETURNok;

      // Send TAU accept to the UE
      rc = _emm_tracking_area_update_accept (emm_context, data);
    } else {
      /*
       * The TAU procedure failed
       */
      OAILOG_WARNING (LOG_NAS_EMM, "EMM-PROC  - Failed to respond to TAU Request");
      emm_context->emm_cause = EMM_CAUSE_PROTOCOL_ERROR;
      rc = _emm_tracking_area_update_reject (emm_context);
    }
  }

  OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
}
/*
 * --------------------------------------------------------------------------
 * Timer handlers
 * --------------------------------------------------------------------------
 */

/** \fn void _emm_tau_t3450_handler(void *args);
\brief T3450 timeout handler
On the first expiry of the timer, the network shall retransmit the TRACKING AREA UPDATE ACCEPT
message and shall reset and restart timer T3450. The retransmission is performed four times, i.e. on the fifth
expiry of timer T3450, the tracking area updating procedure is aborted. Both, the old and the new GUTI shall be
considered as valid until the old GUTI can be considered as invalid by the network (see subclause 5.4.1.4).
During this period the network acts as described for case a above.
@param [in]args TAU accept data
*/
//------------------------------------------------------------------------------
static void *_emm_tau_t3450_handler (void *args)
{
  OAILOG_FUNC_IN (LOG_NAS_EMM);
  tau_data_t                      *data = (tau_data_t *) (args);

  // Requirement MME24.301R10_5.5.3.2.7_c Abnormal cases on the network side - T3450 time-out
  /*
   * Increment the retransmission counter
   */
  data->retransmission_count += 1;
  OAILOG_WARNING (LOG_NAS_EMM, "EMM-PROC  - T3450 timer expired, retransmission counter = %d", data->retransmission_count);
  /*
   * Get the UE's EMM context
   */
  ue_mm_context_t *ue_mm_context = mme_ue_context_exists_mme_ue_s1ap_id (&mme_app_desc.mme_ue_contexts, data->ue_id);

  if (data->retransmission_count < TAU_COUNTER_MAX) {
    /*
     * Send attach accept message to the UE
     */
    _emm_tracking_area_update_accept (&ue_mm_context->emm_context, data);
  } else {
    /*
     * Abort the attach procedure
     */
    _emm_tracking_area_update_abort (&ue_mm_context->emm_context);
  }

  OAILOG_FUNC_RETURN (LOG_NAS_EMM, NULL);
}

/** \fn void _emm_tracking_area_update_reject(void *args);
    \brief Performs the tracking area update procedure not accepted by the network.
     @param [in]args UE EMM context data
     @returns status of operation
*/
//------------------------------------------------------------------------------
static int _emm_tracking_area_update_security (emm_context_t * emm_context)
{
  OAILOG_FUNC_IN (LOG_NAS_EMM);
  int                                     rc = RETURNerror;

  if (emm_context) {
    mme_ue_s1ap_id_t                        ue_id = PARENT_STRUCT(emm_context, ue_mm_context_t, emm_context)->mme_ue_s1ap_id;
    OAILOG_INFO (LOG_NAS_EMM, "EMM-PROC  - Setup NAS security (ue_id=" MME_UE_S1AP_ID_FMT ")", ue_id);

    /*
     * Create new NAS security context
     */


    emm_ctx_clear_security(emm_context);
    /*
     * Initialize the security mode control procedure
     */
    rc = emm_proc_security_mode_control (PARENT_STRUCT(emm_context, ue_mm_context_t, emm_context)->mme_ue_s1ap_id, 0,        // TODO: eksi != 0
                                         _emm_tracking_area_update,
                                         _emm_tracking_area_update_reject,
                                         _emm_tracking_area_update_reject);

    if (rc != RETURNok) {
      /*
       * Failed to initiate the security mode control procedure
       */
      OAILOG_WARNING (LOG_NAS_EMM, "EMM-PROC  - Failed to initiate security mode control procedure");
      emm_context->emm_cause = EMM_CAUSE_ILLEGAL_UE;
      /*
       * Do not accept the UE to attach to the network
       */
      rc = _emm_tracking_area_update_reject (emm_context);
    }
  }
  OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
}

/** \fn void _emm_tracking_area_update_reject(void *args);
    \brief Performs the tracking area update procedure not accepted by the network.
     @param [in]args UE EMM context data
     @returns status of operation
*/
//------------------------------------------------------------------------------
static int _emm_tracking_area_update_reject (emm_context_t * emm_context)
{
  OAILOG_FUNC_IN (LOG_NAS_EMM);
  int                                     rc = RETURNerror;

  if (emm_context) {
    emm_sap_t                               emm_sap;
    memset(&emm_sap, 0, sizeof(emm_sap_t));
    mme_ue_s1ap_id_t                        ue_id = PARENT_STRUCT(emm_context, ue_mm_context_t, emm_context)->mme_ue_s1ap_id;

    OAILOG_WARNING (LOG_NAS_EMM, "EMM-PROC  - EMM tracking area update procedure not accepted " "by the network (ue_id=" MME_UE_S1AP_ID_FMT ", cause=%d)",
            ue_id, emm_context->emm_cause);
    /*
     * Notify EMM-AS SAP that Tracking Area Update Reject message has to be sent
     * onto the network
     */
    emm_sap.primitive = EMMAS_ESTABLISH_REJ;
    emm_sap.u.emm_as.u.establish.ue_id = ue_id;
    emm_sap.u.emm_as.u.establish.eps_id.guti = NULL;

    if (emm_context->emm_cause == EMM_CAUSE_SUCCESS) {
    	emm_context->emm_cause = EMM_CAUSE_ILLEGAL_UE;
    }

    emm_sap.u.emm_as.u.establish.emm_cause = emm_context->emm_cause;
    emm_sap.u.emm_as.u.establish.nas_info = EMM_AS_NAS_INFO_TAU;
    emm_sap.u.emm_as.u.establish.nas_msg = NULL;
    /*
     * Setup EPS NAS security data
     */
    emm_as_set_security_data (&emm_sap.u.emm_as.u.establish.sctx, &emm_context->_security, false, true);
    MSC_LOG_TX_MESSAGE (MSC_NAS_EMM_MME, MSC_NAS_EMM_MME, NULL, 0, "0 EMMAS_ESTABLISH_REJ ue id " MME_UE_S1AP_ID_FMT " ", ue_id);
    rc = emm_sap_send (&emm_sap);
  }

  OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
}

/** \fn void _emm_tracking_area_update_accept (emm_context_t * emm_ctx,tau_accept_data_t * data);
    \brief Sends ATTACH ACCEPT message and start timer T3450.
     @param [in]emm_ctx UE EMM context data
     @param [in]data    UE TAU accept data
     @returns status of operation (RETURNok, RETURNerror)
*/
//------------------------------------------------------------------------------
static int _emm_tracking_area_update_accept (
  emm_context_t * emm_context,
  tau_data_t * data)
{
  OAILOG_FUNC_IN (LOG_NAS_EMM);
  int                                     rc = RETURNerror;
  emm_sap_t                               emm_sap;
  memset(&emm_sap, 0, sizeof(emm_sap_t));

  // may be caused by timer not stopped when deleted context
  if (emm_context) {
    /*
     * Notify EMM-AS SAP that Attach Accept message together with an Activate
     * Default EPS Bearer Context Request message has to be sent to the UE
     */

    emm_sap.primitive = EMMAS_ESTABLISH_CNF;
    emm_sap.u.emm_as.u.establish.ue_id = PARENT_STRUCT(emm_context, ue_mm_context_t, emm_context)->mme_ue_s1ap_id;
    AssertFatal(emm_sap.u.emm_as.u.establish.ue_id == data->ue_id, "Mismatch in UE ids: ctx UE id" MME_UE_S1AP_ID_FMT " data UE id" MME_UE_S1AP_ID_FMT "\n",
           emm_sap.u.emm_as.u.establish.ue_id, data->ue_id);

    emm_sap.u.emm_as.u.establish.eps_update_result = EPS_UPDATE_RESULT_TA_UPDATED;
    if (nas_config.t3412_min > 0) {
      emm_sap.u.emm_as.u.establish.t3412 = &nas_config.t3412_min;
    }
    // TODO Reminder
    emm_sap.u.emm_as.u.establish.guti = NULL;

    memcpy(&emm_sap.u.emm_as.u.establish.tai_list, &emm_context->_tai_list, sizeof(tai_list_t));
    emm_sap.u.emm_as.u.establish.nas_info = EMM_AS_NAS_INFO_TAU;

    // TODO Reminder
    emm_sap.u.emm_as.u.establish.eps_bearer_context_status = NULL;
    emm_sap.u.emm_as.u.establish.location_area_identification = NULL;
    emm_sap.u.emm_as.u.establish.combined_tau_emm_cause = NULL;


    if (nas_config.t3402_min > 0) {
      emm_sap.u.emm_as.u.establish.t3402 = &nas_config.t3402_min;
    }
    emm_sap.u.emm_as.u.establish.t3423 = NULL;
    // TODO Reminder
    emm_sap.u.emm_as.u.establish.equivalent_plmns = NULL;
    emm_sap.u.emm_as.u.establish.emergency_number_list = NULL;

    emm_sap.u.emm_as.u.establish.eps_network_feature_support = &_emm_data.conf.eps_network_feature_support;
    emm_sap.u.emm_as.u.establish.additional_update_result = NULL;
    emm_sap.u.emm_as.u.establish.t3412_extended = NULL;


    /*
     * Setup EPS NAS security data
     */
    emm_as_set_security_data (&emm_sap.u.emm_as.u.establish.sctx, &emm_context->_security, false, true);
    OAILOG_INFO (LOG_NAS_EMM, "EMM-PROC  - encryption = 0x%X ", emm_sap.u.emm_as.u.establish.encryption);
    OAILOG_INFO (LOG_NAS_EMM, "EMM-PROC  - integrity  = 0x%X ", emm_sap.u.emm_as.u.establish.integrity);
    emm_sap.u.emm_as.u.establish.encryption = emm_context->_security.selected_algorithms.encryption;
    emm_sap.u.emm_as.u.establish.integrity = emm_context->_security.selected_algorithms.integrity;
    OAILOG_INFO (LOG_NAS_EMM, "EMM-PROC  - encryption = 0x%X (0x%X)", emm_sap.u.emm_as.u.establish.encryption, emm_context->_security.selected_algorithms.encryption);
    OAILOG_INFO (LOG_NAS_EMM, "EMM-PROC  - integrity  = 0x%X (0x%X)", emm_sap.u.emm_as.u.establish.integrity, emm_context->_security.selected_algorithms.integrity);
    /*
     * Get the activate default EPS bearer context request message to
     * transfer within the ESM container of the attach accept message
     */
    //emm_sap.u.emm_as.u.establish.nas_msg = ...;
    //LOG_INFO (LOG_NAS_EMM, "EMM-PROC  - nas_msg  src size = %d nas_msg  dst size = %d ", data->esm_msg.length, emm_sap.u.emm_as.u.establish.nas_msg.length);
    MSC_LOG_TX_MESSAGE (MSC_NAS_EMM_MME, MSC_NAS_EMM_MME, NULL, 0, "0 EMMAS_ESTABLISH_CNF ue id " MME_UE_S1AP_ID_FMT " ", data->ue_id);
    rc = emm_sap_send (&emm_sap);

    if (rc != RETURNerror) {
      if (emm_context->T3450.id != NAS_TIMER_INACTIVE_ID) {
        /*
         * Re-start T3450 timer
         */
        emm_context->T3450.id = nas_timer_stop (emm_context->T3450.id);
        MSC_LOG_EVENT (MSC_NAS_EMM_MME, "T3450 Stopped UE " MME_UE_S1AP_ID_FMT " (TAU)", data->ue_id);
      }
      /*
       * Start T3450 timer
       */
      emm_context->T3450.id = nas_timer_start (emm_context->T3450.sec, 0  /*usec*/, _emm_tau_t3450_handler, data);
      MSC_LOG_EVENT (MSC_NAS_EMM_MME, "T3450 started UE " MME_UE_S1AP_ID_FMT " (TAU)", data->ue_id);

      OAILOG_INFO (LOG_NAS_EMM, "EMM-PROC  - Timer T3450 (%ld) expires in %ld seconds (TAU)", emm_context->T3450.id, emm_context->T3450.sec);
    }
  } else {
    OAILOG_WARNING (LOG_NAS_EMM, "EMM-PROC  - emm_ctx NULL");
  }

  OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
}

//------------------------------------------------------------------------------
static int _emm_tracking_area_update_abort (emm_context_t * emm_context)
{
  OAILOG_FUNC_IN (LOG_NAS_EMM);
  int                                     rc = RETURNerror;

  if (emm_context) {

    if (emm_ctx_is_specific_procedure_running(emm_context, EMM_CTXT_SPEC_PROC_TAU)) {
      //tau_data_t                          *data = &emm_ctx->specific_proc->arg.u.tau_data;
      mme_ue_s1ap_id_t                        ue_id = PARENT_STRUCT(emm_context, ue_mm_context_t, emm_context)->mme_ue_s1ap_id;
      OAILOG_WARNING (LOG_NAS_EMM, "EMM-PROC  - Abort the TAU procedure (ue_id=" MME_UE_S1AP_ID_FMT ")", ue_id);

      /*
       * Stop timer T3450
       */
      if (emm_context->T3450.id != NAS_TIMER_INACTIVE_ID) {
        OAILOG_INFO (LOG_NAS_EMM, "EMM-PROC  - Stop timer T3450 (%ld)", emm_context->T3450.id);
        emm_context->T3450.id = nas_timer_stop (emm_context->T3450.id);
        MSC_LOG_EVENT (MSC_NAS_EMM_MME, "T3450 stopped UE " MME_UE_S1AP_ID_FMT " (TAU)", ue_id);
      }

      /*
       * Release retransmission timer parameters
       */
      // no contained struct to free

      /*
       * Notify EMM that EPS attach procedure failed
       */
      emm_sap_t                               emm_sap;
      memset(&emm_sap, 0, sizeof(emm_sap_t));

      emm_sap.primitive = EMMREG_ATTACH_REJ;
      emm_sap.u.emm_reg.ue_id = ue_id;
      emm_sap.u.emm_reg.ctx = emm_context;
      MSC_LOG_TX_MESSAGE (MSC_NAS_EMM_MME, MSC_NAS_EMM_MME, NULL, 0, "0 EMMREG_ATTACH_REJ ue id " MME_UE_S1AP_ID_FMT " ", ue_id);
      rc = emm_sap_send (&emm_sap);
    }
  }
  OAILOG_FUNC_RETURN (LOG_NAS_EMM, rc);
}
