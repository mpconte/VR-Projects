/* procedures for working with strings */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <bluescript.h>

/*@bsdoc

package string {
    longname {String Procedures}
    desc {Procedures for manipulating strings.}
}

 */

#define TRIM_LEFT (1<<0)
#define TRIM_RIGHT (1<<1)

/*@bsdoc
procedure strtrim {
    usage {strtrim <string> [<trimchars>]}
    returns {The string with any character in <i>trimchars</i> removed
	from the left and the right.}
    doc {Trims unwanted characters from the left and right of a
	string.  If <i>trimchars</i> is not specified, then it
	defaults to the set of white-space characters.
    }
    example {Trimming a string} {strtrim
	{&nbsp;&nbsp;foo&nbsp;&nbsp;}} {foo}
}

procedure strltrim {
    usage {strltrim <string> [<trimchars>]}
    returns {The string with any character in <i>trimchars</i> removed
	from the left of the string.}
    doc {Trims unwanted characters from the left of a
	string.  If <i>trimchars</i> is not specified, then it
	defaults to the set of white-space characters.
    }
    example {Left-trimming a string} {strtrim
	{&nbsp;&nbsp;foo&nbsp;&nbsp;}} {foo&nbsp;&nbsp;}
}

procedure strrtrim {
    usage {strrtrim <string> [<trimchars>]}
    returns {The string with any character in <i>trimchars</i> removed
	from the right of the string.}
    doc {Trims unwanted characters from the right of a
	string.  If <i>trimchars</i> is not specified, then it
	defaults to the set of white-space characters.
    }
    example {Right-trimming a string} {strtrim
	{&nbsp;&nbsp;foo&nbsp;&nbsp;}} {&nbsp;&nbsp;foo}
}

*/
static int sp_strtrim(BSInterp *i, int objc,
		      BSObject *objv[], void *cdata) {
  char *trimchars = " \r\t\n\v";
  int flags = (int)(cdata);
  char *s, *e;
  if (objc != 2 && objc != 3) {
    bsClearResult(i);
    bsAppendResult(i,"usage: ",bsObjGetStringPtr(objv[0])," <string> [<trimchars>]",NULL);
    return BS_ERROR;
  }
  if (objc >= 3)
    trimchars = bsObjGetStringPtr(objv[2]);
  s = bsObjGetStringPtr(objv[1]);
  if (flags & TRIM_LEFT) {
    while (*s && strchr(trimchars,*s))
      s++;
  }
  s = bsStrdup(s);
  e = (s + strlen(s) - 1);
  if (flags & TRIM_RIGHT) {
    while ((e >= s) && strchr(trimchars,*e)) {
      *e = '\0';
      e--;
    }
  }
  bsSetStringResult(i,s,BS_S_DYNAMIC);
  return BS_OK;
}

/*@bsdoc

procedure strlen {
    usage {strlen <string>}
    returns {The length of <i>string</i> in characters.}
    example {Determining the length of a string} {strlen "abcdefg"} 7
}

*/
static int sp_strlen(BSInterp *i, int objc,
		     BSObject *objv[], void *cdata) {
  static char *usage = "usage: strlen <string>";
  BSString *str;
 
  if (objc != 2) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  bsSetIntResult(i,bsStringLength(bsObjGetString(objv[1])));
  return BS_OK;
}

/*@bsdoc
procedure strstr {
    usage {strstr <string> <sub>}
    returns {The offset of the first occurrence of string <i>sub</i>
	in <i>string</i> or -1 if the string cannot be found.}
    doc {
	Finds the location of a string (<i>sub</i>) within another
	string (<i>string</i>).  If the string is found, the offset of
	the first occurrence (i.e. the left-most occurrence) is
	returned.  If the sub-string occurs at the beginning of the
	string, then the offset is 0, if it occurs at the second
	character, the offset is 1, etc.
    }
    example {Finding a substring} {strstr foobar bar} 3
    example {Searching for a non-existent substring} {strstr foobar
	bug} -1

}
*/
static int sp_strstr(BSInterp *i, int objc,
		     BSObject *objv[], void *cdata) {
  static char *usage = "usage: strstr <string> <sub>";
  BSString *str, *x;
  char *ptr;

  if (objc != 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  str = bsObjGetString(objv[1]);
  x = bsObjGetString(objv[2]);
  ptr = strstr(bsStringPtr(str),bsStringPtr(x));
  bsSetIntResult(i,ptr ? (ptr-bsStringPtr(str)) : -1);
  return BS_OK;
}

/*@bsdoc
procedure substr {
    usage {substr <string> <start> [<length>]}
    returns {A substring of <i>string</i> of <i>length</i> characters
	starting at offset <i>start</i>.}
    doc {
	Extracts a substring of <i>string</i> starting at offset
	<i>start</i>.  An offset of 0 corresponds to the beginning of
	the string.  The substring will be <i>length</i> characters
	long.  If <i>length</i> is not specified, then the substring
	will run to the end of <i>string</i>.
    }
    example {Extracting a substring} {substr foobar 1 3} {oob}
    example {Extracting a substring to the end} {substr foobar 1} {oobar}
}
*/
static int sp_substr(BSInterp *i, int objc,
		     BSObject *objv[], void *cdata) {
  static char *usage = "usage: substr <string> <start> [<length>]";
  BSString *str;
  BSString *rstr;
  BSInt start, length;
  int n;

  if (objc < 3 || objc > 4) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if (bsObjGetInt(i,objv[2],&start) != BS_OK)
    return BS_ERROR;
  if ((objc > 3) && (bsObjGetInt(i,objv[3],&length) != BS_OK))
    return BS_ERROR;
  str = bsObjGetString(objv[1]);
  n = bsStringLength(str);

  bsClearResult(i);
  if (start < 0)
    start = 0;
  if (start >= n)
    start = n;
  if (length >= (n - start))
    length = (n - start);
  if (length <= 0)
    return BS_OK; /* nothing to do - leave result clear */
  rstr = bsObjGetString(bsGetResult(i));
  bsStringSet(rstr,bsStringPtr(str)+start,length,BS_S_VOLATILE);
  bsObjInvalidate(bsGetResult(i),BS_T_STRING);
  return BS_OK;
}

/*@bsdoc
procedure split {
    usage {split <string> [<chars>]}
    returns {A list built by splitting <i>string</i> using
	<i>chars</i> as delimiters.}
    doc {
	<p>
	Splits a string into a list using the given delimiters.  If
	<i>chars</i> is not specified then the set of white-space
	characters is used.  Empty elements are not discarded.
	</p>
	<p>
	A special case is when <i>chars</i> is specified as an empty
	string.  In this case, <i>string</i> will be split into a
	string where each element is exactly one character long.
	</p>
    }
    example {Splitting a string} {split abc::d:ef:g :} {abc {} d ef g}
    example {Splitting a string into characters} {split {abc def} {}} \
	{a b c { } d e f}
}
*/
static int sp_split(BSInterp *i, int objc,
		    BSObject *objv[], void *cdata) {
  static char *usage = "split <string> [<chars>]";
  char *start, *p, *chars = " \r\t\n\v";
  BSList *l;

  if (objc < 2 || objc > 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  start = bsObjGetStringPtr(objv[1]);
  if (objc > 2)
    chars = bsObjGetStringPtr(objv[2]);
  bsClearResult(i);
  l = bsObjGetList(i,bsGetResult(i));
  assert(l != NULL);
  bsObjInvalidate(bsGetResult(i),BS_T_LIST);

  if (strlen(chars) == 0) {
    /* special case - add every character as an element */
    while (*start) {
      bsListPush(l,bsObjString(start,1,BS_S_VOLATILE),BS_TAIL);
      start++;
    }
  } else {
    while ((p = strpbrk(start,chars))) {
      bsListPush(l,bsObjString(start,p-start,BS_S_VOLATILE),BS_TAIL);
      start = p+1;
    }
    bsListPush(l,bsObjString(start,-1,BS_S_VOLATILE),BS_TAIL);
  }

  return BS_OK;
}

/*@bsdoc
procedure script {
    usage {script <string>}
    returns {A list built by splitting <i>string</i> in the same
	manner as a script.}
    doc {
	Splits a string using the rules for parsing scripts. This
	means:
	<ul>
	<li>each element corresponds to a line (unless the end-of-line
						character is escaped
						or quoted)</li>
	<li>blank lines and lines beginning with '#' (comments) are
	ignored</li>
	</ul>
	Note that only parsing rules are applied - there is no
	evaluation or substitution that is otherwise applied.
    }
    example {Splitting a string like a script} {script {# this is a
	comment<br /> 
set x 40<br />
set y 50<br />
struct {<br />
    &nbsp; &nbsp; line<br />
}<br />
}} {{set x 40} {set y 50} {struct {<br />
&nbsp; &nbsp; line<br />
}}}
}
*/
static int sp_script(BSInterp *i, int objc,
		     BSObject *objv[], void *cdata) {
  static char *usage = "script <string>";
  BSParseSource ps;
  BSObject *o;
  BSList *l;
  int code;

  if (objc != 2) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  o = bsObjList(0,NULL);
  l = bsObjGetList(i,o);
  assert(l != NULL);
  bsClearResult(i);
  if ((code = bsParseScript(i,bsStringSource(&ps,bsObjGetStringPtr(objv[1]))
			    ,l)) == BS_OK)
    bsSetResult(i,o);
  bsObjDelete(o);
  return code;
}

static int is_float(char *s) {
  int seendigit = 0;

  while (isspace(*s))
    s++;

  if (*s == '+' || *s == '-')
    s++;

  if (isdigit(*s)) {
    seendigit = 1;
    while (isdigit(*s))
      s++;
  }

  if (*s == '.')
    s++;

  if (isdigit(*s)) {
    seendigit = 1;
    while (isdigit(*s))
      s++;
  }

  if (*s == 'E' || *s == 'e') {
    s++;
    if (*s == '+' || *s == '-')
      s++;
    if (!isdigit(*s)) /* must be a number after '[eE]' */
      return 0;
    while (isdigit(*s))
      s++;
  }
  /* if we consumed everything then it is a valid float */
  return (seendigit && (*s == '\0' ? 1 : 0));
}

static int is_integer(char *s) {
  /* if we start with '+/-' assume that it is decimal */
  while (isspace(*s))
    s++;

  if (*s == '0') {
    s++;
    if (*s == 'x' || *s == 'X') {
      s++;
      while (isxdigit(*s))
	s++;
    } else {
      while (*s >= '0' && *s <= '7')
	s++;
    }
  } else {
    if (*s == '+' || *s == '-')
      s++;
    /* normal decimal */
    while (isdigit(*s))
      s++;
  }
  return (*s == '\0' ? 1 : 0);
}

/*@bsdoc
procedure stris {
    usage {stris (integer|float|number|blank) <string>}
    returns {True (1) if the string matches the given type or false
	(0) if the string does not match.}
    doc {
	The following types are recognized:
	<ul>
	<li><b>integer</b> - any of the BlueScript integer forms
	(signed, unsigned, decimal, octal, hexadecimal)</li>
	<li><b>float</b> - any of the BlueScript floating-point
	forms</li>
	<li><b>number</b> - true if the argument matches either the
	float or integer representations</li>
	<li><b>blank</b> - true if the argument is either empty or 
	comprised solely of white-space characters.</li>
	</ul>
    }
    example {Checking for an integer} {stris integer 124} 1
    example {Checking for an (incorrect) integer} \
	{stris integer R12F} 0
}
*/

static int sp_stris(BSInterp *i, int objc,
		    BSObject *objv[], void *cdata) {
  static char *usage = "usage: stris <type> <string>";
  char *type;
  char *s;

  if (objc != 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  type = bsObjGetStringPtr(objv[1]);
  s = bsObjGetStringPtr(objv[2]);
  if (strcmp(type,"integer") == 0) {
    bsSetIntResult(i,is_integer(s));
  } else if (strcmp(type,"float") == 0) {
    bsSetIntResult(i,is_float(s));
  } else if (strcmp(type,"number") == 0) {
    bsSetIntResult(i,is_integer(s) || is_float(s));
  } else if (strcmp(type,"blank") == 0) {
    while (isspace(*s))
      s++;
    bsSetIntResult(i,(*s == '\0' ? 1 : 0));
  } else {
    bsClearResult(i);
    bsAppendResult(i,bsObjGetStringPtr(objv[0]),
		   ": unknown string type: ",type,NULL);
    return BS_ERROR;
  }
  return BS_OK;
}

/*@bsdoc
procedure append {
    usage {append <var> <string>}
    returns {The contents of variable <i>var</i> after appending
	<i>string</i>.}
    doc {
	Appends <i>string</i> to the contents of variable <i>var</i>.
	Note that the variable is passed as the variable name and not
	as a variable substitution (i.e. the '$' is not prepended).
	This allows the procedure to modify the variable itself.  This
	is generally more efficient than using string constructs and
	setting the variable.  For example, the following command
	<div class="eg">append x data</div>
	has the same effect as:
	<div class="eg">set x "${x}data"</div>
	but the <i>append</i> procedure avoids copying the string.
	This can greatly improve performance, especially if data is
	being appended to a large string.
    }
    example {Appending to a string} {set x "Hello"<br />
	append x ", World!"} {Hello, World!}
}
*/

static int sp_append(BSInterp *i, int objc,
		     BSObject *objv[], void *cdata) {
  static char *usage = "append <var> <string>";
  BSObject *o;
  BSString *s1, *s2;
  if (objc != 3) {
    bsSetStringResult(i,usage,BS_S_STATIC);
    return BS_ERROR;
  }
  if (!(o = bsGetObj(i,NULL,objv[1],0)))
    return BS_ERROR;
  s1 = bsObjGetString(o);
  s2 = bsObjGetString(objv[2]);
  bsStringAppend(s1,bsStringPtr(s2),bsStringLength(s2));
  bsSetStringResult(i,bsStringPtr(s1),BS_S_VOLATILE);
  return BS_OK;
}

void bsStringProcs(BSInterp *i) {
  bsSetExtProc(i,"strtrim",sp_strtrim,(void *)(TRIM_LEFT|TRIM_RIGHT));
  bsSetExtProc(i,"strltrim",sp_strtrim,(void *)(TRIM_LEFT));
  bsSetExtProc(i,"strrtrim",sp_strtrim,(void *)(TRIM_RIGHT));
  bsSetExtProc(i,"strstr",sp_strstr,NULL);
  bsSetExtProc(i,"substr",sp_substr,NULL);
  bsSetExtProc(i,"split",sp_split,NULL);
  bsSetExtProc(i,"stris",sp_stris,NULL);
  bsSetExtProc(i,"append",sp_append,NULL);
  bsSetExtProc(i,"script",sp_script,NULL);
  bsSetExtProc(i,"strlen",sp_strlen,NULL);
}
