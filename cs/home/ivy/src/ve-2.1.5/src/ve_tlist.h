#ifndef VETLIST_H
#define VETLIST_H

#include <stdio.h>

/** misc
    The ve_tlist module provides simple nested-list structure parsing
    This is used for parsing various string representations and data files.
    The syntax for lists is very similar to that used by Tcl with some
    simplifications.
    <p>
    Lists are generally sequences of words seprated by whitespace.  For
    example:
    <blockquote>apple pear orange banana</blockquote>
    is a list with four elements.  If an element of a list contains whitespace
    it can be escaped by putting it in double quotes.  For example:
    <blockquote>apple "pear orange" banana</blockquote>
    is a list with only three elements.  The second element is "pear orange".
    Elements of lists can also be escaped with curly braces.  The following
    example:
    <blockquote>apple {pear orange} banana</blockquote>
    is equivalent to the previous example.  If curly braces or double quotes
    are meaningful as data, they can be escaped with a backslash ('\').
    For example:
    <blockquote>apple \{pear orange\} banana</blockquote>
    is a list with 4 elements.  The second element is "{pear" and the third
    element is "orange}".  The backslash character can be used to escape
    any character, including white-space.  For example:
    <blockquote>apple pear\ orange banana</blockquote>
    is a list with 3 elements again and the second element is "pear orange".
    <p>Most of the parsing functions for lists are re-entrant but destructive.
    That is, they operate on strings that must be modifiable.
*/

/* Helper f'ns for parsing Tcl-like list/trees */

/** function veNextLElem
    Returns the next element in a list.  As an example, given a string pointed
    to by char pointer
    <code>str</code> which can be safely modified:
<pre>
        char *s;
        while (s = veNextLElem(&str)) {
                ... do something with s ...
        }
</pre>
    this code will visit every element in the list in turn.  Note that
    the value of str will change after each call to veNextLElem.  If you
    want to preserve the value of <code>str</code> (e.g. for freeing a
    string) then use another variable, e.g.
<pre>
        char *s, *ptr;
        ptr = str;
        while (s = veNextLElem(&ptr)) {
                ... do something with s ...
        }
</pre>

    @param ptr
    This should be a pointer to a modifiable pointer to a character.
    The value it points to should contain the starting point of the search
    for the next element of the list.  After the search is finished,
    this value will contain a pointer to the character immediately after
    the list element returned, or <code>NULL</code> if the string has
    been exhausted.
    
    @returns
    A pointer to the next element in the list, or <code>NULL</code> if
    there are no elements left.
*/
char *veNextLElem(char **ptr);   /* retrieve next list element */

/** function veNextScrElem
    Returns the next complete line (see <code>veListComplete()</code>) 
    in a string.  Blank lines and lines beginning with '#' are ignored.
    If a single line is insufficient for a complete list, then subsequent
    lines are added until a complete list is generated.  This function
    is generally used for parsing a script or lines of description 
    information.
    <p>For example, given the following input:
<pre>
        apple hello there
        pear {
                some more
                data goes here
        }
        banana
</pre>
    The first call to <code>veNextScrElem()</code> would return:
<pre>
        apple hello there
</pre>
    The second call would return:
<pre>
        pear {
                some more
                data goes here
        }
</pre>
    And the third call would return:
<pre>
        banana
</pre>
    A fourth call would return <code>NULL</code> as there would be
    no data left in the string.

    @param ptr
    This should be a pointer to a modifiable pointer to a character.
    The value it points to should contain the starting point of the search
    for the next element of the list.  After the search is finished,
    this value will contain a pointer to the character immediately after
    the list element returned, or <code>NULL</code> if the string has
    been exhausted.
    
    @param lineno
    A pointer to line counter.  If specified, this value will be incremented
    each time a newline character is consumed.  If this is <code>NULL</code>
    then it is ignored.

    @returns
    A pointer to next complete line in the string, or <code>NULL</code>
    if there are no lines left.  If the last line in the string is
    incomplete, then as much data as is available will be returned.
*/
char *veNextScrElem(char **ptr, int *lineno); 

/** function veNextFScrElem
    Similar to <code>veNextScrElem()</code> except that it reads data
    from a stdio stream rather than from a string.

    @param f
    A reference to a stdio stream from which data should be read.

    @param lineno
    A pointer to line counter.  If specified, this value will be incremented
    each time a newline character is consumed.  If this is <code>NULL</code>
    then it is ignored.

    @returns
    A pointer to next complete line in the stream, or <code>NULL</code>
    if there are no lines left.  If the last line in the stream is
    incomplete, then as much data as is available will be returned.
*/
char *veNextFScrElem(FILE *f, int *lineno);      

/** function veListComplete
    A complete list is one which has all of its quoting operators
    (i.e. {} and "") closed off.  That is, every double quote must have
    a matching double quote and every opening curly brace has a closing
    curly brace.

    @param str
    The string to check for completeness.  The string is not modified
    by checking.
    
    @returns
    A boolean value - 0 if the string is an incomplete list, non-zero
    if it is complete.
*/
int veListComplete(char *str); /* predicate:  is "str" a complete list? */

#endif /* VETLIST_H */
