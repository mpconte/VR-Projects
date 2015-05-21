/* provide access to hashes of objects as an opaque */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <bluescript.h>

/*@bsdoc

package hash {
    longname {Hash Procedures}
    desc {Allows for the creation and manipulation of hash tables that
	map strings to arbitrary values or objects.}
    doc {Note that hash objects need not be explicitly destroyed.
	They are automatically destroyed when the last reference to
	them is destroyed.
    }
}

*/
static int hash_makerep(BSObject *o, int rep);
static int hash_destroy(BSObject *o);
static int hash_proc(BSObject *o, BSInterp *i,
		     int objc, BSObject *objv[]);

/*@bsdoc
object hash {
    doc {Hash tables are represented as opaque objects.  This
	means that they are passed by reference and not by value.}
}
*/

static BSOpaqueDriver hash_driver = {
  "hash",
  0,
  hash_makerep,
  hash_destroy,
  hash_proc
};

static BSHash *gethash(BSObject *o) {
  assert(o != NULL);
  assert(o->opaqueRep != NULL);
  assert(o->opaqueRep->driver == &hash_driver);
  assert(o->opaqueRep->data != NULL);
  return (BSHash *)(o->opaqueRep->data);
}

static int hash_destroy(BSObject *o) {
  bsHashDestroy(gethash(o),(void (*)(void *))bsObjDelete);
  return BS_OK;
}

static BSObject *hash_keylist(BSHash *h) {
  BSObject *o;
  BSList *l;
  BSHashWalk w;
  char *s;
  o = bsObjList(0,NULL);
  l = bsObjGetList(NULL,o);
  assert(l != NULL);
  bsHashWalk(&w,h);
  while ((s = bsHashNextKey(&w))) 
    bsListPush(l,bsObjString(s,-1,BS_S_VOLATILE),BS_TAIL);
  return o;
}

static BSObject *hash_valuelist(BSHash *h) {
  BSObject *o;
  BSList *l;
  BSHashWalk w;
  char *s;
  o = bsObjList(0,NULL);
  l = bsObjGetList(NULL,o);
  assert(l != NULL);
  bsHashWalk(&w,h);
  while (bsHashNext(&w) == 0) {
    assert(bsHashObj(&w) != NULL);
    bsListPush(l,bsObjCopy((BSObject *)bsHashObj(&w)),BS_TAIL);
  }
  return o;
}

static BSObject *hash_keyvaluelist(BSHash *h) {
  BSObject *o;
  BSList *l;
  BSHashWalk w;
  char *s;
  o = bsObjList(0,NULL);
  l = bsObjGetList(NULL,o);
  assert(l != NULL);
  bsHashWalk(&w,h);
  while (bsHashNext(&w) == 0) {
    BSObject *ol[2];
    ol[0] = bsObjString(bsHashKey(&w),-1,BS_S_VOLATILE);
    ol[1] = bsObjCopy((BSObject *)bsHashObj(&w));
    bsListPush(l,bsObjList(2,ol),BS_TAIL);
  }
  return o;
}

static int hash_makerep(BSObject *o, int rep) {
  if (rep == BS_T_STRING || rep == BS_T_LIST) {
    BSObject *x;
    x = hash_keyvaluelist(gethash(o));
    if (rep == BS_T_STRING) {
      bsStringCopy(&(o->stringRep),bsObjGetString(x));
    } else if (rep == BS_T_LIST) {
      bsListClear(&(o->listRep));
      bsListAppend(&(o->listRep),&(x->listRep));
    }
    bsObjDelete(x);
    o->repValid |= rep;
    return 0;
  }
  return -1;
}

static BSObject *mkhash(void) {
  BSOpaque *p;
  
  p = bsAllocObj(BSOpaque);
  if (hash_driver.id == 0)
    hash_driver.id = bsUniqueId();
  p->driver = &hash_driver;
  p->data = (void *)bsHashCreate();
  return bsObjOpaque(p);
}

enum {
  H_KEYS,
  H_VALUES,
  H_EXPORT,
  H_IMPORT,
  H_CLEAR,
  H_COPY,
  H_HAS,
  H_SET,
  H_UNSET,
  H_GET
};

static struct {
  char *name;
  int id;
} hash_proc_names[] = {
  { "keys", H_KEYS },
  { "values", H_VALUES },
  { "export", H_EXPORT },
  { "import", H_IMPORT },
  { "clear", H_CLEAR },
  { "copy", H_COPY },
  { "has", H_HAS },
  { "set", H_SET },
  { "unset", H_UNSET },
  { "get", H_GET },
  { NULL, -1 }
};

/*@bsdoc
object hash {
    procedure keys {
	usage {<hash> keys}
	returns {A list of the keys in the hash table.}
	desc {Retrieves a list of the keys in the hash table.  The
	    order in which the keys are returned is arbitrary.}
	example {Retrieving the keys of a hash table} {
	    set h [hash]<br />
	    $h set foo 0<br />
	    $h set bar 1<br />
	    $h set joe 2<br />
	    $h keys
	} {bar joe foo}
    }
    procedure values {
	usage {<hash> values}
	returns {A list of the values in the hash table.}
	desc {Retrieves a list of the values in the hash table.  The
	    order in which the values are returned is arbitrary.}
	example {Retrieving the values of a hash table} {
	    set h [hash]<br />
	    $h set foo 0<br />
	    $h set bar 1<br />
	    $h set joe 2<br />
	    $h values
	} {1 2 0}
    }
    procedure export {
	usage {<hash> export}
	returns {The contents of the hash table as a list.}
	desc {Provides a method for extracting the entire contents of
	    the hash table.  The contents are represented as a list. Each
	    element of the list is itself a list of two elements - a
	    <i>key</i> and a <i>value</i>.  This format can be used to
	    reconstruct the hash table at a later time using the
	    <code>import</code> procedure.}
	example {Retrieving the values of a hash table} {
	    set h [hash]<br />
	    $h set foo 0<br />
	    $h set bar 1<br />
	    $h set joe 2<br />
	    $h export
	} {{bar 1} {joe 2} {foo 0}}
    }
    procedure import {
	usage {<hash> import <list>}
	desc {Adds the given elements in the list to the hash table.
	    Each element of the list must itself be a list of two
	    elements - a <i>key</i> and a <i>value</i>.  That is, the
	    format of <i>list</i> must be the same as the output of
	    the <i>export</i> procedure.  These key-value pairs are
	    merged into the existing contents of the hash table,
	    overriding any existing cases which share the same key.
	    Note that the hash table is not cleared before importing.
	}
	example {Importing a list of key-value pairs} {
	    set h [hash]<br />
	    $h import {{foo 0} {bar 1} {joe 2}}<br />
	    $h export
	} {{bar 1} {joe 2} {foo 0}}
    }
    procedure clear {
	usage {<hash> clear}
	desc {Empties the contents of the hash table.  After calling
	    this procedure, the hash table will be empty.}
	example {Emptying a hash table} {
	    set h [hash]<br />
	    $h set foo 0<br />
	    $h set bar 1<br />
	    $h set joe 2<br />
	    $h clear<br />
	    $h export
	} {}
    }
    procedure copy {
	usage {<hash> copy}
	returns {A new hash object which contains the same key/value
	    mappings as the original.}
	example {Making a copy of a hash table} {
	    set h [hash]<br />
	    $h set foo 0<br />
	    $h set bar 1<br />
	    $h set joe 2<br />
	    set h2 [$h copy]<br />
	    $h2 export
	} {{bar 1} {joe 2} {foo 0}}
	doc {Note that there is an important difference between this
	    operation and simple assignment.  For example:
	    <div class="eg">set h2 $h</div>
	    will create a duplicate <em>reference</em> to the original
	    hash table.  That is, modifying the hash table referred
	    to by <i>$h2</i> will also modify the table referred to by
	    <i>$h</i>.  However, if a copy of the table is mad:
	    <div class="eg">set h2 [$h copy]</div>
	    then modifying the table referred to by <i>$h2</i> will
	    <em>not</em> modify the table referred to by <i>$h</i>.
	}
    }
    procedure has {
	usage {<hash> has <key>}
	returns {True (1) if the hash table contains an entry for the
	    given key, or false (0) if the hash table does not contain
	    an entry for the given key.}
	example {Checking for the existence of keys} {
	    set h [hash]<br />
	    $h set foo 0<br />
	    $h has foo
	} {1}
	example {Checking for non-existent keys} {
	    set h [hash]<br />
	    $h set foo 0<br />
	    $h has bar
	} {0}
    }
    procedure set {
	usage {<hash> set <key> [<value>]}
	returns {The value corresponding to <i>key</i> in the hash
	    table.  If <i>key</i> is not in the hash table, then an
	    error is generated.}
	desc {Creates entries in the hash table if <i>value</i> is
	    specified.  Returns values for keys that exist in the
	    table.}
	example {Create an entry in the hash table} {
	    set h [hash]<br />
	    $h set foo 0
	} {0}
	example {Retrieve a value from the same hash table} {
	    $h set foo
	} {0}
    }
    procedure unset {
	usage {<hash> unset <key>}
	desc {Removes an entry from the table.  If the given key does
	    not exist in the table, then this procedure has no
	    effect.}
	example {Remove an entry from the hash table} {
	    set h [hash]<br />
	    $h set foo 0<br />
	    $h set bar 1<br />
	    $h set joe 2<br />
	    $h unset foo<br />
	    $h export
	} {{bar 1} {joe 2}}
    }
    procedure get {
	usage {<hash> get <key>}
	returns {The value of a key in the table, if it exists, or a
	    blank string otherwise.}
	example {Retrieve a value from the hash table} {
	    set h [hash]<br />
	    $h set foo 0<br />
	    $h get foo
	} {0}
	example {Retrieve a non-existent value from the same hash
	    table} {
		$h get bar
	    } {}
    }
}
*/

static int hash_proc(BSObject *o, BSInterp *i, int objc, BSObject *objv[]) {
  BSHash *h = gethash(o);
  int k;

  if ((k = bsObjGetConstant(i,objv[0],hash_proc_names,
			    sizeof(hash_proc_names[0]),"hash proc")) < 0)
    return BS_ERROR;
  switch (hash_proc_names[k].id) {
  case H_KEYS:
    if (objc != 1) {
      bsSetStringResult(i,"usage: <hash> keys",BS_S_STATIC);
      return BS_ERROR;
    }
    bsSetObjResult(i,hash_keylist(h));
    return BS_OK;

  case H_VALUES:
    if (objc != 1) {
      bsSetStringResult(i,"usage: <hash> values",BS_S_STATIC);
      return BS_ERROR;
    }
    bsSetObjResult(i,hash_valuelist(h));
    return BS_OK;

  case H_EXPORT:
    if (objc != 1) {
      bsSetStringResult(i,"usage: <hash> export",BS_S_STATIC);
      return BS_ERROR;
    }
    bsSetObjResult(i,hash_keyvaluelist(h));
    return BS_OK;
    
  case H_IMPORT:
    if (objc != 2) {
      bsSetStringResult(i,"usage: <hash> import <list>",BS_S_STATIC);
      return BS_ERROR;
    }
    {
      BSObject *x;
      BSList *l,*el;
      if (!(l = bsObjGetList(i,objv[1])))
	return BS_ERROR;
      /* check format before doing anything */
      for (x = l->head; x; x = x->next) {
	if (!(el = bsObjGetList(i,x))) {
	  return BS_ERROR;
	  if (bsListSize(el) != 2) {
	    bsSetStringResult(i,"import: element in list has wrong size",
			      BS_S_STATIC);
	    return BS_ERROR;
	  }
	}
      }
      /* now for real */
      for (x = l->head; x; x = x->next) {
	BSObject *y;
	char *nm;
	el = bsObjGetList(i,x);
	assert(el != NULL);
	nm = bsObjGetStringPtr(bsListIndex(el,0));
	if ((y = bsHashLookup(h,nm)))
	  bsObjDelete(y);
	bsHashInsert(h,nm,bsObjCopy(bsListIndex(el,1)));
      }
      bsObjInvalidate(o,BS_T_OPAQUE);
    }
    return BS_OK; 

  case H_CLEAR:
    if (objc != 1) {
      bsSetStringResult(i,"usage: <hash> clear",BS_S_STATIC);
      return BS_ERROR;
    }
    bsHashClear(h,(void (*)(void *))bsObjDelete);
    bsObjInvalidate(o,BS_T_OPAQUE);
    return BS_OK;

  case H_COPY:
    if (objc != 1) {
      bsSetStringResult(i,"usage: <hash> copy",BS_S_STATIC);
      return BS_ERROR;
    }
    {
      BSHash *h2;
      BSHashWalk w;
      BSObject *x;
      x = mkhash();
      h2 = gethash(x);
      bsHashWalk(&w,h2);
      while (bsHashNext(&w) == 0)
	bsHashInsert(h2,bsHashKey(&w),bsObjCopy(bsHashObj(&w)));
      bsSetObjResult(i,x);
    }
    return BS_OK; /* for now */

  case H_HAS:
    if (objc != 2) {
      bsSetStringResult(i,"usage: <hash> has <key>",BS_S_STATIC);
      return BS_ERROR;
    }
    bsSetIntResult(i,bsHashLookup(h,bsObjGetStringPtr(objv[1])) ? 1 : 0);
    return BS_OK;

  case H_SET:
    { 
      BSObject *x;
      char *nm;
      if (objc != 3 && objc != 2) {
	bsSetStringResult(i,"usage: <hash> set <key> [<value>]",BS_S_STATIC);
	return BS_ERROR;
      }
      nm = bsObjGetStringPtr(objv[1]);
      if (objc == 3) {
	if ((x = bsHashLookup(h,nm)))
	  bsObjDelete(x);
	bsHashInsert(h,nm,bsObjCopy(objv[2]));
	bsObjInvalidate(o,BS_T_OPAQUE);
      }
      if ((x = bsHashLookup(h,nm))) {
	bsSetResult(i,x);
	return BS_OK;
      } else {
	bsClearResult(i);
	bsAppendResult(i,"no such hash key: ",nm,NULL);
	return BS_ERROR;
      }
    }
    break;

  case H_GET:
    {
      BSObject *x;
      if (objc != 2) {
	bsSetStringResult(i,"usage: <hash> get <key>",BS_S_STATIC);
	return BS_ERROR;
      }
      bsClearResult(i);
      if ((x = bsHashLookup(h,bsObjGetStringPtr(objv[1]))))
	bsSetResult(i,x);
    }
    return BS_OK;

  case H_UNSET:
    {
      BSObject *x;
      if (objc != 2) {
	bsSetStringResult(i,"usage: <hash> unset <key>",BS_S_STATIC);
	return BS_ERROR;
      }
      bsClearResult(i);
      if ((x = bsHashLookup(h,bsObjGetStringPtr(objv[1])))) {
	bsSetResult(i,x);
	bsHashDelete(h,bsObjGetStringPtr(objv[1]));
	bsObjInvalidate(o,BS_T_OPAQUE);
      }
    }
    return BS_OK;

  default:
    BS_FATAL("invalid proc id for hash opaque");
  }
}

/*@bsdoc
procedure hash {
    usage {hash}
    returns {A new hash object.}
}
*/

/* creates a hash table */
static int h_hash(BSInterp *i, int objc, BSObject *objv[],
		  void *cdata) {
  if (objc != 1) {
    bsSetStringResult(i,"usage: hash",BS_S_STATIC);
    return BS_ERROR;
  }
  bsSetObjResult(i,mkhash());
  return BS_OK;
}

void bsHashProcs(BSInterp *i) {
  bsSetExtProc(i,"hash",h_hash,NULL);
}

