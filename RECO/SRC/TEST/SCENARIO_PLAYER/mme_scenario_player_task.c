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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>

#include <libxml/xmlwriter.h>
#include <libxml/xpath.h>
#include "bstrlib.h"

#include "log.h"
#include "msc.h"
#include "assertions.h"
#include "conversions.h"
#include "3gpp_23.003.h"
#include "3gpp_24.008.h"
#include "3gpp_33.401.h"
#include "3gpp_24.007.h"
#include "3gpp_36.401.h"
#include "3gpp_36.331.h"
#include "3gpp_24.301.h"
#include "security_types.h"
#include "common_types.h"
#include "emm_msg.h"
#include "esm_msg.h"
#include "intertask_interface.h"
#include "timer.h"
#include "dynamic_memory_check.h"
#include "common_defs.h"
#include "mme_scenario_player.h"
#include "xml_msg_tags.h"
#include "xml_msg_load_itti.h"
#include "itti_free_defined_msg.h"
#include "mme_config.h"
#include "mme_scenario_player_task.h"
#include "mme_scenario_player_rx_itti.h"
#include "xml_load.h"
#include "xml_msg_dump_itti.h"

scenario_player_t g_msp_scenarios = {0};

//------------------------------------------------------------------------------
void *mme_scenario_player_event_handler (void *args)
{
  itti_mark_task_ready (TASK_MME_SCENARIO_PLAYER);

  while (1) {
    MessageDef                             *received_message_p = NULL;
    /*
     * Trying to fetch a message from the message queue.
     * * * * If the queue is empty, this function will block till a
     * * * * message is sent to the task.
     */
    itti_receive_msg (TASK_MME_SCENARIO_PLAYER, &received_message_p);
    DevAssert (received_message_p != NULL);

    switch (ITTI_MSG_ID (received_message_p)) {

    case NAS_DOWNLINK_DATA_REQ:
      mme_scenario_player_handle_nas_downlink_data_req (received_message_p->ittiMsgHeader.instance, received_message_p);
      break;

    case MME_APP_CONNECTION_ESTABLISHMENT_CNF:
      mme_scenario_player_handle_mme_app_connection_establishment_cnf (received_message_p->ittiMsgHeader.instance, received_message_p);
      break;

    case S1AP_UE_CONTEXT_RELEASE_COMMAND:
      mme_scenario_player_handle_s1ap_ue_context_release_command (received_message_p->ittiMsgHeader.instance, received_message_p);
      break;

    case S1AP_E_RAB_SETUP_REQ:
      mme_scenario_player_handle_s1ap_e_rab_setup_req (received_message_p->ittiMsgHeader.instance, received_message_p);
      break;


    case TIMER_HAS_EXPIRED:
      msp_handle_timer_expiry (&received_message_p->ittiMsg.timer_has_expired);
      break;

    case ACTIVATE_MESSAGE:
      OAILOG_DEBUG(LOG_MME_SCENARIO_PLAYER, "TASK_MME_SCENARIO_PLAYER asked to start running scenarios\n");
      msp_run_scenario(g_msp_scenarios.head_scenarios);
      break;

    case SP_SCENARIO_TICK:
      //AssertFatal ((scenario_t*)(received_message_p->ittiMsg.scenario_tick.scenario) == g_msp_scenarios.current_scenario,
      //    "Missing message assertion in scenario %s ?", ((scenario_t*)received_message_p->ittiMsg.scenario_tick.scenario)->name->data);
      msp_run_scenario(g_msp_scenarios.current_scenario);
      break;

    case TERMINATE_MESSAGE:
      mme_scenario_player_exit();
      OAI_FPRINTF_INFO("TASK_MME_SCENARIO_PLAYER terminated\n");
      itti_free (ITTI_MSG_ORIGIN_ID (received_message_p), received_message_p);
      itti_exit_task ();
      break;

    case MESSAGE_TEST:
      OAILOG_DEBUG (LOG_MME_SCENARIO_PLAYER, "Received MESSAGE_TEST\n");
      break;

    default:
      OAILOG_ERROR (LOG_MME_SCENARIO_PLAYER, "Unknown message ID %d:%s\n", ITTI_MSG_ID (received_message_p), ITTI_MSG_NAME (received_message_p));
      break;
    }
    itti_free_msg_content(received_message_p);
    itti_free (ITTI_MSG_ORIGIN_ID (received_message_p), received_message_p);
    received_message_p = NULL;
  }

  return NULL;
}

//------------------------------------------------------------------------------
int mme_scenario_player_init (const mme_config_t * mme_config_p)
{
  OAILOG_DEBUG (LOG_MME_SCENARIO_PLAYER, "Initializing\n");
  if (RUN_MODE_SCENARIO_PLAYER == mme_config_p->run_mode) {
    if (mme_config_p->scenario_player_config.scenario_file) {
      g_msp_scenarios.result_fd = fopen ("/tmp/mme_scenario_player.log", "w");
      AssertFatal (g_msp_scenarios.result_fd != NULL, "Could not MME scenario player log file /tmp/mme_scenario_player.log : %s", strerror (errno));

      if (msp_load_scenario (mme_config_p->scenario_player_config.scenario_file, &g_msp_scenarios, NULL) < 0) {
        fclose (g_msp_scenarios.result_fd);
        return RETURNerror;
      }

      if (itti_create_task (TASK_MME_SCENARIO_PLAYER, &mme_scenario_player_event_handler, NULL) < 0) {
        OAILOG_ERROR (LOG_MME_SCENARIO_PLAYER, "Error while creating TASK_MME_SCENARIO_PLAYER task\n");
        fclose (g_msp_scenarios.result_fd);
        return RETURNerror;
      }
    } else {
      OAILOG_ERROR (LOG_MME_SCENARIO_PLAYER, "Scenario player triggered, but no scenario file defined in MME config file\n");
      return RETURNerror;
    }
  }
  OAILOG_DEBUG (LOG_MME_SCENARIO_PLAYER, "Initializing: DONE\n");
  return RETURNok;
}
//------------------------------------------------------------------------------
void mme_scenario_player_exit (void)
{
  // TODO would need mutex on g_msp_scenarios
  msp_free_scenario(g_msp_scenarios.head_scenarios);
  if (NULL != g_msp_scenarios.result_fd) {
    fflush (g_msp_scenarios.result_fd);
    fclose (g_msp_scenarios.result_fd);
  }
  xmlCleanupParser();
}

