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

/*! \file nas_mme_task.c
   \brief
   \author  Sebastien ROUX, Lionel GAUTHIER
   \date
   \email: lionel.gauthier@eurecom.fr
*/


#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "bstrlib.h"
#include <libxml/xmlwriter.h>
#include <libxml/xpath.h>

#include "log.h"
#include "msc.h"
#include "assertions.h"
#include "common_defs.h"
#include "intertask_interface.h"
#include "itti_free_defined_msg.h"
#include "mme_config.h"
#include "nas_defs.h"
#include "nas_network.h"
#include "nas_proc.h"
#include "emm_main.h"
#include "nas_timer.h"
#include "xml_msg_dump_itti.h"
#include "nas_config.h"

//------------------------------------------------------------------------------
static void *nas_intertask_interface (void *args_p)
{
  class nas_cxx *nas_obj = (class nas_cxx *)args_p;
  itti_mark_task_ready (TASK_NAS_MME);

  while (1) {
    MessageDef                             *received_message_p = NULL;

    itti_receive_msg (TASK_NAS_MME, &received_message_p);

    switch (ITTI_MSG_ID (received_message_p)) {
    case MESSAGE_TEST:{
        OAI_FPRINTF_INFO("TASK_NAS_MME received MESSAGE_TEST\n");
      }
      break;

    case MME_APP_CREATE_DEDICATED_BEARER_REQ:
      nas_obj->nas_proc_create_dedicated_bearer(&MME_APP_CREATE_DEDICATED_BEARER_REQ (received_message_p));
      break;

    case NAS_DOWNLINK_DATA_CNF:{
        nas_obj->nas_proc_dl_transfer_cnf (NAS_DL_DATA_CNF (received_message_p).ue_id, NAS_DL_DATA_CNF (received_message_p).err_code);
      }
      break;

    case NAS_DOWNLINK_DATA_REJ:{
        nas_obj->nas_proc_dl_transfer_rej (NAS_DL_DATA_REJ (received_message_p).ue_id, static_cast<nas_error_code_t>(NAS_DL_DATA_REJ (received_message_p).err_code));
      }
      break;

    case NAS_INITIAL_UE_MESSAGE:{
        nas_establish_ind_t                    *nas_est_ind_p = NULL;

        nas_est_ind_p = &received_message_p->ittiMsg.nas_initial_ue_message.nas;
        enb_s1ap_id_key_t enb_s1ap_id_key = 0;
        MME_APP_ENB_S1AP_ID_KEY(enb_s1ap_id_key,
            received_message_p->ittiMsg.nas_initial_ue_message.transparent.e_utran_cgi.cell_identity.enb_id,
            received_message_p->ittiMsg.nas_initial_ue_message.transparent.enb_ue_s1ap_id);
        nas_obj->nas_proc_establish_ind (enb_s1ap_id_key,
            nas_est_ind_p->ue_id,
            nas_est_ind_p->tai,
            nas_est_ind_p->ecgi,
            nas_est_ind_p->as_cause,
            nas_est_ind_p->s_tmsi,
            &nas_est_ind_p->initial_nas_msg);
      }
      break;

    case NAS_PDN_CONFIG_RSP:{
      nas_obj->nas_proc_pdn_config_res (&NAS_PDN_CONFIG_RSP (received_message_p));
    }
    break;

    case NAS_PDN_CONNECTIVITY_FAIL:{
        nas_obj->nas_proc_pdn_connectivity_fail (&NAS_PDN_CONNECTIVITY_FAIL (received_message_p));
      }
      break;

    case NAS_PDN_CONNECTIVITY_RSP:{
        nas_obj->nas_proc_pdn_connectivity_res (&NAS_PDN_CONNECTIVITY_RSP (received_message_p));
      }
      break;

    case NAS_SIGNALLING_CONNECTION_REL_IND:{
      nas_obj->nas_proc_signalling_connection_rel_ind (NAS_SIGNALLING_CONNECTION_REL_IND (received_message_p).ue_id);
      }
      break;

    case NAS_UPLINK_DATA_IND:{
      XML_MSG_DUMP_ITTI_NAS_UPLINK_DATA_IND(&NAS_UL_DATA_IND (received_message_p), TASK_S1AP, TASK_NAS_MME, NULL);
      nas_obj->nas_proc_ul_transfer_ind (NAS_UL_DATA_IND (received_message_p).ue_id,
            NAS_UL_DATA_IND (received_message_p).tai,
            NAS_UL_DATA_IND (received_message_p).cgi,
            &NAS_UL_DATA_IND (received_message_p).nas_msg);
      }
      break;

    case S1AP_DEREGISTER_UE_REQ:{
        nas_obj->nas_proc_deregister_ue (S1AP_DEREGISTER_UE_REQ (received_message_p).mme_ue_s1ap_id);
      }
      break;

    case S6A_AUTH_INFO_ANS:{
        /*
         * We received the authentication vectors from HSS, trigger a ULR
         * for now. Normaly should trigger an authentication procedure with UE.
         */
        nas_obj->nas_proc_authentication_info_answer (&S6A_AUTH_INFO_ANS(received_message_p));
      }
      break;

    case TERMINATE_MESSAGE:{
        nas_obj->nas_exit();
        OAI_FPRINTF_INFO("TASK_NAS_MME terminated\n");
        itti_free_msg_content(received_message_p);
        itti_free (ITTI_MSG_ORIGIN_ID (received_message_p), received_message_p);
        itti_exit_task ();
      }
      break;

    case TIMER_HAS_EXPIRED:{
        /*
         * Call the NAS timer api
         */
        nas_timer_handle_signal_expiry (TIMER_HAS_EXPIRED (received_message_p).timer_id, static_cast<nas_itti_timer_arg_t*>(TIMER_HAS_EXPIRED (received_message_p).arg));
      }
      break;

    default:{
        OAILOG_DEBUG (LOG_NAS, "Unkwnon message ID %d:%s from %s\n", ITTI_MSG_ID (received_message_p), ITTI_MSG_NAME (received_message_p), ITTI_MSG_ORIGIN_NAME (received_message_p));
      }
      break;
    }

    itti_free_msg_content(received_message_p);
    itti_free (ITTI_MSG_ORIGIN_ID (received_message_p), received_message_p);
    received_message_p = NULL;
  }
  return NULL;
}

//------------------------------------------------------------------------------
int nas_cxx::nas_init (mme_config_t * mme_config_p)
{
  // parse nas configuration file
  nas_config_parse(&nas_config);

  OAILOG_DEBUG (LOG_NAS, "*******************************************\n");
  OAILOG_DEBUG (LOG_NAS, "******Initializing NAS task interface******\n");
  OAILOG_DEBUG (LOG_NAS, "*******************************************\n");
  nas_network_initialize (mme_config_p, this);

  if (itti_create_task (TASK_NAS_MME, &nas_intertask_interface, this) < 0) {
    OAILOG_ERROR (LOG_NAS, "Create task failed");
    OAILOG_DEBUG (LOG_NAS, "Initializing NAS task interface: FAILED\n");
    return -1;
  }

  OAILOG_DEBUG (LOG_NAS, "Initializing NAS task interface: DONE\n");
  return 0;
}

//------------------------------------------------------------------------------
void nas_cxx::nas_exit(void)
{
  OAILOG_DEBUG (LOG_NAS, "Cleaning NAS task interface\n");
  nas_network_cleanup(this);
  OAILOG_DEBUG (LOG_NAS, "Cleaning NAS task interface: DONE\n");
}
