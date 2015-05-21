#
# Clock configuration - choose an implementation
# - we prefer clock_gettime if it is available
feature set IMPL value unixc haslibrary clock_gettime
feature set IMPL value unix haslibrary gettimeofday
require IMPL
