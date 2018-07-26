/*
 * Copyright (c) 2015, EURECOM (www.eurecom.fr)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those
 * of the authors and should not be interpreted as representing official policies,
 * either expressed or implied, of the FreeBSD Project.
 */

#ifndef FILE_MME_SCENARIO_PLAYER_RX_ITTI_SEEN
#  define FILE_MME_SCENARIO_PLAYER_RX_ITTI_SEEN

bstring mme_scenario_player_dump_nas_downlink_data_req (const MessageDef * const received_message);
bstring mme_scenario_player_dump_s1ap_ue_context_release_command (const MessageDef * const received_message);
bstring mme_scenario_player_dump_s11_create_bearer_request (const MessageDef * const received_message);
bstring mme_scenario_player_dump_s1ap_e_rab_setup_req (const MessageDef * const received_message);

void mme_scenario_player_handle_nas_downlink_data_req (instance_t instance, const MessageDef * const received_message);
void mme_scenario_player_handle_mme_app_connection_establishment_cnf (instance_t instance, const MessageDef * const received_message);
void mme_scenario_player_handle_s1ap_ue_context_release_command (instance_t instance, const MessageDef * const received_message);
void mme_scenario_player_handle_s1ap_e_rab_setup_req (instance_t instance, const MessageDef * const received_message);

#endif


