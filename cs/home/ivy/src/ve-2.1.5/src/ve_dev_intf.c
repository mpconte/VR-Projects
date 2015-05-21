/* Built-in filters */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include <ve_debug.h>
#include <ve_device.h>
#include <ve_util.h>
#include <ve_keysym.h>

#define MODULE "ve_dev_intf"

/* The rename filter - allows for simple mapping (renames device and
   element specs) - if either part is left blank (device or elem) then
   that piece is left untouched, e.g.
   
   x.y - sets device to "x", element to "y"
   x   - sets device to "x", element untouched
   x.  - sets device to "x", element untouched
   .y  - sets element to "y", device untouched
*/

struct rename_filt_inst {
  char *devname;  /* new devname - NULL means leave untouched */
  char *elemname; /* new elemname - NULL means leave untouched */
};

static int rename_filt_proc(VeDeviceEvent *e, void *arg) {
  struct rename_filt_inst *inst = (struct rename_filt_inst *)arg;
  
  if (inst->devname) {
    if (e->device)
      free(e->device);
    e->device = veDupString(inst->devname);
  }
  if (inst->elemname) {
    if (e->elem)
      free(e->elem);
    e->elem = veDupString(inst->elemname);
  }
  return VE_FILT_CONTINUE;
}

static void rename_deinst(VeDeviceFilterInst *inst) {
  if (inst) {
    struct rename_filt_inst *rinst = (struct rename_filt_inst *)(inst->arg);
    if (rinst) {
      if (rinst->devname)
	free(rinst->devname);
      if (rinst->elemname)
	free(rinst->elemname);
      free(rinst);
    }
    free(inst);
  }
}

static VeDeviceFilterInst *rename_inst(VeDeviceFilter *f, char **args,
				       int nargs, VeStrMap optargs) {
  VeDeviceFilterInst *inst;
  struct rename_filt_inst *rinst;
  char *c, *dev, *elem;
  
  dev = NULL;
  elem = NULL;

  if (args && args[0]) {
    if (!(c = strchr(args[0],'.'))) {
      if (strlen(args[0]) > 0)
	dev = veDupString(args[0]);
    } else {
      if ((c-args[0]) > 0) {
	dev = malloc(c-args[0]+1);
	assert(dev != NULL);
	strncpy(dev,args[0],c-args[0]);
	dev[c-args[0]] = '\0';
      }
      c++;
      if (strlen(c) > 0)
	elem = veDupString(c);
    }
  }
  rinst = malloc(sizeof(struct rename_filt_inst));
  assert(rinst != NULL);
  rinst->devname = dev;
  rinst->elemname = elem;
  inst = veDeviceCreateFilterInst();
  inst->filter = f;
  inst->arg = rinst;
  return inst;
}

static VeDeviceFilter rename_filter = {
  "rename", rename_filt_proc, NULL, rename_inst, rename_deinst
};

/* The copy filter - similar to rename except that it creates a copy
   of the event with the new name and leaves the original untouched. 
   Semantics are similar, but just "copy" is now meaningful (creates
   an exact duplicate of the event).  Note that copy can get you into
   trouble.  e.g.:

   filter foo.bar { copy }
   - will cause any event from foo.bar to be repeated infinitely
*/
struct copy_filt_inst {
  char *devname;  /* new devname - NULL means leave untouched */
  char *elemname; /* new elemname - NULL means leave untouched */
};

static int copy_filt_proc(VeDeviceEvent *orig, void *arg) {
  VeDeviceEvent *e;
  struct copy_filt_inst *inst = (struct copy_filt_inst *)arg;

  e = veDeviceEventCopy(orig);
  if (inst->devname) {
    if (e->device)
      free(e->device);
    e->device = veDupString(inst->devname);
  }
  if (inst->elemname) {
    if (e->elem)
      free(e->elem);
    e->elem = veDupString(inst->elemname);
  }
  /* push the event at the head of the queue so it is processed next */
  veDevicePushEvent(e,VE_QUEUE_HEAD);
  return VE_FILT_CONTINUE;
}

static void copy_deinst(VeDeviceFilterInst *inst) {
  if (inst) {
    struct copy_filt_inst *rinst = (struct copy_filt_inst *)(inst->arg);
    if (rinst) {
      if (rinst->devname)
	free(rinst->devname);
      if (rinst->elemname)
	free(rinst->elemname);
      free(rinst);
    }
    free(inst);
  }
}

static VeDeviceFilterInst *copy_inst(VeDeviceFilter *f, char **args,
				       int nargs, VeStrMap optargs) {
  VeDeviceFilterInst *inst;
  struct copy_filt_inst *rinst;
  char *c, *dev, *elem;
  
  dev = NULL;
  elem = NULL;

  if (args && args[0]) {
    if (!(c = strchr(args[0],'.'))) {
      if (strlen(args[0]) > 0)
	dev = veDupString(args[0]);
    } else {
      if ((c-args[0]) > 0) {
	dev = malloc(c-args[0]+1);
	assert(dev != NULL);
	strncpy(dev,args[0],c-args[0]);
	dev[c-args[0]] = '\0';
      }
      c++;
      if (strlen(c) > 0)
	elem = veDupString(c);
    }
  }
  rinst = malloc(sizeof(struct copy_filt_inst));
  assert(rinst != NULL);
  rinst->devname = dev;
  rinst->elemname = elem;
  inst = veDeviceCreateFilterInst();
  inst->filter = f;
  inst->arg = rinst;
  return inst;
}

static VeDeviceFilter copy_filter = {
  "copy", copy_filt_proc, NULL, copy_inst, copy_deinst
};

/* conversion procedures (to_trigger, to_switch, to_valuator, to_keyboard)
   Generic conversion procedures to force events to a certain type:

   to_trigger   - unconditional change to trigger (no meaningful args)
   to_switch    - threshold=x invert=bool state=0|1
       threshold=x (from valuator - if (value is < x), state = 0, else 1,
                    default is 0.0)
       invert=0|1 (valuator - inverts result of threshold (if 1)
                   switch - inverts current value of switch (if 1))
       state=0|1  (sets state of switch - trumps all other options)
   to_valuator  - expr=[infix expr in x] value=x
       expr=[infix expr in x] (x is set to current value of valuator,
                               state of switch/keyboard (0 or 1), 
			       or 1 in the case of a trigger)
       value=x (sets value of valuator - trumps all other options)
   to_keyboard  - [same as switch plus] key=k
       key=k - sets explicit value of key
*/
/* flags */
#define CONV_THRESHOLD  (1<<0)
#define CONV_INVERT     (1<<1)
#define CONV_STATE      (1<<2)
#define CONV_EXPR       (1<<3)
#define CONV_VALUE      (1<<4)
#define CONV_KEY        (1<<5)
#define CONV_MIN        (1<<6)
#define CONV_MAX        (1<<7)

/* expressions */
#define EX_PASS    (-3)  /* subexpression (parentheses) */
#define EX_NUM     (-2)  /* number */
#define EX_X       (-1)  /* original value */
#define EX_ADD       0
#define EX_SUB       1
#define EX_MULT      2
#define EX_DIV       3
#define EX_EXP       4
#define EX_NEG       5

struct expr_node {
  int type;
  float num;
  char *func;
  int key;
  /* next is used strictly for tokenization */
  struct expr_node *arg1, *arg2, *next;
};

static void dump_expr(struct expr_node *e) {
  switch(e->type) {
  case EX_NUM:  fprintf(stderr,"%g",e->num); break;
  case EX_X:    fprintf(stderr,"x"); break;
  case EX_ADD: 
    fprintf(stderr,"+ (");
    dump_expr(e->arg1);
    fprintf(stderr,") (");
    dump_expr(e->arg2);
    fprintf(stderr,")");
    break;
  case EX_SUB: 
    fprintf(stderr,"- (");
    dump_expr(e->arg1);
    fprintf(stderr,") (");
    dump_expr(e->arg2);
    fprintf(stderr,")");
    break;
  case EX_MULT: 
    fprintf(stderr,"* (");
    dump_expr(e->arg1);
    fprintf(stderr,") (");
    dump_expr(e->arg2);
    fprintf(stderr,")");
    break;
  case EX_DIV: 
    fprintf(stderr,"/ (");
    dump_expr(e->arg1);
    fprintf(stderr,") (");
    dump_expr(e->arg2);
    fprintf(stderr,")");
    break;
  case EX_EXP: 
    fprintf(stderr,"^ (");
    dump_expr(e->arg1);
    fprintf(stderr,") (");
    dump_expr(e->arg2);
    fprintf(stderr,")");
    break;
  case EX_NEG:
    fprintf(stderr,"! (");
    dump_expr(e->arg1);
    fprintf(stderr,")");
    break;
  case EX_PASS:
    fprintf(stderr,"(");
    dump_expr(e->arg1);
    fprintf(stderr,")");
    break;
  }
}

static float eval_expr_node(struct expr_node *e, float x) {
  switch(e->type) {
  case EX_NUM:  return e->num;
  case EX_X:    return x;
  case EX_ADD:  return eval_expr_node(e->arg1,x)+eval_expr_node(e->arg2,x);
  case EX_SUB:  return eval_expr_node(e->arg1,x)-eval_expr_node(e->arg2,x);
  case EX_MULT: return eval_expr_node(e->arg1,x)*eval_expr_node(e->arg2,x);
  case EX_DIV:  return eval_expr_node(e->arg1,x)/eval_expr_node(e->arg2,x);
  case EX_EXP:  return pow(eval_expr_node(e->arg1,x),eval_expr_node(e->arg2,x));
  case EX_NEG:  return -(eval_expr_node(e->arg1,x));
  case EX_PASS: return eval_expr_node(e->arg1,x);
  default:
    veFatalError(MODULE, "eval_expr_node: invalid node type: %d", e->type);
  }
  return 0.0; /* avoid warning */
}

static int expr_prec(int type) {
  switch (type) {
  case EX_NUM:
  case EX_X:
  case EX_PASS:
    return -1;
  case EX_ADD:
  case EX_SUB:
    return 0;
  case EX_MULT:
  case EX_DIV:
    return 1;
  case EX_EXP:
    return 2;
  case EX_NEG:
    return 3;
  default:
    veFatalError(MODULE,"expr_prec:  cannot determine precedence for type %d",
		 type);
  }
  return -1; /* avoid warning */
}

static struct expr_node *parse_subexpr(struct expr_node *n) {
  /* given list, find lowest precedence, right-most operator and handle
     at that level */
  struct expr_node *e, *cur, *prev, *curprev;
  int p, curp;
  cur = curprev = NULL;
  for(e = n, prev = NULL; e; prev = e, e = e->next) {
    if ((p = expr_prec(e->type)) >= 0) {
      if (!cur || p <= curp) {
	cur = e;
	curp = p;
	curprev = prev;
      }
    }
  }
  if (!cur) {
    /* only "terminals" */
    if (!n) {
      veError(MODULE, "parse_subexpr: missing terminal");
      return NULL;
    }
    if (n->next) {
      veError(MODULE, "parse_subexpr: n->type = %d   n->next->type = %d",
	      n->type, n->next->type);
      veError(MODULE, "parse_subexpr: missing operator (two terminals next to each other)");
      return NULL;
    }
    return n;
  } else {
    switch (cur->type) {
      /* binary operators */
    case EX_ADD:
    case EX_SUB:
    case EX_MULT:
    case EX_DIV:
    case EX_EXP:
      if (!curprev || !(cur->next)) {
	veError(MODULE, "parse_subexpr: missing second operand to binary operator");
	return NULL;
      }
      curprev->next = NULL; /* unhook it */
      /* start previous chain at the start "n" */
      if (!(cur->arg1 = parse_subexpr(n)) ||
	  !(cur->arg2 = parse_subexpr(cur->next)))
	return NULL;
      /* unhook me here... */
      cur->next = NULL;
      return cur;
      break;

      /* unary operators (arg on right) */
    case EX_NEG:
      if (curprev) {
	veError(MODULE, "parse_subexpr: unexpected operator or operand before negation");
	return NULL;
      }
      if (!(cur->next)) {
	veError(MODULE, "parse_subexpr: missing argument to negation");
	return NULL;
      }
      if (!(cur->arg1 = parse_subexpr(cur->next)))
	return NULL;
      return cur;
      break;
    default:
      veFatalError(MODULE, "parse_subexpr: unexpected node type: %d",cur->type);
    }
  }
  return NULL; /* avoid warning */
}

static struct expr_node *newexpr(int type) {
  struct expr_node *e;
  e = calloc(1,sizeof(struct expr_node));
  assert(e != NULL);
  e->type = type;
  return e;
}

#define ADDIT(e) { if (tail) { tail->next = (e); tail = (e); } else { head = tail = e; } }
static struct expr_node *parse_expr(char *expr, char term, char **end) {
  char *estart = expr, *eend;
  struct expr_node *head = NULL, *tail = NULL, *e;
  int left_is_op = 1;
 
  while (*expr != term && *expr != '\0') {
    if (isdigit(*expr) || *expr == '.') {
      /* read a number */
      float f;
      int n;
      if (sscanf(expr,"%f%n",&f,&n) != 1)
	veFatalError(MODULE, "parse_expr: bad expression: %s",estart);
      expr += n;
      e = newexpr(EX_NUM);
      e->num = f;
      ADDIT(e);
    } else {
      switch(*expr) {
      case 'x':  e = newexpr(EX_X); ADDIT(e); expr++; break;
      case '+':  e = newexpr(EX_ADD); ADDIT(e); expr++; break;
	/* note '-' may be negation - we fix it later when we build the tree */
      case '-':  e = newexpr(left_is_op ? EX_NEG : EX_SUB); ADDIT(e); expr++; break;
      case '*':  e = newexpr(EX_MULT); ADDIT(e); expr++; break;
      case '/':  e = newexpr(EX_DIV); ADDIT(e); expr++; break;
      case '^':  e = newexpr(EX_EXP); ADDIT(e); expr++; break;
      case '(':
	e = newexpr(EX_PASS);
	e->arg1 = parse_expr(expr+1,')',&eend);
	expr = eend;
	if (*expr != ')')
	  veFatalError(MODULE, "missing ')' in expression: %s", estart);
	expr++;
	if (!(e->arg1))
	  veFatalError(MODULE, "failed to parse expression: %s",estart);
	ADDIT(e);
	break;
      default:
	veFatalError(MODULE, "parse_expr: bad expression: %s at '%c'",
		     estart,*expr);
      }
    }
    left_is_op = (e->type >= 0 && e->type != EX_NEG);
  }
  if (end)
    *end = expr;
  if (head) {
    if (!(e = parse_subexpr(head)))
      veFatalError(MODULE, "failed to parse expression: %s",estart);
    return e;
  } else {
    return NULL;
  }
}

struct convert_filt_inst {
  int flags;
  float threshold;
  int invert;
  int state;
  int key;
  float value,min,max;
  int trigger;
  struct expr_node *expr;
};

static VeDeviceFilterInst *convert_inst(VeDeviceFilter *f, char **args,
					int nargs, VeStrMap optargs) {
  VeDeviceFilterInst *inst;
  struct convert_filt_inst *rinst;
  char *c;

  if (nargs > 0) {
    veError(MODULE,"convert: no positional args expected");
    return NULL;
  }
  rinst = calloc(1,sizeof(struct convert_filt_inst));
  assert(rinst != NULL);
  if ((c = veStrMapLookup(optargs,"threshold"))) {
    if (sscanf(c,"%f",&(rinst->threshold)) != 1) {
      veError(MODULE,"convert: threshold= argument must be float");
      return NULL;
    }
    rinst->flags |= CONV_THRESHOLD;
  }
  if ((c = veStrMapLookup(optargs,"value"))) {
    if (sscanf(c,"%f",&(rinst->value)) != 1) {
      veError(MODULE,"convert: value= argument must be float");
      return NULL;
    }
    rinst->flags |= CONV_VALUE;
  }
  if ((c = veStrMapLookup(optargs,"min"))) {
    if (sscanf(c,"%f",&(rinst->min)) != 1) {
      veError(MODULE,"convert: min= argument must be float");
      return NULL;
    }
    rinst->flags |= CONV_MIN;
  }
  if ((c = veStrMapLookup(optargs,"max"))) {
    if (sscanf(c,"%f",&(rinst->max)) != 1) {
      veError(MODULE,"convert: max= argument must be float");
      return NULL;
    }
    rinst->flags |= CONV_MAX;
  }
  if ((c = veStrMapLookup(optargs,"invert"))) {
    if (sscanf(c,"%d",&(rinst->invert)) != 1) {
      veError(MODULE,"convert: invert= argument must be int");
      return NULL;
    }
    rinst->flags |= CONV_INVERT;
  }
  if ((c = veStrMapLookup(optargs,"state"))) {
    if (sscanf(c,"%d",&(rinst->state)) != 1) {
      veError(MODULE,"convert: state= argument must be int");
      return NULL;
    }
    if (rinst->state)
      rinst->state = 1; /* clamp value */
    rinst->flags |= CONV_STATE;
  }
  if ((c = veStrMapLookup(optargs,"expr"))) {
    if (!(rinst->expr = parse_expr(c,'\0',NULL))) {
      veError(MODULE,"convert: could not parse expr= argument");
      return NULL;
    }
#if 0
    dump_expr(rinst->expr);
    fprintf(stderr,"\n");
#endif
    rinst->flags |= CONV_EXPR;
  }
  if (strcmp(f->name,"to_oneshot") == 0)
    rinst->trigger = 1;
  inst = veDeviceCreateFilterInst();
  inst->filter = f;
  inst->arg = rinst;
  return inst;
}

static void convert_deinst(VeDeviceFilterInst *inst) {
  if (inst) {
    struct convert_filt_inst *rinst = (struct convert_filt_inst *)(inst->arg);
    if (rinst) {
      if (rinst->expr) {
	/* TBD: freeing an expression tree ... */
      }
      free(rinst);
    }
    free(inst);
  }
}

static int convert_to_trigger_proc(VeDeviceEvent *orig, void *arg) {
  VeDeviceEContent *ec;
  ec = veDeviceEContentCreate(VE_ELEM_TRIGGER,0);
  veDeviceEContentDestroy(orig->content);
  orig->content = ec;
  return VE_FILT_CONTINUE;
}

static int convert_to_switch_proc(VeDeviceEvent *orig, void *arg) {
  VeDeviceE_Switch *sw;
  struct convert_filt_inst *f = (struct convert_filt_inst *)arg;

  sw = (VeDeviceE_Switch *)veDeviceEContentCreate(VE_ELEM_SWITCH,0);
  if (f->flags & CONV_STATE)
    sw->state = f->state;
  else {
    switch(orig->content->type) {
    case VE_ELEM_VECTOR:
    case VE_ELEM_TRIGGER:
      /* error invalid type */
      veError(MODULE, "to_switch: cannot convert vector or trigger without 'state=' arg");
      return VE_FILT_ERROR;
    case VE_ELEM_SWITCH:
      sw->state = ((VeDeviceE_Switch *)(orig->content))->state;
      break;
    case VE_ELEM_KEYBOARD:
      sw->state = ((VeDeviceE_Keyboard *)(orig->content))->state;
      break;
    case VE_ELEM_VALUATOR:
      {
	float thresh;
	if (f->flags & CONV_THRESHOLD)
	  thresh = f->threshold;
	else
	  thresh = 0.0;
	if (((VeDeviceE_Valuator *)(orig->content))->value < thresh)
	  sw->state = 0;
	else
	  sw->state = 1;
      }
      break;
    default:
      veFatalError(MODULE, "to_switch: unexpected event content type: %d",
		   orig->content->type);
    }
    if ((f->flags & CONV_INVERT) && f->invert)
      sw->state = sw->state ? 0 : 1;
  }

  if (f->trigger) {
    /* make it a trigger */
    if (sw->state) {
      veDeviceEContentDestroy(orig->content);
      veDeviceEContentDestroy((VeDeviceEContent *)sw);
      orig->content = veDeviceEContentCreate(VE_ELEM_TRIGGER,0);
      return VE_FILT_CONTINUE;
    } else {
      veDeviceEContentDestroy((VeDeviceEContent *)sw);
      return VE_FILT_DISCARD;
    }
  } else {
    veDeviceEContentDestroy(orig->content);
    orig->content = (VeDeviceEContent *)sw;
    return VE_FILT_CONTINUE;
  }
}

static int convert_to_valuator_proc(VeDeviceEvent *orig, void *arg) {
  VeDeviceE_Valuator *val;
  struct convert_filt_inst *f = (struct convert_filt_inst *)arg;
  float x;

  val = (VeDeviceE_Valuator *)veDeviceEContentCreate(VE_ELEM_VALUATOR,0);
  if (f->flags & CONV_VALUE)
    val->value = f->value;
  else if (!(f->flags & CONV_EXPR)) {
    veDeviceEContentDestroy((VeDeviceEContent *)val);
    veError(MODULE,"to_valuator: cannot convert without 'value=' or 'expr=' arg");
    return VE_FILT_ERROR;
  } else {
    switch(orig->content->type) {
    case VE_ELEM_VECTOR:
      /* error invalid type */
      veError(MODULE, "to_valuator: cannot convert vector without 'value=' arg");
      return VE_FILT_ERROR;
    case VE_ELEM_TRIGGER:
      x = 1.0;
      break;
    case VE_ELEM_SWITCH:
      x = ((VeDeviceE_Switch *)(orig->content))->state ? 1.0 : 0.0;
      break;
    case VE_ELEM_KEYBOARD:
      x = ((VeDeviceE_Keyboard *)(orig->content))->state ? 1.0 : 0.0;
      break;
    case VE_ELEM_VALUATOR:
      x = ((VeDeviceE_Valuator *)(orig->content))->value;
      val->min = ((VeDeviceE_Valuator *)(orig->content))->min;
      val->max = ((VeDeviceE_Valuator *)(orig->content))->max;
      break;
    default:
      veFatalError(MODULE,"to_valuator: unexpected event content type: %d",
		   orig->content->type);
    }
    val->value = eval_expr_node(f->expr,x);
  }
  if (f->flags & CONV_MIN)
    val->min = f->min;
  if (f->flags & CONV_MAX)
    val->max = f->max;
  veDeviceEContentDestroy(orig->content);
  orig->content = (VeDeviceEContent *)val;
  return VE_FILT_CONTINUE;
}

static int convert_to_keyboard_proc(VeDeviceEvent *orig, void *arg) {
  VeDeviceE_Keyboard *kb;
  struct convert_filt_inst *f = (struct convert_filt_inst *)arg;

  kb = (VeDeviceE_Keyboard *)veDeviceEContentCreate(VE_ELEM_KEYBOARD,0);
  if (f->flags & CONV_KEY)
    kb->key = f->key;
  else
    kb->key = VEK_UNKNOWN;
  if (f->flags & CONV_STATE)
    kb->state = f->state;
  else {
    switch(orig->content->type) {
    case VE_ELEM_VECTOR:
    case VE_ELEM_TRIGGER:
      /* error invalid type */
      veError(MODULE, "to_keyboard: cannot convert vector or trigger without 'state=' arg");
      return VE_FILT_ERROR;
    case VE_ELEM_SWITCH:
      kb->state = ((VeDeviceE_Switch *)(orig->content))->state;
      break;
    case VE_ELEM_KEYBOARD:
      kb->state = ((VeDeviceE_Keyboard *)(orig->content))->state;
      if (kb->key == VEK_UNKNOWN)
	kb->key = ((VeDeviceE_Keyboard *)(orig->content))->key;
      break;
    case VE_ELEM_VALUATOR:
      {
	float thresh;
	if (f->flags & CONV_THRESHOLD)
	  thresh = f->threshold;
	else
	  thresh = 0.0;
	if (((VeDeviceE_Valuator *)(orig->content))->value < thresh)
	  kb->state = 0;
	else
	  kb->state = 1;
      }
      break;
    default:
      veFatalError(MODULE, "to_switch: unexpected event content type: %d",
		   orig->content->type);
    }
    if ((f->flags & CONV_INVERT) && f->invert)
      kb->state = kb->state ? 0 : 1;
  }
  veDeviceEContentDestroy(orig->content);
  orig->content = (VeDeviceEContent *)kb;
  return VE_FILT_CONTINUE;
}

static VeDeviceFilter convert_filters[] = {
  { "to_trigger", convert_to_trigger_proc, NULL, convert_inst, convert_deinst },
  { "to_switch", convert_to_switch_proc, NULL, convert_inst, convert_deinst },
  { "to_valuator", convert_to_valuator_proc, NULL, convert_inst, convert_deinst },
  { "to_keyboard", convert_to_keyboard_proc, NULL, convert_inst, convert_deinst },
  { "to_oneshot", convert_to_switch_proc, NULL, convert_inst, convert_deinst },
  { NULL, NULL, NULL, NULL, NULL }
};

static int clamp_proc(VeDeviceEvent *orig, void *arg) {
  switch(orig->content->type) {
  case VE_ELEM_SWITCH:
    {
      VeDeviceE_Switch *sw = (VeDeviceE_Switch *)(orig->content);
      if (sw->state)
	sw->state = 1;
    }
    break;
  case VE_ELEM_KEYBOARD:
    {
      VeDeviceE_Keyboard *kb = (VeDeviceE_Keyboard *)(orig->content);
      if (kb->state)
	kb->state = 1;
    }
    break;
  case VE_ELEM_VALUATOR:
    {
      VeDeviceE_Valuator *val = (VeDeviceE_Valuator *)(orig->content);
      if (val->min != 0.0 || val->max != 0.0) {
	if (val->value < val->min)
	  val->value = val->min;
	else if (val->value > val->max)
	  val->value = val->max;
      }
    }
    break;
  case VE_ELEM_VECTOR:
    {
      VeDeviceE_Vector *val = (VeDeviceE_Vector *)(orig->content);
      int k;
      for(k = 0; k < val->size; k++) {
	if (val->min[k] != 0.0 || val->max[k] != 0.0) {
	  if (val->value[k] < val->min[k])
	    val->value[k] = val->min[k];
	  else if (val->value[k] > val->max[k])
	    val->value[k] = val->max[k];
	}
      }
    }
    break;
  default:
    /* this space intentionally left blank */;
  }
  return VE_FILT_CONTINUE;
}

static VeDeviceFilter clamp_filter = {
  "clamp", clamp_proc, NULL, NULL, NULL
};

#define CKNULL(x) ((x)?(x):"<null>")

static int dump_proc(VeDeviceEvent *e, void *arg) {
  switch(e->content->type) {
  case VE_ELEM_TRIGGER:
    {
      fprintf(stderr, "[%ld] %s.%s trigger\n",
	      e->timestamp, CKNULL(e->device), CKNULL(e->elem));
    }
    break;
  case VE_ELEM_SWITCH:
    {
      VeDeviceE_Switch *sw = (VeDeviceE_Switch *)(e->content);
      fprintf(stderr, "[%ld] %s.%s switch %d\n",
	      e->timestamp, CKNULL(e->device), CKNULL(e->elem),
	      sw->state);
    }
    break;
  case VE_ELEM_KEYBOARD:
    {
      VeDeviceE_Keyboard *kb = (VeDeviceE_Keyboard *)(e->content);
      fprintf(stderr, "[%ld] %s.%s keyboard %d %d\n",
	      e->timestamp, CKNULL(e->device), CKNULL(e->elem),
	      kb->key,kb->state);
    }
    break;
  case VE_ELEM_VALUATOR:
    {
      VeDeviceE_Valuator *val = (VeDeviceE_Valuator *)(e->content);
      fprintf(stderr, "[%ld] %s.%s valuator %g (%g:%g)\n",
	      e->timestamp, CKNULL(e->device), CKNULL(e->elem),
	      val->value, val->min, val->max);
    }
    break;
  case VE_ELEM_VECTOR:
    {
      VeDeviceE_Vector *vec = (VeDeviceE_Vector *)(e->content);
      int i;
      fprintf(stderr, "[%ld] %s.%s vector %d",
	      e->timestamp, CKNULL(e->device), CKNULL(e->elem),
	      vec->size);
      for(i = 0; i < vec->size; i++)
	fprintf(stderr, " %g (%g:%g)", vec->value[i], vec->min[i],
		vec->max[i]);
      fprintf(stderr, "\n");
    }
    break;
  default:
    fprintf(stderr, "[%ld] unknown event (type = %d)\n",
	    e->timestamp, e->content->type);
  }
  return VE_FILT_CONTINUE;
}

static VeDeviceFilter dump_filter = {
  "dump", dump_proc, NULL, NULL, NULL
};

void veDeviceBuiltinFilters(void) {
  int i;
  VE_DEBUG(1,("Adding built-in filters"));
  if (veDeviceAddFilter(&rename_filter))
    veFatalError(MODULE,"veDeviceBuiltinFilters: failed to add rename");
  if (veDeviceAddFilter(&copy_filter))
    veFatalError(MODULE,"veDeviceBuiltinFilters: failed to add copy");
  if (veDeviceAddFilter(&clamp_filter))
    veFatalError(MODULE,"veDeviceBuiltinFilters: failed to add clamp");
  if (veDeviceAddFilter(&dump_filter))
    veFatalError(MODULE,"veDeviceBuiltinFilters: failed to add dump");
  for(i = 0; convert_filters[i].name; i++)
    if (veDeviceAddFilter(&(convert_filters[i])))
      veFatalError(MODULE,"veDeviceBuiltinFilters: failed to add %s",
		   convert_filters[i].name);
}
