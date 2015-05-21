#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ve_alloc.h>
#include <ve_stats.h>
#include <ve_thread.h>
#include <ve_clock.h>

#define MODULE "ve_stats"

VeThrMutex *stat_list_mutex = NULL;
VeThrMutex *stat_cback_list_mutex = NULL;
static VeStatisticList *stat_list = NULL;
static VeStatCallbackList *stat_cback_list = NULL;

void veStatInit(void) {
  stat_list_mutex = veThrMutexCreate();
  stat_cback_list_mutex = veThrMutexCreate();
}

int veAddStatistic(VeStatistic *stat) {
  VeStatisticList *l;

  /* make sure that stat isn't already in the list */
  for(l = stat_list; l; l = l->next)
    if (l->stat == stat) {
      veError(MODULE, "attempted to add statistic %s twice",
	      stat->name ? stat->name : "<null>");
      return -1;
    }
  veThrMutexLock(stat_list_mutex);
  l = veAllocObj(VeStatisticList);
  assert(l != NULL);
  l->stat = stat;
  l->next = stat_list;
  stat_list = l;
  veThrMutexUnlock(stat_list_mutex);
  return 0;
}

int veRemoveStatistic(VeStatistic *stat) {
  VeStatisticList *l, *pre;

  veThrMutexLock(stat_list_mutex);
  for(pre = NULL, l = stat_list; l; pre = l, l = l->next)
    if (l->stat == stat) {
      /* remove this one */
      if (pre == NULL)
	stat_list = l->next;
      else 
	pre->next = l->next;
      veFree(l);
      break;
    }
  veThrMutexUnlock(stat_list_mutex);
  if (l)
    return 0;
  else {
    veError(MODULE, "attempt to remove statistic not added: %s",
	    stat->name ? stat->name : "<null>");
    return -1;
  }
}

VeStatisticList *veGetStatistics() {
  return stat_list;
}

static void add_cback_list(VeStatCallbackList **list, VeStatCallback cback,
			  long min_interval) {
  VeStatCallbackList *l;
  l = veAllocObj(VeStatCallbackList);
  assert(l != NULL);
  l->cback = cback;
  l->last_call = veClock();
  l->min_interval = min_interval;
  l->next = (*list);
  *list = l;
}

static int rem_cback_list(VeStatCallbackList **list, VeStatCallback cback) {
  VeStatCallbackList *l, *pre;
  for(pre = NULL, l = *list; l; pre = l, l = l->next) {
    if (l->cback == cback) {
      if (pre)
	pre->next = l->next;
      else
	(*list) = l->next;
      veFree(l);
      return 0;
    }
  }
  veError(MODULE, "attempt to remove cback that is not in list");
  return -1;
}

int veAddStatCallback(VeStatistic *stat, VeStatCallback cback, 
		      long min_interval) {
  VeStatCallbackList *l;
  
  for(l = stat_cback_list; l; l = l->next)
    if (l->cback == cback) {
      veError(MODULE, "attempt to add cback which is already general cback");
      return -1;
    }
  if (stat) {
    for(l = stat->listeners; l; l = l->next)
      if (l->cback == cback) {
	veError(MODULE, "attempt to add cback to stat it is already listening to");
	return -1;
      }
    veThrMutexLock(stat->listen_mutex);
    add_cback_list(&(stat->listeners),cback,min_interval);
    veThrMutexUnlock(stat->listen_mutex);
  } else {
    VeStatisticList *s = stat_list;
    while(s != NULL) {
      for(l = s->stat->listeners; l; l = l->next)
	if (l->cback == cback) {
	  veError(MODULE, "attempt to add cback as general but is already listening to specific stat %s", s->stat->name ? s->stat->name : "<null>");
	  return -1;
	}
      s = s->next;
    }
    veThrMutexLock(stat_cback_list_mutex);
    add_cback_list(&stat_cback_list,cback,min_interval);
    veThrMutexUnlock(stat_cback_list_mutex);
  }
  return 0;
}

int veRemoveStatCallback(VeStatistic *stat, VeStatCallback cback) {
  int res;

  if (stat) {
    veThrMutexLock(stat->listen_mutex);
    res = rem_cback_list(&stat->listeners,cback);
    veThrMutexUnlock(stat->listen_mutex);
  } else {
    veThrMutexLock(stat_cback_list_mutex);
    res = rem_cback_list(&stat_cback_list,cback);
    veThrMutexUnlock(stat_cback_list_mutex);
  }
  return res;
}

int veUpdateStatistic(VeStatistic *stat) {
  /* if NULL call all general callbacks */
  VeStatCallbackList *l;
  long now = veClock();

  if (stat) {
    for(l = stat->listeners; l ; l = l->next) {
      if ((now - l->last_call) >= l->min_interval) {
	l->cback(stat);
	l->last_call = now;
      }
    }
  }
  for(l = stat_cback_list; l ; l = l->next) {
    if ((now - l->last_call) >= l->min_interval) {
      l->cback(stat);
      l->last_call = now;
    }
  }
  return 0;
}

VeStatistic *veNewStatistic(char *module, char *name, char *units) {
  VeStatistic *v;

  v = veAllocObj(VeStatistic);
  assert(v != NULL);
  v->listen_mutex = veThrMutexCreate();
  v->last_change = veClock();
  v->module = module;
  v->name = name;
  v->units = units;
  return v;
}

int veStatToString(VeStatistic *stat, char *str_ret, int len) {
  str_ret[0] = '\0';
  switch(stat->type) {
  case VE_STAT_INT:
    veSnprintf(str_ret, len, "%s:%s = %8d %s", stat->module, stat->name,
		*(int *)(stat->data), stat->units);
    break;
  case VE_STAT_FLOAT:
    veSnprintf(str_ret, len, "%s:%s = %g %s", stat->module, stat->name,
		*(float *)(stat->data), stat->units);
    break;
  case VE_STAT_STRING:
    veSnprintf(str_ret, len, "%s:%s = %16s %s", stat->module, stat->name,
		*(char **)(stat->data), stat->units);
    break;
  }
  return 0;
}
