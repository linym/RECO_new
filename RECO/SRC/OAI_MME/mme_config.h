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

/*! \file mme_config.h
  \brief
  \author Lionel Gauthier
  \company Eurecom
  \email: lionel.gauthier@eurecom.fr
*/

#ifndef FILE_MME_CONFIG_SEEN
#define FILE_MME_CONFIG_SEEN
#include <arpa/inet.h>

#include "mme_default_values.h"
#include "3gpp_23.003.h"
#include "common_dim.h"
#include "common_types.h"
#include "bstrlib.h"
#include "log.h"

#ifdef __cplusplus
extern "C" {
#endif


#define MAX_GUMMEI                2

#define MME_CONFIG_STRING_MME_CONFIG                     "MME"
#define MME_CONFIG_STRING_REALM                          "REALM"
#define MME_CONFIG_STRING_MAXENB                         "MAXENB"
#define MME_CONFIG_STRING_MAXUE                          "MAXUE"
#define MME_CONFIG_STRING_RELATIVE_CAPACITY              "RELATIVE_CAPACITY"
#define MME_CONFIG_STRING_STATISTIC_TIMER                "MME_STATISTIC_TIMER"

#define MME_CONFIG_STRING_EMERGENCY_ATTACH_SUPPORTED     "EMERGENCY_ATTACH_SUPPORTED"
#define MME_CONFIG_STRING_UNAUTHENTICATED_IMSI_SUPPORTED "UNAUTHENTICATED_IMSI_SUPPORTED"

#define EPS_NETWORK_FEATURE_SUPPORT_IMS_VOICE_OVER_PS_SESSION_IN_S1       "EPS_NETWORK_FEATURE_SUPPORT_IMS_VOICE_OVER_PS_SESSION_IN_S1"
#define EPS_NETWORK_FEATURE_SUPPORT_EMERGENCY_BEARER_SERVICES_IN_S1_MODE  "EPS_NETWORK_FEATURE_SUPPORT_EMERGENCY_BEARER_SERVICES_IN_S1_MODE"
#define EPS_NETWORK_FEATURE_SUPPORT_LOCATION_SERVICES_VIA_EPC             "EPS_NETWORK_FEATURE_SUPPORT_LOCATION_SERVICES_VIA_EPC"
#define EPS_NETWORK_FEATURE_SUPPORT_EXTENDED_SERVICE_REQUEST              "EPS_NETWORK_FEATURE_SUPPORT_EXTENDED_SERVICE_REQUEST"


#define MME_CONFIG_STRING_INTERTASK_INTERFACE_CONFIG     "INTERTASK_INTERFACE"
#define MME_CONFIG_STRING_INTERTASK_INTERFACE_QUEUE_SIZE "ITTI_QUEUE_SIZE"

#define MME_CONFIG_STRING_SCTP_CONFIG                    "SCTP"
#define MME_CONFIG_STRING_SCTP_INSTREAMS                 "SCTP_INSTREAMS"
#define MME_CONFIG_STRING_SCTP_OUTSTREAMS                "SCTP_OUTSTREAMS"

#define MME_CONFIG_STRING_GUMMEI_LIST                    "GUMMEI_LIST"
#define MME_CONFIG_STRING_MME_CODE                       "MME_CODE"
#define MME_CONFIG_STRING_MME_GID                        "MME_GID"
#define MME_CONFIG_STRING_TAI_LIST                       "TAI_LIST"
#define MME_CONFIG_STRING_MCC                            "MCC"
#define MME_CONFIG_STRING_MNC                            "MNC"
#define MME_CONFIG_STRING_TAC                            "TAC"

#define MME_CONFIG_STRING_NETWORK_INTERFACES_CONFIG      "NETWORK_INTERFACES"
#define MME_CONFIG_STRING_INTERFACE_NAME_FOR_S1_MME      "MME_INTERFACE_NAME_FOR_S1_MME"
#define MME_CONFIG_STRING_IPV4_ADDRESS_FOR_S1_MME        "MME_IPV4_ADDRESS_FOR_S1_MME"
#define MME_CONFIG_STRING_INTERFACE_NAME_FOR_S11_MME     "MME_INTERFACE_NAME_FOR_S11_MME"
#define MME_CONFIG_STRING_IPV4_ADDRESS_FOR_S11_MME       "MME_IPV4_ADDRESS_FOR_S11_MME"
#define MME_CONFIG_STRING_MME_PORT_FOR_S11               "MME_PORT_FOR_S11_MME"


#define MME_CONFIG_STRING_ASN1_VERBOSITY                 "ASN1_VERBOSITY"
#define MME_CONFIG_STRING_ASN1_VERBOSITY_NONE            "none"
#define MME_CONFIG_STRING_ASN1_VERBOSITY_ANNOYING        "annoying"
#define MME_CONFIG_STRING_ASN1_VERBOSITY_INFO            "info"

#define MME_CONFIG_STRING_SGW_LIST_SELECTION             "S-GW_LIST_SELECTION"
#define MME_CONFIG_STRING_ID                             "ID"

#define MME_CONFIG_STRING_MOD_LIST                        "MOD_LIST"
#define MME_CONFIG_STRING_MODNAME                         "MODNAME"

typedef enum {
   RUN_MODE_BASIC,
   RUN_MODE_SCENARIO_PLAYER
} run_mode_t;

typedef struct mme_config_s {
  /* Reader/writer lock for this configuration */
  pthread_rwlock_t rw_lock;


  bstring config_file;
  bstring realm;

  run_mode_t  run_mode;

  uint32_t max_enbs;
  uint32_t max_ues;

  uint8_t relative_capacity;

  uint32_t mme_statistic_timer;

  uint8_t unauthenticated_imsi_supported;

  struct {
    uint8_t ims_voice_over_ps_session_in_s1;
    uint8_t emergency_bearer_services_in_s1_mode;
    uint8_t location_services_via_epc;
    uint8_t extended_service_request;
  } eps_network_feature_support;

  struct {
    int      nb;
    gummei_t gummei[MAX_GUMMEI];
  } gummei;

#define TRACKING_AREA_IDENTITY_LIST_TYPE_ONE_PLMN_NON_CONSECUTIVE_TACS 0x00
#define TRACKING_AREA_IDENTITY_LIST_TYPE_ONE_PLMN_CONSECUTIVE_TACS     0x01
#define TRACKING_AREA_IDENTITY_LIST_TYPE_MANY_PLMNS                    0x02
  struct {
    uint8_t   list_type;
    uint8_t   nb_tai;
    uint16_t *plmn_mcc;
    uint16_t *plmn_mnc;
    uint16_t *plmn_mnc_len;
    uint16_t *tac;
  } served_tai;

  struct {
    uint16_t in_streams;
    uint16_t out_streams;
  } sctp_config;

  struct {
    bstring    if_name_s1_mme;
    struct in_addr s1_mme;
    int        netmask_s1_mme;

    bstring    if_name_s11;
    struct in_addr s11;
    int        netmask_s11;
    uint16_t   port_s11;
  } ipv4;

  struct {
    uint32_t  queue_size;
    bstring   log_file;
  } itti_config;

  struct {
    int            nb_sgw_entries;
#define MME_CONFIG_MAX_SGW 16
    bstring        sgw_id[MME_CONFIG_MAX_SGW];
    struct in_addr sgw_ip_addr[MME_CONFIG_MAX_SGW];
  } e_dns_emulation;

#if TRACE_XML
  struct {
    bstring scenario_file;
    bool    stop_on_error;
  } scenario_player_config;
#endif

  log_config_t log_config;
} mme_config_t;

extern mme_config_t mme_config;

int mme_config_find_mnc_length(const char mcc_digit1P,
                               const char mcc_digit2P,
                               const char mcc_digit3P,
                               const char mnc_digit1P,
                               const char mnc_digit2P,
                               const char mnc_digit3P);

int mme_config_parse_opt_line(int argc, char *argv[], mme_config_t *mme_config);


void mme_config_exit (void);



#define mme_config_read_lock(mMEcONFIG)  pthread_rwlock_rdlock(&(mMEcONFIG)->rw_lock)
#define mme_config_write_lock(mMEcONFIG) pthread_rwlock_wrlock(&(mMEcONFIG)->rw_lock)
#define mme_config_unlock(mMEcONFIG)     pthread_rwlock_unlock(&(mMEcONFIG)->rw_lock)


#ifdef __cplusplus
}
#endif

#endif /* FILE_MME_CONFIG_SEEN */
