// PC test of the sequential layout engine. From users/tokioka:
//   gcc -I. test/test_seqkana.c seqkana.c naginata.c -o /tmp/t && /tmp/t
// Each line is "ok name keys => romaji sent". Keys are QWERTY positions:
// a bare letter is a tap, +a and -a press and release, @N moves the clock N ms,
// and ! is Del (it shows [DEL] when there was no prefix to drop). ~ is the
// wait between the parts of a kana typed with more than one key.
#include "naginata.h"
#include <stdio.h>
#include <string.h>
static char out[512];
void ng_send_romaji(const char *r){strcat(out,r);strcat(out," ");}
void ng_ime(bool on){strcat(out,on?"[ON] ":"[OFF] ");}
void ng_tap_esc(void){strcat(out,"[ESC] ");}
void ng_wait_ms(uint16_t ms){(void)ms;strcat(out,"~ ");}
static const char *K="qwertyuiopasdfghjkl;zxcvbnm,./";
// Onishi letters on layer 0 by QWERTY position
static const char *ON="qlu,.fwryp" "eiao-ktnsh" "xjcv/gdmzb";
char ng_letter(uint8_t p){char c=ON[p];return c>='a'&&c<='z'?c:0;}
static int pos(char c){const char*p=strchr(K,c);return p?p-K:-1;}
static int fails;

static void run(const char *name,const char *sc,const char *want){
  out[0]=0; uint16_t t=1000;
  for(const char*p=sc;*p;){
    if(*p==' '){p++;continue;}
    if(*p=='@'){int n;sscanf(p+1,"%d",&n);t+=n;p++;while(*p>='0'&&*p<='9')p++;continue;}
    if(*p=='!'){p++; if(!ng_drop_prefix(t)) strcat(out,"[DEL] "); continue;}
    if(*p=='+'||*p=='-'){char op=*p++, c=*p++; if(op=='+')ng_press(pos(c),t);else ng_release(pos(c),t); t+=10; continue;}
    ng_press(pos(*p),t); t+=30; ng_release(pos(*p),t); t+=30; p++;
  }
  ng_flush(t+500);
  int bad=strcmp(out,want)!=0; fails+=bad;
  printf("%s %-14s %-26s => %s%s%s\n", bad?"NG":"ok", name, sc, out, bad?" want ":"", bad?want:"");
}
int main(){
  ng_sequential=true;
  run("ko","w","ko ");
  run("D same hand","dw","ltu ~ ko ");
  run("mi K","kw","m ~ i ");
  run("ha","kd","h ~ a ");
  run("ke","dk","k ~ e ");
  run("ra","dl","r ~ a ");
  run("ltu same","ds","ltu ~ i ");
  run("ltu K","kj","ltu ~ nn ");
  run("DD same","dds","h ~ u ");
  run("DD opp","ddj","l ~ tu ~ sa ");
  run("KK same","kkj","s ~ a ");
  run("KK opp","kks","l ~ tu ~ hu ");
  run("K then L","kl/","ltu g ~ i ");
  run("ga","lf","g ~ a ");
  run("ba","ld","b ~ a ");
  run("ge","lk","g ~ e ");
  run("bo","lr","b ~ o ");
  run("vu","lu","v ~ u ");
  run("be","lg","b ~ e ");
  run("L yo plain","lc","y ~ o ");
  run("pa","pd","p ~ a ");
  run("pi","ph","p ~ i ");
  run("sha","iz","si xya ");
  run("nya exc","rz","ni ya ");
  run("nya via L","rlz","ni x ~ ya ");
  run("nyo exc","rc","ni yo ");
  run("kyou","/g","ki xyou ");
  run("syuu","iu","si xyuu ");
  run("siu via L","ilu","si u ");
  run("sio via L","ilg","si o ");
  run("gyo","l/c","g ~ i xyo ");
  run("pyo","phc","p ~ i xyo ");
  run("roll ka-i","+f +s -f -s","ka i ");
  run("roll rev rel","+f +s -s -f","ka ~ i ");
  run("held prefix","+d +j -j -d","s ~ a ");
  run("held same","+d +s -s -d","ltu ~ i ");
  run("fi","p +y +s -y -s","f ~ i ");
  run("dhi","p +m +; +s -m -; -s","d ~ h ~ i ");
  run("pu group1","p +s -s","p ~ u ");
  run("P no match","p +h +j -h -j","p ~ i ~ nn ");
  run("Del cancel","d!w","ko ");
  run("Del no prefix","w!","ko [DEL] ");
  run("Del held D","+d !-d w","ko ");
  run("arigatou","kvolfau","a ri g ~ a to u ");
  run("gozaimasu","lwljsdm,","g ~ o z ~ a i m ~ a su ");
  run("touten","t",", ");
  run("ta","v","ta ");
  run("ltu ta","dv","ltu ~ ta ");
  printf("%d failures\n",fails);
  return fails != 0;
}
