#ifndef BLUESCRIPT_H
#define BLUESCRIPT_H

#include <stdio.h> /* needed for parsing... */

#ifdef BT_MT
#include <bt_mt.h>
#else
/* place-holders */
#define BS_MUTEX int mutex
#define BS_GLOCK
#define BS_GUNLOCK
#define BS_LOCK(x)
#define BS_UNLOCK(x)
#define BS_MUTEX_INIT(x)
#define BS_COND_INIT(x)
#endif /* BT_MT */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* support pieces (non-language components) */

struct bs_interp;

/* system warnings */
#define BS_WARN(m) bsWarn(__FILE__,__LINE__,(m))
void bsWarn(char *file, int line, char *msg);
#define BS_FATAL(m) bsFatal(__FILE__,__LINE__,(m))
void bsFatal(char *file, int line, char *msg);
int bsShowWarnings(int x);

/* allocation procedures */
void bsOomCallback(void (*cback)(void));
void *bsAlloc(int bytes, int zero);
void *bsRealloc(void *v, int bytes);
#define bsAllocObj(x) bsAlloc(sizeof(x),1)
void bsFree(void *);
char *bsStrdup(char *s);

/** type BSInt
    Used to represent all integer data objects.  Abstracted
    here to allow for some flexibility in the future (e.g.
    enforcing 64-bit ops everywhere).
 */
typedef long BSInt;

/** type BSFloat
    Use to represent all floating-point data objects.  Abstracted
    here to allow for some flexibility in the future.
 */
typedef float BSFloat;

/** type BSId
    An unsigned integer identifier.  Specific size of
    BSId is not specified.
 */
typedef unsigned long BSId;

/** function bsUniqueId
    Returns the value of a monotonically increasing 
    positive counter.
    Never returns the same value twice in the same execution.

    @returns
    An unsigned integer value unique to this execution.
    This function is guaranteed to never return 0 so
    other parts of the system may reserve 0 as an 
    "unassigned" identifier.
 */
BSId bsUniqueId(void);

/* a hash maps strings to void pointers */
#define BS_HASHSIZE 127
typedef struct bs_hash_list {
  struct bs_hash_list *next;
  char *key; /* the string key */
  int slen; /* cached string length */
  void *obj; /* the stored object */
} BSHashList;

typedef struct bs_hash {
  BSHashList *hash[BS_HASHSIZE];
} BSHash;

typedef struct bs_hash_walk {
  BSHash *hash;
  int start;  /* flag - indicates that the walk should be restarted */
  int i;      /* current hash index */
  char *key;  /* current string - becomes invalid if this key is erased */
  void *obj;  /* current object */
  BSHashList *next;   /* next record in walk (NULL if none left) */
} BSHashWalk;

BSHash *bsHashCreate(void);
void bsHashClear(BSHash *, void (*freeproc)(void *));
void bsHashDestroy(BSHash *, void (*freeproc)(void *));

int bsHashExists(BSHash *, char *key);
void *bsHashLookup(BSHash *, char *key);
int bsHashInsert(BSHash *, char *key, void *obj);
void bsHashDelete(BSHash *, char *key);

int bsHashWalk(BSHashWalk *, BSHash *); /* initialize a walk */
int bsHashNext(BSHashWalk *);
#define bsHashKey(w) ((w)?((w)->key):(NULL))
#define bsHashObj(w) ((w)?((w)->obj):(NULL))
#define bsHashNextKey(w) ((bsHashNext(w) == 0)?(bsHashKey(w)):NULL)


#define BS_S_STATIC   (0)
#define BS_S_DYNAMIC  (1)
#define BS_S_VOLATILE (2)

typedef struct bs_string {
  int type; /* current allocation type - should never be BS_S_VOLATILE */
  char *buf;
  int spc; /* space allocated */
  int use; /* space used (length) */
} BSString;

#define bsStringPtr(s) (((s)&&(s)->buf)?(s)->buf:((char *)""))
void bsStringInit(BSString *);
void bsStringSet(BSString *, char *, int sz, int type);
void bsStringAppend(BSString *, char *, int sz);
void bsStringAppQuote(BSString *, char *, int sz);
#define bsStringDynamic(s) bsStringAppend((s),NULL,0)
void bsStringAppChr(BSString *, int);
int bsStringLength(BSString *);
void bsStringClear(BSString *);
void bsStringFreeSpace(BSString *);
void bsStringCopy(BSString *, BSString *);

/** section Lists
    A list is a sequence of objects.  This list is stored as a linked list
    using the <i>next</i> and <i>prev</i> members of the <code>BSObject</code>
    class.  Note that this means that a specific <code>BSObject</code> 
    structure can only belong to a single list.  This is referred to as the 
    "linked list" representation.
    <p>For efficiency, a small list threshold  is defined 
    (macro <code>BS_SMALL_LIST_SZ</code>). 
    The first <code>BS_SMALL_LIST_SZ - 1</code> objects will be cached in an
    array.  The last slot of the array is always <code>NULL</code>).  This
    improves the efficiency of operations on small lists (e.g. retrieving
    a particular index) and for procedure calls, which typically have a limited
    number of arguments.  This is referred to as the "small list" 
    representation.  For lists with more than <code>BS_SMALL_LIST_SZ - 1</code>
    elements, the first <code>BS_SMALL_LIST_SZ - 1</code> elements are 
    cached in the array and the remaining elements are accessed with the 
    <i>next</i> and <i>prev</i> links.
    <p>Because of all of this, it is strongly recommended that modules
    avoid directly modifying lists unless you are prepared to ensure that
    the small list representation remains consistent or you are prepared to
    call <code>bsListRecache()</code> after list operations.
    <p><i>Tweaking</i> - <code>BS_SMALL_LIST_SZ</code> can be adjusted to any 
    non-negative value.
    Values &lt; 2 effectively disable the small list representation.
    However changing it will require that bluescript and any modules that 
    directly modify the list without calling <code>bsListRecache()</code> 
    will need to be recompiled.  Changing this constant changes the size of 
    the <code>BSList</code> structure and thus also the size of the 
    <code>BSObject</code> structure.  If
    a module refers to these types with anything other than a pointer, then it
    will need to be recompiled.  In short:  if you change this value, recompile
    <em>everything</em>.
    <p><i>Tips</i> - pushing and popping is most efficient when done from the
    tail.</p>
 */

#define BS_SMALL_LIST_SZ 8

typedef struct bs_list {
  /** member head
      A pointer to the object at the head of the list.
      <code>NULL</code> implies that the list is empty.
   */
  struct bs_object *head;
  /** member tail
      A pointer to the object at the tail of the list.
      <code>NULL</code> implies that the list is empty.
   */
  struct bs_object *tail;
  /** member objc
      The size of the list (in objects).
   */
  int objc;
  /** member objv
      The "small list" representation.  This is deliberately the
      last member of the structure as its size depends upon the
      <code>BS_SMALL_LIST_SZ</code> constant.
   */
#if BS_SMALL_LIST_SZ >= 2
  struct bs_object *objv[BS_SMALL_LIST_SZ];
#else
  struct bs_object *objv[1]; /* leave space for a NULL value */
#endif /* BS_SMALL_LIST_SZ */
} BSList;

/* headtail:  0 == head, 1 == tail */
#define BS_HEAD (0)
#define BS_TAIL (1)
#define BS_IS_SMALL(l) ((l)->size < BS_SMALL_LIST_SZ)
void bsListPush(BSList *, struct bs_object *, int headtail);
struct bs_object *bsListPop(BSList *, int headtail);
void bsListClear(BSList *);
void bsListInit(BSList *);
int bsListSize(BSList *);
void bsListAppend(BSList *, BSList *);
struct bs_object *bsListIndex(BSList *, int i);

/** function bsListRecache
    A list object maintains a "small list" representation which
    caches up to <code>BS_SMALL_LIST_SZ</code> objects in an array
    representation.  This function ensures that the small list
    representation matches the linked list representation.  Use this
    function if the linked list representation and the <i>objc</i>
    member have been modified, but the <i>objv</i> member has not
    been updated.

    @param l
    The list for which to rebuild the small list cache.
 */
void bsListRecache(BSList *l);

/** function bsListRecalc
    List <code>bsListRecache()</code> except that it also recalculates
    the list size (objc).  Use this when the linked list representation
    has been modified but neither the <i>objc</i> nor the <i>objv</i>
    members have been updated.

    @param l
    The list for which to recalculate the size and rebuild the cache.
 */
void bsListRecalc(BSList *l);

/*** Q:  How do I tell if an Opaque driver is the driver
     that *I* want - guaranteed uniqueness?
     Is uniqueid the right answer? ***/
typedef struct bs_opaque_driver {
  char *name;  /* user-presentable name for opaque object type */
  BSId id;     /* a unique identifier assigned when registered */
  int (*makerep)(struct bs_object *, int rep); /* make other representations */
  int (*destroy)(struct bs_object *);          /* destroy (after all references exhausted) */
  int (*proc)(struct bs_object *, struct bs_interp *, int objc, struct bs_object **objv);  /* generic process handler */
} BSOpaqueDriver;

typedef struct bs_opaque {
  BSOpaqueDriver *driver;
  /* private data (driver-specific) */
  void *data;
  /* garbage collection info */
  BSList links; /* list of all opaques we link to */
  int refcnt;  /* non-object reference count to this opaque */
  int linkcnt; /* number of links (i.e. links from other opaques) */
  int crumb;   /* crumb trail for search */
} BSOpaque;

/* This assumes that refcnt has *not* been added yet... */
int bsOpaqueLink(BSOpaque *p, struct bs_object *o);
int bsOpaqueUnlink(BSOpaque *p, struct bs_object *o);

/* standard opaque functions */
#define BS_OPAQUE_ISA    (0)  /* identify class */
#define BS_OPAQUE_DUP    (1)  /* create a copy of this object */

/* substitution lists */
#define BS_SUBS_STRING   (0)
#define BS_SUBS_VAR      (1)
#define BS_SUBS_PROC     (2)
/* flags */
#define BS_SUBS_NOVARS    (1<<0)
#define BS_SUBS_NOPROCS   (1<<1)
#define BS_SUBS_NOESCAPES (1<<2)

typedef struct bs_subs_elem {
  struct bs_subs_elem *next;
  int type;
  union {
    BSString string;
    BSString var;
    BSList proc;
  } data;
} BSSubsElem;

typedef struct bs_subs_list {
  BSSubsElem *head, *tail;
  int flags;  /* flags we were parsed with */
} BSSubsList;

#define BS_T_STRING (1<<0)     /* unprocessed string */
#define BS_T_LIST   (1<<1)     /* list */
#define BS_T_INT    (1<<2)     /* integer */
#define BS_T_FLOAT  (1<<3)     /* floating-point */
#define BS_T_OPAQUE (1<<4)     /* opaque object */

/* quoting style */
/* even though these are (deliberately) bit masks, they should
   be used as individual values in the BSObject record */
#define BS_Q_NONE      (0)
#define BS_Q_LIST      (1<<1)   /* {} */
#define BS_Q_ELIST     (1<<2)   /* [] */
#define BS_Q_STRING    (1<<3)   /* "" */
#define BS_Q_VARIABLE  (1<<4)   /* $x */
#define BS_Q_QVARIABLE (1<<5)   /* ${x} */

#define BS_Q__ALL      (BS_Q_LIST|BS_Q_ELIST|BS_Q_STRING|BS_Q_VARIABLE)
#define BS_Q__NONE     (0)

/** struct BSCacheDriver */
typedef struct bs_cache_driver {
  char *name;
  BSId id;
  void (*freeproc)(void *priv, void *cdata);
  void *(*copyproc)(void *priv, void *cdata);
  void *cdata;
} BSCacheDriver;

/** struct BSCachedRep
    A cached representation for an object.  Allows various parts of the
    system to cache forms of the object (e.g. a procedure call may cache
    a pointer to the procedure call slot or a variable reference may cache
    a pointer to the variable slot).  Cached representations have the
    following properties:
    <ul>
    <li>They are never <em>authoritative</em>.  This means that they will
    never be used to generate another representation.</li>
    <li>They are expendable.  That is, if the system must dispose of
    a cached representation, it can be rebuilt as necessary.
    </ul>
    The execution engine only needs to know how to dispose of a 
    cached representation.
    It is up to the various modules to worry about how to generate
    cached representations.
    <p>Cached representations are identified by a "unique" identifier
    that must be assigned by the system.  Modules that want to cache
    representations should call <code>bsUniqueId()</code> before 
    creating any representations.
 */
typedef struct bs_cached_rep {
  /** member next
      Cached representations are stored in an object as a
      singly-linked list.  This member points to next element
      in the list (or is <code>NULL</code> if it is the last
      element in the list.
  */
  struct bs_cached_rep *next;
  /** member drv
      Identifies the cache driver for this representation which
      identifies *what* is in the cache as well as how to work
      with it.
   */
  BSCacheDriver *drv;
  /** member priv
      Private data.  This field must be generated by the module
      and will be freed (if so desired) by <i>freeproc</i> below.
   */
  void *priv;
} BSCachedRep;

/** struct BSObject
    An object has one or more <em>fundamental</em> representations.
    An object with no <em>fundamental</em> representations is referred
    to as a <em>null object</em>.  A <em>null object</em> is considered
    to have an empty string or an empty list as its value (as needed).
    <p>In addition to a fundamental representation, an object may have
    0 or more cached representations.  
*/

typedef struct bs_object {
  /* list entry */
  struct bs_object *prev, *next;
  /* is this object quoted? */
  int quote;
  /* representations */
  BSString stringRep;      /* ASCII 8-bit char string */
  BSList listRep;	   /* list */
  BSInt intRep;            /* integer */
  BSFloat floatRep;        /* real number */
  BSOpaque *opaqueRep;     /* opaque object */
  int opaqueLink;          /* non-zero if this is a link rather than a
			      reference */
  BSCachedRep *cachedRep;  /* cached representations */
  /* flags to inidicate what representations are valid */
  int repValid; /* see BS_T_* macros */
} BSObject;

BSObject *bsObjNew(void); /* no representations */
BSObject *bsObjString(char *str, int len, int type);
BSObject *bsObjList(int objc, BSObject *objv[]);
BSObject *bsObjInt(int i);
BSObject *bsObjFloat(float f);
BSObject *bsObjOpaque(BSOpaque *); /* create a new ref to an opaque obj */
BSObject *bsObjCopy(BSObject *); /* make a copy of an object */
void bsObjDelete(BSObject *o);
void bsObjInvalidate(BSObject *o, int mask);
int bsObjIsNull(BSObject *o);

void bsObjClearCache(BSObject *o);
void bsObjAddCache(BSObject *o, BSCacheDriver *drv, void *priv);
int bsObjGetCache(BSObject *o, BSId id, void **priv_r);
void bsObjDelCache(BSObject *o, BSId id);

void bsObjSetString(BSObject *, char *str, int len, int type);
void bsObjSetList(BSObject *, int objc, BSObject *objv[]);
void bsObjSetInt(BSObject *, int i);
void bsObjSetFloat(BSObject *, float f);
void bsObjSetOpaque(BSObject *, BSOpaque *);
void bsObjSetCopy(BSObject *, BSObject *orig);

/* cached value - constant table lookup */
int bsObjGetConstant(struct bs_interp *, BSObject *o, void *table, int offset,
		     char *msg);

#define BS_PROC_NONE   (0)  /* place-holder */
#define BS_PROC_EXT (1)
#define BS_PROC_SCRIPT (2)

typedef int (*BSExtProc)(struct bs_interp *, int objc, 
			 BSObject *objv[], void *);

#define BS_PROC_CATCHALL_NAME "args"
/* proc flags */
#define BS_PROC_CATCHALL (1<<0)     /* CATCHALL_NAME consumes remaining arguments */
typedef struct bs_proc_arg {
  char *name;
  BSObject *defvalue; /* default value,
			 NULL implies no default */
} BSProcArg;

typedef struct bs_proc {
  int type;
  union {
    struct {
      int nargs;
      BSProcArg *args;
      BSObject *body;
      int flags;
    } script;
    struct {
      BSExtProc proc;
      void *cdata;
    } ext;
  } data;
} BSProc;

typedef struct bs_context {
  struct bs_context *up;     /* points to higher context it not nested
				- opaque link */
  struct bs_context *left;   /* points to parent if nested
				- transparent link */
  BSHash *vars;     /* variables in this context */
  int refcnt;       /* how many threads are using this context? */
  BSProc *unknown;  /* handler for unknown procedures in this context */
  BSHash *cproctable;  /* context procedures */
  BS_MUTEX;
} BSContext;

/* grow/shrink vertically */
BSContext *bsContextPush(BSContext *stack);
BSContext *bsContextPop(BSContext *stack);

/* increase ref count */
BSContext *bsContextLink(BSContext *stack);
/* decreate ref count */
void bsContextUnlink(BSContext *stack);

/* grow/shrink horizontally */
BSContext *bsContextNest(BSContext *stack);
BSContext *bsContextUnnest(BSContext *stack);

/* interpreter */
typedef struct bs_interp {
  BSContext *global;     /* global context */
  /* The "top-of-stack" is really thread-specific and will need to
     change if multiple threads are to co-exist in a single
     interpreter */
  BSContext *stack;      /* top of context stack */
  BSHash *proctable;     /* table of procedures */
  /* "result" of current work */
  /* This is also thread-specific */
  BSObject *result;
  int opt;  /* optimization flags */
} BSInterp;

#define BS_OPT_MEMORY (1<<0)

int bsProcInit(BSProc *);
void bsProcClear(BSProc *);
BSProc *bsProcExt(BSProc *, BSExtProc proc, void *cdata);
BSProc *bsProcScript(BSProc *, int nargs, BSProcArg *args,
		     BSObject *body, int flags);
BSProc *bsProcAlloc(void);
void bsProcDestroy(BSProc *);

/* Define a variable as a different kind of object to allow
   more complex linking in the future than just storage */
#define BS_V_UNSET    (0)   /* indicates a variable that does not actually
			       exist (record is here just as a placeholder) */
#define BS_V_LOCAL    (1)   /* a variable that is implemented locally
			       (no special treatment) */
#define BS_V_LINK     (2)   /* no data here - just a link to another stack */
/* A link *must* be to a variable that is in
   a *parent* context (otherwise the variable could get destroyed while we are
   pointing at it, or we could end up with a reference loop) */

typedef struct bs_variable {
  int type;
  BSObject *o;      /* storage - may be real value, may be cached value */
  /* future expansion goes here */
  struct bs_variable *link; /* linked variable */
} BSVariable;

BSInterp *bsInterpCreate(void);
void bsInterpDestroy(BSInterp *);
void bsInterpOptSet(BSInterp *, int flags, int val);

/* handy context support */
void bsInterpPush(BSInterp *);
void bsInterpPop(BSInterp *);
void bsInterpNest(BSInterp *);
void bsInterpUnnest(BSInterp *);

/* variable control */
BSVariable *bsVarCreate(void);
BSVariable *bsVarLink(BSContext *ctx, char *name);
void bsVarClear(BSVariable *);
void bsVarFree(BSVariable *);
BSVariable *bsGetVar(BSInterp *, BSContext *ctx, char *name, int force);
BSVariable *bsResolveVar(BSInterp *, BSVariable *);
int bsSetVar(BSInterp *, BSContext *ctx, char *name, BSVariable *var);
int bsSet(BSInterp *, BSContext *ctx, char *name, BSObject *value);
int bsUnset(BSInterp *, BSContext *ctx, char *name);
BSObject *bsGet(BSInterp *, BSContext *ctx, char *name, int force);

/* ??? */
int bsSetObj(BSInterp *, BSContext *ctx, BSObject *, BSObject *);
BSObject *bsGetObj(BSInterp *, BSContext *ctx, BSObject *, int force);

/* unknown procedure handler */
int bsSetExtUnknown(BSInterp *, BSContext *, BSExtProc proc, void *cdata);
int bsSetScriptUnknown(BSInterp *, BSContext *, int nargs, BSProcArg *args,
		       BSObject *body, int flags);
int bsSetExtCProc(BSInterp *, BSContext *, char *name, 
		  BSExtProc proc, void *cdata);
int bsSetScriptCProc(BSInterp *, BSContext *, char *name,
		     int nargs, BSProcArg *args,
		     BSObject *body, int flags);
void bsUnsetUnknown(BSInterp *, BSContext *);
int bsUnsetCProc(BSInterp *, BSContext *, char *name);
BSProc *bsGetUnknown(BSInterp *, BSContext *);
BSProc *bsGetCProc(BSInterp *, BSContext *, char *name, int force);

/* procedures */
int bsSetExtProc(BSInterp *, char *name, BSExtProc proc, void *cdata);
int bsSetScriptProc(BSInterp *, char *name, int nargs, BSProcArg *args, 
		    BSObject *body, int flags);
int bsUnsetProc(BSInterp *, char *name);
BSProc *bsGetProc(BSInterp *, char *name, int force);
BSProc *bsResolveProc(BSInterp *, BSContext *, char *name);
BSProc *bsGetProcFromHash(BSHash *, char *name, int force);
int bsCallProc(BSInterp *interp, BSProc *proc, int objc, BSObject **objv);
int bsProcessLine(BSInterp *interp, BSList *line);

int bsSetResult(BSInterp *, BSObject *);
/* Like bsSetResult, except that it explicitly does *not* copy 
   the object, but instead reuses the given pointer (which should
   not be referenced elsewhere */
int bsSetObjResult(BSInterp *, BSObject *);
int bsSetStringResult(BSInterp *, char *, int type);
int bsSetIntResult(BSInterp *, BSInt x);
int bsSetFloatResult(BSInterp *, BSFloat x);
int bsClearResult(BSInterp *); /* resets result to empty string 
				  (null object) */
int bsIsResultClear(BSInterp *);
/* append strings - last arg must be NULL */
int bsAppendResult(BSInterp *, ...);
BSObject *bsGetResult(BSInterp *i);

/* conversion routines */
/* string conversion is special - it is mandated that it must
   always succeed */
BSString *bsObjGetString(BSObject *);
char *bsObjGetStringPtr(BSObject *);
/* other conversion routines may fail and should use BSInterp's
   result (if specified) to declare *why* they failed */
BSList *bsObjGetList(BSInterp *, BSObject *);
int bsObjGetInt(BSInterp *, BSObject *, BSInt *);
int bsObjGetFloat(BSInterp *, BSObject *, BSFloat *);
BSProc *bsObjGetProc(BSInterp *, BSObject *);
BSProc *bsObjGetProcFromHash(BSHash *, BSObject *, int force);
void bsObjCacheProc(BSObject *, BSProc *);
/* there is no "GetOpaque" since there is no way to convert
   from other types to an opaque type */

/* result codes */
#define BS_OK       (0)
#define BS_ERROR    (1)
#define BS_CONTINUE (2)
#define BS_BREAK    (3)
#define BS_RETURN   (4)

char *bsCodeToString(int code);

#define BS_EOF    (-1)
#define BS_SERROR (-2)  /* stream error */
#define BS_UNGET_DEPTH 2
#define BS_P_STRING 0
#define BS_P_FILE   1
typedef struct bs_parse_source {
  int type;
  union {
    char *string;
    FILE *file;
  } data;
  int lineno;
  int charno;
  int eol;
  int ungotten[BS_UNGET_DEPTH];
} BSParseSource;

BSParseSource *bsStringSource(BSParseSource *, char *);
BSParseSource *bsFileSource(BSParseSource *, FILE *);
void bsSkipSpace(BSParseSource *);
int bsGetc(BSParseSource *);
void bsUngetc(BSParseSource *, int c);
int bsEof(BSParseSource *);

int bsParseList(BSInterp *, BSParseSource *, BSList *);
int bsParseScript(BSInterp *, BSParseSource *, BSList *);
int bsScriptComplete(char *str);

BSObject *bsParseVariable(BSInterp *, BSParseSource *);
BSObject *bsParseQList(BSInterp *, BSParseSource *);
BSObject *bsParseEList(BSInterp *, BSParseSource *);
BSObject *bsParseString(BSInterp *, BSParseSource *);
BSObject *bsParseName(BSInterp *, BSParseSource *);

/* special characters */
#define BS_C_LISTOPEN '{'
#define BS_C_LISTCLOSE '}'
#define BS_C_ELISTOPEN '['
#define BS_C_ELISTCLOSE ']'
#define BS_C_STRINGOPEN '"'
#define BS_C_STRINGCLOSE '"'
#define BS_C_VARIABLE '$'
#define BS_C_ESCAPE '\\'
#define BS_C_ISSPECIAL(c) ((c) == BS_C_LISTOPEN || \
			   (c) == BS_C_LISTCLOSE || \
			   (c) == BS_C_ELISTOPEN || \
			   (c) == BS_C_ELISTCLOSE || \
			   (c) == BS_C_STRINGOPEN || \
			   (c) == BS_C_STRINGCLOSE || \
			   (c) == BS_C_VARIABLE || \
			   (c) == BS_C_ESCAPE)

int bsSubsIsStatic(char *s);
int bsSubsParse(BSInterp *i, BSSubsList *, char *s, int flags);
int bsSubsList(BSInterp *i, BSString *dst, BSSubsList *l);
void bsSubsInit(BSSubsList *);
void bsSubsClear(BSSubsList *);
void bsSubsCopy(BSSubsList *dst, BSSubsList *src);
int bsSubs(BSInterp *i, BSString *dst, char *s, int flags);
int bsSubsObj(BSInterp *i, BSString *dst, BSObject *o, int flags);
void bsSubsElemFree(BSSubsElem *);

BSList *bsGetScript(BSInterp *i, BSObject *o, int *mustfree_r);

int bsEval(BSInterp *, BSObject *);
int bsEvalSource(BSInterp *, BSParseSource *);
int bsEvalString(BSInterp *, char *);
int bsEvalStream(BSInterp *, FILE *);
int bsEvalFile(BSInterp *, char *filename);

/* infix expression engine */
/* 
   Expressions operate on:  strings, ints, floats
*/
/* an expression result is like a strongly-typed object */
typedef struct bs_expr_result {
  int type;
  union {
    BSString stringValue;
    BSInt intValue;
    BSFloat floatValue;
  } data;
} BSExprResult;

typedef void *BSParsedExpr; /* handle to parsed expression */
void bsExprFreeResult(BSExprResult *r);
void bsExprFree(BSParsedExpr *);
BSParsedExpr *bsExprParse(BSInterp *i, BSString *s);
int bsExprEval(BSInterp *i, BSParsedExpr *, BSExprResult *);
int bsExprEvalObj(BSInterp *i, BSObject *, BSExprResult *); /* quoting 
							       counts */
int bsExprBoolean(BSExprResult *r); /* get a boolean result */
char *bsExprString(BSExprResult *r);
int bsExprFloat(BSInterp *i, BSExprResult *r, BSFloat *f);
int bsExprInt(BSInterp *i, BSExprResult *r, BSInt *x);

void bsCoreProcs(BSInterp *);

/* create an interpreter with "standard" functions */
BSInterp *bsInterpCreateStd(void);

/* stream operations */
/* This is deliberately kept *simple* */
struct bs_stream;

#define BS_POLL_READ  (0)
#define BS_POLL_WRITE (1)

typedef struct bs_stream_drv {
  int (*close)(BSInterp *, struct bs_stream *);
  int (*read)(BSInterp *, struct bs_stream *, void *buf, int nbytes);
  int (*write)(BSInterp *, struct bs_stream *, void *buf, int nbytes);
  int (*flush)(BSInterp *, struct bs_stream *);
  int (*poll)(BSInterp *, struct bs_stream *, int flags);
} BSStreamDriver;

#define BS_SBUFSZ 128
typedef struct bs_stream {
  BSStreamDriver *drv;  /* the driver */
  void *priv;         /* private */
  /* buffer (used when readln called) */
  char buf[BS_SBUFSZ];
  int use,top; /* pointers into buffer */
} BSStream;

/* stream operations */
int bsStreamPoll(BSInterp *i, BSStream *, int flags);
int bsStreamClose(BSInterp *i, BSStream *);
int bsStreamRead(BSInterp *i, BSStream *, void *buf, int nbytes, int exact);
int bsStreamWrite(BSInterp *i, BSStream *, void *buf, int nbytes, int exact);
int bsStreamFlush(BSInterp *i, BSStream *);
/* append line to given string */
int bsStreamGetc(BSInterp *i, BSStream *);
int bsStreamReadLn(BSInterp *i, BSStream *, BSString *);
/* write string with optional 'nl' to given stream */
int bsStreamWriteStr(BSInterp *i, BSStream *, char *, int nl);

/* makes an initialized (but driverless) stream object */
BSStream *bsStreamMake(void);
BSStream *bsStreamGet(BSInterp *i, BSObject *o);

BSObject *bsStreamMakeOpaque(BSStream *);

/* special built-in Standard-C based stream implementations */
BSStream *bsStream_File(char *fname, char *mode);
void bsStream_FileProcs(BSInterp *);
BSStream *bsStream_Stdio(FILE *read_src, FILE *write_dst);
void bsStream_StdioProcs(BSInterp *);
void bsStream_ExecProcs(BSInterp *); /* popen */

/* null stream, does nothing (generates no input, discards all output */
BSStream *bsStream_Null(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* BLUESCRIPT_H */
