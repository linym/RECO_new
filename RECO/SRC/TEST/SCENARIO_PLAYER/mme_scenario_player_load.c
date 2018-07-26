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
#include <pthread.h>

#include <libxml/xmlwriter.h>
#include <libxml/xpath.h>
#include "bstrlib.h"

#include "xml_load.h"
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
#include "securityDef.h"
#include "common_types.h"
#include "mme_api.h"
#include "emm_data.h"
#include "emm_msg.h"
#include "esm_msg.h"
#include "intertask_interface.h"
#include "timer.h"
#include "dynamic_memory_check.h"
#include "mme_scenario_player.h"
#include "common_defs.h"
#include "secu_defs.h"
#include "xml_msg_tags.h"
#include "xml_msg_load_itti.h"
#include "itti_free_defined_msg.h"
#include "sp_xml_load.h"

extern scenario_player_t g_msp_scenarios;

//------------------------------------------------------------------------------
unsigned long msp_get_seq_uid(void)
{
  // TODO not really thread-safe, but should not interfere with calling threads
  static unsigned long uid = 0;
  __sync_fetch_and_add (&uid, 1);
  return uid;
}

//------------------------------------------------------------------------------
xmlChar * msp_get_attr_file (xmlDocPtr doc, xmlNodePtr cur)
{
  xmlChar *file_path = NULL;

  cur = cur->xmlChildrenNode;
  while (cur != NULL) {
    if ((!xmlStrcmp(cur->name, (const xmlChar *)FILE_ATTR_XML_STR))) {
      file_path = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
      return file_path;
      // reminder xmlFree(file_path);
    }
    cur = cur->next;
  }
  OAILOG_ERROR (LOG_MME_SCENARIO_PLAYER, "Could not get attr %s from node %s\n", FILE_ATTR_XML_STR, cur->name);
  return NULL;
}
//------------------------------------------------------------------------------
usim_data_t * msp_get_usim_data(scenario_t * const scenario)
{
  scenario_t * scenario_p = scenario;
  while (scenario_p) {
    if (scenario_p->usim_data) {
        return scenario_p->usim_data;
    }
    scenario_p = scenario_p->parent;
  }
  return NULL;
}
//------------------------------------------------------------------------------
emm_security_context_t * msp_get_ue_emulated_emm_security_context(scenario_t * const scenario)
{
  scenario_t * scenario_p = scenario;
  while (scenario_p) {
    if (scenario_p->ue_emulated_emm_security_context) {
        return scenario_p->ue_emulated_emm_security_context;
    }
    scenario_p = scenario_p->parent;
  }
  return NULL;
}
//------------------------------------------------------------------------------
scenario_player_item_t* msp_load_message_file (scenario_t * const scenario, xmlDocPtr const xml_doc, xmlXPathContextPtr  xpath_ctx, xmlNodePtr node, bstring scenario_file_path)
{
  OAILOG_FUNC_IN (LOG_MME_SCENARIO_PLAYER);
  scenario_player_item_t * spi = calloc(1, sizeof(*spi));
  spi->item_type = SCENARIO_PLAYER_ITEM_ITTI_MSG;
  // get a UID
  spi->uid = msp_get_seq_uid();


  // ACTION attribute
  xmlChar *attr = xmlGetProp(node, (const xmlChar *)ACTION_ATTR_XML_STR);
  if (attr) {
    OAILOG_TRACE (LOG_MME_SCENARIO_PLAYER, "Found %s=%s\n", ACTION_ATTR_XML_STR, attr);
    if (!strcasecmp((const char*)attr, SEND_VAL_XML_STR)) {
      spi->u.msg.is_tx = true;
    } else if (!strcasecmp((const char*)attr, RECEIVE_VAL_XML_STR)) {
      spi->u.msg.is_tx = false;
    } else {
      AssertFatal(0, "Syntax Error %s=%s  choice in {%s, %s}", ACTION_ATTR_XML_STR, attr, SEND_VAL_XML_STR, RECEIVE_VAL_XML_STR);
    }
    xmlFree(attr);
  } else {
    AssertFatal(0, "Syntax Error %s={0}  choice in {%s, %s} for file %s", ACTION_ATTR_XML_STR, SEND_VAL_XML_STR, RECEIVE_VAL_XML_STR, bdata(scenario_file_path));
  }



  // TIME REF attribute
  attr = xmlGetProp(node, (const xmlChar *)TIME_REF_ATTR_XML_STR);
  if (attr) {
    if (!strcasecmp((const char *)attr, THIS_VAL_XML_STR)) {
      spi->u.msg.time_out_relative_to_msg_uid  = spi->uid;
    } else {
      int       val = 0;
      int ret = sscanf((const char*)attr, "%d", &val);
      AssertFatal(1 == ret, "Could not read %s=%s", TIME_REF_ATTR_XML_STR, attr);
      AssertFatal(0 >= val, "Bad positive value %s=%d", TIME_REF_ATTR_XML_STR, val);
      int counter = -1;
      scenario_player_item_t * cur = scenario->tail_item;

      while ((counter > val) && (cur)){
        cur = cur->previous_item;
        counter -= 1;
      }
      AssertFatal((cur), "Out of range %s=%d", TIME_REF_ATTR_XML_STR, val);
      spi->u.msg.time_out_relative_to_msg_uid = cur->uid;
    }
    OAILOG_TRACE (LOG_MME_SCENARIO_PLAYER, "Found %s=%s uid=%u\n", TIME_REF_ATTR_XML_STR, attr, spi->u.msg.time_out_relative_to_msg_uid);
    xmlFree(attr);
  }

  // TIME attribute
  attr = xmlGetProp(node, (const xmlChar *)TIME_ATTR_XML_STR);
  if (attr) {
    if (!strcasecmp((const char *)attr, NOW_VAL_XML_STR)) {
      spi->u.msg.time_out.tv_sec  = 0;
      spi->u.msg.time_out.tv_usec = 0;
    } else {
      bstring b = bfromcstr((char *)attr);
      struct bstrList * blist = bsplit (b, '.');
      if (0 < blist->qty) {
        uint32_t  val = 0;
        int ret = sscanf((const char *)blist->entry[0]->data, "%"SCNu32, &val);
        AssertFatal (1 == ret, "Could not convert time seconds: %s (%s)", blist->entry[0]->data, attr);
        spi->u.msg.time_out.tv_sec  = val;
        spi->u.msg.time_out.tv_usec  = 0;
        if (1 < blist->qty) {
          uint32_t  val = 0;
          while (6 > blength(blist->entry[1])) {
            bconchar (blist->entry[1], '0');
          }
          int ret = sscanf((const char *)blist->entry[1]->data, "%"SCNu32, &val);
          AssertFatal (1 == ret, "Could not convert time useconds: %s (%s)", blist->entry[1]->data, attr);
          spi->u.msg.time_out.tv_usec  = val;
        }
      } else {
        uint32_t  val = 0;
        int ret = sscanf((const char *)attr, "%"SCNu32, &val);
        AssertFatal (1 == ret, "Could not convert time seconds: %s", attr);
        spi->u.msg.time_out.tv_sec  = val;
        spi->u.msg.time_out.tv_usec  = 0;
      }
      bstrListDestroy(blist);
      bdestroy_wrapper(&b);
    }
    OAILOG_TRACE (LOG_MME_SCENARIO_PLAYER, "Found %s=%ld.%ld sec\n", TIME_ATTR_XML_STR, spi->u.msg.time_out.tv_sec, spi->u.msg.time_out.tv_usec);
    xmlFree(attr);
  } else {
    // by default immediate sent or infinite delay for receiving
    if (spi->u.msg.is_tx) {
      spi->u.msg.time_out.tv_sec  = 0;
      spi->u.msg.time_out.tv_usec = 0;
    } else {
      spi->u.msg.time_out.tv_sec  = 1000;
      spi->u.msg.time_out.tv_usec = 0;
    }
  }

  bstring xpath_expr = bformat("./%s",FILE_ATTR_XML_STR);
  char filename[PATH_MAX];
  bool res = xml_load_leaf_tag(xml_doc, xpath_ctx, xpath_expr, "%s", (void*)filename, NULL);
  if (res) {
    // remove file name from file_path
    bstring bstr = NULL;
    if ('/' == filename[0]) {
      // Absolute path
      bstr = bfromcstr ((const char *)filename);
      int ret = msp_load_message(scenario, bstr, spi);
      bdestroy_wrapper (&bstr);
      if (0 > ret) {
        msp_free_message_content(&spi->u.msg);
        free(spi);
        OAILOG_FUNC_RETURN (LOG_MME_SCENARIO_PLAYER, NULL);
      }
    } else {
      // relative path
      int pos = bstrrchrp (scenario_file_path, '/', blength(scenario_file_path) - 1);
      if (BSTR_ERR != pos) {
        bstr = bstrcpy(scenario_file_path);
        btrunc(bstr, pos + 1); // keep '/'
        bcatblk(bstr, filename, strlen(filename));
        int ret = msp_load_message(scenario, bstr, spi);
        bdestroy_wrapper (&bstr);
        if (0 > ret) {
          msp_free_message_content(&spi->u.msg);
          free(spi);
          OAILOG_FUNC_RETURN (LOG_MME_SCENARIO_PLAYER, NULL);
        }
      } else {
        OAILOG_ERROR (LOG_MME_SCENARIO_PLAYER, "Could not find char / in scenarios path %s\n", scenario_file_path->data);
        free(spi);
        OAILOG_FUNC_RETURN (LOG_MME_SCENARIO_PLAYER, NULL);
      }
    }
  }
  message_log_status(scenario, filename, spi->uid, SCENARIO_XML_ITEM_MSG_FILE_LOAD_SUCCEED, NULL);
  bdestroy_wrapper (&xpath_expr);
  OAILOG_FUNC_RETURN (LOG_MME_SCENARIO_PLAYER, spi);
}

//------------------------------------------------------------------------------
scenario_player_item_t* msp_load_var (scenario_t * const scenario, xmlDocPtr const xml_doc, xmlXPathContextPtr  xpath_ctx, xmlNodePtr node)
{
  bstring var_name = NULL;

  xmlChar *attr = xmlGetProp(node, (const xmlChar *)NAME_ATTR_XML_STR);
  if (attr) {
    OAILOG_TRACE (LOG_MME_SCENARIO_PLAYER, "Found var %s=%s\n", NAME_ATTR_XML_STR, attr);
    var_name  = bfromcstr((const char *)attr);
    xmlFree(attr);
  } else {
    AssertFatal(0, "Could not find var name");
  }

  scenario_player_item_t * spi = NULL;

  // Check if var already exists
  void                                   *uid_ptr = NULL;
  hashtable_rc_t rc = obj_hashtable_ts_get (scenario->var_items, bdata(var_name), blength(var_name), (void**)&uid_ptr);
  AssertFatal(HASH_TABLE_OK != rc, "var %s already declared", bdata(var_name));

  // create it
  spi = calloc(1, sizeof(*spi));
  spi->item_type = SCENARIO_PLAYER_ITEM_VAR;
  // get a UID
  spi->uid = msp_get_seq_uid();
  spi->u.var.name  = var_name;
  var_name = NULL;
  spi->u.var.value.value_u64 = 0;
  spi->u.var.var_ref_uid = 0;



  attr = xmlGetProp(node, (const xmlChar *)VALUE_ATTR_XML_STR);
  if (attr) {
    OAILOG_TRACE (LOG_MME_SCENARIO_PLAYER, "Found int var %s=%s\n", bdata(spi->u.var.name), attr);
    char * value = (char*)attr;
    int ret = sscanf((const char*)attr, "%"SCNx64, &spi->u.var.value.value_u64);
    if ( 1 == ret) {
      spi->u.var.value_type = VAR_VALUE_TYPE_INT64;
    } else if (value[0] == '-') {
      int ret = sscanf((const char*)attr, "%"SCNi64, &spi->u.var.value.value_64);
      AssertFatal( ret == 1, "Failed to load var %s  signed value %s", bdata(spi->u.var.name), attr);
      spi->u.var.value_type = VAR_VALUE_TYPE_INT64;
    } else if (value[0] == '$') {
      void                                   *uid_ref = NULL;
      hashtable_rc_t rc_ref = obj_hashtable_ts_get (scenario->var_items, &value[1], strlen(value) - 1, (void**)&uid_ref);
      AssertFatal (HASH_TABLE_OK == rc_ref, "Could not find ref var %s", &value[1]);
      spi->u.var.var_ref_uid = (int)(uintptr_t)uid_ref;
      // check if uid_ref exist
      rc_ref = hashtable_ts_is_key_exists (scenario->scenario_items, (hash_key_t)spi->u.var.var_ref_uid);
      AssertFatal(HASH_TABLE_OK == rc_ref, "Error in getting uid ref %u", spi->u.var.var_ref_uid);
    } else {
      int ret = sscanf((const char*)attr, "%"SCNu64, &spi->u.var.value.value_u64);
      AssertFatal( ret == 1, "Failed to load var %s  unsigned value %s", bdata(spi->u.var.name), attr);
      spi->u.var.value_type = VAR_VALUE_TYPE_INT64;
    }
    xmlFree(attr);
  } else if ((attr = xmlGetProp(node, (const xmlChar *)ASCII_STREAM_VALUE_ATTR_XML_STR))) {
    char * value = (char*)attr;
    spi->u.var.value.value_bstr = bfromcstr((const char *)value);
    spi->u.var.value_type = VAR_VALUE_TYPE_ASCII_STREAM;
    xmlFree(attr);
  } else if ((attr = xmlGetProp(node, (const xmlChar *)HEX_STREAM_VALUE_ATTR_XML_STR))) {
    char * value = (char*)attr;
    int len = strlen((const char*)value);
    uint8_t hex[len/2];
    int ret = ascii_to_hex ((uint8_t *) hex, (const char *)value);
    AssertFatal(ret, "Could not convert hex stream value");
    spi->u.var.value.value_bstr = blk2bstr(hex,len/2);
    spi->u.var.value_type = VAR_VALUE_TYPE_HEX_STREAM;
    xmlFree(attr);
  } else {
    AssertFatal(0, "Could not find var value type");
  }

  rc = obj_hashtable_ts_insert (scenario->var_items, bdata(spi->u.var.name), blength(spi->u.var.name), (void*)(uintptr_t)spi->uid);
  AssertFatal(HASH_TABLE_OK == rc, "Error in putting var name in hashtable %d", rc);
  msp_display_var(spi);
  return spi;
}

//------------------------------------------------------------------------------
scenario_player_item_t* msp_load_set_var (scenario_t * const scenario, xmlDocPtr const xml_doc, xmlXPathContextPtr  xpath_ctx, xmlNodePtr node)
{
  bstring var_name = NULL;

  xmlChar *attr = xmlGetProp(node, (const xmlChar *)NAME_ATTR_XML_STR);
  if (attr) {
    OAILOG_TRACE (LOG_MME_SCENARIO_PLAYER, "Found set var %s=%s\n", NAME_ATTR_XML_STR, attr);
    var_name  = bfromcstr((const char *)attr);
    xmlFree(attr);
  } else {
    AssertFatal(0, "Could not find var name");
  }

  scenario_player_item_t * spi = NULL;

  // Check if var already exists
  scenario_player_item_t * spi_var = sp_get_var(scenario, (unsigned char *)bdata(var_name));
  AssertFatal(spi_var, "var %s not found", bdata(var_name));

  // create it
  spi = calloc(1, sizeof(*spi));
  spi->item_type = SCENARIO_PLAYER_ITEM_VAR_SET;
  // get a UID
  spi->uid = msp_get_seq_uid();
  spi->u.set_var.name  = var_name;
  var_name = NULL;
  spi->u.set_var.value.value_u64 = 0;
  spi->u.set_var.var_uid         = spi_var->uid;
  spi->u.set_var.var_ref_uid     = 0;


  attr = xmlGetProp(node, (const xmlChar *)VALUE_ATTR_XML_STR);
  if (attr) {
    OAILOG_TRACE (LOG_MME_SCENARIO_PLAYER, "Found var %s=%s\n", bdata(spi->u.set_var.name), attr);
    char * value = (char*)attr;
    if (!strncasecmp("0x", value, 2)) {
      int ret = sscanf((const char*)(attr+2), "%"SCNx64, &spi->u.set_var.value.value_u64);
      AssertFatal( ret == 1, "Failed to load var %s  hex value %s", bdata(spi->u.set_var.name), attr);
      spi->u.set_var.value_type = VAR_VALUE_TYPE_INT64;
    } else if (value[0] == '-') {
      int ret = sscanf((const char*)attr, "%"SCNi64, &spi->u.set_var.value.value_64);
      AssertFatal( ret == 1, "Failed to load var %s  signed value %s", bdata(spi->u.set_var.name), attr);
      spi->u.set_var.value_type = VAR_VALUE_TYPE_INT64;
    } else if (value[0] == '$') {
      scenario_player_item_t * ref_var = sp_get_var(scenario, (unsigned char *)&value[1]);
      AssertFatal(ref_var, "var %s not found", &value[1]);
      spi->u.set_var.var_ref_uid = ref_var->uid;
    } else {
      int ret = sscanf((const char*)attr, "%"SCNu64, &spi->u.set_var.value.value_u64);
      AssertFatal( ret == 1, "Failed to load var %s  unsigned value %s", bdata(spi->u.set_var.name), attr);
      spi->u.set_var.value_type = VAR_VALUE_TYPE_INT64;
    }
    xmlFree(attr);
  } else if ((attr = xmlGetProp(node, (const xmlChar *)ASCII_STREAM_VALUE_ATTR_XML_STR))) {
    OAILOG_TRACE (LOG_MME_SCENARIO_PLAYER, "Found ascii var %s=%s\n", bdata(spi->u.set_var.name), attr);
    char * value = (char*)attr;
    spi->u.set_var.value.value_bstr = bfromcstr((const char *)value);
    spi->u.set_var.value_type = VAR_VALUE_TYPE_ASCII_STREAM;
    xmlFree(attr);
  } else if ((attr = xmlGetProp(node, (const xmlChar *)HEX_STREAM_VALUE_ATTR_XML_STR))) {
    OAILOG_TRACE (LOG_MME_SCENARIO_PLAYER, "Found hex stream var %s=%s\n", bdata(spi->u.set_var.name), attr);
    char * value = (char*)attr;
    int len = strlen((const char*)value);
    uint8_t hex[len/2];
    int ret = ascii_to_hex ((uint8_t *) hex, (const char *)value);
    AssertFatal(ret, "Could not convert hex stream value");
    spi->u.set_var.value.value_bstr = blk2bstr(hex,len/2);
    spi->u.set_var.value_type = VAR_VALUE_TYPE_HEX_STREAM;
    xmlFree(attr);
  } else {
    AssertFatal(0, "Could not find var value type");
  }
  return spi;
}

//------------------------------------------------------------------------------
scenario_player_item_t* msp_load_incr_var (scenario_t * const scenario, xmlDocPtr const xml_doc, xmlXPathContextPtr  xpath_ctx, xmlNodePtr node)
{
  scenario_player_item_t * spi = calloc(1, sizeof(*spi));
  spi->item_type = SCENARIO_PLAYER_ITEM_VAR_INCR;
  // get a UID
  spi->uid = msp_get_seq_uid();


  scenario_player_item_t * spi_var = NULL;
  xmlChar *attr = xmlGetProp(node, (const xmlChar *)NAME_ATTR_XML_STR);
  if (attr) {
    OAILOG_TRACE (LOG_MME_SCENARIO_PLAYER, "Found incr var %s=%s uid=%u\n", NAME_ATTR_XML_STR, attr, spi->uid);
    // Check if var already exists
    spi_var = sp_get_var(scenario, (unsigned char *)attr);
    AssertFatal(spi_var, "var %s not found", attr);
    xmlFree(attr);
  } else {
    AssertFatal(0, "Could not find var name");
  }
  spi->u.uid_incr_var = spi_var->uid;
  return spi;
}
//------------------------------------------------------------------------------
scenario_player_item_t* msp_load_decr_var (scenario_t * const scenario, xmlDocPtr const xml_doc, xmlXPathContextPtr  xpath_ctx, xmlNodePtr node)
{
  scenario_player_item_t * spi = calloc(1, sizeof(*spi));
  spi->item_type = SCENARIO_PLAYER_ITEM_VAR_DECR;
  // get a UID
  spi->uid = msp_get_seq_uid();

  scenario_player_item_t * spi_var = NULL;
  xmlChar *attr = xmlGetProp(node, (const xmlChar *)NAME_ATTR_XML_STR);
  if (attr) {
    OAILOG_TRACE (LOG_MME_SCENARIO_PLAYER, "Found decr var %s=%s uid=%u\n", NAME_ATTR_XML_STR, attr, spi->uid);
    // Check if var already exists
    spi_var = sp_get_var(scenario, (unsigned char *)attr);
    AssertFatal(spi_var, "var %s not found", attr);
    xmlFree(attr);
  } else {
    AssertFatal(0, "Could not find var name");
  }
  spi->u.uid_decr_var = spi_var->uid;
  return spi;
}
//------------------------------------------------------------------------------
scenario_player_item_t* msp_load_sleep (scenario_t * const scenario, xmlDocPtr const xml_doc, xmlXPathContextPtr  xpath_ctx, xmlNodePtr node)
{
  scenario_player_item_t * spi = calloc(1, sizeof(*spi));
  spi->item_type = SCENARIO_PLAYER_ITEM_SLEEP;
  // get a UID
  spi->uid = msp_get_seq_uid();

  xmlChar *attr = xmlGetProp(node, (const xmlChar *)SECONDS_ATTR_XML_STR);
  if (attr) {
    OAILOG_TRACE (LOG_MME_SCENARIO_PLAYER, "Found sleep %s seconds\n", attr);
    int ret = sscanf((const char*)attr, "%u", &spi->u.sleep.seconds);
    AssertFatal( ret == 1, "Failed to load sleep value %s", attr);
    xmlFree(attr);
  } else {
    AssertFatal(0, "Could not find seconds");
  }
  attr = xmlGetProp(node, (const xmlChar *)USECONDS_ATTR_XML_STR);
  if (attr) {
    OAILOG_TRACE (LOG_MME_SCENARIO_PLAYER, "Found sleep %s useconds\n",attr);
    int ret = sscanf((const char*)attr, "%u", &spi->u.sleep.useconds);
    AssertFatal( ret == 1, "Failed to load sleep value %s", attr);
    xmlFree(attr);
  } else {
    AssertFatal(0, "Could not find useconds");
  }
  return spi;
}
//------------------------------------------------------------------------------
int msp_load_usim_data (scenario_t * const scenario, xmlDocPtr const xml_doc, xmlXPathContextPtr  xpath_ctx, xmlNodePtr node)
{

  if (scenario->usim_data) {
    memset((void*)scenario->usim_data, 0, sizeof(usim_data_t));
  } else {
    scenario->usim_data = calloc(1, sizeof(usim_data_t));
  }
  xmlChar *attr = xmlGetProp(node, (const xmlChar *)LTE_K_ATTR_XML_STR);
  if (attr) {
    OAILOG_TRACE (LOG_MME_SCENARIO_PLAYER, "Found %s=%s\n", LTE_K_ATTR_XML_STR, attr);
    char * value = (char*)attr;
    int len = strlen((const char*)value);
    uint8_t hex[len/2];
    int ret = ascii_to_hex ((uint8_t *) hex, (const char *)value);
    AssertFatal(ret, "Could not convert hex stream value");
    AssertFatal(len == 2*USIM_LTE_K_SIZE, "Could not convert hex stream value");
    memcpy(scenario->usim_data->lte_k, hex, USIM_LTE_K_SIZE);
    xmlFree(attr);
  } else {
    AssertFatal(0, "Could not find %s", LTE_K_ATTR_XML_STR);
  }

  attr = xmlGetProp(node, (const xmlChar *)SQN_MS_ATTR_XML_STR);
  if (attr) {
    OAILOG_TRACE (LOG_MME_SCENARIO_PLAYER, "Found %s=%s\n", SQN_MS_ATTR_XML_STR, attr);
    uint64_t sqn_ms = 0;
    int ret = sscanf((const char*)attr, "%"SCNx64, &sqn_ms);
    AssertFatal( ret == 1, "Failed to load %s value %s", SQN_MS_ATTR_XML_STR, attr);
    AssertFatal(0x0000FFFFFFFFFFFF > sqn_ms, "SQN Bad value %"PRIx64" ", sqn_ms);
    scenario->usim_data->sqn_ms[0] = (sqn_ms >> 5) & 0x00000000000000FF;
    scenario->usim_data->sqn_ms[1] = (sqn_ms >> 4) & 0x00000000000000FF;
    scenario->usim_data->sqn_ms[2] = (sqn_ms >> 3) & 0x00000000000000FF;
    scenario->usim_data->sqn_ms[3] = (sqn_ms >> 2) & 0x00000000000000FF;
    scenario->usim_data->sqn_ms[4] = (sqn_ms >> 1) & 0x00000000000000FF;
    scenario->usim_data->sqn_ms[5] =  sqn_ms       & 0x00000000000000FF;
    xmlFree(attr);
  } else {
    AssertFatal(0, "Could not find %s", SQN_MS_ATTR_XML_STR);
  }

  // ue_emulated_emm_security_context allocated in the same scenario as usim_data
  scenario->ue_emulated_emm_security_context = calloc(1, sizeof(emm_security_context_t));
  scenario->ue_emulated_emm_security_context->direction_encode = SECU_DIRECTION_UPLINK;
  scenario->ue_emulated_emm_security_context->direction_decode = SECU_DIRECTION_DOWNLINK;


  return RETURNok;
}

//------------------------------------------------------------------------------
scenario_player_item_t *msp_load_update_emm_security_context (scenario_t * const scenario, xmlDocPtr const xml_doc, xmlXPathContextPtr  xpath_ctx, xmlNodePtr node)
{
  scenario_player_item_t * spi = calloc(1, sizeof(*spi));
  spi->item_type = SCENARIO_PLAYER_ITEM_UPDATE_EMM_SECURITY_CONTEXT;
  // get a UID
  spi->uid = msp_get_seq_uid();

  xmlChar *attr = xmlGetProp(node, (const xmlChar *)SELECTED_EEA_ATTR_XML_STR);
  if (attr) {
    OAILOG_TRACE (LOG_MME_SCENARIO_PLAYER, "Found %s=%s\n", SELECTED_EEA_ATTR_XML_STR, attr);
    spi->u.updata_emm_sc.is_seea_present = true;
    int ret = sscanf((const char*)attr, "%"SCNx8, &spi->u.updata_emm_sc.seea.value_u8);
    if (ret == 1) {
      spi->u.updata_emm_sc.is_seea_a_uid = false;
    } else if ('$' == attr[0]) {
      spi->u.updata_emm_sc.is_seea_a_uid = true;
      void           *uid = NULL;
      hashtable_rc_t rc = obj_hashtable_ts_get (scenario->var_items, (void*)&attr[1], xmlStrlen(attr)-1, (void**)&uid);
      if (HASH_TABLE_OK == rc) {
        spi->u.updata_emm_sc.seea.uid = (uintptr_t)uid;
      } else {
        AssertFatal (0, "Could not find %s var, should have been declared in scenario\n", &attr[1]);
      }
    } else {
      AssertFatal(0, "Unable to handle %s", attr);
    }
    xmlFree(attr);
  }

  attr = xmlGetProp(node, (const xmlChar *)SELECTED_EIA_ATTR_XML_STR);
  if (attr) {
    OAILOG_TRACE (LOG_MME_SCENARIO_PLAYER, "Found %s=%s\n", SELECTED_EIA_ATTR_XML_STR, attr);
    spi->u.updata_emm_sc.is_seia_present = true;
    int ret = sscanf((const char*)attr, "%"SCNx8, &spi->u.updata_emm_sc.seia.value_u8);
    if (ret == 1) {
      spi->u.updata_emm_sc.is_seia_a_uid = false;
    } else if ('$' == attr[0]) {
      spi->u.updata_emm_sc.is_seia_a_uid = true;
      void           *uid = NULL;
      hashtable_rc_t rc = obj_hashtable_ts_get (scenario->var_items, (void*)&attr[1], xmlStrlen(attr)-1, (void**)&uid);
      if (HASH_TABLE_OK == rc) {
        spi->u.updata_emm_sc.seia.uid = (uintptr_t)uid;
      } else {
        AssertFatal (0, "Could not find %s var, should have been declared in scenario\n", &attr[1]);
      }
    } else {
      AssertFatal(0, "Unable to handle %s", attr);
    }
    xmlFree(attr);
  }

  attr = xmlGetProp(node, (const xmlChar *)UL_COUNT_ATTR_XML_STR);
  if (attr) {
    OAILOG_TRACE (LOG_MME_SCENARIO_PLAYER, "Found %s=%s\n", UL_COUNT_ATTR_XML_STR, attr);
    spi->u.updata_emm_sc.is_ul_count_present = true;
    int ret = sscanf((const char*)attr, "%"SCNx32, &spi->u.updata_emm_sc.ul_count.value_u32);
    if (ret == 1) {
      spi->u.updata_emm_sc.is_ul_count_a_uid = false;
    } else if ('$' == attr[0]) {
      spi->u.updata_emm_sc.is_ul_count_a_uid = true;
      // Check if var already exists
      scenario_player_item_t * spi_var = sp_get_var(scenario, (unsigned char *)&attr[1]);
      AssertFatal(spi_var, "var %s not found", &attr[1]);
      spi->u.updata_emm_sc.ul_count.uid = spi_var->uid;
    } else {
      AssertFatal(0, "Unable to handle %s", attr);
    }
    xmlFree(attr);
  }
  return spi;
}

//------------------------------------------------------------------------------
scenario_player_item_t* msp_load_compute_authentication_response_parameter (scenario_t * const scenario, xmlDocPtr const xml_doc, xmlXPathContextPtr  xpath_ctx, xmlNodePtr node)
{
  scenario_player_item_t * spi = calloc(1, sizeof(*spi));
  spi->item_type = SCENARIO_PLAYER_ITEM_COMPUTE_AUTHENTICATION_RESPONSE_PARAMETER;
  // get a UID
  spi->uid = msp_get_seq_uid();
  return spi;
}

//------------------------------------------------------------------------------
scenario_player_item_t* msp_load_compute_authentication_sync_failure_parameter (scenario_t * const scenario, xmlDocPtr const xml_doc, xmlXPathContextPtr  xpath_ctx, xmlNodePtr node)
{
  scenario_player_item_t * spi = calloc(1, sizeof(*spi));
  spi->item_type = SCENARIO_PLAYER_ITEM_COMPUTE_AUTHENTICATION_SYNC_FAILURE_PARAMETER;
  // get a UID
  spi->uid = msp_get_seq_uid();
  return spi;
}

//------------------------------------------------------------------------------
scenario_player_item_t* msp_load_label (scenario_t * const scenario, xmlDocPtr const xml_doc, xmlXPathContextPtr  xpath_ctx, xmlNodePtr node)
{
  scenario_player_item_t * spi = calloc(1, sizeof(*spi));
  spi->item_type = SCENARIO_PLAYER_ITEM_LABEL;
  // get a UID
  spi->uid = msp_get_seq_uid();

  xmlChar *attr = xmlGetProp(node, (const xmlChar *)NAME_ATTR_XML_STR);
  if (attr) {
    OAILOG_TRACE (LOG_MME_SCENARIO_PLAYER, "Found label %s=%s uid=%u\n", NAME_ATTR_XML_STR, attr, spi->uid);
    spi->u.label = bfromcstr((const char *)attr);
    xmlFree(attr);
  } else {
    AssertFatal(0, "Could not find %s", NAME_ATTR_XML_STR);
  }
  hashtable_rc_t rc = obj_hashtable_ts_insert (scenario->label_items, bdata(spi->u.label), blength(spi->u.label), (void*)(uintptr_t)spi->uid);
  AssertFatal(HASH_TABLE_OK == rc, "Error in putting label %s in hashtable %d", bdata(spi->u.label), rc);
  return spi;
}
//------------------------------------------------------------------------------
scenario_player_item_t* msp_load_exit (scenario_t * const scenario, xmlDocPtr const xml_doc, xmlXPathContextPtr  xpath_ctx, xmlNodePtr node)
{
  scenario_player_item_t * spi = calloc(1, sizeof(*spi));
  spi->item_type = SCENARIO_PLAYER_ITEM_EXIT;
  // get a UID
  spi->uid = msp_get_seq_uid();
  spi->u.exit = SCENARIO_STATUS_NULL;

  xmlChar *attr = xmlGetProp(node, (const xmlChar *)STATUS_ATTR_XML_STR);
  if (attr) {
    OAILOG_TRACE (LOG_MME_SCENARIO_PLAYER, "Found exit status %s=%s UID=%u\n", STATUS_ATTR_XML_STR, attr, spi->uid);
    if (!strcasecmp((const char *)attr, FAILED_VAL_XML_STR)) {
      spi->u.exit = SCENARIO_STATUS_PLAY_FAILED;
    }  else if (!strcasecmp((const char *)attr, SUCCESS_VAL_XML_STR)) {
      spi->u.exit = SCENARIO_STATUS_PLAY_SUCCESS;
    }  else {
      AssertFatal(0, "Exit status not allowed %s", attr);
    }
    xmlFree(attr);
  }
  return spi;
}
//------------------------------------------------------------------------------
scenario_player_item_t* msp_load_jcond (scenario_t * const scenario, xmlDocPtr const xml_doc, xmlXPathContextPtr  xpath_ctx, xmlNodePtr node)
{
  scenario_player_item_t * spi = calloc(1, sizeof(*spi));
  spi->item_type = SCENARIO_PLAYER_ITEM_JUMP_COND;
  // get a UID
  spi->uid = msp_get_seq_uid();

  xmlChar *attr = xmlGetProp(node, (const xmlChar *)VAR_NAME_ATTR_XML_STR);
  void                                   *uid = NULL;
  if (attr) {
    OAILOG_TRACE (LOG_MME_SCENARIO_PLAYER, "Found %s=%s uid=%u\n", VAR_NAME_ATTR_XML_STR, attr, spi->uid);

    scenario_player_item_t * spi_var = sp_get_var(scenario, (unsigned char *)attr);
    AssertFatal(spi_var, "var %s not found", attr);
    xmlFree(attr);
    spi->u.cond.var_uid = (int)(uintptr_t)spi_var->uid;
  } else {
    AssertFatal(0, "Could not find var_name");
  }

  attr = xmlGetProp(node, (const xmlChar *)COND_ATTR_XML_STR);
  if (attr) {
    OAILOG_TRACE (LOG_MME_SCENARIO_PLAYER, "Found %s=%s\n", COND_ATTR_XML_STR, attr);
    char * value = (char*)attr;
    if (!strcasecmp("eq", value)) {
      spi->u.cond.test_operator = TEST_EQ;
    } else if (!strcasecmp("ne", value)) {
      spi->u.cond.test_operator = TEST_NE;
    } else  if (!strcasecmp("gt", value)) {
      spi->u.cond.test_operator = TEST_GT;
    } else  if (!strcasecmp("ge", value)) {
      spi->u.cond.test_operator = TEST_GE;
    } else  if (!strcasecmp("lt", value)) {
      spi->u.cond.test_operator = TEST_LT;
    } else  if (!strcasecmp("le", value)) {
      spi->u.cond.test_operator = TEST_LE;
    } else {
      AssertFatal(0, "Could not find understand cond:%s", attr);
    }
    xmlFree(attr);
  } else {
    AssertFatal(0, "Could not find %s", COND_ATTR_XML_STR);
  }

  attr = xmlGetProp(node, (const xmlChar *)LABEL_ATTR_XML_STR);
  uid = NULL;
  if (attr) {
    OAILOG_TRACE (LOG_MME_SCENARIO_PLAYER, "Found %s=%s uid=%u\n", LABEL_ATTR_XML_STR, attr, spi->uid);
    hashtable_rc_t rc = obj_hashtable_ts_get (scenario->label_items, (const void *)attr, xmlStrlen(attr), (void **)&uid);
    // may be jump forward label (defined lines after the jump condition)
    if (HASH_TABLE_OK != rc) {
      spi->u.cond.jump_label_uid = -1;
    } else {
      spi->u.cond.jump_label_uid = (int)(uintptr_t)uid;
    }
    spi->u.cond.jump_label = bfromcstr((const char *)attr);
    xmlFree(attr);
  } else {
    AssertFatal(0, "Could not find %s", LABEL_ATTR_XML_STR);
  }

  attr = xmlGetProp(node, (const xmlChar *)VALUE_ATTR_XML_STR);
  uid = NULL;
  if (attr) {
    OAILOG_TRACE (LOG_MME_SCENARIO_PLAYER, "Found %s=%s uid=%u\n", VALUE_ATTR_XML_STR, attr, spi->uid);
    spi->u.cond.var_test_value = atoi((const char *)attr);
    xmlFree(attr);
  } else {
    AssertFatal(0, "Could not find %s", VALUE_ATTR_XML_STR);
  }

  return spi;
}

//------------------------------------------------------------------------------
int msp_load_message (scenario_t * const scenario, bstring file_path, scenario_player_item_t * const scenario_player_item)
{
  OAILOG_FUNC_IN (LOG_MME_SCENARIO_PLAYER);
  xmlNodePtr  cur = NULL;
  int         rc = RETURNerror;

  OAILOG_DEBUG (LOG_MME_SCENARIO_PLAYER, "Loading message from XML file %s\n", bdata(file_path));

  if (!scenario_player_item->u.msg.xml_doc) {
    scenario_player_item->u.msg.xml_doc = xmlParseFile(bdata(file_path));
    if (NULL == scenario_player_item->u.msg.xml_doc ) {
      OAILOG_ERROR (LOG_MME_SCENARIO_PLAYER, "Document %s not parsed successfully\n", bdata(file_path));
      OAILOG_FUNC_RETURN (LOG_MME_SCENARIO_PLAYER, RETURNerror);
    }
  }
  cur = xmlDocGetRootElement(scenario_player_item->u.msg.xml_doc);
  if (cur == NULL) {
    // do not consider this as an error
    OAILOG_WARNING (LOG_MME_SCENARIO_PLAYER, "Empty document %s \n", bdata(file_path));
    xmlFreeDoc(scenario_player_item->u.msg.xml_doc);
    scenario_player_item->u.msg.xml_doc = NULL;
    OAILOG_FUNC_RETURN (LOG_MME_SCENARIO_PLAYER, RETURNok);
  }

  scenario_player_item->u.msg.xpath_ctx = xmlXPathNewContext(scenario_player_item->u.msg.xml_doc);

  // just for validating XML
  if (!xmlStrcmp(cur->name, (const xmlChar *) ITTI_SCTP_NEW_ASSOCIATION_XML_STR)) {
    rc = xml_msg_load_itti_sctp_new_association(scenario, &scenario_player_item->u.msg);
  } else if (!xmlStrcmp(cur->name, (const xmlChar *) ITTI_SCTP_CLOSE_ASSOCIATION_XML_STR)) {
    rc = xml_msg_load_itti_sctp_close_association(scenario, &scenario_player_item->u.msg);
  } else if (!xmlStrcmp(cur->name, (const xmlChar *) ITTI_S1AP_E_RAB_SETUP_REQ_XML_STR)) {
    rc = xml_msg_load_itti_s1ap_e_rab_setup_req(scenario, &scenario_player_item->u.msg);
  } else if (!xmlStrcmp(cur->name, (const xmlChar *) ITTI_S1AP_E_RAB_SETUP_RSP_XML_STR)) {
    rc = xml_msg_load_itti_s1ap_e_rab_setup_rsp(scenario, &scenario_player_item->u.msg);
  } else if (!xmlStrcmp(cur->name, (const xmlChar *) ITTI_S1AP_UE_CONTEXT_RELEASE_REQ_XML_STR)) {
    rc = xml_msg_load_itti_s1ap_ue_context_release_req(scenario, &scenario_player_item->u.msg);
  } else if (!xmlStrcmp(cur->name, (const xmlChar *) ITTI_S1AP_UE_CONTEXT_RELEASE_COMMAND_XML_STR)) {
    rc = xml_msg_load_itti_s1ap_ue_context_release_command(scenario, &scenario_player_item->u.msg);
  } else if (!xmlStrcmp(cur->name, (const xmlChar *) ITTI_S1AP_UE_CONTEXT_RELEASE_COMPLETE_XML_STR)) {
    rc = xml_msg_load_itti_s1ap_ue_context_release_complete(scenario, &scenario_player_item->u.msg);
  } else if (!xmlStrcmp(cur->name, (const xmlChar *) ITTI_S1AP_INITIAL_UE_MESSAGE_XML_STR)) {
    rc = xml_msg_load_itti_s1ap_initial_ue_message(scenario, &scenario_player_item->u.msg);
  } else if (!xmlStrcmp(cur->name, (const xmlChar *) ITTI_MME_APP_INITIAL_CONTEXT_SETUP_RSP_XML_STR)) {
    rc = xml_msg_load_itti_mme_app_initial_context_setup_rsp(scenario, &scenario_player_item->u.msg);
  } else if (!xmlStrcmp(cur->name, (const xmlChar *) ITTI_MME_APP_CONNECTION_ESTABLISHMENT_CNF_XML_STR)) {
    rc = xml_msg_load_itti_mme_app_connection_establishment_cnf(scenario, &scenario_player_item->u.msg);
  } else if (!xmlStrcmp(cur->name, (const xmlChar *) ITTI_NAS_UPLINK_DATA_IND_XML_STR)) {
    rc = xml_msg_load_itti_nas_uplink_data_ind(scenario, &scenario_player_item->u.msg);
  } else if (!xmlStrcmp(cur->name, (const xmlChar *) ITTI_NAS_DOWNLINK_DATA_REQ_XML_STR)) {
    rc = xml_msg_load_itti_nas_downlink_data_req(scenario, &scenario_player_item->u.msg);
  } else if (!xmlStrcmp(cur->name, (const xmlChar *) ITTI_NAS_DOWNLINK_DATA_REJ_XML_STR)) {
    rc = xml_msg_load_itti_nas_downlink_data_rej(scenario, &scenario_player_item->u.msg);
  } else if (!xmlStrcmp(cur->name, (const xmlChar *) ITTI_NAS_DOWNLINK_DATA_CNF_XML_STR)) {
    rc = xml_msg_load_itti_nas_downlink_data_cnf(scenario, &scenario_player_item->u.msg);
  } else {
    OAILOG_ERROR (LOG_MME_SCENARIO_PLAYER, "Unrecognized message %s\n", cur->name);
  }
  OAILOG_FUNC_RETURN (LOG_MME_SCENARIO_PLAYER, rc);
}

//------------------------------------------------------------------------------
int msp_reload_message (scenario_t * const scenario, scenario_player_item_t * const scenario_player_item)
{
  xmlNodePtr  cur = NULL;
  int         rc = RETURNerror;

  OAILOG_DEBUG (LOG_MME_SCENARIO_PLAYER, "Reloading message from XML doc\n");

  AssertFatal(scenario_player_item->u.msg.xml_doc, "No XML doc found!");

  cur = xmlDocGetRootElement(scenario_player_item->u.msg.xml_doc);
  AssertFatal (cur, "Could not get root element of XML doc");
  // better to clean instead of reset ?
  xmlXPathFreeContext(scenario_player_item->u.msg.xpath_ctx);
  scenario_player_item->u.msg.xpath_ctx = xmlXPathNewContext(scenario_player_item->u.msg.xml_doc);

  // free it (may be called from msp_reload_message)
  if (scenario_player_item->u.msg.itti_msg) {
    itti_free_msg_content (scenario_player_item->u.msg.itti_msg);
    itti_free (ITTI_MSG_ORIGIN_ID (scenario_player_item->u.msg.itti_msg), scenario_player_item->u.msg.itti_msg);
    scenario_player_item->u.msg.itti_msg = NULL;
  }

  if (!xmlStrcmp(cur->name, (const xmlChar *) ITTI_SCTP_NEW_ASSOCIATION_XML_STR)) {
    rc = xml_msg_load_itti_sctp_new_association(scenario, &scenario_player_item->u.msg);
  } else if (!xmlStrcmp(cur->name, (const xmlChar *) ITTI_SCTP_CLOSE_ASSOCIATION_XML_STR)) {
    rc = xml_msg_load_itti_sctp_close_association(scenario, &scenario_player_item->u.msg);
  } else if (!xmlStrcmp(cur->name, (const xmlChar *) ITTI_S1AP_E_RAB_SETUP_REQ_XML_STR)) {
    rc = xml_msg_load_itti_s1ap_e_rab_setup_req(scenario, &scenario_player_item->u.msg);
  } else if (!xmlStrcmp(cur->name, (const xmlChar *) ITTI_S1AP_E_RAB_SETUP_RSP_XML_STR)) {
    rc = xml_msg_load_itti_s1ap_e_rab_setup_rsp(scenario, &scenario_player_item->u.msg);
  } else if (!xmlStrcmp(cur->name, (const xmlChar *) ITTI_S1AP_UE_CONTEXT_RELEASE_REQ_XML_STR)) {
    rc = xml_msg_load_itti_s1ap_ue_context_release_req(scenario, &scenario_player_item->u.msg);
  } else if (!xmlStrcmp(cur->name, (const xmlChar *) ITTI_S1AP_UE_CONTEXT_RELEASE_COMMAND_XML_STR)) {
    rc = xml_msg_load_itti_s1ap_ue_context_release_command(scenario, &scenario_player_item->u.msg);
  } else if (!xmlStrcmp(cur->name, (const xmlChar *) ITTI_S1AP_UE_CONTEXT_RELEASE_COMPLETE_XML_STR)) {
    rc = xml_msg_load_itti_s1ap_ue_context_release_complete(scenario, &scenario_player_item->u.msg);
  } else if (!xmlStrcmp(cur->name, (const xmlChar *) ITTI_S1AP_INITIAL_UE_MESSAGE_XML_STR)) {
    rc = xml_msg_load_itti_s1ap_initial_ue_message(scenario, &scenario_player_item->u.msg);
  } else if (!xmlStrcmp(cur->name, (const xmlChar *) ITTI_MME_APP_INITIAL_CONTEXT_SETUP_RSP_XML_STR)) {
    rc = xml_msg_load_itti_mme_app_initial_context_setup_rsp(scenario, &scenario_player_item->u.msg);
  } else if (!xmlStrcmp(cur->name, (const xmlChar *) ITTI_MME_APP_CONNECTION_ESTABLISHMENT_CNF_XML_STR)) {
    rc = xml_msg_load_itti_mme_app_connection_establishment_cnf(scenario, &scenario_player_item->u.msg);
  } else if (!xmlStrcmp(cur->name, (const xmlChar *) ITTI_NAS_UPLINK_DATA_IND_XML_STR)) {
    rc = xml_msg_load_itti_nas_uplink_data_ind(scenario, &scenario_player_item->u.msg);
  } else if (!xmlStrcmp(cur->name, (const xmlChar *) ITTI_NAS_DOWNLINK_DATA_REQ_XML_STR)) {
    rc = xml_msg_load_itti_nas_downlink_data_req(scenario, &scenario_player_item->u.msg);
  } else if (!xmlStrcmp(cur->name, (const xmlChar *) ITTI_NAS_DOWNLINK_DATA_REJ_XML_STR)) {
    rc = xml_msg_load_itti_nas_downlink_data_rej(scenario, &scenario_player_item->u.msg);
  } else if (!xmlStrcmp(cur->name, (const xmlChar *) ITTI_NAS_DOWNLINK_DATA_CNF_XML_STR)) {
    rc = xml_msg_load_itti_nas_downlink_data_cnf(scenario, &scenario_player_item->u.msg);
  }
  return rc;
}

//------------------------------------------------------------------------------
void msp_free_message_content (scenario_player_msg_t * msg)
{
  if (msg) {
    if (msg->xml_doc) {
      xmlFreeDoc(msg->xml_doc);
      msg->xml_doc = NULL;
    }
    if (msg->xpath_ctx) {
      xmlXPathFreeContext(msg->xpath_ctx);
      msg->xpath_ctx = NULL;
    }

    if (msg->itti_msg) {
      //TODO  need to do more than that
      itti_free_msg_content(msg->itti_msg);
      itti_free (ITTI_MSG_ORIGIN_ID (msg->itti_msg), msg->itti_msg);
      msg->itti_msg = NULL;
    }
  }
}

//------------------------------------------------------------------------------
void msp_scenario_add_item(scenario_t * const scenario, scenario_player_item_t * const item)
{
  if ((scenario) && (item)) {
    // add it in hashtable
    // test if key exist before because insert will overwrite entry and free it if item is var -> memory corruption
    hashtable_rc_t rc = hashtable_ts_is_key_exists (scenario->scenario_items, (hash_key_t)item->uid);
    if (HASH_TABLE_OK == rc) {
      AssertFatal(SCENARIO_PLAYER_ITEM_VAR == item->item_type, "Error duplicate item in scenario, allowed only for vars, item_type=%d", item->item_type);
    } else {
      hashtable_rc_t rc = hashtable_ts_insert (scenario->scenario_items, (hash_key_t)item->uid, (void *)item);
      AssertFatal(HASH_TABLE_OK == rc, "Error in putting item in hashtable");
    }

    // add it in list
    if (!scenario->head_item) {
      scenario->head_item = item;
    }

    if (scenario->tail_item) {
      scenario->tail_item->next_item = item;
    }
    item->next_item = NULL;
    item->previous_item = scenario->tail_item;
    scenario->tail_item = item;

    if (SCENARIO_PLAYER_ITEM_SCENARIO == item->item_type) {
      item->u.scenario->parent = scenario;
    }
    OAILOG_DEBUG (LOG_MME_SCENARIO_PLAYER, "Scenario added item type %d uid %u\n", item->item_type, item->uid);
  }
}
//------------------------------------------------------------------------------
void msp_scenario_player_add_scenario(scenario_player_t * const scenario_player, scenario_t * const scenario)
{
  if ((scenario_player) && (scenario)) {
    if (!(scenario_player->head_scenarios)) {
      scenario_player->head_scenarios = scenario;
      scenario_player->tail_scenarios = scenario;
    } else {
      scenario_player->tail_scenarios->next = scenario;
      scenario_player->tail_scenarios = scenario;
      scenario->next = NULL;
    }
  }
}
//------------------------------------------------------------------------------
bool msp_exist_scenario(scenario_player_t * const scenario_player, const char * const name)
{
  if (scenario_player) {
    scenario_t *  scenario = scenario_player->head_scenarios;
    while (scenario) {
      if (!strcmp(name, (const char *)scenario->name->data)) {
        return true;;
      }
      scenario = scenario->next;
    }
  }
  return false;
}
//------------------------------------------------------------------------------
scenario_player_item_t *  msp_load_scenario (bstring file_path, scenario_player_t * const scenario_player, scenario_t * parent_scenario)
{
  OAILOG_FUNC_IN(LOG_MME_SCENARIO_PLAYER);
  xmlDocPtr               doc = NULL;
  xmlNodePtr              cur = NULL;
  scenario_player_item_t *scenario_item = NULL;
  scenario_player_item_t *scenario_player_item = NULL;

  OAILOG_DEBUG (LOG_MME_SCENARIO_PLAYER, "Loading scenario from XML file %s\n", bdata(file_path));
  doc = xmlParseFile(bdata(file_path));
  AssertFatal ((doc), "Document %s not parsed successfully\n", bdata(file_path));

  cur = xmlDocGetRootElement(doc);
  if (cur == NULL) {
    // do not consider this as an error
    OAILOG_WARNING (LOG_MME_SCENARIO_PLAYER, "empty document %s \n", bdata(file_path));
    xmlFreeDoc(doc);
    OAILOG_FUNC_RETURN(LOG_MME_SCENARIO_PLAYER, NULL);
  }

  if (xmlStrcmp(cur->name, (const xmlChar *) SCENARIO_NODE_XML_STR)) {
    AssertFatal (0, "document %s of the wrong type, root node != %s %s\n", bdata(file_path), SCENARIO_NODE_XML_STR, cur->name);
    xmlFreeDoc(doc);
    OAILOG_FUNC_RETURN(LOG_MME_SCENARIO_PLAYER, NULL);
  }


  xmlChar *attr = xmlGetProp(cur, (const xmlChar *)NAME_ATTR_XML_STR);
  if (attr) {
    OAILOG_TRACE (LOG_MME_SCENARIO_PLAYER, "Found scenario %s %s\n", NAME_ATTR_XML_STR, attr);
  } else {
    AssertFatal(0, "Could not find scenario %s", NAME_ATTR_XML_STR);
  }

  scenario_item             = calloc(1, sizeof(scenario_player_item_t));
  scenario_t * scenario            = calloc(1, sizeof(scenario_t));
  scenario_item->item_type  = SCENARIO_PLAYER_ITEM_SCENARIO;
  scenario_item->uid        = msp_get_seq_uid();
  scenario_item->u.scenario = scenario;

  msp_init_scenario(scenario);
  scenario_set_status(scenario, SCENARIO_STATUS_LOADING, SCENARIO_REASON_NONE, NULL);
  scenario->parent = parent_scenario;
  scenario->name = bfromcstr((const char *)attr);
  xmlFree(attr);

  msp_scenario_player_add_scenario(scenario_player, scenario);

  xmlXPathContextPtr xpath_ctx = xmlXPathNewContext(doc);
  bool res = true;
  bstring xpath_expr = bformat("/%s/*",SCENARIO_NODE_XML_STR);
  xmlXPathObjectPtr xpath_obj = xml_find_nodes(doc, &xpath_ctx, xpath_expr);
  if (xpath_obj) {
    xmlNodeSetPtr nodes = xpath_obj->nodesetval;
    int size = (nodes) ? nodes->nodeNr : 0;
    for (int item = 0 ; item < size; item ++) {
      xmlNodePtr saved_node_ptr = xpath_ctx->node;
      res = (RETURNok == xmlXPathSetContextNode(nodes->nodeTab[item], xpath_ctx));

      if ((!xmlStrcmp(nodes->nodeTab[item]->name, (const xmlChar *)SCENARIO_FILE_NODE_XML_STR))){
        xmlChar* scenario_file = msp_get_attr_file (doc, nodes->nodeTab[item]);
        if (scenario_file) {
          // remove file name from file_path
          bstring bstr = NULL;
          if ('/' == scenario_file[0]) {
            // Absolute path
            bstr = bfromcstr ((const char *)scenario_file);
            xmlFree(scenario_file);
            if ((scenario_player_item = msp_load_scenario(bstr, scenario_player, scenario))) {
              msp_scenario_add_item(scenario, scenario_player_item);
            }
            bdestroy_wrapper (&bstr);
          } else {
            // relative path
            int pos = bstrrchrp (file_path, '/', blength(file_path) - 1);
            if (BSTR_ERR != pos) {
              bstr = bstrcpy(file_path);
              btrunc(bstr, pos + 1); // keep '/'
              bcatblk(bstr, scenario_file, xmlStrlen(scenario_file)+1);
              xmlFree(scenario_file);
              if ((scenario_player_item = msp_load_scenario(bstr, scenario_player, scenario))) {
                msp_scenario_add_item(scenario, scenario_player_item);
              }
              bdestroy_wrapper (&bstr);
            } else {
              OAILOG_ERROR (LOG_MME_SCENARIO_PLAYER, "Could not find char / in scenario path %s\n", file_path->data);
              xmlFree(scenario_file);
              OAILOG_FUNC_RETURN(LOG_MME_SCENARIO_PLAYER, NULL);
            }
          }
        } else {
          OAILOG_ERROR (LOG_MME_SCENARIO_PLAYER, "Could not get scenario file attr \n");
          OAILOG_FUNC_RETURN(LOG_MME_SCENARIO_PLAYER, NULL);
        }
      }

      if ((!xmlStrcmp(nodes->nodeTab[item]->name, (const xmlChar *)MESSAGE_FILE_NODE_XML_STR))) {
        if ((scenario_player_item = msp_load_message_file(scenario, doc, xpath_ctx, nodes->nodeTab[item], file_path))) {
          msp_scenario_add_item(scenario, scenario_player_item);
        } else {
          scenario_set_status(scenario, SCENARIO_STATUS_LOAD_FAILED, SCENARIO_XML_ITEM_MSG_FILE_LOAD_FAILED, NULL);
        }
      } else if ((!xmlStrcmp(nodes->nodeTab[item]->name, (const xmlChar *)EXIT_ATTR_XML_STR))) {
        if ((scenario_player_item = msp_load_exit(scenario, doc, xpath_ctx, nodes->nodeTab[item]))) {
          msp_scenario_add_item(scenario, scenario_player_item);
        } else {
          scenario_set_status(scenario, SCENARIO_STATUS_LOAD_FAILED, SCENARIO_XML_ITEM_EXIT_LOAD_FAILED, NULL);
        }
      } else if ((!xmlStrcmp(nodes->nodeTab[item]->name, (const xmlChar *)LABEL_ATTR_XML_STR))) {
        if ((scenario_player_item = msp_load_label(scenario, doc, xpath_ctx, nodes->nodeTab[item]))) {
          msp_scenario_add_item(scenario, scenario_player_item);
        } else {
          scenario_set_status(scenario, SCENARIO_STATUS_LOAD_FAILED, SCENARIO_XML_ITEM_LABEL_LOAD_FAILED, NULL);
        }
      } else if ((!xmlStrcmp(nodes->nodeTab[item]->name, (const xmlChar *)VAR_NODE_XML_STR))) {
        if ((scenario_player_item = msp_load_var(scenario, doc, xpath_ctx, nodes->nodeTab[item]))) {
          msp_scenario_add_item(scenario, scenario_player_item);
        } else {
          scenario_set_status(scenario, SCENARIO_STATUS_LOAD_FAILED, SCENARIO_XML_ITEM_VAR_LOAD_FAILED, NULL);
        }
      } else if ((!xmlStrcmp(nodes->nodeTab[item]->name, (const xmlChar *)SET_VAR_NODE_XML_STR))) {
        if ((scenario_player_item = msp_load_set_var(scenario, doc, xpath_ctx, nodes->nodeTab[item]))) {
          msp_scenario_add_item(scenario, scenario_player_item);
        } else {
          scenario_set_status(scenario, SCENARIO_STATUS_LOAD_FAILED, SCENARIO_XML_ITEM_SET_VAR_LOAD_FAILED, NULL);
        }
      } else if ((!xmlStrcmp(nodes->nodeTab[item]->name, (const xmlChar *)INCR_VAR_INC_NODE_XML_STR))) {
        if ((scenario_player_item = msp_load_incr_var(scenario, doc, xpath_ctx, nodes->nodeTab[item]))) {
          msp_scenario_add_item(scenario, scenario_player_item);
        } else {
          scenario_set_status(scenario, SCENARIO_STATUS_LOAD_FAILED, SCENARIO_XML_ITEM_INCR_VAR_LOAD_FAILED, NULL);
        }
      } else if ((!xmlStrcmp(nodes->nodeTab[item]->name, (const xmlChar *)DECR_VAR_INC_NODE_XML_STR))) {
        if ((scenario_player_item = msp_load_decr_var(scenario, doc, xpath_ctx, nodes->nodeTab[item]))) {
          msp_scenario_add_item(scenario, scenario_player_item);
        } else {
          scenario_set_status(scenario, SCENARIO_STATUS_LOAD_FAILED, SCENARIO_XML_ITEM_DECR_VAR_LOAD_FAILED, NULL);
        }
      } else if ((!xmlStrcmp(nodes->nodeTab[item]->name, (const xmlChar *)JUMP_ON_COND_XML_STR))) {
        if ((scenario_player_item = msp_load_jcond(scenario, doc, xpath_ctx, nodes->nodeTab[item]))) {
          msp_scenario_add_item(scenario, scenario_player_item);
        } else {
          scenario_set_status(scenario, SCENARIO_STATUS_LOAD_FAILED, SCENARIO_XML_ITEM_JUMP_ON_COND_LOAD_FAILED, NULL);
        }
      } else if ((!xmlStrcmp(nodes->nodeTab[item]->name, (const xmlChar *)SLEEP_NODE_XML_STR))) {
        if ((scenario_player_item = msp_load_sleep(scenario, doc, xpath_ctx, nodes->nodeTab[item]))) {
          msp_scenario_add_item(scenario, scenario_player_item);
        } else {
          scenario_set_status(scenario, SCENARIO_STATUS_LOAD_FAILED, SCENARIO_XML_ITEM_SLEEP_LOAD_FAILED, NULL);
        }
      } else if ((!xmlStrcmp(nodes->nodeTab[item]->name, (const xmlChar *)USIM_NODE_XML_STR))) {
        // not a scenario_player_item
        if ((msp_load_usim_data(scenario, doc, xpath_ctx, nodes->nodeTab[item]))) {
          scenario_set_status(scenario, SCENARIO_STATUS_LOAD_FAILED, SCENARIO_XML_USIM_LOAD_FAILED, NULL);
        }
      } else if ((!xmlStrcmp(nodes->nodeTab[item]->name, (const xmlChar *)COMPUTE_AUTHENTICATION_RESPONSE_PARAMETER_NODE_XML_STR))) {
        if ((scenario_player_item = msp_load_compute_authentication_response_parameter(scenario, doc, xpath_ctx, nodes->nodeTab[item]))) {
          msp_scenario_add_item(scenario, scenario_player_item);
        } else {
          scenario_set_status(scenario, SCENARIO_STATUS_LOAD_FAILED, SCENARIO_XML_ITEM_COMPUTE_AUTHENTICATION_RESPONSE_PARAMETER_LOAD_FAILED, NULL);
        }
      } else if ((!xmlStrcmp(nodes->nodeTab[item]->name, (const xmlChar *)COMPUTE_AUTHENTICATION_SYNC_FAILURE_PARAMETER_NODE_XML_STR))) {
        if ((scenario_player_item = msp_load_compute_authentication_sync_failure_parameter(scenario, doc, xpath_ctx, nodes->nodeTab[item]))) {
          msp_scenario_add_item(scenario, scenario_player_item);
        } else {
          scenario_set_status(scenario, SCENARIO_STATUS_LOAD_FAILED, SCENARIO_XML_ITEM_COMPUTE_AUTHENTICATION_SYNC_FAILURE_PARAMETER_LOAD_FAILED, NULL);
        }
      } else if ((!xmlStrcmp(nodes->nodeTab[item]->name, (const xmlChar *)UPDATE_EMM_SECURITY_CONTEXT_NODE_XML_STR))) {
        if ((scenario_player_item = msp_load_update_emm_security_context(scenario, doc, xpath_ctx, nodes->nodeTab[item]))) {
          msp_scenario_add_item(scenario, scenario_player_item);
        } else {
          scenario_set_status(scenario, SCENARIO_STATUS_LOAD_FAILED, SCENARIO_XML_ITEM_UPDATE_EMM_SECURITY_CONTEXT_LOAD_FAILED, NULL);
        }
      } else {
        OAILOG_TRACE (LOG_MME_SCENARIO_PLAYER, "Found %s\n", nodes->nodeTab[item]->name);
      }
      if (saved_node_ptr) {
        res = (RETURNok == xmlXPathSetContextNode(saved_node_ptr, xpath_ctx)) & res;
      }
    }
    xmlXPathFreeObject(xpath_obj);
  }
  xmlXPathFreeContext(xpath_ctx);
  xmlFreeDoc(doc);
  bdestroy_wrapper (&xpath_expr);

  AssertFatal(SCENARIO_STATUS_LOADING == scenario->status, "Failed to load scenario %s: status=%d\n", bdata(file_path), scenario->status);
  if (SCENARIO_STATUS_LOADING == scenario->status) {
    scenario_set_status(scenario, SCENARIO_STATUS_LOADED, SCENARIO_REASON_NONE, NULL);
  }
  OAILOG_FUNC_RETURN(LOG_MME_SCENARIO_PLAYER, scenario_item);
}


//------------------------------------------------------------------------------
void msp_free_scenario_player_item (scenario_player_item_t * item)
{
  if (item) {
    switch (item->item_type) {
    case SCENARIO_PLAYER_ITEM_SCENARIO:
      msp_free_scenario(item->u.scenario);
      break;

    case SCENARIO_PLAYER_ITEM_ITTI_MSG:
      msp_free_message_content(&item->u.msg);
      break;

    case SCENARIO_PLAYER_ITEM_LABEL:
      bdestroy_wrapper (&item->u.label);
      break;

    case SCENARIO_PLAYER_ITEM_JUMP_COND:
      bdestroy_wrapper (&item->u.cond.jump_label);
      break;

    case SCENARIO_PLAYER_ITEM_VAR:
      if ((VAR_VALUE_TYPE_HEX_STREAM == item->u.set_var.value_type) || (VAR_VALUE_TYPE_ASCII_STREAM == item->u.set_var.value_type)) {
        bdestroy_wrapper(&item->u.var.value.value_bstr);
      }
      bdestroy_wrapper(&item->u.var.name);
      break;

    case SCENARIO_PLAYER_ITEM_VAR_SET:
      if ((VAR_VALUE_TYPE_HEX_STREAM == item->u.set_var.value_type) || (VAR_VALUE_TYPE_ASCII_STREAM == item->u.set_var.value_type)) {
        bdestroy_wrapper(&item->u.set_var.value.value_bstr);
      }
      bdestroy_wrapper(&item->u.set_var.name);
      break;

    case SCENARIO_PLAYER_ITEM_EXIT:
    case SCENARIO_PLAYER_ITEM_SLEEP:
    case SCENARIO_PLAYER_ITEM_VAR_INCR:
    case SCENARIO_PLAYER_ITEM_VAR_DECR:
    case SCENARIO_PLAYER_ITEM_COMPUTE_AUTHENTICATION_RESPONSE_PARAMETER:
    case SCENARIO_PLAYER_ITEM_COMPUTE_AUTHENTICATION_SYNC_FAILURE_PARAMETER:
    case SCENARIO_PLAYER_ITEM_UPDATE_EMM_SECURITY_CONTEXT:
      //NOP
      break;

    default:
      OAILOG_WARNING (LOG_MME_SCENARIO_PLAYER, "Unknown item type %d\n", item->item_type);
    }
    free(item);
  }
}
//------------------------------------------------------------------------------
void msp_free_scenario (scenario_t * scenario)
{
  if (scenario) {
    OAILOG_DEBUG (LOG_MME_SCENARIO_PLAYER, "Freeing scenario %s\n", bdata(scenario->name));
    pthread_mutex_lock(&scenario->lock);
    if (scenario->usim_data) {
      free_wrapper((void**)scenario->usim_data);
    }
    scenario_player_item_t *item = scenario->head_item;
    while (item) {
      scenario_player_item_t *last_item = item;
      item = item->next_item;
      msp_free_scenario_player_item(last_item);
    }
    xmlFreeDoc(scenario->xml_doc);
    bdestroy_wrapper (&scenario->name);
    hashtable_ts_destroy(scenario->scenario_items);
    obj_hashtable_ts_destroy(scenario->var_items);
    obj_hashtable_ts_destroy(scenario->label_items);
    if (scenario->ue_emulated_emm_security_context) {
      free_wrapper((void**)&scenario->ue_emulated_emm_security_context);
    }
    pthread_mutex_unlock(&scenario->lock);
    pthread_mutex_destroy(&scenario->lock);
    free_wrapper((void**)&scenario);
  }
}

//------------------------------------------------------------------------------
void msp_fsm(void)
{
}
