# vim:ft=nroff
cat <<EOF
.TH TERMKEY_GETKEY 3
.SH NAME
termkey_getkey, termkey_getkey_force \- retrieve the next key event
.SH SYNOPSIS
.nf
.B #include <termkey.h>
.sp
.BI "TermKeyResult termkey_getkey(TermKey *" tk ", TermKeyKey *" key );
.BI "TermKeyResult termkey_getkey_force(TermKey *" tk ", TermKeyKey *" key );
.fi
.sp
Link with \fI-ltermkey\fP.
.SH DESCRIPTION
\fBtermkey_getkey\fP() attempts to retrieve a single keypress event from the \fBtermkey\fP(7) instance buffer, and put it in the structure referred to by \fIkey\fP. It returns one of the following values:
.in
.TP
.B TERMKEY_RES_KEY
a complete keypress was removed from the buffer, and has been placed in the \fIkey\fP structure.
.TP
.B TERMKEY_RES_AGAIN
a partial keypress event was found in the buffer, but it does not yet contain all the bytes required. An indication of what \fBtermkey_getkey_force\fP() would return has been placed in the \fIkey\fP structure.
.TP
.B TERMKEY_RES_NONE
no bytes are waiting in the buffer.
.TP
.B TERMKEY_RES_EOF
 no bytes are ready and the input stream is now closed.
.TP
.B TERMKEY_RES_ERROR
called with terminal IO stopped, due to \fBtermkey_stop\fP(3). In this case \fIerrno\fP will be set to \fBEINVAL\fP.
.PP
\fBtermkey_getkey_force\fP() is similar to \fBtermkey_getkey\fP() but will not return \fBTERMKEY_RES_AGAIN\fP if a partial match is found. Instead, it will force an interpretation of the bytes, even if this means interpreting the start of an Escape-prefixed multi-byte sequence as a literal \fIEscape\fP key followed by normal letters.
.PP
Neither of these functions will block or perform any IO operations on the underlying filehandle. To use the instance in an asynchronous program, see \fBtermkey_advisereadable\fP(3). For a blocking call suitable for use in a synchronous program, use \fBtermkey_waitkey\fP(3) instead of \fBtermkey_getkey\fP(). For providing input without a readable filehandle, use \fBtermkey_push_bytes\fP(3).
.PP
Before returning, this function canonicalises the \fIkey\fP structure according to the rules given for \fBtermkey_canonicalise\fP(3).
.SH "RETURN VALUE"
\fBtermkey_getkey\fP() returns an enumeration of one of \fBTERMKEY_RES_KEY\fP, \fBTEMRKEY_RES_AGAIN\fP, \fBTERMKEY_RES_NONE\fP, \fBTERMKEY_RES_EOF\fP or \fBTERMKEY_RES_ERROR\fP. \fBtermkey_getkey_force\fP() returns one of the above, except for \fBTERMKEY_RES_AGAIN\fP.
.SH EXAMPLE
The following example program prints details of every keypress until the user presses \fICtrl-C\fP. It demonstrates how to use the \fBtermkey\fP instance in a typical \fBpoll\fP(2)-driven asynchronous program, which may include mixed IO with other file handles.
.PP
.in +4n
.nf
EOF
sed "s/\\\\/\\\\\\\\/g" demo-async.c
cat <<EOF
.in
.fi
.SH "SEE ALSO"
.BR termkey_advisereadable (3),
.BR termkey_waitkey (3),
.BR termkey_get_waittime (3),
.BR termkey (7)
EOF
