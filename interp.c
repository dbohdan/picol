/* An interactive Picol interpreter. */

#include "picol.h"

int main(int argc, char **argv) {
  picolInterp *i = picolCreateInterp();
  char buf[MAXSTR] = "";
  int  rc;
  FILE* fp = fopen("init.pcl","r");
  picolSetVar(i,"argv0", argv[0]);
  picolSetVar(i,"argv",  "");
  picolSetVar(i,"argc",  "0");
  picolSetVar(i,"auto_path", "");
  picolEval(i,"array set env {}"); /* populated from real env on demand */
  if(fp) {
    fclose(fp);
    rc = picolSource(i,"init.pcl");
    if(rc != PICOL_OK) puts(i->result);
    i->current = NULL; /* avoid misleading error traceback */
  }
  if (argc == 1) {           /* no arguments - interactive mode */
    while(1) {
      printf("picol> "); fflush(stdout);
      if (fgets(buf,sizeof(buf),stdin) == NULL) return 0;
      rc = picolEval(i,buf);
      if (i->result[0] != '\0' || rc != PICOL_OK)
        printf("[%d] %s\n", rc, i->result);
    }
  } else if(argc==3 && EQ(argv[1],"-e")) {   /* script in argv[2] */
    rc = picolEval(i,argv[2]);
    if(rc != PICOL_OK) {
      picolVar *v = picolGetVar(i,"::errorInfo");
      if(v) puts(v->val);
    } else puts(i->result);
  } else {    /* first arg is file to source, rest goes to argv */
    picolSetVar(i,   "argv0",argv[1]);
    picolSetVar(i,   "argv", picolConcat(buf,argc-1,argv+1));
    picolSetIntVar(i,"argc",argc-2);
    rc = picolSource(i,argv[1]);
    if(rc != PICOL_OK) {
      picolVar *v = picolGetVar(i,"::errorInfo");
      if(v) puts(v->val);
    }
  }
  return rc & 0xFF;
}
