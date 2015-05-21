#
# Example template for actually generating documentation
#

proc conv_usage {l} {
    regsub -all {<([^>]*)>} $l {</b><i>\1</i><b>} l
    regsub -all {[\[\]()|]} $l {</b>&<b>} l
    regsub -all {\.\.\.} $l {</b>...<b>} l
    return "<b>$l</b>"
}

proc proc_body {f x} {
    global _procs _proc_usage _proc_desc _proc_returns _proc_package _proc_doc
    global _proc_example

    if { [info exists _proc_usage($x)] } {
	puts $f "<div class=\"label\">Usage</div>"
	puts $f "<div class=\"usage\">"
	set i 0
	foreach y $_proc_usage($x) {
	    if { $i > 0 } {
		puts $f "&nbsp; &nbsp; <i>or</i><br />"
	    }
	    puts $f "[conv_usage $y]<br />"
	    incr i
	}
	puts $f "</div>"
    }
    if { [info exists _proc_returns($x)] } {
	puts $f "<div class=\"label\">Returns</div>"
	puts $f "<div class=\"text\">$_proc_returns($x)</div>"
    }
    if { [info exists _proc_desc($x)] } {
	puts $f "<div class=\"label\">Description</div>"
	puts $f "<div class=\"text\">$_proc_desc($x)</div>"
    }
    if { [info exists _proc_doc($x)] } {
	puts $f "<div class=\"label\"></div>"
	puts $f $_proc_doc($x)
    }

    if { [info exists _proc_example($x)] } {
	puts $f "<div class=\"label\">Examples</div>"
	set i 0
	foreach e $_proc_example($x) {
	    incr i
	    puts $f "<div class=\"example\">${i}. [lindex $e 0]"
	    puts $f "<div class=\"eg\">[lindex $e 1]</div>"
	    if { [lindex $e 2] != {} } {
		puts $f "<div class=\"result\">Result: <span class=\"egspan\">[lindex $e 2]</span></div></div>"
	    } else {
		puts $f "<div class=\"result\">Result: <i>none</i></div>"
	    }
	    puts $f "</div>"
	}
    }
}

#
# Still need to do an index of sorts...
#

foreach x [array names _packages] {
    if { $x == {} } {
	continue
    }

    set f [open pkg_$x.html w]

    puts $f "<html>"
    puts $f "<head>"
    puts $f "<title>BlueScript: $_packages($x)</title>"
    puts $f "<link rel=\"stylesheet\" href=\"bluescript.css\" type=\"text/css\" />"
    puts $f "</head>"
    puts $f "<body>"

    puts $f "<h1>BlueScript: $_packages($x)</h1>"

    set objs {}
    catch {unset objprocs}
    foreach y [array names _objects] {
	if { $_object_package($y) == $x } {
	    lappend objs $y
	    set objprocs($y) {}
	}
    }
    set procs {}
    foreach y [array names _procs] {
	if { $_proc_package($y) == $x } {
	    if { [string match *::* $y] } {
		# object procedure
		set o [lindex [split $y :] 0]
		set y [lindex [split $y :] 2]
		if { [info exists objprocs($o)] } {
		    lappend objprocs($o) $y
		}
	    } else {
		lappend procs $y
	    }
	}
    }

    puts $f "<h2>Contents</h2>"
    puts $f "<ul>";
    if { [llength $objs] > 0 } {
	puts $f "<li><a href=\"#objs\">Objects</a>"
	puts $f "<ul>"
	foreach o [lsort $objs] {
	    puts $f "<li><a href=\"#obj_$o\">$o</a>"
	    if { [llength $objprocs($o)] > 0 } {
		puts $f "<ul>"
		foreach p [lsort $objprocs($o)] {
		    puts $f "<li><a href=\"#oproc_${o}__$p\"><i>$o</i> $p</a></li>"
		}
		puts $f "</ul>"
	    }
	    puts $f "</li>"
	}
	puts $f "</ul>"
    }
    if { [llength $procs] > 0 } {
	puts $f "<li><a href=\"#procs\">Procedures</a>"
	puts $f "<ul>"
	foreach o [lsort $procs] {
	    puts $f "<li><a href=\"#proc_$o\">$o</a></li>"
	}
	puts $f "</ul>"
    }
    puts $f "</ul>"

    if { [info exists _package_desc($x)] } {
	puts $f "<h2>Description</h2>"
	puts $f $_package_desc($x)
    }

    if { [info exists _package_doc($x)] } {
	puts $f "<h2>Documentation</h2>"
	puts $f $_package_doc($x)
    }
    
    if { [llength $objs] > 0 } {
	puts $f "<a name=\"objs\"></a>"
	puts $f "<h2>Objects</h2>"
	puts $f "<ul>"
	foreach y [lsort $objs] {
	    puts $f "<li><a href=\"#obj_$y\">$y</a></li>"
	}
	puts $f "</ul>"

	foreach y [lsort $objs] {
	    puts $f "<a name=\"obj_$y\"></a>"
	    puts $f "<h3>$y</h3>"
	    if { [info exists _object_doc($y)] } {
		puts $f $_object_doc($y)
	    }
	    foreach p [lsort $objprocs($y)] {
		puts $f "<a name=\"oproc_${y}__$p\"></a>"
		puts $f "<div class=\"proctitle\"><i>$y</i> $p</div>"
		proc_body $f ${y}::$p
	    }
	}
    }

    if { [llength $procs] > 0 } {
	puts $f "<a name=\"procs\"></a>"
	puts $f "<h2>Procedures</h2>"
	puts $f "<ul>"
	foreach y [lsort $procs] {
	    puts $f "<li><a href=\"#proc_$y\">$y</a></li>"
	}
	puts $f "</ul>"
	foreach y [lsort $procs] {
	    puts $f "<a name=\"proc_$y\"></a>"
	    puts $f "<div class=\"proctitle\">$y</div>"
	    proc_body $f $y
	}
    }
}

# index
set f [open index.html w]
puts $f "<html>"
puts $f "<head>"
puts $f "<title>BlueScript Reference</title>"
puts $f "<link rel=\"stylesheet\" href=\"bluescript.css\" type=\"text/css\" />"
puts $f "</head>"
puts $f "<body>"

puts $f "<h1>BlueScript Reference</h1>"

puts $f "<h2>Packages</h2>"

puts $f "<ul>"
foreach x [lsort [array names _packages]] {
    puts $f "<li><a href=\"pkg_$x.html\">$_packages($x)</a>"
    if { [info exists _package_desc($x)] } {
	puts $f "<br />"
	puts $f "&nbsp; &nbsp; $_package_desc($x)"
    }
    puts $f "</li>"
}
puts $f "</ul>"

puts $f $_gendoc

