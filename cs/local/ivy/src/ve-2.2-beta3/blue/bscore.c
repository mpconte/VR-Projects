#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <bluescript.h>

/*@bsdoc

package core {
    longname {Core Procedures}
    desc {Core BlueScript functionality which is included in most
	BlueScript interpreters.}
    doc {
	Core procedures are present in most BlueScript interpreters.
	Core procedures include the main control constructs 
	(<code>if</code>, <code>while</code>, <code>foreach</code>,
	 etc.), variable storage and retrieval (<code>set</code>,
						<code>get</code>),
	and other basic functionality.
    }
}

*/

/*@bsdoc
procedure block {
    usage {block <body>}
    desc {Processes a script as a nested block.}
    returns {The result of the last command executed in the block.}
    doc {
	<p>
	A nested block is a script that uses a separate context from
	the context in which it is called.  This new context is
	<em>nested</em> in the calling context.  This has the
	following effects:
	<ul>
	<li>Variables in the calling context are visible and
	modifiable in the nested context.</li>
	<li>Procedures in the calling context are visible and
	modifiable in the nested context.</li>
	<li>Variables that are created in the nested context only
	exist in the nested context and are unset when the block call
	finishes.</li>
	<li>The nested context is created each time the block call is
	evaluated.  That is, variables that are set in the nested
	context are not visible from call to call.</li>
	<li><code>BREAK</code>, <code>CONTINUE</code>,
	<code>RETURN</code>, and <code>ERROR</code> conditions all
	have the same effect as though they were generated in the
	calling context.</li>
	</ul>
	</p>
	<p>Consider the following example:
	<div class="eg">
	set x 10
	block {
	    &nbsp; &nbsp; set y 20
	    &nbsp; &nbsp; set x 30
	}
	</div>
	After the block call, the variable <i>y</i> is no longer,
	visible, but <i>x</i> will be set to 30.
	</p>
    }
}
*/
static int core_block(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  int code;
  if (objc != 2) {
    bsSetStringResult(i,"usage: block <body>",BS_S_STATIC);
    return BS_ERROR;
  }
  bsClearResult(i);
  bsInterpNest(i);
  code = bsEval(i,objv[1]);
  bsInterpUnnest(i);
  return code;
}

/*@bsdoc
procedure break {
    usage {break}
    desc {Halts processing with a <code>BREAK</code> condition.  This
	condition will halt processing of a loop 
	(e.g. the body of <code>while</code> or <code>foreach</code>)
	and continue execution of the script after the end of the loop
	construct.  If execution is not currently within a loop then
	an error will be raised.}
}
*/
static int core_break(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  if (objc != 1) {
    bsSetStringResult(i,"too many arguments to break",BS_S_STATIC);
    return BS_ERROR;
  }
  return BS_BREAK;
}

/*@bsdoc
procedure continue {
    usage {continue}
    desc {Halts processing with a <code>CONTINUE</code> condition.
	This condition will halt processing of the <em>current</em>
	iteration of a loop
	(e.g. the body of <code>while</code> or <code>foreach</code>)
	and continue on to the next iteration of the loop.  If the
	loop involves a test condition, then the test condition is
	evaluated before starting the next interation.}
}
*/
static int core_continue(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  if (objc != 1) {
    bsSetStringResult(i,"too many arguments to continue",BS_S_STATIC);
    return BS_ERROR;
  }
  return BS_CONTINUE;
}

/*@bsdoc
procedure return {
    usage {return [<value>]}
    desc {Halts processing with a <code>RETURN</code> condition.
	The result of the interpreter is set to the argument.  If the
	argument is not specified then the result of the interpreter
	is set to the empty string.}
}
*/
static int core_return(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  if (objc > 2) {
    bsSetStringResult(i,"too many arguments to return",BS_S_STATIC);
    return BS_ERROR;
  }
  bsSetResult(i,((objc > 1) ? objv[1] : NULL));
  return BS_RETURN;
}

/*@bsdoc
procedure error {
    usage {error [<string>]}
    desc {Halts processing with an <code>ERROR</code> condition.
	The result of the interpreter is set to the argument, which is
	analogous to an error message.  An error exception will then
	be generated which will halt processing entirely, unless it is
	caught (e.g. by a <code>catch</code> call).
    }
}
*/
static int core_error(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  if (objc > 1) {
    bsSetResult(i,objv[1]);
  } else {
    bsSetStringResult(i,"error",BS_S_STATIC);
  }
  return BS_ERROR;
}

/*@bsdoc
procedure expr {
    usage {expr <expression>}
    returns {The result of evaluating the expression.}
    desc {<p>Evaluates the given infix expression.  Any variable or
	procedure calls in the expression are evaluated in the context
	of the <code>expr</code> call.  The result of the evaluation
	is returned.</p>
	<p>For efficiency, it is better to use a list construct rather
	than a string construct.  For example, the following:
	<div class="eg">expr {$x + $y}</div>
	is generally more efficient than:
	<div class="eg">expr "$x + $y"</div>
	If the argument to the expression is static, then the
	interpreter can cache the parse tree for the expression.  In
	the latter case, the argument to expr is not static - it is
	the result of a substitution, so the parse tree of the infix
	expression must be rebuilt on each call to the
	<code>expr</code> procedure.
	</p>}
}
*/
static int core_expr(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: expr <expr>";
  BSExprResult r;
  
  if (objc != 2) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }

  bsClearResult(i);
  if (bsExprEvalObj(i,objv[1],&r) != BS_OK)
    return BS_ERROR;
  switch (r.type) {
  case BS_T_STRING:
    bsSetStringResult(i,bsStringPtr(&(r.data.stringValue)),BS_S_VOLATILE);
    break;
  case BS_T_INT:
    bsSetIntResult(i,r.data.intValue);
    break;
  case BS_T_FLOAT:
    bsSetFloatResult(i,r.data.floatValue);
    break;
  default:
    BS_FATAL("core_expr: unexpected result type from bsExprEvalObj");
    return BS_ERROR;
  }
  bsExprFreeResult(&r);
  return BS_OK;
}

/*@bsdoc
procedure if {
    usage {if <expr> <true-cond> [elseif <expr> <true-cond> ...] [else
							      <false-cond>]}
    returns {The last result of the evaluated condition, or an empty
	string if no condition is executed.}
    desc {A classical "if" construct.}
}
*/
static int core_if(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: if <expr> <true-clause> [else <false-clause> -or- elseif <expr> ...";
  BSExprResult r;
  int test, k;

  if (objc < 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  bsClearResult(i);
  if (bsExprEvalObj(i,objv[1],&r) != BS_OK)
    return BS_ERROR; /* bad expression */
  test = bsExprBoolean(&r);
  bsExprFreeResult(&r);

  if (test) {
    bsClearResult(i);
    return bsEval(i,objv[2]);
  }
  
  /* check for other clauses */
  k = 3;
  while (k < objc) {
    char *nm = bsObjGetStringPtr(objv[k]);
    if (strcmp(nm,"else") == 0) {
      if ((objc-k) != 2) {
	bsSetStringResult(i,usage,BS_S_STATIC);
	return BS_ERROR;
      }
      return bsEval(i,objv[k+1]);
    } else if (strcmp(nm,"elseif") == 0) {
      if ((objc-k) < 3) {
	bsSetStringResult(i,usage,BS_S_STATIC);
	return BS_ERROR;
      }
      bsClearResult(i);
      if (bsExprEvalObj(i,objv[k+1],&r) != BS_OK)
	return BS_ERROR;
      test = bsExprBoolean(&r);
      if (test) {
	bsClearResult(i);
	return bsEval(i,objv[k+2]);
      }
      k += 3;
    } else {
      bsSetStringResult(i,"invalid clause in if-else-elseif",BS_S_STATIC);
      return BS_ERROR;
    }
  }

  return BS_OK; /* no else clause */
}

/*@bsdoc
procedure while {
    usage {while <expr> <body>}
    desc {A classic "while" construct.  The script <i>body</i> is
	evaluated so long as the infix expression <i>expr</i>
	evaluates to a true value.}
}
*/
static int core_while(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: while <expr> <clause>";
  BSExprResult r;
  int test;
  int k;

  if (objc != 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  bsClearResult(i);
  if (bsExprEvalObj(i,objv[1],&r) != BS_OK)
    return BS_ERROR;
  test = bsExprBoolean(&r);
  bsExprFreeResult(&r);

  while (test) {
    if (((k = bsEval(i,objv[2])) != BS_OK) && (k != BS_CONTINUE)) {
      if (k == BS_BREAK)
	return BS_OK; /* explicit break from loop */
      else
	return k;
    }
    if (bsExprEvalObj(i,objv[1],&r) != BS_OK)
      return BS_ERROR;
    test = bsExprBoolean(&r);
    bsExprFreeResult(&r);
  }
  return BS_OK; /* loop test failed */
}

/*@bsdoc
procedure foreach {
    usage {foreach <var> <list> <body>}
    desc {The script <i>body</i> is evaluated once for each element in
	<i>list</i>, in the order in which the elements appear in
	<i>list</i>.  For each iteration, the variable <i>var</i> is
	set to the current element of <i>list</i> for which
	<i>body</i> is being evaluated.}
}
*/
static int core_foreach(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: foreach <var> <list> <clause>";
  BSList l, *vl;
  BSVariable *v;
  BSObject *o;
  int code;

  /* test... */
  if (objc != 4) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }

  bsClearResult(i);

  /* get variable reference... */;
  if (!(v = bsGetVar(i,NULL,bsObjGetStringPtr(objv[1]),1)) ||
      !(v = bsResolveVar(i,v)))
    return BS_ERROR;

  assert(v->type != BS_V_LINK); /* should no longer be a link */

  if (v->type == BS_V_UNSET) {
    v->o = bsObjNew();
    v->type = BS_V_LOCAL;
  }

  assert(v->type == BS_V_LOCAL);

  if (!(vl = bsObjGetList(i,objv[2])))
    return BS_ERROR;
  /* make a copy of the list, in case somebody tries to change it underfoot */
  bsListInit(&l);
  bsListAppend(&l,vl);

  for (o = l.head; o; o = o->next) {
    bsObjSetCopy(v->o,o);
    bsClearResult(i);
    code = bsEval(i,objv[3]);
    if (code != BS_OK && code != BS_CONTINUE) {
      bsListClear(&l);
      if (code == BS_BREAK)
	return BS_OK;
      else
	return code;
    }
  }
  bsListClear(&l);
  return BS_OK;
}

/*@bsdoc
procedure get {
    usage {get <var>}
    returns {The value of variable <i>var</i> if is set,
	or an empty string otherwise.}
}
*/
static int core_get(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: get <var>";
  BSObject *o;

  if (objc != 2) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if ((o = bsGetObj(i,NULL,objv[1],0)))
    bsSetResult(i,o);
  else
    bsClearResult(i);
  return BS_OK;
}

/*@bsdoc
procedure unset {
    usage {unset <var>}
    returns {An empty string.}
    desc {
	"Unsets" the given variable <i>var</i> if it is set.
	If it is not set, then this procedure call has no effect.
	After it is "unset", it is as though the variable had never
	been set.
    }
}
*/
static int core_unset(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: unset <var>";
  
  if (objc != 2) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  (void) bsUnset(i,NULL,bsObjGetStringPtr(objv[1]));
  bsClearResult(i);
  return BS_OK;
}

/*@bsdoc
procedure set {
    usage {set <var> [<value>]}
    returns {The value of variable <i>var</i>.}
    desc {
	If <i>value</i> is specified, then the value of variable
	<i>var</i> is set to <i>value</i>.  The value of variable
	<i>var</i> is returned if and only if it is set.
    }
}
*/
static int core_set(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: set <var> [<value>]";
  BSObject *o;
  
  bsClearResult(i);

  if (objc != 2 && objc != 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }

  if (objc == 3) {
    /* set it first */
    if (bsSetObj(i,NULL,objv[1],objv[2]))
      return BS_ERROR;
  }
  if (!(o = bsGetObj(i,NULL,objv[1],0)))
    return BS_ERROR;
  bsSetResult(i,o);
  return BS_OK;
}

static int create_proc(BSInterp *i, BSContext *ctx,
		       BSObject *o_name, BSObject *o_argl, BSObject *o_body) {
  int nargs;
  BSList *argl;
  BSProcArg *args = NULL;
  int flags = 0;
  int k = 0;

  /* build arg list */
  if (!(argl = bsObjGetList(i,o_argl)))
    return BS_ERROR;

  nargs = bsListSize(argl);
  if (nargs > 0) {
    /* check for catch-all case */
    BSObject *o;
    o = bsListIndex(argl,nargs-1);
    if (strcmp(bsObjGetStringPtr(o),BS_PROC_CATCHALL_NAME) == 0) {
      flags |= BS_PROC_CATCHALL;
      nargs--;
    }
  }

  if (nargs > 0) {
    args = bsAlloc(nargs*sizeof(BSProcArg),0);
    for (k = 0; k < nargs; k++) {
      BSObject *ao = NULL, *an = NULL, *ad = NULL;
      BSList *l = NULL;
      ao = bsListIndex(argl,k);
      if (!(l = bsObjGetList(i,ao)) || bsListSize(l) <= 1) {
	an = ao; /* simple case */
	ad = NULL;
      } else if (bsListSize(l) > 2) {
	bsSetStringResult(i,"argument element is too long",BS_S_STATIC);
	goto cleanup;
      } else {
	/* argument with default */
	an = bsListIndex(l,0);
	ad = bsListIndex(l,1);
      }
      args[k].name = bsStrdup(bsObjGetStringPtr(an));
      args[k].defvalue = (ad ? bsObjCopy(ad) : NULL);
    }
  }

  bsClearResult(i);
  if (!ctx) {
    if (bsSetScriptProc(i,bsObjGetStringPtr(o_name),nargs,args,o_body,flags))
      goto cleanup;
  } else {
    if (bsSetScriptCProc(i,ctx,bsObjGetStringPtr(o_name),nargs,args,
			 o_body,flags))
      goto cleanup;
  }
  return BS_OK;

 cleanup:
  /* free used memory on error */
  k--;
  while (k >= 0) {
    /* free args... */
    bsFree(args[k].name);
    if (args[k].defvalue)
      bsObjDelete(args[k].defvalue);
    k--;
  }
  bsFree(args);
  return BS_ERROR;
}

/*@bsdoc
procedure proc {
    usage {proc <name> <arglist> <body>}
    desc {
	<p>
	Creates a global procedure.  A global procedure is one that
	can be accessed from any context.  Procedures use a
	different namespace from variables.  Thus it is valid to
	have a procedure and a variable with the same name.  These
	two objects will be unrelated.
	</p>
	<p>
	<i>args</i> specifies the number and names of arguments that
	the procedure takes as a list.  For example an <i>args</i>
	value of:
	</p>
	<div class="eg">
	x y z
	</div>
	<p>
	specifies that the procedure takes three arguments.  
	When the procedure is called, then a new local context is
	created and the three arguments will be stored in the local
	variables <i>x</i>, <i>y</i>, <i>z</i> in the new context.
	</p>
	<p>
	The argument list has two mechanisms for supporing variable
	numbers of arguments.  The first is the specification of
	<i>default values</i>.  For example, the specification of an
	argument list as:
	</p>
	<div class="eg">
	x y {z foo}
	</div>
	<p>
	means that if the third argument (<i>z</i>) is not specified
	in a call to the procedure, then it should take on the
	default value "foo".  Default values can only be defined for
	the right-most arguments.  For example, the following
	definition is invalid:
	</p>
	<div class="eg">
	x {y junk} z
	</div>
	<p>
	However, more than one argument can be given a default value
	so long as, for every argument with a default value, every
	argument to its right must also have a default value.  For
	example, the following:
	</p>
	<div class="eg">
	{x foo} {y bar} {z glue}
	</div>
	<p>
	is a valid way of specifying default values for all
	arguments.
	</p>
	<p>
	The second method is the <i>catch-all</i> argument.  If the
	last argument name in the argument list is <i>args</i> then
	any arguments passed to the procedure that are not consumed
	by other arguments in the list are placed in a local
	variable called <i>args</i>.  In other words, <i>args</i>
	will contain a list of all arguments not handled by other
	arguments (which will be empty if there are no extra
		   arguments).  For example, if a procedure has the following
	argument list:
	</p>
	<div class="eg">
	x y args
	</div>
	<p>
	and the procedure is called with the following arguments:
	</p>
	<div class="eg">
	a b c d e
	</div>
	<p>
	The variable <i>x</i> would be set to "a", the variable
	<i>y</i> would be set to "b" and the variable <i>args</i>
	would be set to the list "c d e".
	</p>
	<p>
	Default values and catch-all arguments can be combined, for
	example: the following argument list is valid:
	</p>
	<div class="eg">
	x {y foo} {z bar} args
	</div>
    }
}
*/
static int core_proc(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: proc <name> <arglist> <body>";

  bsClearResult(i);
  if (objc != 4) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  return create_proc(i,NULL,objv[1],objv[2],objv[3]);
}

/*@bsdoc
procedure cproc {
    usage {cproc <name> <arglist> <body>}
    desc {
	<p>Creates a contextual procedure.  A contextual procedure is
	  like a global procedure except that it is only visible in the
	  current context as well as any nested context (e.g. the inside
	  of a foreach or while loop).  The procedure is removed when
	  the context is destroyed (e.g. the
	  function returns).</p>
	<p>See the <code>proc</code> command below for details on
	  arguments.</p>
    }
}
*/
static int core_cproc(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "usage: cproc <name> <arglist> <body>";
  
  bsClearResult(i);
  if (objc != 4) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  return create_proc(i,i->stack,objv[1],objv[2],objv[3]);
}

/*@bsdoc
procedure global {
    usage {global <var> [<var> ...]}
    desc {
	<p>
	  Declares that variable <i>var</i> is global.  Any references
	  to the variable <i>var</i> after a <code>global</code>
	  command, within the same context or a nested context will
	  refer to the variable <i>var</i> in the global context
	  rather than the local context.  The variable <i>var</i> need
	  not already exist in the global context.  However if it does
	  not exist in the global context it will not be set to any
	  value until it is explicitly set (e.g. via the
	  <code>set</code> command).
	</p>
	<p>
	  Note that the use of <code>global</code> in the middle of a
	  script can have some unintended side effects.  For example,
	  if the following script is part of a procedure body:
	</p>
	<div class="eg">
	  set x 100
	  global x
	  set x 200
	</div>
	<p>
	  Then the first reference to <i>x</i> (before the call to
	  <code>global</code>) refers to <i>x</i> in the local
	  context.  The call to <code>global</code> actually destroys
	  the local variable <i>x</i> - the value is lost and cannot
	  be retrieved.  Also note that there is no way to undo the
	  effects of a <code>global</code> in the current context.
	  In this case, the reference to <i>x</i> will be to the
	  global <i>x</i> until the local context is destroyed.
	</p>
	<p>
	  Calling <code>global</code> while in the global context has
	  no effect.
	</p>
    }
}
*/
static int core_global(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "global <name> [<name> ...]";
  BSVariable *vg, *vl;
  int k;
  if (!i)
    return BS_ERROR;
  if (objc < 1) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  /* if we are in the global context, then this is a no-op */
  if (i->stack == i->global)
    return BS_OK;
  for (k = 1; k < objc; k++) {
    vg = bsGetVar(i,i->global,bsObjGetStringPtr(objv[k]),1);
    vl = bsGetVar(i,NULL,bsObjGetStringPtr(objv[k]),1);
    assert(vg != vl);
    bsVarClear(vl);
    vl->type = BS_V_LINK;
    vl->link = vg;
  }
  return BS_OK;
}

/*@bsdoc
procedure catch {
    usage {catch <body> [<var>]}
    returns {The exception code corresponding to the termination code
	of the given script <i>body</i>.  A termination code of 0
	corresponds to normal termination.  See the description for
	other termination codes.}
    desc {
	<p>
	  Evaluates the given script trapping errors if they occur.
	  If an optional variable name is specified then the result of
	  evaluating the script is stored in the given variable,
	  regardless of whether the script generates a result or an
	  error.  In other words, if the script generates a result,
	  the result is stored in the given variable and if the script
	  generates an error, the error message is stored in the given
	  variable.  If no variable name is specified then the result
	  of evaluating the script is discarded.
	</p>
	<p>
	  This procedure returns the result code generated by
	  evaluting the script, as an integer.  
	  The following result codes are
	  defined:
	<dl>
	  <dt><b>0</b></dt>
	  <dd>OK - indicates that evaluation completed
	    successfully.</dd>
	  <dt><b>1</b></dt>
	  <dd>ERROR - indicates than an error was generated.</dd>
	  <dt><b>2</b></dt>
	  <dd>CONTINUE - indicates that an uncaught "continue" 
	    command was encountered.</dd>
	  <dt><b>3</b></dt>
	  <dd>BREAK - indicates that an uncaught "break" command
	    was encountered.</dd>
	  <dt><b>4</b></dt>
	  <dd>RETURN - indicates that a return command was
	    encoutnered.</dd> 
	</dl>
	  Integers are used so that a Boolean test on the result can
	  be performed to detect abnormal termination.  For example:
	  <div class="eg">if [catch {cmd x y z}] { ... do something on error
	      ... }</div>
	</p>
    }
}
*/
static int core_catch(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "catch <body> [<varname>]";
  int code;
  
  if (objc < 2 || objc > 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }

  bsClearResult(i);
  code = bsEval(i,objv[1]);
  if (objc > 2)
    bsSetObj(i,NULL,objv[2],bsGetResult(i));
  bsSetIntResult(i,code);
  return BS_OK;
}

/*@bsdoc
procedure try {
    usage {try <script> <post>}
    desc {
	<p>
	  Evaluates <i>script</i> until it halts - whether from
	  reaching the end, encountering an exceptional condition
	  (<code>break</code>, <code>continue</code>,
	  <code>return</code>), or encountering an error.  The script
	  <i>post</i> will be executed regardless of how <i>script</i>
	  halts.  The idea is that <i>post</i> can be used to contain
	  "clean-up" code (e.g. closing an open file).  After
	  <i>post</i> is evaluated, the procedure returns as though
	  <i>post</i> had never been called - i.e. with whatever
	  condition came out of <i>script</i>.
	</p>
	<p>
	  Note that if an error is generated while processing
	  <i>post</i>, processing of <i>post</i> will halt and an
	  error will be generated.
	</p>
    }
}
*/
static int core_try(BSInterp *i, int objc, BSObject *objv[], void *cdata) {
  static char *usage = "try <script> <post>";
  int code, xcode;
  BSObject *o;
  
  if (objc != 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  bsClearResult(i);
  code = bsEval(i,objv[1]);
  /* save result */
  o = bsObjCopy(bsGetResult(i));
  bsClearResult(i);
  if ((xcode = bsEval(i,objv[2])) == BS_ERROR)
    return xcode;
  /* restore result */
  bsSetResult(i,o);
  bsObjDelete(o);
  return code;
}

/*@bsdoc
procedure isproc {
    usage {isproc <name>}
    returns {True (1) if <i>name</i> is the name of a procedure in the
	current context or false (0) if there is no such procedure.
    }
}
 */
static int core_isproc(BSInterp *i,
		       int objc, BSObject *objv[],
		       void *cdata) {
  static char *usage = "usage: isproc <name>";

  bsClearResult(i);
  if (objc != 2) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if (!bsObjGetProc(i,objv[1]))
    bsSetIntResult(i,0);
  else
    bsSetIntResult(i,1);
  return BS_OK;
}

/*@bsdoc
procedure isset {
    usage {isset <name>}
    returns {True (1) if <i>name</i> is the name of a variable that
	is set in the current context, or false (0) if there is no such
	variable.}
}
*/
static int core_isset(BSInterp *i,
		      int objc, BSObject *objv[],
		      void *cdata) {
  static char *usage = "usage: isset <name>";

  bsClearResult(i);
  if (objc != 2) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if (!(bsGetObj(i,NULL,objv[1],0)))
    bsSetIntResult(i,0);
  else
    bsSetIntResult(i,1);
  return BS_OK;
}

void bsCoreProcs(BSInterp *i) {
  bsSetExtProc(i,"block",core_block,NULL);
  bsSetExtProc(i,"break",core_break,NULL);
  bsSetExtProc(i,"continue",core_continue,NULL);
  bsSetExtProc(i,"return",core_return,NULL);
  bsSetExtProc(i,"error",core_error,NULL);
  bsSetExtProc(i,"if",core_if,NULL);
  bsSetExtProc(i,"while",core_while,NULL);
  bsSetExtProc(i,"foreach",core_foreach,NULL);
  bsSetExtProc(i,"set",core_set,NULL);
  bsSetExtProc(i,"proc",core_proc,NULL);
  bsSetExtProc(i,"cproc",core_cproc,NULL);
  bsSetExtProc(i,"expr",core_expr,NULL);
  bsSetExtProc(i,"global",core_global,NULL);
  bsSetExtProc(i,"catch",core_catch,NULL);
  bsSetExtProc(i,"try",core_try,NULL);
  bsSetExtProc(i,"get",core_get,NULL);
  bsSetExtProc(i,"unset",core_unset,NULL);
  bsSetExtProc(i,"isset",core_isset,NULL);
  bsSetExtProc(i,"isproc",core_isproc,NULL);
}
