#!/bin/sh
# restart with tclsh \
exec tclsh "$0" "$@"
#
# Cheat - use Tcl for now to do bsdoc processing
# - when nesting contexts do a better job of overriding
#   functions and restoring them later
#

set ctx_fname {}
set ctx_lineno 0
set _gendoc {}
variable _packages
variable _package_desc
variable _package_doc
set _curpackage {}
variable _procs
variable _proc_usage
variable _proc_desc
variable _proc_returns
variable _proc_package
variable _proc_doc
variable _proc_example
set _curproc {} 
variable _objects
variable _object_package
variable _object_doc
set _curobject {}

proc scrsplit {body} {
    set lines [split $body \n]
    set out {}
    while { [llength $lines] > 0 } {
        set l [lindex $lines 0]
        set lines [lrange $lines 1 end]
        while { ![info complete $l] } {
            if { [llength $lines] == 0 } {
                error "syntax error in body (unterminated command)"
            }
            append l \n
            append l [lindex $lines 0]
            set lines [lrange $lines 1 end]
        }
        set tl [string trim $l]
        if { ![string match "\#*" $tl] && [string compare $tl {}] } {
            # not a comment or blank
            lappend out $l
        }
    }
    return $out
}

proc include {fname} {
    set f [open $fname]
    read $d
    close $f
    return $d
}

proc doc {d} {
    global _gendoc _curpackage _curobject _curproc
    global _package_doc _object_doc _proc_doc

    if { $_curpackage == {} && $_curobject == {} && $_curproc == {} } {
	append _gendoc "\n$d"
    } elseif { $_curobject == {} && $_curproc == {} } {
	if { ![info exists _package_doc($_curpackage)] } {
	    set _package_doc($_curpackage) $d
	} else {
	    append _package_doc($_curpackage) "\n"
	    append _package_doc($_curpackage) $d
	}
    } elseif { $_curobject != {} && $_curproc == {} } {
	if { ![info exists _object_doc($_curobject)] } {
	    set _object_doc($_curobject) $d
	} else {
	    append _object_doc($_curobject) "\n"
	    append _object_doc($_curobject) $d
	}
    } elseif { $_curproc != {} } {
	if { ![info exists _proc_doc($_curproc)] } {
	    set _proc_doc($_curproc) $d
	} else {
	    append _proc_doc($_curproc) "\n"
	    append _proc_doc($_curproc) $d
	}
    }
}

proc pkg_desc {d} {
    global _package_desc _curpackage
    set _package_desc($_curpackage) $d
}

proc pkg_longname {d} {
    global _packages _curpackage
    set _packages($_curpackage) $d
}

proc package {name {body {}}} {
    global _packages _curpackage

    set _curpackage $name
    set _packages($name) $name
    if { $body != {} } {
	interp alias {} desc {} pkg_desc
	interp alias {} longname {} pkg_longname
	uplevel \#0 $body
	interp alias {} desc {}
	interp alias {} longname {}
	
    }
}

proc _proc_usage {x} {
    global _curproc _proc_usage
    if { [info exists _proc_usage($_curproc)] } {
	lappend _proc_usage($_curproc) $x
    } else {
	set _proc_usage($_curproc) [list $x]
    }
}

proc _proc_example {args} {
    global _curproc _proc_example
    if { [info exists _proc_example($_curproc)] } {
	lappend _proc_example($_curproc) $args
    } else {
	set _proc_example($_curproc) [list $args]
    }
}

proc _proc_desc {x} {
    global _curproc _proc_desc
    set _proc_desc($_curproc) $x
}

proc _proc_returns {x} {
    global _curproc _proc_returns
    set _proc_returns($_curproc) $x
}

proc _proc_partof {x} {
    global _curproc _proc_package
    set _proc_package($_curproc) $x
}

proc procedure {name {body {}}} {
    global _procs _curproc _curobject _curpackage _proc_package

    if {$_curobject != {}} {
	set name "${_curobject}::$name"
    }
    
    set _procs($name) 1
    set _proc_package($name) $_curpackage
    set _curproc $name
    interp alias {} usage {} _proc_usage
    interp alias {} desc {} _proc_desc
    interp alias {} returns {} _proc_returns
    interp alias {} partof {} _proc_partof
    interp alias {} example {} _proc_example
    uplevel \#0 $body
    interp alias {} example {}
    interp alias {} partof {}
    interp alias {} returns {}
    interp alias {} desc {}
    interp alias {} usage {}
    set _curproc {}
}

proc object {name {body {}}} {
    global _curobject _objects _object_package _curpackage
    set save $_curobject
    set _curobject $name
    set _objects($_curobject) 1
    set _object_package($name) $_curpackage
    uplevel \#0 $body
    set _curobject $save
}

proc extract_c {fname} {
    global ctx_fname ctx_lineno
    set f [open $fname]
    set save_fname $ctx_fname
    set save_lineno $ctx_lineno
    set ctx_fname $fname
    set ctx_lineno 0
    while { [gets $f l] >= 0 } {
	incr ctx_lineno
	set x [string trim $l]
	if { $x == "/*@bsdoc" } {
	    set body {}
	    if { [gets $f l] < 0 } {
		close $f
		set lineno $ctx_lineno
		set ctx_fname $save_fname
		set ctx_lineno $save_lineno
		error "unexpected eof reading bsdoc declaration in $fname, line $lineno"
	    }
	    incr ctx_lineno
	    while { [set z [string first */ $l]] < 0 } {
		append body "$l\n"
		if { [gets $f l] < 0 } {
		    close $f
		    set lineno $ctx_lineno
		    set ctx_fname $save_fname
		    set ctx_lineno $save_lineno
		    error "unexpected eof reading bsdoc declaration in $fname, line $lineno"
		}
		incr ctx_lineno
	    }
	    append body "[string range $l 0 [expr $z - 1]]\n"
	    if { [catch {uplevel \#0 $body} err] } {
		global errorInfo
		set saved $errorInfo
		puts "error encountered in $ctx_fname at line $ctx_lineno"
		set ctx_fname $save_fname
		set ctx_lineno $save_lineno
		error $err $saved
	    }
	}
    }
    close $f
    set ctx_fname $save_fname
    set ctx_lineno $save_lineno
}

proc extract_bsc {fname} {
    set f [open $fname]
    while { [gets $f l] > 0 } {
	set x [string trim $l]
	if { $x == "#@bsdoc" } {
	    set body {}
	    while { [gets $f l] < 0 && [string match "#*" [string trimleft $l]] } {
		append $body "[string range [string trimleft $l] 1 end]\n"
	    }
	    uplevel \#0 "eval {$body}"
	}
    }
    close $f
}

if { [llength $argv] < 2 } {
    puts stderr "usage: $argv0 <template> <outputdir> \[<src> ...\]"
    exit 1
}

set template [lindex $argv 0]
set outputdir [lindex $argv 1]
set argv [lrange $argv 2 end]

foreach x $argv {
    if { [string match *.c $x] || [string match *.h $x] || \
	     [string match *.cpp $x] || [string match *.hpp $x] || \
	     [string match *.C $x] || [string match *.H $x] || \
	     [string match *.cc $x] } {
	puts "Extracting from C file $x"
	extract_c $x
    } else {
	puts "Extracting from BlueScript file $x"
	extract_bsc $x
    }
}

puts "Reading template $template"
set f [open $template]
set d [read $f]
close $f

puts "Generating output in $outputdir"
cd $outputdir
eval $d
