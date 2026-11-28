// PC test of the sequential layout engine. From users/tokioka:
//   gcc -I. test/test_seqkana.c seqkana.c naginata.c -o /tmp/t && /tmp/t
// Each line is "ok name keys => romaji sent". Keys are QWERTY positions:
// a bare letter is a tap, +a and -a press and release, @N moves the clock N ms
// and ticks the engine, | shows in the output where it is, and ! is Del (it shows [DEL] when there was no prefix to drop). ~ is the
// wait between the parts of a kana typed with more than one key.
#include "naginata.h"
#include "seqkana.h"
#include <stdio.h>
#include <string.h>
static char out[512];
void ng_send_romaji(const char *r){strcat(out,r);strcat(out," ");}
void ng_ime(bool on){strcat(out,on?"[ON] ":"[OFF] ");}
void ng_tap_esc(void){strcat(out,"[ESC] ");}
void ng_wait_ms(uint16_t ms){(void)ms;strcat(out,"~ ");}
// The bottom row tap-hold keys of the boards: Z, X, C, V, M, comma and full stop.
bool ng_is_tap_hold(uint8_t p){return p==20||p==21||p==22||p==23||p==26||p==27||p==28;}
static const char *K="qwertyuiopasdfghjkl;zxcvbnm,./";
static int pos(char c){const char*p=strchr(K,c);return p?p-K:-1;}
static int fails;

static void run(const char *name,const char *sc,const char *want){
  out[0]=0; uint16_t t=1000; sq_reset();
  for(const char*p=sc;*p;){
    if(*p==' '){p++;continue;}
    if(*p=='@'){int n;sscanf(p+1,"%d",&n);t+=n;ng_tick(t);p++;while(*p>='0'&&*p<='9')p++;continue;}
    if(*p=='|'){p++;strcat(out,"| ");continue;}
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
  ng_stroke_gap_ms=15;
  run("ko","w","ko ");
  run("esc","q","[ESC] ");
  run("esc after kana","wq","ko [ESC] ");
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
  run("roll rev rel","+f +s -s -f","ka i ");
  run("held prefix","+d +j -j -d","s ~ a ");
  run("held same","+d +s -s -d","ltu ~ i ");
  run("fi","p +y +s -y -s","f ~ i ");
  run("je onchoku","p +x +a -x -a","j ~ e ");
  run("ze is no gairaion","p +. +a -. -a","t ~ o ");
  run("pu group1","p +s -s","p ~ u ");
  run("P no match","p +h +j -h -j","p ~ i nn ");
  run("fa on tick","p +y +d @60 | -y -d","f ~ a | ");
  run("fa waits","p +y +d @40 | -d -y","| f ~ a ");
  run("fa seq","pyd","f ~ a ");
  run("kw then a","p +h +u -h -u d","k ~ w ~ a ");
  run("kwa held","p +h +u +d -h -u -d","k ~ w ~ a ");
  run("pi u slow","p +h @70 +u -h -u","p ~ i xyuu ");
  run("thi seq","pj;s","t ~ h ~ i ");
  run("zye seq","p. +s +a -s -a","z ~ y ~ e ");
  run("tsa seq hold","pj +l +d -l -d","t ~ s ~ a ");
  run("pi then a","phf","p ~ i ka ");
  run("cons then kana","pyw","k ~ o ");
  run("cons then Del","py!w","k ~ o ");
  run("zye cons first","p +. +s -. +a -s -a","z ~ y ~ e ");
  run("fi cons first","p +y +s -y @60 | -s","f ~ i | ");
  run("zye","p +. +s +a -. -s -a","z ~ y ~ e ");
  run("zyeku","p +. +s +d +f +z -. -s -d -f -z","z ~ y ~ e ~ k ~ u ");
  run("fann","p +y +d +v -y -d -v","f ~ a ~ n ~ n ");
  run("tyuu","p +j +s +e +f -j -s -e -f","t ~ y ~ u ~ u ");
  run("dhi held P","+p +m +; +s -m -; -s -p","d ~ h ~ i ");
  run("vowel first","p +d +y -d -y","p ~ a . ");
  run("consonant let go","p +h -h +f -f","p ~ i ka ");
  run("pi si slow","p +h @70 +i -h -i","p ~ i si ");
  run("th no word","p +j @10 +; @60 -j -; s","t ~ h ~ i ");
  run("pi si fast","p +h @10 +i -h -i","p ~ i si ");
  run("dhi","p +m @10 +; @10 +s -m -; -s","d ~ h ~ i ");
  run("pitta roll","p +h +d +v -h -d -v","p ~ i ltu ~ ta ");
  run("other key closes","p +y +d +h -y -d -h","f ~ a ~ - ");
  run("Del cancel","d!w","k ~ o ");
  run("Del carried","d!a","t ~ o ");
  run("Del no prefix","w!","ko [DEL] ");
  run("Del held D","+d !-d w","k ~ o ");
  run("arigatou","kvolfau","a r ~ i g ~ a to u ");
  run("gozaimasu","lwljsdm,","g ~ o z ~ a i m ~ a su ");
  run("touten","t",", ");
  run("on press","+f | -f","ka | ");
  run("prefix on next press","+k +w | -k -w","m ~ i | ");
  run("tap-hold on release","+v | -v","| ta ");
  run("inside tap-hold","+v +f | -f -v","| ta ~ ka ");
  run("after tap-hold","v +f | -f","ta ka | ");
  run("exclamation","+r @10 +t @40 -r -t","! ");
  run("question","+u @10 +y @40 -u -y","? ");
  run("ni then comma","+r @70 +t -r -t","ni , ");
  run("ni waits","+r @30 | @30 | -r","| ni | ");
  run("ni then ka","+r @10 +f | -r -f","ni ~ ka | ");
  run("suru roll","+, @60 +. @40 -, -.","su ru ");
  run("suru apart","+, @40 -, @20 +. @40 -.","su ru ");
  run("ltu alone","dn","l ~ tu ");
  run("nakaguro","kb","/ ");
  run("exclamation","kt","! ");
  run("question","dy","? ");
  run("ltu maru","ky","ltu ~ . ");
  run("toku arrow","dph","x ~ k ");
  run("toku arrow K","dpk","x ~ n ");
  run("toku leader","dpt","z ~ , ");
  run("toku other","dpw","k ~ o ");
  run("ltu pa","kpd","ltu p ~ a ");
  run("ta","v","ta ");
  run("ltu ta","dv","ltu ~ ta ");
  run("a carried","kv f","a k ~ a ");
  run("e a carried","kx kv ws","e a k ~ o i ");
  /* [ after the toku key and after L then P */
  sq_reset(); out[0]=0; ng_press(12,1); ng_release(12,2); ng_press(9,3); ng_release(9,4);
  { int c=ng_take_bracket_prefix(); printf("%s toku bracket %c\n", c=='z'?"ok":"NG", c?c:'-'); fails+=c!='z'; }
  sq_reset(); ng_press(18,1); ng_release(18,2); ng_press(9,3); ng_release(9,4);
  { int c=ng_take_bracket_prefix(); printf("%s lenticular %c\n", c=='s'?"ok":"NG", c?c:'-'); fails+=c!='s'; }
  printf("%d failures\n",fails);
  return fails != 0;
}
