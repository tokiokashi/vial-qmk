// PC test of the sequential layout engine. From users/tokioka:
//   gcc -I. test/test_seqkana.c seqkana.c naginata.c -o /tmp/t && /tmp/t
// Each line is "ok name keys => romaji sent". Keys are QWERTY positions:
// a bare letter is a tap, +a and -a press and release, @N moves the clock N ms,
// and ! is Del (it shows [DEL] when there was no prefix to drop).
#include "naginata.h"
#include <stdio.h>
#include <string.h>
static char out[512];
void ng_send_romaji(const char *r){strcat(out,r);strcat(out," ");}
void ng_ime(bool on){strcat(out,on?"[ON] ":"[OFF] ");}
void ng_tap_esc(void){strcat(out,"[ESC] ");}
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
  run("D same hand","dw","ltu ko ");
  run("mi K","kw","mi ");
  run("ha","kd","ha ");
  run("ke","dk","ke ");
  run("ra","dl","ra ");
  run("ltu same","ds","ltu i ");
  run("ltu K","kj","ltu nn ");
  run("DD same","dds","hu ");
  run("DD opp","ddj","ltu sa ");
  run("KK same","kkj","sa ");
  run("KK opp","kks","ltu hu ");
  run("K then L","kl/","ltu gi ");
  run("ga","lf","ga ");
  run("ba","ld","ba ");
  run("ge","lk","ge ");
  run("bo","lr","bo ");
  run("vu","lu","vu ");
  run("be (o not voiced)","lg","be ");
  run("L yo plain","lc","yo ");
  run("pa","pd","pa ");
  run("pi","ph","pi ");
  run("sha","iz","si xya ");
  run("nya exc","rz","ni ya ");
  run("nya via L","rlz","ni xya ");
  run("nyo exc","rc","ni yo ");
  run("kyou","/g","ki xyou ");
  run("syuu","iu","si xyuu ");
  run("siu via L","ilu","si u ");
  run("sio via L","ilg","si o ");
  run("gyo","l/c","gi xyo ");
  run("pyo","phc","pi xyo ");
  run("roll ka-i","+f +s -f -s","ka i ");
  run("roll rev rel","+f +s -s -f","ka i ");
  run("held prefix","+d +j -j -d","sa ");
  run("held same","+d +s -s -d","ltu i ");
  run("fi","p +y +s -y -s","fi ");
  run("dhi","p +m +; +s -m -; -s","dhi ");
  run("pu group1","p +s -s","pu ");
  run("P no match","p +h +j -h -j","pi nn ");
  run("Del cancel","d!w","ko ");
  run("Del no prefix","w!","ko [DEL] ");
  run("Del held D","+d !-d w","ko ");
  run("arigatou","kvolfau","a ri ga to u ");
  run("gozaimasu","lwljsdm,","go za i ma su ");
  run("touten","t",", ");
  run("ta","v","ta ");
  run("ltu ta","dv","ltu ta ");
  printf("%d failures\n",fails);
  return fails != 0;
}
