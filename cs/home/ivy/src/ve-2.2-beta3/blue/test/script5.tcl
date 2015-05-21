#
# The equivalent of script5 in Tcl
#
set count 0
while { [gets stdin l] >= 0 } {
    incr count
}
puts "$count lines"
