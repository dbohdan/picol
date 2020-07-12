s|nelem|reg_nelem|g
s|REPINF|REG_REPINF|g
/#define MAXSUB/d
s| MAXSUB| REG_MAXSUB|g
s|\[MAXSUB\]|[REG_MAXSUB]|g
s|MAXPROG|REG_MAXPROG|g
s|ESCAPES|REG_ESCAPES|g

s|L_|REG_L_|g

s|regcomp|reg_comp|g
s|regexec|reg_exec|g
s|regfree|reg_free|g

s|isalpharune|reg_isalpharune|g
s|toupperrune|reg_toupperrune|g
s|chartorune|reg_chartorune|g
s|die|reg_die|g
s|canon|reg_canon|g
s|hex|reg_hex|g
s|dec|reg_dec|g
s|isunicodeletter|reg_isunicodeletter|g
s|nextrune|reg_nextrune|g
s|lexcount|reg_lexcount|g
s|newcclass|reg_newcclass|g
s|addrange|reg_addrange|g
s|addranges_d|reg_addranges_d|g
s|addranges_D|reg_addranges_D|g
s|addranges_s|reg_addranges_s|g
s|addranges_S|reg_addranges_S|g
s|addranges_w|reg_addranges_w|g
s|addranges_W|reg_addranges_W|g
s|lexclass|reg_lexclass|g
s|lex|reg_lex|g
s|newnode|reg_newnode|g
s|empty|reg_empty|g
s|newrep|reg_newrep|g
s|next|reg_next|g
s|accept|reg_accept|g
s|parsealt|reg_parsealt|g
s|parseatom|reg_parseatom|g
s|parserep|reg_parserep|g
s|parsecat|reg_parsecat|g
s|parsealt|reg_parsealt|g
s|count|reg_count|g
s|emit|reg_emit|g
s|compile|reg_compile|g
s|dumpnode|reg_dumpnode|g
s|dumpprog|reg_dumpprog|g
s|isnewline|reg_isnewline|g
s|iswordchar|reg_iswordchar|g
s|incclass|reg_incclass|g
s|incclasscanon|reg_incclasscanon|g
s|strncmpcanon|reg_strncmpcanon|g
s|match|reg_match|g

s|reg_reg_|reg_|g
s|REG_REG_|REG_|g

s|Reclass|RegClass|g
s|Reinst|RegInst|g
s|Renode|RegNode|g
s|Reprog|RegProg|g
s|Resub|RegSub|g
s|Rethread|RegThread|g
s|Rune|RegRune|g
