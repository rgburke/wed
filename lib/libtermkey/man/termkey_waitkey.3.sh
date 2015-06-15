# vim:ft=nroff
cat <<EOF
.TH TERMKEY_WAITKEY 3
.SH NAME
termkey_waitkey \- wait for and retrieve the next key event
.SH SYNOPSIS
.nf
.B #include <termkey.h>
.sp
.BI "TermKeyResult termkey_waitkey(TermKey *" tk ", TermKeyKey *" key );
.fi
.sp
Link with \fI-ltermkey\fP.
.SH DESCRIPTION
\fBtermkey_waitkey\fP() attempts to retrieve a single keypress event from the \fBtermkey\fP(7) instance buffer, and put it in the structure referred to by \fIkey\fP. If successful it will return \fBTERMKEY_RES_KEY\fP to indicate that the structure now contains a new keypress event. If nothing is in the buffer it will block until one is available. If no events are ready and the input stream is now closed, will return \fBTERMKEY_RES_EOF\fP. If no filehandle is associated with this instance, \fBTERMKEY_RES_ERROR\fP is returned with \fIerrno\fP set to \fBEBADF\fP.
.PP
Before returning, this function canonicalises the \fIkey\fP structure according to the rules given for \fBtermkey_canonicalise\fP(3).
.PP
Some keypresses generate multiple bytes from the terminal. Because there may be network or other delays between the terminal and an application using \fBtermkey\fP, \fBtermkey_waitkey\fP() will attempt to wait for the remaining bytes to arrive if it detects the start of a multibyte sequence. If no more bytes arrive within a certain time, then the bytes will be reported as they stand, even if this results in interpreting a partially-complete Escape sequence as a literal Escape key followed by some normal letters or other symbols. The amount of time to wait can be set by \fBtermkey_set_waittime\fP(3).
.SH "RETURN VALUE"
\fBtermkey_waitkey\fP() returns one of the following constants:
.TP
.B TERMKEY_RES_KEY
A key event as been provided.
.TP
.B TERMKEY_RES_EOF
No key events are ready and the terminal has been closed, so no more will arrive.
.TP
.B TERMKEY_RES_ERROR
An IO error occured. \fIerrno\fP will be preserved. If the error is \fBEINTR\fP then this will only be returned if \fBTERMKEY_FLAG_EINTR\fP flag is not set; if it is then the IO operation will be retried instead. If this is called with terminal IO stopped, due to \fBtermkey_stop\fP(3) then \fIerrno\fP will be set to \fBEINVAL\fP.
.SH EXAMPLE
The following example program prints details of every keypress until the user presses \fICtrl-C\fP.
.PP
.in +4n
.nf
EOF
sed "s/\\\\/\\\\\\\\/g" demo.c
cat <<EOF
.in
.fi
.SH "SEE ALSO"
.BR termkey_getkey (3),
.BR termkey_set_waittime (3),
.BR termkey (7)
EOF
