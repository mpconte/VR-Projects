#
# TXM features
#
feature set JPEGINC hasinclude jpeglib.h -- sys/types.h stdio.h
feature set JEPGINC value -I/cs/local/include hasinclude jpeglib.h -I/cs/local/include -- sys/types.h stdio.h
feature set JPEGINC value -I/sw/include hasinclude jpeglib.h -I/sw/include -- sys/types.h stdio.h
feature set JPEGINC value -I/usr/local/include hasinclude jpeglib.h -I/usr/local/include -- sys/types.h stdio.h
feature append CFLAGS value "$ACFG_JPEGINC" test "X$ACFG_JPEGINC" != "X"
feature set JPEGLIB haslibrary jpeg_finish_decompress -ljpeg 
feature set JPEGLIB haslibrary jpeg_finish_decompress -L/usr/local/lib -ljpeg 
feature set JPEGLIB haslibrary jpeg_finish_decompress -L/cs/local/lib -ljpeg 
feature set JPEGLIB haslibrary jpeg_finish_decompress -L/sw/lib -ljpeg 
