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


/*! \file mme_app_transport.c
  \brief
  \author Lionel Gauthier
  \company Eurecom
  \email: lionel.gauthier@eurecom.fr
*/
#include "log.h"
#include "msc.h"
#include "assertions.h"
#include "conversions.h"
#include "intertask_interface.h"
#include "mme_config.h"

#include "mme_app_extern.h"
#include "mme_app_ue_context.hpp"
#include "mme_app_defs.h"
#include "sgw_ie_defs.h"

#include "secu_defs.h"
#include "common_defs.h"

//------------------------------------------------------------------------------
int mme_app_handle_nas_dl_req (
    itti_nas_dl_data_req_t *const nas_dl_req_pP)
//------------------------------------------------------------------------------
{
  OAILOG_FUNC_IN (LOG_MME_APP);
  MessageDef                             *message_p    = NULL;
  int                                     rc = RETURNok;
  enb_ue_s1ap_id_t                        enb_ue_s1ap_id = INVALID_ENB_UE_S1AP_ID;


  message_p = itti_alloc_new_message (TASK_MME_APP, NAS_DOWNLINK_DATA_REQ);

  ue_mm_context_t   *ue_context = mme_app_desc.GetUeContextByMmeUeS1apId(nas_dl_req_pP->ue_id);
  if (ue_context) {
    OAILOG_DEBUG (LOG_MME_APP, "DOWNLINK NAS TRANSPORT Found enb_ue_s1ap_id " ENB_UE_S1AP_ID_FMT " mme_ue_s1ap_id " MME_UE_S1AP_ID_FMT "\n",
        ue_context->enb_ue_s1ap_id, nas_dl_req_pP->ue_id);

    enb_ue_s1ap_id = ue_context->enb_ue_s1ap_id;

    if (!ue_context->is_s1_ue_context_release) {
      NAS_DL_DATA_REQ (message_p).enb_ue_s1ap_id         = enb_ue_s1ap_id;
      NAS_DL_DATA_REQ (message_p).ue_id                  = nas_dl_req_pP->ue_id;
      NAS_DL_DATA_REQ (message_p).nas_msg                = nas_dl_req_pP->nas_msg;
      nas_dl_req_pP->nas_msg                             = NULL;

      MSC_LOG_TX_MESSAGE (MSC_MMEAPP_MME, MSC_S1AP_MME, NULL, 0,
          "0 DOWNLINK NAS TRANSPORT enb_ue_s1ap_id " ENB_UE_S1AP_ID_FMT " ue id " MME_UE_S1AP_ID_FMT " ",
          enb_ue_s1ap_id, nas_dl_req_pP->ue_id);

      int to_task = (RUN_MODE_SCENARIO_PLAYER == mme_config.run_mode) ? TASK_MME_SCENARIO_PLAYER:TASK_S1AP;
      rc = itti_send_msg_to_task (static_cast<task_id_t>(to_task), INSTANCE_DEFAULT, message_p);
    } else {
      OAILOG_DEBUG (LOG_MME_APP, "DOWNLINK NAS TRANSPORT stopped because of S1AP UE CONTEXT RELEASE procedure mme_ue_s1ap_id " MME_UE_S1AP_ID_FMT "\n",
          nas_dl_req_pP->ue_id);
    }
  } else {
    OAILOG_DEBUG (LOG_MME_APP, "DOWNLINK NAS TRANSPORT failed mme_ue_s1ap_id " MME_UE_S1AP_ID_FMT " not found\n", nas_dl_req_pP->ue_id);

  }

  OAILOG_FUNC_RETURN (LOG_MME_APP, rc);
}

