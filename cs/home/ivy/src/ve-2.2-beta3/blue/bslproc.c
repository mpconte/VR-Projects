/* procedures for working with lists */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <bluescript.h>

/*@bsdoc
  
package list {
    longname {List Procedures}
    desc {Procedures for creating and manipulating lists.}
    doc {Many of the procedures in this package allow for
	the specification of an <em>index</em> into a string.
	An index can have any of the following formats:
	<ul>
	<li><i>n</i> - where <i>n</i> is an integer representing an
	offset into the list.  The first element in the list is index
	0, the second element is 1, etc.</li>
	<li><code>end</code> - indicating the last element in the list.</li>
	<li><code>end-</code><i>n</i> - indicating the
	last-but-<i>n</i>'th element of the list.  For example,
	<code>end-0</code> is equivalent to <code>end</code> while
	<code>end-1</code> is the second-last element of the list.</li>
	</ul>
	All elements outside of a list's valid elements are
	assumed to be empty strings.  For example, "lindex $l -1" will
	always return an empty string.  However, if a request is made
	for a range of elements that extends outside of the list then
	the result is trimmed to be just those elements that are part
	of the actual list.  For example, "lrange $l -3 5" has the
	same result as "lrange $l 0 5".  If "$l" has only 3 elements,
	then "lrange $l 0 5" has the same result as "lrange $l 0 2".
    }
}

*/

static int get_index(BSInterp *i, BSList *l, BSObject *o, int *k) {
  BSInt x;
  char *s;
  bsClearResult(i);
  if (bsObjGetInt(i,o,&x) == BS_OK) {
    if (k)
      *k = x;
    return BS_OK;
  } else if (l) {
    bsClearResult(i);
    /* non-numeric cases */
    s = bsObjGetStringPtr(o);
    if (strcmp(s,"end") == 0) {
      if (k)
	*k = bsListSize(l)-1;
    } else if (strncmp(s,"end-",4) == 0) {
      int xx;
      if (sscanf(s+4,"%d",&xx) != 1) {
	bsSetStringResult(i,"invalid index - no integer after 'end-'",
			  BS_S_STATIC);
	return BS_ERROR;
      }
      if (k)
	*k = bsListSize(l)-1-xx;
    } else {
      bsAppendResult(i,"invalid index: ",s,NULL);
      return BS_ERROR;
    }
  } else {
    bsAppendResult(i,"invalid index: ",s,NULL);
    return BS_ERROR;
  }
  if (l && k) {
    /* corral value... */
    if ((*k) > bsListSize(l)-1)
      (*k) = bsListSize(l)-1;
    if ((*k) < 0)
      (*k) = 0;
  }
  return BS_OK;
}

/*@bsdoc
procedure lindex {
    usage {lindex <list> <i>}
    returns {The element located at index <i>i</i> in <i>list</i>.}
    example {Retrieve the third element in the list {a b c}.} {lindex
	{a b c} 2} {c}
}
*/
static int lp_lindex(BSInterp *i, int objc,
		     BSObject *objv[], void *cdata) {
  static char *usage = "usage: lindex <list> <index>";
  BSList *l;
  int ind;

  if (objc != 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  bsClearResult(i);
  if (!(l = bsObjGetList(i,objv[1])))
    return BS_ERROR;
  if (get_index(i,l,objv[2],&ind))
    return BS_ERROR;
  bsSetResult(i,bsListIndex(l,ind));
  return BS_OK;
}

/*@bsdoc
procedure lrange {
    usage {lrange <list> <start> <finish>}
    returns {A list containing the elements of <i>list</i> between indices
	<i>start</i> and <i>finish</i> inclusive.}
    example {Return the first three elements of the list {a b c d e
	f}} {lrange {a b c d e f} 0 2} {a b c}
    example {Return the last 2 elements of the list {a b c d e f}} \
	{lrange {a b c d e f} end-1 end} {e f}
}
*/
static int lp_lrange(BSInterp *i, int objc,
		     BSObject *objv[], void *cdata) {
  static char *usage = "usage: lrange <list> <start> <finish>";
  BSList *l, *sl;
  BSObject *o;
  int x,y;
  
  if (objc != 4) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  bsClearResult(i);
  if (!(l = bsObjGetList(i,objv[1])))
    return BS_ERROR;
  if (get_index(i,l,objv[2],&x) ||
      get_index(i,l,objv[3],&y))
    return BS_ERROR;
  bsClearResult(i);
  o = bsGetResult(i);
  sl = bsObjGetList(i,o);
  assert(sl != NULL);

  while (x <= y) {
    if ((o = bsListIndex(l,x)))
      bsListPush(sl,bsObjCopy(o),BS_TAIL);
    x++;
  }

  return BS_OK;
}

/*@bsdoc
procedure lreplace {
    usage {lreplace <list> <start> <finish> [<elem> ...]}
    returns {A modified copy of <i>list</i>.}
    doc {Creates a copy of <i>list</i>
	with the range of elements between <i>start</i> and
	<i>finish</i> inclusive replaced with the new elements
	(<i>elem</i> ...) specified on the command line.  If no
	new elements are specified, then the returned copy of
	<i>list</i> will merely have the elements between <i>start</i>
	and <i>finish</i> inclusive, deleted.
    }
    example {Replace the third element of {a b c d} with {e f}} \
	{lreplace {a b c d} 2 2 e f} {a b e f d}
    example {Remove the first two elements of {a b c d e f}} {lreplace
	{a b c d e f} 0 1} {c d e f}
}
*/
static int lp_lreplace(BSInterp *i, int objc,
		       BSObject *objv[], void *cdata) {
  static char *usage = "usage: lreplace <list> <start> <finish> [<elem> ...]";
  BSList *l, *sl;
  BSObject *o;
  int x,y,k,n;
  
  if (objc < 4) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  bsClearResult(i);
  if (!(l = bsObjGetList(i,objv[1])))
    return BS_ERROR;
  if (get_index(i,l,objv[2],&x) ||
      get_index(i,l,objv[3],&y))
    return BS_ERROR;
  bsClearResult(i);
  sl = bsObjGetList(i,bsGetResult(i));
  assert(sl != NULL);
  for (k = 0; k < x; k++) {
    if (o = bsListIndex(l,k))
      bsListPush(l,bsObjCopy(o),BS_TAIL);
  }
  for (k = 4; k < objc; k++)
    bsListPush(l,bsObjCopy(objv[k]),BS_TAIL);
  k = (y >= x ? y : x) + 1;
  n = bsListSize(l);
  while (k < n) {
    if (o = bsListIndex(l,k))
      bsListPush(l,bsObjCopy(o),BS_TAIL);
    n++;
  }
  return BS_OK;
}

/*@bsdoc
procedure llength {
    usage {llength <list>}
    returns {The number of elements in the list.}
    example {Getting the length of a list} {llength {a b c d}} 4
}
*/
static int lp_llength(BSInterp *i, int objc,
		      BSObject *objv[], void *cdata) {
  static char *usage = "usage: llength <list>";
  BSList *l;

  if (objc != 2) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  bsClearResult(i);
  if (!(l = bsObjGetList(i,objv[1])))
    return BS_ERROR;
  bsSetIntResult(i,bsListSize(l));
  return BS_OK;
}

/* note: push/pop/shift/unshift modify a list in place, so
   we need to invalidate the object's other representations */

/*@bsdoc
procedure lpush {
    usage {lpush <var> <elem>}
    example {Pushing an element} {set x {a b c}<br />
	lpush x d<br />
	set x} {a b c d}
}
*/
static int lp_lpush(BSInterp *i, int objc, BSObject *objv[],
		    void *cdata) {
  static char *usage = "usage: lpush <list> <elem>";
  BSObject *o;
  BSList *l;

  if (objc != 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  bsClearResult(i);
  if (!(o = bsGetObj(i,NULL,objv[1],0)))
    return BS_ERROR;
  if (!(l = bsObjGetList(i,o)))
    return BS_ERROR;
  bsListPush(l,bsObjCopy(objv[2]),BS_TAIL);
  bsObjInvalidate(o,BS_T_LIST);
  return BS_OK;
}

/*@bsdoc
procedure lunshift {
    usage {lunshift <var> <elem>}
    example {Unshifting an element} {set x {a b c}<br />
	lunshift x d<br />
	set x} {d a b c}
}
*/
static int lp_lunshift(BSInterp *i, int objc, BSObject *objv[],
		       void *cdata) {
  static char *usage = "usage: lunshift <list> <elem>";
  BSList *l;
  BSObject *o;

  if (objc != 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  bsClearResult(i);
  if (!(o = bsGetObj(i,NULL,objv[1],0)))
    return BS_ERROR;
  if (!(l = bsObjGetList(i,o)))
    return BS_ERROR;
  bsListPush(l,bsObjCopy(objv[2]),BS_HEAD);
  bsObjInvalidate(o,BS_T_LIST);
  return BS_OK;
}

/*@bsdoc
procedure lpop {
    usage {lpop <var>}
    example {Popping an element} {set x {a b c}<br />
	lpop x} {c}
    example {List after popping} {set x} {a b}
}
*/
static int lp_lpop(BSInterp *i, int objc, BSObject *objv[],
		   void *cdata) {
  static char *usage = "usage: lpop <list>";
  BSList *l;
  BSObject *o, *lo;
  
  if (objc != 2) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  bsClearResult(i);
  if (!(o = bsGetObj(i,NULL,objv[1],0)))
    return BS_ERROR;
  if (!(l = bsObjGetList(i,o)))
    return BS_ERROR;
  lo = bsListPop(l,BS_TAIL);
  bsObjInvalidate(o,BS_T_LIST);
  bsSetResult(i,lo);
  if (lo)
    bsObjDelete(lo);
  return BS_OK;
}

/*@bsdoc
procedure lshift {
    usage {lshift <var>}
    example {Shifting an element} {set x {a b c}<br />
	lshift x} {a}
    example {List after shifting} {set x} {b c}
}
*/
static int lp_lshift(BSInterp *i, int objc, BSObject *objv[],
		     void *cdata) {
  static char *usage = "usage: lshift <list>";
  BSList *l;
  BSObject *o, *lo;
  
  if (objc != 2) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  bsClearResult(i);
  if (!(o = bsGetObj(i,NULL,objv[1],0)))
    return BS_ERROR;
  if (!(l = bsObjGetList(i,o)))
    return BS_ERROR;
  lo = bsListPop(l,BS_HEAD);
  bsObjInvalidate(o,BS_T_LIST);
  bsSetResult(i,lo);
  if (lo)
    bsObjDelete(lo);
  return BS_OK;
}

/*@bsdoc
procedure lempty {
    usage {lempty <list>}
    example {Testing an empty list} {lempty {}} 1
    example {Testing a non-empty list} {lempty {a b c}} 0
}
*/
static int lp_lempty(BSInterp *i, int objc, BSObject *objv[],
		     void *cdata) {
  static char *usage = "usage: lempty <list>";
  BSList *l;
  BSObject *o;

  if (objc != 2) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  bsClearResult(i);
  if (!(l = bsObjGetList(i,objv[1])))
    return BS_ERROR;
  bsSetIntResult(i,(bsListSize(l)) ? 0 : 1);
  return BS_OK;
}

/*@bsdoc
procedure lconcat {
    usage {lconcat [<list> ...]}
    example {Concatenating two lists} {lconcat {a b c} {d e f}} {a b c
	d e f}
}
*/
static int lp_lconcat(BSInterp *i, int objc, BSObject *objv[],
		      void *cdata) {
  static char *usage = "usage: lconcat [<list> ...]";
  BSList *l, *ll;
  BSObject *o, *ol;
  int k;

  o = bsObjList(0,NULL);
  l = bsObjGetList(NULL,o);
  for (k = 1; k < objc; k++) {
    if (!(ll = bsObjGetList(i,objv[k]))) {
      bsObjDelete(o);
      return BS_ERROR;
    }
    for (ol = ll->head; ol; ol = ol->next)
      bsListPush(l,bsObjCopy(ol),BS_TAIL);
  }
  bsSetResult(i,o);
  return BS_OK;
}

/*@bsdoc
procedure list {
    usage {list [<elem> ...]}
    example {Creating an empty list} {list}
    example {Creating a list of three elements} {list a b c} {a b c}
    doc {Generally, creating a list via the <code>list</code> command
	is safer and faster than explicitly creating a string
	(e.g. through quote substitution) and then allowing the engine
	to parse the generated string.
    }
}
*/
static int lp_list(BSInterp *i, int objc, BSObject *objv[],
		   void *cdata) {
  static char *usage = "usage: list [<elem> ...]";
  BSObject *o;
  int k;
  BSList *l;
  o = bsObjList(0,NULL);
  l = bsObjGetList(NULL,o);
  assert(l != NULL);
  for (k = 1; k < objc; k++)
    bsListPush(l,bsObjCopy(objv[k]),BS_TAIL);
  bsSetObjResult(i,o);
  return BS_OK;
}

void bsListProcs(BSInterp *i) {
  bsSetExtProc(i,"lindex",lp_lindex,NULL);
  bsSetExtProc(i,"lrange",lp_lrange,NULL);
  bsSetExtProc(i,"lreplace",lp_lreplace,NULL);
  bsSetExtProc(i,"llength",lp_llength,NULL);
  bsSetExtProc(i,"lpush",lp_lpush,NULL);
  bsSetExtProc(i,"lpop",lp_lpop,NULL);
  bsSetExtProc(i,"lshift",lp_lshift,NULL);
  bsSetExtProc(i,"lunshift",lp_lunshift,NULL);
  bsSetExtProc(i,"lempty",lp_lempty,NULL);
  bsSetExtProc(i,"lconcat",lp_lconcat,NULL);
  bsSetExtProc(i,"list",lp_list,NULL);
}
