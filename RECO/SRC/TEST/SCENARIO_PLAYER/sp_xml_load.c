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
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#include <libxml/xmlwriter.h>
#include <libxml/xpath.h>
#include "bstrlib.h"

#include "dynamic_memory_check.h"
#include "log.h"
#include "assertions.h"
#include "common_defs.h"
#include "conversions.h"
#include "mme_scenario_player.h"
#include "xml_load.h"
#include "sp_xml_load.h"

//------------------------------------------------------------------------------
bool sp_u64_from_xml (
    scenario_t            * const scenario,
    scenario_player_msg_t * const msg,
    uint64_t              * const container,
    char                  * const xml_tag_str)
{
  bstring xpath_expr = bformat("./%s",xml_tag_str);
  bstring xml_var = NULL;
  bool res = xml_load_leaf_tag(msg->xml_doc, msg->xpath_ctx, xpath_expr, "%"SCNx64, (void*)container, &xml_var);
  if (!res && xml_var) {
    if ('$' == xml_var->data[0]) {
      if (BSTR_OK == bdelete(xml_var, 0, 1)) {
        // Check if var already exists
        scenario_player_item_t * spi_var = sp_get_var(scenario, (unsigned char *)bdata(xml_var));
        AssertFatal(spi_var, "var %s not found", bdata(xml_var));
        *container = spi_var->u.var.value.value_u64;
        AssertFatal (spi_var->u.var.value_type == VAR_VALUE_TYPE_INT64, "Bad var type %d", spi_var->u.var.value_type);
        res = true;
        OAILOG_TRACE (LOG_MME_SCENARIO_PLAYER, "Set %s= %" PRIx64 " from var\n",
            xml_tag_str, *container);
      }
    } else if ('#' == xml_var->data[0]) {
      if (BSTR_OK == bdelete(xml_var, 0, 1)) {
        // Check if var already exists
        scenario_player_item_t * spi_var = sp_get_var(scenario, (unsigned char *)bdata(xml_var));
        AssertFatal(spi_var, "var %s not found", bdata(xml_var));
        res = true;
        *container = spi_var->u.var.value.value_u64;
        AssertFatal (spi_var->u.var.value_type == VAR_VALUE_TYPE_INT64, "Bad var type %d", spi_var->u.var.value_type);
      }
    }
    bdestroy_wrapper (&xml_var);
  }
  bdestroy_wrapper (&xpath_expr);
  return res;
}


//------------------------------------------------------------------------------
bool sp_xml_load_hex_stream_leaf_tag(
    scenario_t            * const scenario,
    scenario_player_msg_t * const msg,
    bstring                       xpath_expr,
    bstring               * const container)
{
  char value[1024];
  bool res = xml_load_leaf_tag(msg->xml_doc, msg->xpath_ctx, xpath_expr, "%s", (void*)value, NULL /* should always succeed*/);
  if (res) {

    int len = strlen((const char*)value);
    uint8_t hex[len/2];
    int ret = ascii_to_hex ((uint8_t *) hex, (const char *)value);
    if (ret) {
      OAILOG_TRACE (LOG_MME_SCENARIO_PLAYER, "Found %s=%s\n", bdata(xpath_expr), value);
      res = true;
      *container = blk2bstr(hex,len/2);
      return true;
    }
    res = false;
    if ('$' == value[0]) { // TODO add is alpha test
      char * varname = &value[1];

      // Check if var already exists
      scenario_player_item_t * spi_var = sp_get_var(scenario, (unsigned char *)varname);
      AssertFatal(spi_var, "var %s not found", varname);
      if (spi_var->u.var.value.value_bstr) {
        *container = bstrcpy(spi_var->u.var.value.value_bstr);
      } else {
        // return a non null container bstring if var not initialized
        char zero[] = "00";
        *container = bfromcstr(zero);
      }
      res = true;
#if DEBUG_IS_ON
      char ascii[blength(spi_var->u.var.value.value_bstr)*2+1];
      ascii[blength(spi_var->u.var.value.value_bstr)*2] = 0;
      hexa_to_ascii((uint8_t*)bdata(spi_var->u.var.value.value_bstr), ascii, blength(spi_var->u.var.value.value_bstr));
      OAILOG_TRACE (LOG_MME_SCENARIO_PLAYER, "Set %s=%s from var %s to be loaded (len=%d)\n",
          bdata(xpath_expr), ascii,  varname, blength(spi_var->u.var.value.value_bstr));
#endif
    } else if ('#' == value[0]) {
      char * varname = &value[1];

      // Check if var already exists
      scenario_player_item_t * spi_var = sp_get_var(scenario, (unsigned char *)varname);
      AssertFatal(spi_var, "var %s not found", varname);
      res = true;
      *container = bstrcpy(spi_var->u.var.value.value_bstr);
#if DEBUG_IS_ON
      char ascii[blength(spi_var->u.var.value.value_bstr)*2+1];
      ascii[blength(spi_var->u.var.value.value_bstr)*2] = 0;
      hexa_to_ascii((uint8_t*)bdata(spi_var->u.var.value.value_bstr), ascii, blength(spi_var->u.var.value.value_bstr));
      OAILOG_TRACE (LOG_MME_SCENARIO_PLAYER, "Set %s=%s from var %s to be loaded (len=%d)\n",
          bdata(xpath_expr), ascii,  varname, blength(spi_var->u.var.value.value_bstr));
#endif
    }
    return res;
  }
  OAILOG_TRACE (LOG_MME_SCENARIO_PLAYER, "Warning: No result for searching %s\n", bdata(xpath_expr));
  return false;
}

//------------------------------------------------------------------------------
bool sp_xml_load_ascii_stream_leaf_tag(
    scenario_t            * const scenario,
    scenario_player_msg_t * const msg,
    bstring                       xpath_expr,
    bstring               * const container)
{
  char value[1024];
  bool res = xml_load_leaf_tag(msg->xml_doc, msg->xpath_ctx, xpath_expr, "%s", (void*)value, NULL /* should always succeed*/);
  if (res) {
    res = false;
    if ('$' == value[0]) { // TODO add is alpha test
      char * varname = &value[1];

      // Check if var already exists
      scenario_player_item_t * spi_var = sp_get_var(scenario, (unsigned char *)varname);
      AssertFatal(spi_var, "var %s not found", varname);
      if (spi_var->u.var.value.value_bstr) {
        *container = bstrcpy(spi_var->u.var.value.value_bstr);
      } else {
        // return a non null container bstring if var not initialized
        char zero[] = "00";
        *container = bfromcstr(zero);
      }
      res = true;
    } else if ('#' == value[0]) {
      char * varname = &value[1];
      scenario_player_item_t * spi_var = sp_get_var(scenario, (unsigned char *)varname);
      AssertFatal(spi_var, "var %s not found", varname);
      res = true;
      *container = bstrcpy(spi_var->u.var.value.value_bstr);
    } else {
      OAILOG_TRACE (LOG_MME_SCENARIO_PLAYER, "Found %s=%s\n", bdata(xpath_expr), value);
      *container = bfromcstr((const char*)value);
      res = true;
      return true;
    }
    return res;
  }
  OAILOG_TRACE (LOG_MME_SCENARIO_PLAYER, "Warning: No result for searching %s\n", bdata(xpath_expr));
  return false;
}

//------------------------------------------------------------------------------
scenario_player_item_t * sp_get_var(scenario_t * const scenario, unsigned char * var_name)
{
  void                    *uid = NULL;
  hashtable_rc_t           rc = HASH_TABLE_OK;
  scenario_player_item_t * var_item = NULL;
  if (scenario) {

    rc = obj_hashtable_ts_get (scenario->var_items, var_name, strlen((char*)var_name), (void**)&uid);
    if (HASH_TABLE_OK == rc) {
      rc = hashtable_ts_get (scenario->scenario_items, (const hash_key_t)(uintptr_t)uid, (void **)&var_item);
      if ((HASH_TABLE_OK == rc) && (SCENARIO_PLAYER_ITEM_VAR == var_item->item_type)) {
        return var_item;
      } else {
        AssertFatal (0, "Could not find var %s uid %lu, should have been declared in scenario\n", var_name, (uintptr_t)uid);
      }
    } else {
      scenario_t * scenario_up = scenario->parent;
      while (scenario_up) {
        rc = obj_hashtable_ts_get (scenario_up->var_items, var_name, strlen((char*)var_name), (void**)&uid);
        if (HASH_TABLE_OK == rc) {
          rc = hashtable_ts_get (scenario_up->scenario_items, (const hash_key_t)(uintptr_t)uid, (void **)&var_item);
          if ((HASH_TABLE_OK == rc) && (SCENARIO_PLAYER_ITEM_VAR == var_item->item_type)) {
            // make a copy into scenario
            return var_item;
          }
        }
        scenario_up = scenario_up->parent;
      }
    }
  }
  OAILOG_TRACE (LOG_MME_SCENARIO_PLAYER, "Could not find var %s in scenario %s and parent scenarios\n", var_name, scenario->name->data);
  return NULL;
}

//------------------------------------------------------------------------------
scenario_player_item_t * sp_get_var_by_uid(scenario_t * const scenario, const int uid)
{
  hashtable_rc_t           rc = HASH_TABLE_OK;
  scenario_player_item_t * var_item = NULL;
  if (scenario) {

    rc = hashtable_ts_get (scenario->scenario_items, (hash_key_t)uid, (void **)&var_item);
    if (HASH_TABLE_OK == rc) {
      if ((HASH_TABLE_OK == rc) && (SCENARIO_PLAYER_ITEM_VAR == var_item->item_type)) {
        return var_item;
      } else {
        AssertFatal (0, "Could not find uid %lu, should have been declared in scenario\n", (uintptr_t)uid);
      }
    } else {
      scenario_t * scenario_up = scenario->parent;
      while (scenario_up) {
        rc = hashtable_ts_get (scenario_up->scenario_items, (hash_key_t)uid, (void **)&var_item);
        if ((HASH_TABLE_OK == rc) && (SCENARIO_PLAYER_ITEM_VAR == var_item->item_type)) {
          // make a copy into scenario
          return var_item;
        }
        scenario_up = scenario_up->parent;
      }
    }
  }
  return NULL;
}


