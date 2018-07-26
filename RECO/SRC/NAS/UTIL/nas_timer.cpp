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
  Source      nas_timer.c

  Version     0.1

  Date        2012/10/09

  Product     NAS stack

  Subsystem   Utilities

  Author      Frederic Maurel

  Description Timer utilities

*****************************************************************************/

#include <pthread.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>             // memset
#include <stdlib.h>             // malloc, free
#include <sys/time.h>           // setitimer

#include "bstrlib.h"

#include "log.h"
#include "hashtable.h"
#include "intertask_interface.h"
//extern "C"{
#include "timer.h"
#include "nas_timer.h"
//}
#include "commonDef.h"
#include "common_defs.h"
#include "msc.h"
#include "dynamic_memory_check.h"

//extern "C"{
static hash_table_ts_t *nas_timers = NULL;

//------------------------------------------------------------------------------
int nas_timer_init (void)
{
  bstring name = bfromcstr("NAS timers");
  nas_timers = hashtable_ts_create (256, NULL, NULL, name);
  if (nas_timers)
    return (RETURNok);
  else
    return (RETURNerror);
}

//------------------------------------------------------------------------------
void nas_timer_cleanup (void)
{
  hashtable_ts_destroy (nas_timers);
}

//------------------------------------------------------------------------------

long int nas_timer_start (
    long sec,
    long usec,
    nas_timer_callback_t nas_timer_callback,
    void *nas_timer_callback_args)
{
  int                                     ret;
  long                                    timer_id;
  nas_itti_timer_arg_t                   *nas_itti_timer_arg = NULL;
  /*
   * Do not start null timer
   */
  if ((sec == 0) && (usec == 0)){
    return (NAS_TIMER_INACTIVE_ID);
  }

  nas_itti_timer_arg = static_cast<nas_itti_timer_arg_t*>(calloc(1, sizeof(nas_itti_timer_arg_t)));
  //nas_itti_timer_arg = calloc(1, sizeof(nas_itti_timer_arg_t));
  nas_itti_timer_arg->nas_timer_callback = nas_timer_callback;
  nas_itti_timer_arg->nas_timer_callback_arg = nas_timer_callback_args;

  ret = timer_setup (sec, usec, TASK_NAS_MME, INSTANCE_DEFAULT, TIMER_ONE_SHOT, nas_itti_timer_arg, &timer_id);

  if (ret == -1) {
    free_wrapper((void**)&nas_itti_timer_arg);
    return (long)NAS_TIMER_INACTIVE_ID;
  }
  // TODO log if error
  hashtable_ts_insert (nas_timers, (const hash_key_t) timer_id, (void*)nas_itti_timer_arg);

  return (timer_id);
}

//------------------------------------------------------------------------------
long int nas_timer_stop (long int timer_id)
{
  int rc = timer_remove (timer_id);
  if (!rc) {
    // TODO log if error
    hashtable_ts_free (nas_timers, (const hash_key_t) timer_id);
    return (NAS_TIMER_INACTIVE_ID);
  }
  return timer_id;
}

//------------------------------------------------------------------------------
void nas_timer_handle_signal_expiry (long timer_id, nas_itti_timer_arg_t *nas_itti_timer_arg)
{
  /*
   * Get the timer entry for which the system timer expired
   */
  nas_itti_timer_arg->nas_timer_callback (nas_itti_timer_arg->nas_timer_callback_arg);
  // TODO log if error
  hashtable_ts_free (nas_timers, (const hash_key_t) timer_id);
}
//}
