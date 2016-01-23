About Picol
Richard Suchenwirth, 2007-04-06

Update 2007-05-01: 0.1.22: 1700 loc in picol.c, 126 in picol.h
New: emulated the auto_load mechanism: picol.c calls unknown if available; init,pcl
implements it by sourcing tclIndex files as on auto_path; there's one tclIndex that
prepares auto_index(parray) to source parray.tcl.
Long and complicated way, but similar to how Tcl does it :)
Also new: uplevel #0

Update 2007-04-22, 0.1.20: 1719 loc, 88 commands, 277 tests (in test.pcl)
Added some 8.5 features: {*} (only for $var or [cmd], not for {const..} or "const"), 
in/ni operators. Also, at startup ./init.pcl is sourced if present (it currently 
contains workarounds for [glob] and [pwd], which might only work under Cygwin or *n*x).

Update 2007-04-15, 0.1.19:
Several minor fixes. Most important, added ARITY2 macro to display a "wrong # args" 
message, but also to be extracted for a little auto-documentation, as I was getting 
tired of tracking what I've done.
The attached script help.pcl extracts the usage messages from ./picol.c (or the file 
specified on command line) to stdout. See help.txt for a snapshot of it.
Also included is wc-l.pcl as finger exercise  - works mostly like Cygwin's wc -l, 
just slower :)

Update 2007-04-12 0.1.18:
lsort empty list crash fixed
math operators in many cases variadic (FOLD: +, *, -, && new || new), unary minus
simple ternary [expr] introduced

----
Picol was originally created by Salvatore Sanfilippo in March 2007. It
is a small scripting language modeled closely on Tcl (Tool Command
Language), with very little effort - Salvatore's first published version
had just 500 lines of C code. License is BSD.

Picol is of course a very small language,
allowing so far only integer arithmetics, and lacks many features of "big Tcl".
In spirit it is close to pre-8.0 Tcl, where every value was indeed a
string, and [expr $a+$b] would require two atoi() scans and another
sprintf() formatting. Also, arrays are not supported in Picol.

Having used Tcl/Tk for about ten years, and C even longer, I still never
looked much at the C side of the Tcl implementation. Many things were
introduced in that time: bytecode compilation, Unicode support...
leading to a code base of considerable complexity. I settled for letting
the maintainers doing their job, and me being just a "consumer".

Looking at the source of Picol (it is in fact a single file, picol.c,
with no dependencies other than the usual stdio/stdlib), I felt that
this code was transparent enough for the man in the street. I could
understand the architecture (except maybe for the parser :^) and easily
see what was going on, how commands were registered and implemented, and
so on.

The executable can, like tclsh, run either interactively, where you type
commands to a prompt and watch the response, or in "batch mode", when a
script is specified on the command line. Salvatore's original version
had just eight commands: proc, puts, if, while, break, continue, and
return, in addition to about ten arithmetic and comparison operators as
known from [expr], or especially LISP.

Salvatore decided not to implement [expr], with its own little language
with infix operators, whitespace being mostly redundant, and f($x,$y)
notation for functions. Picol still does only integer math, and
comparison operators are also limited to integers. In doing [if] and
[while] he deviated from the Tcl practice that the condition arguments
are in [expr] syntax, but rather scripts like everything else.

This however meant that Picol is an incompatible dialect of Tcl -
it is similar in many respects, but the different interfaces for the
frequently used [if] and [while] commands excluded the possibility that
non-trivial Picol scripts could run in tclsh or vice-versa.

Still, Picol fascinated me - not as a tool for serious work, just as an
educational toy. "Camelot!... It's a model." I began to miss features
from tclsh, like leaving the interpreter with [exit], so the first thing
I did was to implement an exit command, which was pretty easy. What I
also missed was the one-argument [gets] as simplest way to inspect
variables interactively. And access to global variables from inside
procs. And a [source] command. And... you can guess what happened.

I became addicted with Picol. Never before in a long time of coding in C
has the bang/buck ratio been so high. Especially, standard C libraries
offer rich functionality that can rather easily be leveraged. For
instance, most of [clock format] is available from strftime(), it just
remains to call it. My partial implementation of [clock] provides clock
clicks|seconds|format, all from 18 lines of C code.

But two things bothered me: the incompatibilities with real Tcl, and the
lack of the so popular list operations. Fortunately, on Good Friday
2007, I managed to think up simple solutions for both of them. Where in
Tcl you would have a condition as expression
    if {$x < 0} ...
in Picol you'd use a short script with a prefix operator:
    if {< $x 0} ...
which looks similar enough, but can't be parsed as valid expression by
Tcl. On the other hand, the possible Tcl notation (if you have a [<]
command),
    if {[< $x 0]} ...
would be double-evaluated by Picol, leading to an error "unknown command
0" (or "1"). The solution I came up with overnight was to introduce the
elementary identity function, like
    proc I x {set x}
and a variation of picolEval which just prepends "I " to the input
script, and evaluates that. This way, both function results and
numeric constants could easily be used in Tcl syntax, and eight more
lines of code solved that incompatibility problem.

From that point on, I wanted to test my Picol test suite with tclsh as
well, just to make sure that the two languages don't diverge. I first
did this by hand, but very soon got the idea that a minimal [exec] is
feasible for Picol too, so here's what I coded:

int picolCmdExec(struct picolInterp *i, int argc, char **argv, void *pd) {
    char buf[1024]; /* This is far from the real thing, but may be useful */
    int rc = system(picolConcat(buf,argc,argv));
    return picolSetFmtResult(i,"%d",rc);
}

The caller doesn't get the stdout of the called process back, and there
isn't much configurability, but this minimal solution allowed me to
include the line

exec tclsh [info script]

in my test suite, and thus conveniently testing two things at once -
Picol, and the compliance of the test script with Tcl. Some tests have
to be skipped for Tcl, of course - though the arithmetic/comparison
operators are provided, things like [info globals] or [expr] error
messages are not easily unified. I base the skipping of tests and other
conditional code on [info exists tcl_version].

Another modification I find useful is to allow a script on the command
line. If the first argument is "-E", the second is evaluated as script,
its result displayed on stdout, and then exit. This feature is known
from Perl and comes quite handy sometimes:

/_Ricci> picol -e "lsort [info commands]"
! != % * + - / < <= == > >= _l abs append array break catch clock close concat continue eof eq error eval exec exit file flush for foreach format gets if incr i
nfo interp join lappend lindex linsert list llength lrange lreplace lsearch lset
 lsort ne open pid proc puts read rename return scan seek set setenv source split string subst switch tell time unset uplevel while
/_Ricci>


which by the way exhibits how many familiar commands have been added to
Salvatore's original version... As there was no version numbering, I
dubbed it 0.1.0, and have been adding patchlevels, one after the other,
up until the current 0.1.16. Will there ever be a 1.0 version? I have no
idea. Creeping featuritis is always a danger, and it might be a pity if
the clear Picol design gets obscured by yet another bell here and a
whistle there.

Vital statistics (subject to change without notice):
Number of source files           1 (picol.c)
Lines of code                 1079
Implemented commands            55
Number of test cases            66 (in test.pcl)
Size of stripped executable  23552 bytes

A ZIP file containing picol.c, test.pcl, and this document can be
downloaded from http://mini.net/files/picol0-1-17.zip . License
conditions are again BSD, i.e. free for all private and commercial use,
provided the original authorship is acknowledged. And no warranty for
any usability for any purpose, but you know that :)

For me, the main purpose has been to experiment with C without the
hassle of complex dependencies. Hacking on Picol, I felt how a student
at UC Berkeley must have felt in 1988, when Tcl was just a baby...

