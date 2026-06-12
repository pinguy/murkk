/* ============================================================================
 * .murkk — a 96k-spirit homage to .kkrieger (.theprodukkt, Breakpoint 2004)
 *
 * Rules of the compo:
 *   - one C file, no assets on disk, no engine
 *   - textures, level, meshes, audio, font: all synthesized at startup/runtime
 *   - deterministic seed (it's a demo); --seed N to override
 *   - stripped dynamic binary must come in under 96K
 *   - trades RAM and startup time for bytes, exactly like the original
 *
 * build: gcc -Os murkk.c -o murkk -lSDL2 -lGL -lm   (see build.sh for flags)
 * license: CC0 / public domain. greets to farbrausch & .theprodukkt.
 * ==========================================================================*/

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------- GL loader
 * Pull GL2 entry points through SDL_GL_GetProcAddress. Bulletproof across
 * Mesa/NVIDIA without GL_GLEXT_PROTOTYPES link games. */
#define GLFUNCS \
  GF(PFNGLCREATESHADERPROC,      glCreateShader)      \
  GF(PFNGLSHADERSOURCEPROC,      glShaderSource)      \
  GF(PFNGLCOMPILESHADERPROC,     glCompileShader)     \
  GF(PFNGLGETSHADERIVPROC,       glGetShaderiv)       \
  GF(PFNGLGETSHADERINFOLOGPROC,  glGetShaderInfoLog)  \
  GF(PFNGLCREATEPROGRAMPROC,     glCreateProgram)     \
  GF(PFNGLATTACHSHADERPROC,      glAttachShader)      \
  GF(PFNGLLINKPROGRAMPROC,       glLinkProgram)       \
  GF(PFNGLGETPROGRAMIVPROC,      glGetProgramiv)      \
  GF(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog) \
  GF(PFNGLUSEPROGRAMPROC,        glUseProgram)        \
  GF(PFNGLGETUNIFORMLOCATIONPROC,glGetUniformLocation)\
  GF(PFNGLUNIFORM1FPROC,         glUniform1f)         \
  GF(PFNGLUNIFORM1IPROC,         glUniform1i)         \
  GF(PFNGLUNIFORM3FPROC,         glUniform3f)         \
  GF(PFNGLUNIFORM3FVPROC,        glUniform3fv)        \
  GF(PFNGLUNIFORM4FVPROC,        glUniform4fv)        \
  GF(PFNGLUNIFORMMATRIX3FVPROC,  glUniformMatrix3fv)  \
  GF(PFNGLACTIVETEXTUREPROC,     glActiveTexture_)

#define GF(t,n) static t n;
GLFUNCS
#undef GF
static void load_gl(void){
#define GF(t,n) n = (t)SDL_GL_GetProcAddress(#n[strlen(#n)-1]=='_'?"glActiveTexture":#n);
  GLFUNCS
#undef GF
}

/* ---------------------------------------------------------------- constants */
#define WINW 1280
#define WINH 720
#define G 44              /* grid cells per side  */
#define CELL 2.0f         /* world units per cell */
#define WALLH 3.2f
#define EYE 1.62f
#define PI 3.14159265358979f
#define MAXENEMY 24
#define MAXITEM 32
#define MAXLIGHT 48
#define MAXTEMPL 8
#define MAXPART 256
#define MAXVOICE 16
#define SHLIGHTS 8        /* lights fed to the shader per frame */

/* ---------------------------------------------------------------- rng/noise */
static unsigned rngs;
static unsigned xs(void){ rngs^=rngs<<13; rngs^=rngs>>17; rngs^=rngs<<5; return rngs; }
static float frand(void){ return (xs()&0xffffff)/(float)0x1000000; }

static unsigned ihash(unsigned x){
  x^=x>>16; x*=0x7feb352du; x^=x>>15; x*=0x846ca68bu; x^=x>>16; return x;
}
static float hash2(int x,int y,unsigned s){
  return (ihash((unsigned)x*374761393u + (unsigned)y*668265263u + s*1442695041u)&0xffffff)/(float)0x1000000;
}
/* tileable value noise on a power-of-two lattice */
static float vnoise(float x,float y,int per,unsigned s){
  int ix=(int)floorf(x), iy=(int)floorf(y);
  float fx=x-ix, fy=y-iy;
  fx=fx*fx*(3-2*fx); fy=fy*fy*(3-2*fy);
  int m=per-1;
  float a=hash2(ix&m,iy&m,s),     b=hash2((ix+1)&m,iy&m,s);
  float c=hash2(ix&m,(iy+1)&m,s), d=hash2((ix+1)&m,(iy+1)&m,s);
  return a+(b-a)*fx+(c-a)*fy+(a-b-c+d)*fx*fy;
}
static float fbm(float u,float v,int oct,int per,unsigned s){
  float sum=0,amp=0.5f; int p=per;
  for(int i=0;i<oct;i++){ sum+=amp*vnoise(u*p,v*p,p,s+i*131u); p<<=1; amp*=0.5f; }
  return sum;
}

/* ---------------------------------------------------------------- mat3 (col-major) */
static void m3id(float*m){ memset(m,0,36); m[0]=m[4]=m[8]=1; }
static void m3rotY(float*m,float a){ float c=cosf(a),s=sinf(a);
  m[0]=c;m[1]=0;m[2]=-s; m[3]=0;m[4]=1;m[5]=0; m[6]=s;m[7]=0;m[8]=c; }
static void m3rotX(float*m,float a){ float c=cosf(a),s=sinf(a);
  m[0]=1;m[1]=0;m[2]=0; m[3]=0;m[4]=c;m[5]=s; m[6]=0;m[7]=-s;m[8]=c; }
static void m3mul(float*o,const float*a,const float*b){ float t[9];
  for(int j=0;j<3;j++)for(int i=0;i<3;i++)
    t[j*3+i]=a[i]*b[j*3]+a[3+i]*b[j*3+1]+a[6+i]*b[j*3+2];
  memcpy(o,t,36); }
static void m3scl(float*m,float x,float y,float z){
  m[0]*=x;m[1]*=x;m[2]*=x; m[3]*=y;m[4]*=y;m[5]*=y; m[6]*=z;m[7]*=z;m[8]*=z; }
static void m3v(const float*m,float x,float y,float z,float*o){
  o[0]=m[0]*x+m[3]*y+m[6]*z; o[1]=m[1]*x+m[4]*y+m[7]*z; o[2]=m[2]*x+m[5]*y+m[8]*z; }

/* ---------------------------------------------------------------- textures */
enum { TX_WALL, TX_FLOOR, TX_CEIL, TX_GLOW, TX_COUNT };
static GLuint texAlb[TX_COUNT], texNrm[TX_COUNT];
#define TS 256

static GLuint mktex(unsigned char*px){
  GLuint t; glGenTextures(1,&t); glBindTexture(GL_TEXTURE_2D,t);
  glTexParameteri(GL_TEXTURE_2D,GL_GENERATE_MIPMAP,GL_TRUE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,TS,TS,0,GL_RGBA,GL_UNSIGNED_BYTE,px);
  return t;
}
/* heightfield -> tangent-space normal map */
static void h2n(float*h,unsigned char*out,float str){
  for(int y=0;y<TS;y++)for(int x=0;x<TS;x++){
    float l=h[y*TS+((x-1)&(TS-1))], r=h[y*TS+((x+1)&(TS-1))];
    float u=h[((y-1)&(TS-1))*TS+x], d=h[((y+1)&(TS-1))*TS+x];
    float nx=(l-r)*str, ny=(u-d)*str, nz=1.0f;
    float il=1.0f/sqrtf(nx*nx+ny*ny+nz*nz);
    unsigned char*p=&out[(y*TS+x)*4];
    p[0]=(unsigned char)((nx*il*0.5f+0.5f)*255);
    p[1]=(unsigned char)((ny*il*0.5f+0.5f)*255);
    p[2]=(unsigned char)((nz*il*0.5f+0.5f)*255);
    p[3]=255;
  }
}
static void putrgb(unsigned char*p,float r,float g,float b){
  r=r<0?0:r>1?1:r; g=g<0?0:g>1?1:g; b=b<0?0:b>1?1:b;
  p[0]=(unsigned char)(r*255); p[1]=(unsigned char)(g*255); p[2]=(unsigned char)(b*255); p[3]=255;
}

static void gen_textures(void){
  float *hh=malloc(TS*TS*sizeof(float));
  unsigned char *alb=malloc(TS*TS*4), *nrm=malloc(TS*TS*4);

  /* --- bricks (walls): offset rows, mortar grooves, per-brick hue jitter --- */
  for(int y=0;y<TS;y++)for(int x=0;x<TS;x++){
    float u=x/(float)TS, v=y/(float)TS;
    int row=y/32, xo=(row&1)?32:0;
    int bx=(x+xo)&63, by=y&31;
    float dx=bx<32?bx:63-bx, dy=by<16?by:31-by;
    float d=dx<dy?dx:dy;
    float bevel=d/4.0f; if(bevel>1)bevel=1;
    bevel=bevel*bevel*(3-2*bevel);
    float grain=fbm(u,v,5,8,77u);
    float h=bevel*0.85f + grain*0.15f;
    hh[y*TS+x]=h;
    unsigned id=ihash((unsigned)row*73u + (unsigned)((x+xo)>>6)*157u + 9u);
    float j=((id&255)/255.0f-0.5f)*0.16f;
    float grime=fbm(u+3.1f,v+1.7f,4,4,501u);
    float br,bg,bb;
    if(bevel<0.45f){ br=bg=bb=0.20f+grain*0.06f; }            /* mortar */
    else { br=0.42f+j; bg=0.38f+j*0.8f; bb=0.31f+j*0.5f; }    /* brick  */
    float ao=0.55f+0.45f*h;
    float gr=1.0f-0.5f*(grime>0.55f?(grime-0.55f)*2.2f:0.0f);
    putrgb(&alb[(y*TS+x)*4],br*ao*gr,bg*ao*gr,bb*ao*gr*1.04f);
  }
  h2n(hh,nrm,3.0f);
  texAlb[TX_WALL]=mktex(alb); texNrm[TX_WALL]=mktex(nrm);

  /* --- floor: big grimy tiles --- */
  for(int y=0;y<TS;y++)for(int x=0;x<TS;x++){
    float u=x/(float)TS, v=y/(float)TS;
    int tx=x&127, ty=y&127;
    float dx=tx<64?tx:127-tx, dy=ty<64?ty:127-ty;
    float d=dx<dy?dx:dy;
    float bevel=d/5.0f; if(bevel>1)bevel=1;
    float grain=fbm(u,v,5,16,901u);
    float h=bevel*0.7f+grain*0.3f;
    hh[y*TS+x]=h;
    float grime=fbm(u*1.3f,v*1.3f,5,4,313u);
    float base=0.19f+grain*0.08f;
    base*= (bevel<0.4f)?0.55f:1.0f;
    base*= 1.0f-0.65f*grime;
    base = base<0?0:base;
    putrgb(&alb[(y*TS+x)*4],base*1.02f,base,base*0.86f);
  }
  h2n(hh,nrm,2.2f);
  texAlb[TX_FLOOR]=mktex(alb); texNrm[TX_FLOOR]=mktex(nrm);

  /* --- ceiling: bolted metal plates with rust --- */
  for(int y=0;y<TS;y++)for(int x=0;x<TS;x++){
    float u=x/(float)TS, v=y/(float)TS;
    int px_=x&63, py=y&63;
    float dx=px_<32?px_:63-px_, dy=py<32?py:63-py;
    float d=dx<dy?dx:dy;
    float h=d/5.0f; if(h>1)h=1;
    /* bolts inset from each plate corner */
    float bolt=0;
    for(int cy=0;cy<2;cy++)for(int cx=0;cx<2;cx++){
      float bxp=cx?55.0f:8.0f, byp=cy?55.0f:8.0f;
      float rx=px_-bxp, ry=py-byp;
      float r2=rx*rx+ry*ry;
      bolt+=expf(-r2/7.0f);
    }
    float brush=fbm(u*1.0f,v*8.0f,3,8,71u);     /* anisotropic streaks */
    h=h*0.8f + bolt*0.5f + brush*0.12f;
    hh[y*TS+x]=h;
    float rust=fbm(u+9.f,v+2.f,5,4,1207u);
    float r=0.33f,g=0.35f,b=0.38f;
    r+= (brush-0.5f)*0.1f; g+=(brush-0.5f)*0.1f; b+=(brush-0.5f)*0.1f;
    if(rust>0.58f){ float k=(rust-0.58f)*2.4f; if(k>1)k=1;
      r=r*(1-k)+0.40f*k; g=g*(1-k)+0.22f*k; b=b*(1-k)+0.11f*k; }
    float ao=0.5f+0.5f*(h>1?1:h);
    putrgb(&alb[(y*TS+x)*4],r*ao,g*ao,b*ao);
  }
  h2n(hh,nrm,2.6f);
  texAlb[TX_CEIL]=mktex(alb); texNrm[TX_CEIL]=mktex(nrm);

  /* --- radial glow sprite --- */
  for(int y=0;y<TS;y++)for(int x=0;x<TS;x++){
    float dx=(x-128)/128.0f, dy=(y-128)/128.0f;
    float r=sqrtf(dx*dx+dy*dy);
    float a=1.0f-r; if(a<0)a=0; a=a*a*a;
    unsigned char*p=&alb[(y*TS+x)*4];
    p[0]=p[1]=p[2]=(unsigned char)(a*255); p[3]=(unsigned char)(a*255);
  }
  texAlb[TX_GLOW]=mktex(alb);

  free(hh); free(alb); free(nrm);
}

/* ---------------------------------------------------------------- 5x7 bitfont
 * 0-9 A-Z and '-' ; 7 row bytes per glyph, bit4 = leftmost column. */
static const unsigned char font[37][7]={
 {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},{0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
 {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F},{0x1F,0x02,0x04,0x02,0x01,0x11,0x0E},
 {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},{0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
 {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},{0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
 {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},{0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
 {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},{0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
 {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},{0x1C,0x12,0x11,0x11,0x11,0x12,0x1C},
 {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},{0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
 {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F},{0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
 {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},{0x07,0x02,0x02,0x02,0x02,0x12,0x0C},
 {0x11,0x12,0x14,0x18,0x14,0x12,0x11},{0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
 {0x11,0x1B,0x15,0x15,0x11,0x11,0x11},{0x11,0x19,0x15,0x13,0x11,0x11,0x11},
 {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},{0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
 {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},{0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
 {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},{0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
 {0x11,0x11,0x11,0x11,0x11,0x11,0x0E},{0x11,0x11,0x11,0x11,0x11,0x0A,0x04},
 {0x11,0x11,0x11,0x15,0x15,0x1B,0x11},{0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
 {0x11,0x11,0x0A,0x04,0x04,0x04,0x04},{0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
 {0x00,0x00,0x00,0x1F,0x00,0x00,0x00}};

static float textw(const char*s,float sc){ return (float)strlen(s)*6*sc; }
static void draw_text(float x,float y,float sc,const char*s){
  glBegin(GL_QUADS);
  for(;*s;s++,x+=6*sc){
    int gi=-1; char c=*s;
    if(c>='0'&&c<='9')gi=c-'0'; else if(c>='A'&&c<='Z')gi=10+c-'A'; else if(c=='-')gi=36;
    if(gi<0)continue;
    for(int r=0;r<7;r++){ unsigned char row=font[gi][r];
      for(int col=0;col<5;col++) if(row&(0x10>>col)){
        float px=x+col*sc, py=y+r*sc, e=sc*0.92f;
        glVertex2f(px,py); glVertex2f(px+e,py); glVertex2f(px+e,py+e); glVertex2f(px,py+e);
      }}}
  glEnd();
}

/* ---------------------------------------------------------------- level */
static unsigned char grid[G][G];           /* 1 = solid */
typedef struct { int x,y,w,h; } Room;
static Room rooms[12]; static int nrooms;
static float startx,startz, exitx,exitz;

typedef struct { float x,y,z,r; float cr,cg,cb; } Light;
static Light lights[MAXLIGHT]; static int nlights;
typedef struct { float x,y,z,r,life; float cr,cg,cb; } TempL;
static TempL templ_[MAXTEMPL];

typedef struct { float x,z; int type; int taken; } Item;  /* 0 health 1 ammo */
static Item items[MAXITEM]; static int nitems;

typedef struct {
  float x,z,yaw,hp,flash,anim,phase,state_t,deadT;
  int state;   /* 0 sleep 1 chase 2 windup 3 cooldown 4 dead */
} Enemy;
static Enemy en[MAXENEMY]; static int nen;

/* level mesh batches: 0 walls 1 floor 2 ceil; interleaved p3 n3 uv2 */
static float *batch[3]; static int bn[3], bcap[3];
static void emit_v(int b,float px,float py,float pz,float nx,float ny,float nz){
  if(bn[b]+8>bcap[b]){ bcap[b]=bcap[b]?bcap[b]*2:4096; batch[b]=realloc(batch[b],bcap[b]*sizeof(float)); }
  /* tangent/bitangent from axis-aligned normal — must match shader */
  float tx,ty,tz,bx,by,bz;
  if(fabsf(ny)>0.5f){ tx=1;ty=0;tz=0; } else { tx=nz;ty=0;tz=-nx; } /* cross((0,1,0),N) */
  bx=ny*tz-nz*ty; by=nz*tx-nx*tz; bz=nx*ty-ny*tx;                   /* cross(N,T)      */
  float u=(px*tx+py*ty+pz*tz)*0.5f, v=(px*bx+py*by+pz*bz)*0.5f;
  float*o=&batch[b][bn[b]];
  o[0]=px;o[1]=py;o[2]=pz; o[3]=nx;o[4]=ny;o[5]=nz; o[6]=u;o[7]=v;
  bn[b]+=8;
}
static int solid(int cx,int cz){ if(cx<0||cz<0||cx>=G||cz>=G)return 1; return grid[cz][cx]; }

static void carve_room(int x,int y,int w,int h){
  for(int j=y;j<y+h;j++)for(int i=x;i<x+w;i++) grid[j][i]=0;
}
static void add_light(float x,float y,float z,float r,float cr,float cg,float cb){
  if(nlights<MAXLIGHT){ Light*l=&lights[nlights++]; l->x=x;l->y=y;l->z=z;l->r=r;l->cr=cr;l->cg=cg;l->cb=cb; }
}
static void gen_level(unsigned seed){
  rngs=seed?seed:0xC0FFEEu;
  memset(grid,1,sizeof grid);
  nrooms=0; nlights=0; nitems=0; nen=0;
  for(int b=0;b<3;b++)bn[b]=0;

  /* rooms */
  for(int tries=0; tries<80 && nrooms<9; tries++){
    int w=4+(int)(frand()*4), h=4+(int)(frand()*4);
    int x=2+(int)(frand()*(G-w-4)), y=2+(int)(frand()*(G-h-4));
    int ok=1;
    for(int r=0;r<nrooms;r++){
      Room*q=&rooms[r];
      if(x<q->x+q->w+2 && q->x<x+w+2 && y<q->y+q->h+2 && q->y<y+h+2){ ok=0; break; }
    }
    if(!ok)continue;
    rooms[nrooms].x=x;rooms[nrooms].y=y;rooms[nrooms].w=w;rooms[nrooms].h=h;
    carve_room(x,y,w,h); nrooms++;
  }
  /* corridors: L-shapes chaining room centres */
  for(int r=0;r+1<nrooms;r++){
    int ax=rooms[r].x+rooms[r].w/2,   ay=rooms[r].y+rooms[r].h/2;
    int bx=rooms[r+1].x+rooms[r+1].w/2, by=rooms[r+1].y+rooms[r+1].h/2;
    int x=ax,y=ay;
    int horiz_first = xs()&1;
    while(x!=bx||y!=by){
      grid[y][x]=0;
      if(horiz_first ? (x!=bx) : 0) x+= bx>x?1:-1;
      else if(y!=by) y+= by>y?1:-1;
      else x+= bx>x?1:-1;
    }
    grid[by][bx]=0;
    /* dim corridor light at the elbow */
    float ex=(horiz_first?bx:ax)+0.5f, ez=(horiz_first?ay:by)+0.5f;
    add_light(ex*CELL,2.5f,ez*CELL,6.0f,0.40f,0.45f,0.55f);
  }
  startx=(rooms[0].x+rooms[0].w*0.5f)*CELL; startz=(rooms[0].y+rooms[0].h*0.5f)*CELL;
  exitx =(rooms[nrooms-1].x+rooms[nrooms-1].w*0.5f)*CELL;
  exitz =(rooms[nrooms-1].y+rooms[nrooms-1].h*0.5f)*CELL;

  /* room lights: alternate warm sodium / cold mercury, kkrieger murk */
  for(int r=0;r<nrooms;r++){
    float cx=(rooms[r].x+rooms[r].w*0.5f)*CELL, cz=(rooms[r].y+rooms[r].h*0.5f)*CELL;
    float jit=(frand()-0.5f)*2.0f;
    if(r&1) add_light(cx+jit,2.7f,cz-jit,9.5f, 2.4f,1.55f,0.75f);
    else    add_light(cx-jit,2.7f,cz+jit,9.5f, 0.85f,1.35f,2.3f);
    if(rooms[r].w*rooms[r].h>30)
      add_light(cx+3,2.6f,cz+3,6.5f, 1.1f,0.8f,0.45f);
  }
  add_light(exitx,2.2f,exitz,8.0f, 0.7f,2.2f,1.3f);   /* exit: sickly green */

  /* items */
  for(int r=1;r<nrooms;r++){
    int n=1+(rooms[r].w*rooms[r].h>30);
    for(int k=0;k<n && nitems<MAXITEM;k++){
      Item*it=&items[nitems++];
      it->x=(rooms[r].x+1+frand()*(rooms[r].w-2))*CELL;
      it->z=(rooms[r].y+1+frand()*(rooms[r].h-2))*CELL;
      it->type=xs()&1; it->taken=0;
    }
  }
  items[nitems].x=startx+1.5f; items[nitems].z=startz; items[nitems].type=1; items[nitems].taken=0; nitems++;

  /* enemies: none in the start room, extra guarding the exit */
  for(int r=1;r<nrooms && nen<MAXENEMY-2;r++){
    int n=1+rooms[r].w*rooms[r].h/14; if(n>3)n=3;
    if(r==nrooms-1)n+=2;
    for(int k=0;k<n && nen<MAXENEMY;k++){
      Enemy*e=&en[nen++];
      e->x=(rooms[r].x+1+frand()*(rooms[r].w-2))*CELL;
      e->z=(rooms[r].y+1+frand()*(rooms[r].h-2))*CELL;
      e->yaw=frand()*2*PI; e->hp=30; e->state=0; e->flash=0; e->anim=0;
      e->phase=frand()*6.28f; e->state_t=0; e->deadT=0;
    }
  }

  /* mesh: floors, ceilings, boundary walls */
  for(int z=0;z<G;z++)for(int x=0;x<G;x++){
    if(grid[z][x])continue;
    float x0=x*CELL,x1=x0+CELL,z0=z*CELL,z1=z0+CELL;
    emit_v(1,x0,0,z0, 0,1,0); emit_v(1,x1,0,z0, 0,1,0);
    emit_v(1,x1,0,z1, 0,1,0); emit_v(1,x0,0,z1, 0,1,0);
    emit_v(2,x0,WALLH,z0, 0,-1,0); emit_v(2,x0,WALLH,z1, 0,-1,0);
    emit_v(2,x1,WALLH,z1, 0,-1,0); emit_v(2,x1,WALLH,z0, 0,-1,0);
    if(solid(x-1,z)){ emit_v(0,x0,0,z0, 1,0,0); emit_v(0,x0,0,z1, 1,0,0);
                      emit_v(0,x0,WALLH,z1, 1,0,0); emit_v(0,x0,WALLH,z0, 1,0,0); }
    if(solid(x+1,z)){ emit_v(0,x1,0,z1, -1,0,0); emit_v(0,x1,0,z0, -1,0,0);
                      emit_v(0,x1,WALLH,z0, -1,0,0); emit_v(0,x1,WALLH,z1, -1,0,0); }
    if(solid(x,z-1)){ emit_v(0,x1,0,z0, 0,0,1); emit_v(0,x0,0,z0, 0,0,1);
                      emit_v(0,x0,WALLH,z0, 0,0,1); emit_v(0,x1,WALLH,z0, 0,0,1); }
    if(solid(x,z+1)){ emit_v(0,x0,0,z1, 0,0,-1); emit_v(0,x1,0,z1, 0,0,-1);
                      emit_v(0,x1,WALLH,z1, 0,0,-1); emit_v(0,x0,WALLH,z1, 0,0,-1); }
  }
}

/* ---------------------------------------------------------------- audio synth */
enum { V_SHOT, V_TICK, V_EDIE, V_HURT, V_PICK, V_STEP, V_CLICK, V_WIN };
typedef struct { int type,on; float t; } Voice;
static Voice voices[MAXVOICE];
static int audioOK=0; static SDL_AudioDeviceID adev;
static unsigned arng=0xBADC0DEu;
static float arand(void){ arng^=arng<<13;arng^=arng>>17;arng^=arng<<5; return (arng&0xffffff)/(float)0x800000-1.0f; }
static float saw(float ph){ return 2.0f*(ph-floorf(ph))-1.0f; }

static void sfx(int type){
  if(!audioOK)return;
  for(int i=0;i<MAXVOICE;i++) if(!voices[i].on){
    voices[i].type=type; voices[i].t=0; voices[i].on=1; return; }
}
static void audio_cb(void*ud,Uint8*stream,int len){
  (void)ud;
  float*out=(float*)stream; int n=len/4;
  static double gt=0; static float lpn=0, lps=0;
  for(int i=0;i<n;i++){
    float s=0;
    /* ambient drone: detuned 55Hz pair + filtered rumble, slow swell */
    float sw=1.0f+0.35f*sinf((float)(gt*0.6));
    lpn+=0.015f*(arand()-lpn);
    s+= (0.045f*sinf((float)(gt*2*PI*55.0)) + 0.04f*sinf((float)(gt*2*PI*55.8)) + lpn*0.6f)*sw*0.7f;
    for(int v=0;v<MAXVOICE;v++){
      if(!voices[v].on)continue;
      float t=voices[v].t;
      switch(voices[v].type){
        case V_SHOT:{ float nz=arand()*expf(-t*28)*0.55f;
          float f=150.0f-t*420.0f; if(f<35)f=35;
          float th=sinf(2*PI*f*t)*expf(-t*16)*0.8f;
          s+=nz+th; if(t>0.45f)voices[v].on=0; }break;
        case V_TICK: s+=arand()*expf(-t*90)*0.4f + sinf(2*PI*880*t)*expf(-t*55)*0.25f;
          if(t>0.12f)voices[v].on=0;
          break;
        case V_EDIE:{ float f=200.0f*expf(-t*4)+45; s+=saw(f*t)*expf(-t*5)*0.35f;
          if(t>0.7f)voices[v].on=0; }break;
        case V_HURT:{ float sq=sinf(2*PI*68*t)>0?1:-1; s+=sq*expf(-t*11)*0.4f;
          if(t>0.35f)voices[v].on=0; }break;
        case V_PICK:{ float f=t<0.1f?620:930; s+=sinf(2*PI*f*t)*expf(-t*9)*0.3f;
          if(t>0.3f)voices[v].on=0; }break;
        case V_STEP: lps+=0.25f*(arand()-lps); s+=lps*expf(-t*70)*0.9f;
          if(t>0.08f)voices[v].on=0;
          break;
        case V_CLICK: s+=sinf(2*PI*1400*t)*expf(-t*180)*0.25f;
          if(t>0.05f)voices[v].on=0;
          break;
        case V_WIN:{ float f= t<0.18f?330: t<0.36f?440: 660;
          s+=sinf(2*PI*f*t)*expf(-(t>0.36f?(t-0.36f)*4:0))*0.28f;
          if(t>1.1f)voices[v].on=0; }break;
      }
      voices[v].t+=1.0f/44100.0f;
    }
    s=tanhf(s*1.3f)*0.8f;
    out[i]=s; gt+=1.0/44100.0;
  }
}

/* ---------------------------------------------------------------- shaders */
static GLuint prog;
static GLint uCam,uNL,uLpos,uLcol,uM3,uT,uTint,uBump,uEmis,uAlb,uNrm;
static const char*VS=
"#version 120\n"
"uniform mat3 uM3; uniform vec3 uT;\n"
"varying vec3 vP; varying vec3 vN; varying vec2 vUV;\n"
"void main(){\n"
"  vec3 wp = uM3*gl_Vertex.xyz + uT;\n"
"  vP=wp; vN=uM3*gl_Normal; vUV=gl_MultiTexCoord0.xy;\n"
"  gl_Position = gl_ModelViewProjectionMatrix * vec4(wp,1.0);\n"
"}\n";
static const char*FS=
"#version 120\n"
"uniform sampler2D uAlb; uniform sampler2D uNrm;\n"
"uniform vec3 uCam; uniform int uNL;\n"
"uniform vec4 uLpos[8]; uniform vec3 uLcol[8];\n"
"uniform vec3 uTint; uniform float uBump; uniform float uEmis;\n"
"varying vec3 vP; varying vec3 vN; varying vec2 vUV;\n"
"void main(){\n"
"  vec3 base = texture2D(uAlb,vUV).rgb * uTint;\n"
"  vec3 N = normalize(vN);\n"
"  if(uBump>0.5){\n"
"    vec3 T = (abs(N.y)>0.5)? vec3(1.0,0.0,0.0) : vec3(N.z,0.0,-N.x);\n"
"    vec3 B = cross(N,T);\n"
"    vec3 tn = texture2D(uNrm,vUV).xyz*2.0-1.0;\n"
"    N = normalize(T*tn.x + B*tn.y + N*tn.z);\n"
"  }\n"
"  vec3 col = base*0.022;\n"
"  vec3 V = normalize(uCam - vP);\n"
"  for(int i=0;i<8;i++){ if(i>=uNL)break;\n"
"    vec3 Ld = uLpos[i].xyz - vP;\n"
"    float d = length(Ld); Ld/=d;\n"
"    float a = max(0.0, 1.0 - d/uLpos[i].w); a*=a;\n"
"    float dif = max(dot(N,Ld),0.0);\n"
"    vec3 H = normalize(Ld+V);\n"
"    float spec = pow(max(dot(N,H),0.0),28.0)*0.3;\n"
"    col += uLcol[i]*a*(base*dif + vec3(spec)*a);\n"
"  }\n"
"  col = mix(col, base, uEmis);\n"
"  float fd = clamp((distance(uCam,vP)-5.0)/24.0, 0.0, 1.0);\n"
"  col = mix(col, vec3(0.012,0.014,0.018), fd);\n"
"  gl_FragColor = vec4(sqrt(col),1.0);\n"
"}\n";

static GLuint shader(GLenum ty,const char*src){
  GLuint s=glCreateShader(ty);
  glShaderSource(s,1,&src,0); glCompileShader(s);
  GLint ok; glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
  if(!ok){ char log[2048]; glGetShaderInfoLog(s,2048,0,log);
    fprintf(stderr,"[murkk] shader fail:\n%s\n",log); exit(1); }
  return s;
}
static void init_shaders(void){
  prog=glCreateProgram();
  glAttachShader(prog,shader(GL_VERTEX_SHADER,VS));
  glAttachShader(prog,shader(GL_FRAGMENT_SHADER,FS));
  glLinkProgram(prog);
  GLint ok; glGetProgramiv(prog,GL_LINK_STATUS,&ok);
  if(!ok){ char log[2048]; glGetProgramInfoLog(prog,2048,0,log);
    fprintf(stderr,"[murkk] link fail:\n%s\n",log); exit(1); }
  uCam=glGetUniformLocation(prog,"uCam");   uNL =glGetUniformLocation(prog,"uNL");
  uLpos=glGetUniformLocation(prog,"uLpos[0]"); uLcol=glGetUniformLocation(prog,"uLcol[0]");
  uM3 =glGetUniformLocation(prog,"uM3");    uT  =glGetUniformLocation(prog,"uT");
  uTint=glGetUniformLocation(prog,"uTint"); uBump=glGetUniformLocation(prog,"uBump");
  uEmis=glGetUniformLocation(prog,"uEmis");
  uAlb=glGetUniformLocation(prog,"uAlb");   uNrm=glGetUniformLocation(prog,"uNrm");
}

/* ---------------------------------------------------------------- particles */
typedef struct { float x,y,z,vx,vy,vz,life,max; float cr,cg,cb; } Part;
static Part parts[MAXPART]; static int pHead=0;
static void spawn_parts(int n,float x,float y,float z,float spd,float cr,float cg,float cb){
  for(int i=0;i<n;i++){
    Part*p=&parts[pHead]; pHead=(pHead+1)%MAXPART;
    float a=frand()*2*PI, b=(frand()-0.5f)*PI;
    p->x=x;p->y=y;p->z=z;
    p->vx=cosf(a)*cosf(b)*spd*(0.4f+frand());
    p->vy=sinf(b)*spd*(0.4f+frand())+1.5f;
    p->vz=sinf(a)*cosf(b)*spd*(0.4f+frand());
    p->life=p->max=0.25f+frand()*0.3f;
    p->cr=cr;p->cg=cg;p->cb=cb;
  }
}

/* ---------------------------------------------------------------- game state */
enum { ST_TITLE, ST_PLAY, ST_DEAD, ST_WIN };
static int gstate=ST_TITLE;
static float px,pz,pyaw,ppitch,php,pammo;
static float fireCD,dmgFlash,stepT,shake,kick,flashT,bobT,winT,deadT,gtime;
static unsigned gseed=0xC0FFEEu;
static int smoke=0;

static void reset_game(void){
  gen_level(gseed);
  px=startx; pz=startz; pyaw=0; ppitch=0;
  php=100; pammo=36;
  fireCD=dmgFlash=stepT=shake=kick=flashT=bobT=winT=deadT=0;
  for(int i=0;i<MAXTEMPL;i++)templ_[i].life=0;
  for(int i=0;i<MAXPART;i++)parts[i].life=0;
}

/* circle-vs-grid, axis separated */
static int circ_free(float x,float z,float r){
  for(int dz=-1;dz<=1;dz++)for(int dx=-1;dx<=1;dx++){
    int cx=(int)floorf(x/CELL)+dx, cz=(int)floorf(z/CELL)+dz;
    if(!solid(cx,cz))continue;
    float bx0=cx*CELL,bx1=bx0+CELL,bz0=cz*CELL,bz1=bz0+CELL;
    float nx=x<bx0?bx0:(x>bx1?bx1:x), nz=z<bz0?bz0:(z>bz1?bz1:z);
    float ddx=x-nx,ddz=z-nz;
    if(ddx*ddx+ddz*ddz < r*r) return 0;
  }
  return 1;
}
static void move_circ(float*x,float*z,float dx,float dz,float r){
  if(circ_free(*x+dx,*z,r)) *x+=dx;
  if(circ_free(*x,*z+dz,r)) *z+=dz;
}
/* DDA ray vs grid; returns hit distance (<= maxd) */
static float ray_wall(float ox,float oy,float oz,float dx,float dy,float dz,float maxd){
  (void)oy;(void)dy;
  float t=0; int cx=(int)floorf(ox/CELL), cz=(int)floorf(oz/CELL);
  int sx=dx>0?1:-1, sz=dz>0?1:-1;
  float tdx=fabsf(dx)>1e-6f?CELL/fabsf(dx):1e9f, tdz=fabsf(dz)>1e-6f?CELL/fabsf(dz):1e9f;
  float nx=(sx>0?(cx+1)*CELL-ox:ox-cx*CELL), nz=(sz>0?(cz+1)*CELL-oz:oz-cz*CELL);
  float tx=fabsf(dx)>1e-6f?nx/fabsf(dx):1e9f, tz=fabsf(dz)>1e-6f?nz/fabsf(dz):1e9f;
  for(int it=0;it<128;it++){
    if(tx<tz){ t=tx; tx+=tdx; cx+=sx; } else { t=tz; tz+=tdz; cz+=sz; }
    if(t>maxd)return maxd;
    if(solid(cx,cz))return t;
  }
  return maxd;
}
static int los(float ax,float az,float bx,float bz){
  float dx=bx-ax,dz=bz-az; float d=sqrtf(dx*dx+dz*dz);
  if(d<0.01f)return 1;
  return ray_wall(ax,1.0f,az,dx/d,0,dz/d,d) >= d-0.05f;
}
static void add_templ(float x,float y,float z,float r,float life,float cr,float cg,float cb){
  for(int i=0;i<MAXTEMPL;i++) if(templ_[i].life<=0){
    templ_[i]=(TempL){x,y,z,r,life,cr,cg,cb}; return; }
}

/* ---------------------------------------------------------------- combat */
static void fire(void){
  if(fireCD>0)return;
  if(pammo<1){ sfx(V_CLICK); fireCD=0.25f; return; }
  pammo--; fireCD=0.27f; flashT=0.07f; kick=1.0f;
  sfx(V_SHOT);
  float yr=pyaw*PI/180, pr=ppitch*PI/180;
  float dx=sinf(yr)*cosf(pr), dy=-sinf(pr), dz=-cosf(yr)*cosf(pr);
  float ox=px, oy=EYE, oz=pz;
  float tw=ray_wall(ox,oy,oz,dx,dy,dz,40.0f);
  int hit=-1; float th=tw;
  for(int i=0;i<nen;i++){
    if(en[i].state==4)continue;
    float cx=en[i].x-ox, cy=0.70f-oy, cz=en[i].z-oz;
    float b=cx*dx+cy*dy+cz*dz;
    if(b<0)continue;
    float d2=cx*cx+cy*cy+cz*cz-b*b;
    float r=0.65f;
    if(d2<r*r){ float t=b-sqrtf(r*r-d2); if(t<th){ th=t; hit=i; } }
  }
  add_templ(ox+dx*0.7f,oy+dy*0.7f-0.1f,oz+dz*0.7f,5.0f,0.07f,3.5f,2.8f,1.6f);
  if(hit>=0){
    Enemy*e=&en[hit]; e->hp-=12; e->flash=0.12f;
    if(e->state==0){ e->state=1; }
    spawn_parts(8,ox+dx*th,oy+dy*th,oz+dz*th,2.5f,0.5f,0.08f,0.05f);
    sfx(V_TICK);
    if(e->hp<=0){ e->state=4; e->deadT=0; sfx(V_EDIE);
      spawn_parts(20,e->x,0.6f,e->z,3.5f,0.45f,0.07f,0.05f); }
  } else if(tw<39.9f){
    spawn_parts(10,ox+dx*tw,oy+dy*tw,oz+dz*tw,3.0f,1.0f,0.7f,0.3f);
    add_templ(ox+dx*(tw-0.1f),oy+dy*tw,oz+dz*(tw-0.1f),3.0f,0.1f,2.0f,1.4f,0.6f);
  }
}

static void hurt_player(float dmg){
  php-=dmg; dmgFlash=0.5f; shake=0.3f; sfx(V_HURT);
  if(php<=0){ php=0; gstate=ST_DEAD; deadT=0; SDL_SetRelativeMouseMode(SDL_FALSE); }
}

static void update_enemies(float dt){
  for(int i=0;i<nen;i++){
    Enemy*e=&en[i];
    if(e->flash>0)e->flash-=dt;
    if(e->state==4){ e->deadT+=dt; continue; }
    float dx=px-e->x, dz=pz-e->z, d=sqrtf(dx*dx+dz*dz);
    switch(e->state){
      case 0: /* sleep */
        if(d<11.0f && los(e->x,e->z,px,pz)) e->state=1;
        break;
      case 1:{ /* chase */
        e->yaw=atan2f(dx,-dz);
        float spd=2.7f;
        float mx=dx/d*spd*dt, mz=dz/d*spd*dt;
        float ox=e->x, oz=e->z;
        move_circ(&e->x,&e->z,mx,mz,0.32f);
        if(fabsf(e->x-ox)+fabsf(e->z-oz) < spd*dt*0.25f){ /* stuck: sidestep */
          float a=e->yaw+ (sinf(gtime*3+e->phase)>0?1.4f:-1.4f);
          move_circ(&e->x,&e->z,sinf(a)*spd*dt,-cosf(a)*spd*dt,0.32f);
        }
        e->anim+=dt*9.0f;
        if(d<1.4f){ e->state=2; e->state_t=0; }
      } break;
      case 2: /* windup */
        e->state_t+=dt;
        e->yaw=atan2f(dx,-dz);
        if(e->state_t>0.35f){
          if(d<1.7f) hurt_player(12);
          e->state=3; e->state_t=0;
        } break;
      case 3: /* cooldown */
        e->state_t+=dt;
        if(e->state_t>0.7f) e->state=1;
        break;
    }
    /* don't let enemies overlap the player point-blank */
    if(e->state!=4 && d<0.7f && d>0.001f){
      e->x-= dx/d*(0.7f-d); e->z-= dz/d*(0.7f-d);
    }
  }
}

/* ---------------------------------------------------------------- drawing */
static void set_uM(const float*m,float tx,float ty,float tz){
  glUniformMatrix3fv(uM3,1,GL_FALSE,m); glUniform3f(uT,tx,ty,tz);
}
static void box_sh(float sx,float sy,float sz){ /* shader-lit box, centred */
  float x=sx*0.5f,y=sy*0.5f,z=sz*0.5f;
  glBegin(GL_QUADS);
  glNormal3f(0,0,1);  glTexCoord2f(0,0);
  glVertex3f(-x,-y,z); glVertex3f(x,-y,z); glVertex3f(x,y,z); glVertex3f(-x,y,z);
  glNormal3f(0,0,-1);
  glVertex3f(x,-y,-z); glVertex3f(-x,-y,-z); glVertex3f(-x,y,-z); glVertex3f(x,y,-z);
  glNormal3f(1,0,0);
  glVertex3f(x,-y,z); glVertex3f(x,-y,-z); glVertex3f(x,y,-z); glVertex3f(x,y,z);
  glNormal3f(-1,0,0);
  glVertex3f(-x,-y,-z); glVertex3f(-x,-y,z); glVertex3f(-x,y,z); glVertex3f(-x,y,-z);
  glNormal3f(0,1,0);
  glVertex3f(-x,y,z); glVertex3f(x,y,z); glVertex3f(x,y,-z); glVertex3f(-x,y,-z);
  glNormal3f(0,-1,0);
  glVertex3f(-x,-y,-z); glVertex3f(x,-y,-z); glVertex3f(x,-y,z); glVertex3f(-x,-y,z);
  glEnd();
}
static void draw_enemy(Enemy*e){
  float M[9],R[9],S[9];
  float squash = e->state==4 ? (e->deadT>0.5f?0.08f:1.0f-e->deadT*1.84f) : 1.0f;
  if(squash<0.08f)squash=0.08f;
  float pitch = e->state==2 ? -0.55f*sinf(e->state_t/0.35f*PI) : 0.0f;
  float fl = e->flash>0?1.0f:0.0f;
  glUniform3f(uTint, 0.17f+fl*0.8f, 0.19f+fl*0.05f, 0.15f+fl*0.05f);
  glUniform1f(uBump,0); glUniform1f(uEmis,0);

  m3rotY(R,e->yaw); m3rotX(S,pitch); m3mul(M,R,S);
  float B[9]; memcpy(B,M,36); m3scl(B,1.0f,squash,1.0f);
  set_uM(B, e->x, 0.55f*squash+0.02f, e->z);
  box_sh(0.55f,0.38f,0.75f);                              /* body */
  float hofs[3]; m3v(M,0,0.12f,-0.42f,hofs);
  float H[9]; memcpy(H,M,36); m3scl(H,1,squash,1);
  set_uM(H, e->x+hofs[0], 0.55f*squash+hofs[1]*squash, e->z+hofs[2]);
  box_sh(0.32f,0.26f,0.3f);                               /* head */
  /* eyes: emissive */
  glUniform1f(uEmis,1); glUniform3f(uTint, 2.2f,0.6f,0.1f);
  for(int s=-1;s<=1;s+=2){
    float eo[3]; m3v(M,s*0.08f,0.14f,-0.58f,eo);
    set_uM(H, e->x+eo[0], 0.55f*squash+eo[1]*squash, e->z+eo[2]);
    box_sh(0.06f,0.06f,0.02f);
  }
  glUniform1f(uEmis,0); glUniform3f(uTint,0.15f,0.17f,0.13f);
  /* four legs, sin-swung when chasing */
  for(int li=0;li<4;li++){
    float sx=(li&1)?0.30f:-0.30f, sz=(li&2)?0.26f:-0.26f;
    float swing = (e->state==1)? sinf(e->anim + (li&1?PI:0) + (li&2?PI:0))*0.6f : 0.15f;
    if(e->state==4)swing=1.2f;
    float L[9],RX[9];
    m3rotX(RX,swing);
    m3mul(L,M,RX);
    float ho[3]; m3v(M,sx,0,sz,ho);
    float lo[3]; m3v(L,0,-0.28f,0,lo);
    set_uM(L, e->x+ho[0]+lo[0], 0.55f*squash+ho[1]+lo[1], e->z+ho[2]+lo[2]);
    box_sh(0.09f,0.55f,0.09f);
  }
}
static void draw_items(void){
  float M[9];
  for(int i=0;i<nitems;i++){
    if(items[i].taken)continue;
    float bob=0.45f+0.1f*sinf(gtime*2.5f+i);
    m3rotY(M,gtime*1.5f+i);
    glUniform1f(uEmis,1); glUniform1f(uBump,0);
    if(items[i].type==0) glUniform3f(uTint,1.6f,0.15f,0.12f);
    else                 glUniform3f(uTint,1.4f,1.1f,0.15f);
    set_uM(M,items[i].x,bob,items[i].z);
    box_sh(0.26f,0.26f,0.26f);
    float M2[9]; memcpy(M2,M,36); float R[9]; m3rotX(R,PI/4); m3mul(M2,M,R);
    set_uM(M2,items[i].x,bob,items[i].z);
    box_sh(0.22f,0.22f,0.22f);
  }
}
static void billboard(float x,float y,float z,float s,float r,float g,float b,float a,float ryaw){
  float yr=ryaw*PI/180;
  float rx=cosf(yr), rz=sinf(yr);
  glColor4f(r,g,b,a);
  glTexCoord2f(0,0); glVertex3f(x-rx*s-0,y-s,z-rz*s);
  glTexCoord2f(1,0); glVertex3f(x+rx*s,y-s,z+rz*s);
  glTexCoord2f(1,1); glVertex3f(x+rx*s,y+s,z+rz*s);
  glTexCoord2f(0,1); glVertex3f(x-rx*s,y+s,z-rz*s);
}

static void draw_world(float camx,float camy,float camz){
  glUseProgram(prog);
  glUniform3f(uCam,camx,camy,camz);
  glUniform1i(uAlb,0); glUniform1i(uNrm,1);

  /* pick 8 nearest lights (static + temp) */
  float lp[SHLIGHTS*4], lc[SHLIGHTS*3]; int ln=0;
  typedef struct{float d2;int i;int tmp;}LS; LS sl[MAXLIGHT+MAXTEMPL]; int sn=0;
  for(int i=0;i<nlights;i++){ float dx=lights[i].x-camx,dz=lights[i].z-camz;
    sl[sn++] = (LS){dx*dx+dz*dz,i,0}; }
  for(int i=0;i<MAXTEMPL;i++) if(templ_[i].life>0){ float dx=templ_[i].x-camx,dz=templ_[i].z-camz;
    sl[sn++] = (LS){dx*dx+dz*dz,i,1}; }
  for(int a=0;a<sn;a++)for(int b=a+1;b<sn;b++) if(sl[b].d2<sl[a].d2){LS t=sl[a];sl[a]=sl[b];sl[b]=t;}
  for(int k=0;k<sn && ln<SHLIGHTS;k++){
    if(sl[k].tmp){ TempL*t=&templ_[sl[k].i];
      lp[ln*4]=t->x;lp[ln*4+1]=t->y;lp[ln*4+2]=t->z;lp[ln*4+3]=t->r;
      lc[ln*3]=t->cr;lc[ln*3+1]=t->cg;lc[ln*3+2]=t->cb;
    } else { Light*l=&lights[sl[k].i];
      lp[ln*4]=l->x;lp[ln*4+1]=l->y;lp[ln*4+2]=l->z;lp[ln*4+3]=l->r;
      lc[ln*3]=l->cr;lc[ln*3+1]=l->cg;lc[ln*3+2]=l->cb; }
    ln++;
  }
  glUniform1i(uNL,ln);
  glUniform4fv(uLpos,SHLIGHTS,lp);
  glUniform3fv(uLcol,SHLIGHTS,lc);

  /* static level batches */
  float I[9]; m3id(I); set_uM(I,0,0,0);
  glUniform3f(uTint,1,1,1); glUniform1f(uBump,1); glUniform1f(uEmis,0);
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_NORMAL_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  int texof[3]={TX_WALL,TX_FLOOR,TX_CEIL};
  for(int b=0;b<3;b++){
    glActiveTexture_(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,texAlb[texof[b]]);
    glActiveTexture_(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D,texNrm[texof[b]]);
    glVertexPointer(3,GL_FLOAT,32,batch[b]);
    glNormalPointer(GL_FLOAT,32,batch[b]+3);
    glTexCoordPointer(2,GL_FLOAT,32,batch[b]+6);
    glDrawArrays(GL_QUADS,0,bn[b]/8);
  }
  glDisableClientState(GL_VERTEX_ARRAY);
  glDisableClientState(GL_NORMAL_ARRAY);
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);

  glActiveTexture_(GL_TEXTURE0);
  for(int i=0;i<nen;i++) draw_enemy(&en[i]);
  draw_items();
  glUseProgram(0);

  /* additive pass: light orbs, exit beacon, particles */
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE);
  glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D,texAlb[TX_GLOW]);
  glBegin(GL_QUADS);
  for(int i=0;i<nlights;i++)
    billboard(lights[i].x,lights[i].y,lights[i].z,0.32f,
      lights[i].cr*0.25f,lights[i].cg*0.25f,lights[i].cb*0.25f,1,pyaw+90);
  for(int k=0;k<5;k++)
    billboard(exitx, 0.4f+k*0.55f, exitz, 0.7f-k*0.08f, 0.2f,1.0f,0.6f,
      0.5f+0.2f*sinf(gtime*4+k), pyaw+90);
  glEnd();
  glDisable(GL_TEXTURE_2D);
  glPointSize(4);
  glBegin(GL_POINTS);
  for(int i=0;i<MAXPART;i++){
    Part*p=&parts[i]; if(p->life<=0)continue;
    float a=p->life/p->max;
    glColor4f(p->cr,p->cg,p->cb,a);
    glVertex3f(p->x,p->y,p->z);
  }
  glEnd();
  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);
}

static void draw_gun(void){
  glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
  glClear(GL_DEPTH_BUFFER_BIT);
  float bob=sinf(bobT*7.0f)*0.012f, kz=kick*0.07f;
  glTranslatef(0.21f, -0.22f+bob, -0.45f+kz);
  glRotatef(-kick*6,1,0,0);
  struct { float x,y,z,sx,sy,sz; } parts_[3]={
    {0,0.02f,-0.16f, 0.05f,0.055f,0.30f},   /* barrel */
    {0,-0.04f,0.02f, 0.065f,0.09f,0.13f},   /* body   */
    {0,-0.13f,0.06f, 0.05f,0.12f,0.07f}};   /* grip   */
  for(int p=0;p<3;p++){
    float x=parts_[p].sx*0.5f,y=parts_[p].sy*0.5f,z=parts_[p].sz*0.5f;
    float cx=parts_[p].x,cy=parts_[p].y,cz=parts_[p].z;
    float face[6][3]={{0,0,1},{0,0,-1},{1,0,0},{-1,0,0},{0,1,0},{0,-1,0}};
    float shade[6]={0.5f,0.45f,0.62f,0.40f,0.85f,0.25f};
    glBegin(GL_QUADS);
    for(int f=0;f<6;f++){
      float k2=shade[f]*0.22f;
      glColor3f(k2,k2*1.04f,k2*1.1f);
      float nx=face[f][0],ny=face[f][1],nz=face[f][2];
      float ux= ny||nz?1:0, uy= nx?1:0, uz=0;
      float vx=ny*uz-nz*uy, vy=nz*ux-nx*uz, vz=nx*uy-ny*ux;
      float ex=nx*x,ey=ny*y,ez=nz*z;
      float ax=ux*x,ay=uy*y,az=uz*z, bx2=vx*x,by2=vy*y,bz2=vz*z;
      glVertex3f(cx+ex-ax-bx2,cy+ey-ay-by2,cz+ez-az-bz2);
      glVertex3f(cx+ex+ax-bx2,cy+ey+ay-by2,cz+ez+az-bz2);
      glVertex3f(cx+ex+ax+bx2,cy+ey+ay+by2,cz+ez+az+bz2);
      glVertex3f(cx+ex-ax+bx2,cy+ey-ay+by2,cz+ez-az+bz2);
    }
    glEnd();
  }
  if(flashT>0){
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE);
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D,texAlb[TX_GLOW]);
    float s=0.10f+frand()*0.05f;
    glColor4f(1.0f,0.8f,0.4f,flashT/0.07f);
    glBegin(GL_QUADS);
    glTexCoord2f(0,0); glVertex3f(0-s,0.02f-s,-0.34f);
    glTexCoord2f(1,0); glVertex3f(0+s,0.02f-s,-0.34f);
    glTexCoord2f(1,1); glVertex3f(0+s,0.02f+s,-0.34f);
    glTexCoord2f(0,1); glVertex3f(0-s,0.02f+s,-0.34f);
    glEnd();
    glDisable(GL_TEXTURE_2D); glDisable(GL_BLEND);
  }
  glPopMatrix();
}

static void draw_hud(void){
  glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
  glOrtho(0,WINW,WINH,0,-1,1);
  glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

  if(gstate==ST_PLAY){
    /* crosshair */
    glColor4f(1,1,1,0.7f);
    glBegin(GL_QUADS);
    glVertex2f(WINW/2-9,WINH/2-1);glVertex2f(WINW/2-3,WINH/2-1);
    glVertex2f(WINW/2-3,WINH/2+1);glVertex2f(WINW/2-9,WINH/2+1);
    glVertex2f(WINW/2+3,WINH/2-1);glVertex2f(WINW/2+9,WINH/2-1);
    glVertex2f(WINW/2+9,WINH/2+1);glVertex2f(WINW/2+3,WINH/2+1);
    glVertex2f(WINW/2-1,WINH/2-9);glVertex2f(WINW/2+1,WINH/2-9);
    glVertex2f(WINW/2+1,WINH/2-3);glVertex2f(WINW/2-1,WINH/2-3);
    glVertex2f(WINW/2-1,WINH/2+3);glVertex2f(WINW/2+1,WINH/2+3);
    glVertex2f(WINW/2+1,WINH/2+9);glVertex2f(WINW/2-1,WINH/2+9);
    glEnd();
    /* health bar */
    glColor4f(0,0,0,0.55f);
    glBegin(GL_QUADS);glVertex2f(28,WINH-52);glVertex2f(252,WINH-52);glVertex2f(252,WINH-28);glVertex2f(28,WINH-28);glEnd();
    glColor4f(0.85f,0.12f,0.10f,0.9f);
    float hw=220*php/100.0f;
    glBegin(GL_QUADS);glVertex2f(30,WINH-50);glVertex2f(30+hw,WINH-50);glVertex2f(30+hw,WINH-30);glVertex2f(30,WINH-30);glEnd();
    glColor4f(1,1,1,0.9f); draw_text(30,WINH-78,2.4f,"HP");
    /* ammo */
    char am[8]; snprintf(am,8,"%02d",(int)pammo);
    glColor4f(1,0.85f,0.3f,0.95f);
    draw_text(WINW-30-textw(am,5),WINH-66,5,am);
    glColor4f(1,1,1,0.7f); draw_text(WINW-30-textw("AMMO",2.2f),WINH-90,2.2f,"AMMO");
    if(dmgFlash>0){
      glColor4f(0.7f,0.05f,0.02f,dmgFlash*0.55f);
      glBegin(GL_QUADS);glVertex2f(0,0);glVertex2f(WINW,0);glVertex2f(WINW,WINH);glVertex2f(0,WINH);glEnd();
    }
  } else {
    glColor4f(0,0,0,gstate==ST_TITLE?0.45f:0.6f);
    glBegin(GL_QUADS);glVertex2f(0,0);glVertex2f(WINW,0);glVertex2f(WINW,WINH);glVertex2f(0,WINH);glEnd();
    if(gstate==ST_TITLE){
      glColor4f(0.95f,0.93f,0.88f,1);
      draw_text((WINW-textw("MURKK",14))/2,170,14,"MURKK");
      glColor4f(0.6f,0.62f,0.66f,1);
      draw_text((WINW-textw("A 96K-SPIRIT HOMAGE TO KKRIEGER",3))/2,300,3,"A 96K-SPIRIT HOMAGE TO KKRIEGER");
      draw_text((WINW-textw("EVERYTHING SYNTHESIZED - NOTHING ON DISK",2.2f))/2,345,2.2f,"EVERYTHING SYNTHESIZED - NOTHING ON DISK");
      glColor4f(0.9f,0.8f,0.4f,0.7f+0.3f*sinf(gtime*4));
      draw_text((WINW-textw("CLICK TO DESCEND",3.4f))/2,470,3.4f,"CLICK TO DESCEND");
      glColor4f(0.45f,0.45f,0.5f,1);
      draw_text((WINW-textw("WASD MOVE - MOUSE LOOK - LMB FIRE - FIND THE GREEN EXIT",2))/2,560,2,
        "WASD MOVE - MOUSE LOOK - LMB FIRE - FIND THE GREEN EXIT");
    } else if(gstate==ST_DEAD){
      glColor4f(0.85f,0.1f,0.08f,1);
      draw_text((WINW-textw("YOU DIED",10))/2,260,10,"YOU DIED");
      glColor4f(0.8f,0.8f,0.8f,0.8f);
      draw_text((WINW-textw("CLICK TO TRY AGAIN",3))/2,420,3,"CLICK TO TRY AGAIN");
    } else {
      glColor4f(0.3f,1.0f,0.6f,1);
      draw_text((WINW-textw("EXIT REACHED",9))/2,260,9,"EXIT REACHED");
      glColor4f(0.8f,0.8f,0.8f,0.9f);
      char b2[64]; snprintf(b2,64,"CLEARED IN %d SECONDS",(int)winT);
      draw_text((WINW-textw(b2,3))/2,410,3,b2);
      draw_text((WINW-textw("CLICK FOR A NEW RUN",2.4f))/2,470,2.4f,"CLICK FOR A NEW RUN");
    }
  }
  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
  glMatrixMode(GL_PROJECTION); glPopMatrix();
  glMatrixMode(GL_MODELVIEW); glPopMatrix();
}

/* ---------------------------------------------------------------- screenshot */
static void shot_ppm(const char*path){
  unsigned char*buf=malloc(WINW*WINH*3);
  glPixelStorei(GL_PACK_ALIGNMENT,1);
  glReadPixels(0,0,WINW,WINH,GL_RGB,GL_UNSIGNED_BYTE,buf);
  FILE*f=fopen(path,"wb");
  fprintf(f,"P6\n%d %d\n255\n",WINW,WINH);
  for(int y=WINH-1;y>=0;y--) fwrite(buf+y*WINW*3,1,WINW*3,f);
  fclose(f); free(buf);
  printf("[murkk] wrote %s\n",path);
}

/* ---------------------------------------------------------------- main */
int main(int argc,char**argv){
  unsigned t0;
  for(int i=1;i<argc;i++){
    if(!strcmp(argv[i],"--smoke"))smoke=1;
    else if(!strcmp(argv[i],"--seed")&&i+1<argc)gseed=(unsigned)strtoul(argv[++i],0,0);
  }
  if(smoke) SDL_setenv("SDL_AUDIODRIVER","dummy",1);

  if(SDL_Init(SDL_INIT_VIDEO)<0){ fprintf(stderr,"SDL: %s\n",SDL_GetError()); return 1; }
  SDL_InitSubSystem(SDL_INIT_AUDIO);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
  SDL_Window*win=SDL_CreateWindow(".murkk",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
    WINW,WINH,SDL_WINDOW_OPENGL);
  SDL_GLContext ctx=SDL_GL_CreateContext(win);
  if(!ctx){ fprintf(stderr,"GL: %s\n",SDL_GetError()); return 1; }
  SDL_GL_SetSwapInterval(smoke?0:1);
  load_gl();
  printf("[murkk] GL: %s / %s\n",glGetString(GL_RENDERER),glGetString(GL_VERSION));

  t0=SDL_GetTicks(); gen_textures();
  printf("[murkk] textures synthesized in %ums\n",SDL_GetTicks()-t0);
  t0=SDL_GetTicks(); reset_game();
  printf("[murkk] world carved in %ums (%d quads)\n",SDL_GetTicks()-t0,(bn[0]+bn[1]+bn[2])/32);
  init_shaders();
  printf("[murkk] shaders up\n");

  SDL_AudioSpec want={0},have;
  want.freq=44100; want.format=AUDIO_F32SYS; want.channels=1; want.samples=512; want.callback=audio_cb;
  adev=SDL_OpenAudioDevice(0,0,&want,&have,0);
  if(adev){ audioOK=1; SDL_PauseAudioDevice(adev,0); printf("[murkk] audio up\n"); }
  else printf("[murkk] no audio device, running silent\n");

  glEnable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glClearColor(0.012f,0.014f,0.018f,1);

  int running=1, frame=0, wdown=0,adown=0,sdown=0,ddown=0,mdown=0;
  unsigned last=SDL_GetTicks();
  float titleYaw=0;

  while(running){
    SDL_Event ev;
    while(SDL_PollEvent(&ev)){
      if(ev.type==SDL_QUIT)running=0;
      else if(ev.type==SDL_KEYDOWN||ev.type==SDL_KEYUP){
        int d=ev.type==SDL_KEYDOWN;
        switch(ev.key.keysym.sym){
          case SDLK_w:wdown=d;break; case SDLK_a:adown=d;break;
          case SDLK_s:sdown=d;break; case SDLK_d:ddown=d;break;
          case SDLK_ESCAPE: if(d)running=0; break;
        }
      }
      else if(ev.type==SDL_MOUSEMOTION && gstate==ST_PLAY && !smoke){
        pyaw  += ev.motion.xrel*0.13f;
        ppitch+= ev.motion.yrel*0.13f;
        ppitch=ppitch>89?89:ppitch<-89?-89:ppitch;
      }
      else if(ev.type==SDL_MOUSEBUTTONDOWN && ev.button.button==SDL_BUTTON_LEFT){
        if(gstate==ST_TITLE){ gstate=ST_PLAY; SDL_SetRelativeMouseMode(SDL_TRUE); }
        else if(gstate==ST_DEAD||gstate==ST_WIN){ reset_game(); gstate=ST_PLAY; SDL_SetRelativeMouseMode(SDL_TRUE); }
        else mdown=1;
      }
      else if(ev.type==SDL_MOUSEBUTTONUP && ev.button.button==SDL_BUTTON_LEFT)mdown=0;
    }

    unsigned now=SDL_GetTicks();
    float dt=smoke?1.0f/60:(now-last)/1000.0f;
    last=now;
    if(dt>0.05f)dt=0.05f;
    gtime+=dt;

    /* smoke choreography: title shot, then play, teleport a creature in view, fire, gameplay shot */
    if(smoke){
      frame++;
      if(frame==25)shot_ppm("/home/claude/murkk/shot_title.ppm");
      if(frame==30){ /* face the longest open sightline from spawn */
        gstate=ST_PLAY;
        float best=0,besta=0;
        for(int k=0;k<64;k++){
          float a=k*2*PI/64;
          float d=ray_wall(px,EYE,pz,sinf(a),0,-cosf(a),40);
          if(d>best){best=d;besta=a;}
        }
        pyaw=besta*180/PI;
      }
      if(frame==112&&nen>0){ /* stage a creature in frame */
        float yr=pyaw*PI/180;
        for(float d=3.6f;d>1.0f;d-=0.3f){
          float ex=px+sinf(yr)*d, ez=pz-cosf(yr)*d;
          if(circ_free(ex,ez,0.4f)&&los(px,pz,ex,ez)){ en[0].x=ex; en[0].z=ez; en[0].state=1; break; }
        }
      }
      if(frame>=120&&frame<128&&nen>0){ /* aim at it */
        float dx=en[0].x-px, dz=en[0].z-pz;
        float d=sqrtf(dx*dx+dz*dz);
        pyaw=atan2f(dx,-dz)*180/PI;
        ppitch=atan2f(EYE-0.55f,d)*180/PI;
      }
      if(frame==124)fire();
      if(frame==127)shot_ppm("/home/claude/murkk/shot_game.ppm");
      if(frame>=140){ printf("[murkk] SMOKE OK\n"); running=0; }
    }

    if(gstate==ST_TITLE){ titleYaw+=dt*9; pyaw=titleYaw; px=startx; pz=startz; }

    if(gstate==ST_PLAY){
      float yr=pyaw*PI/180;
      float fx=sinf(yr),fz=-cosf(yr), rx=cosf(yr),rz=sinf(yr);
      float mx=0,mz=0;
      if(wdown){mx+=fx;mz+=fz;} if(sdown){mx-=fx;mz-=fz;}
      if(ddown){mx+=rx;mz+=rz;} if(adown){mx-=rx;mz-=rz;}
      float ml=sqrtf(mx*mx+mz*mz);
      if(ml>0.01f){
        mx/=ml;mz/=ml;
        move_circ(&px,&pz,mx*5.2f*dt,mz*5.2f*dt,0.34f);
        bobT+=dt; stepT-=dt;
        if(stepT<=0){ sfx(V_STEP); stepT=0.42f; }
      }
      if(mdown)fire();
      if(fireCD>0)fireCD-=dt;
      if(flashT>0)flashT-=dt;
      if(kick>0)kick-=dt*7;
      if(dmgFlash>0)dmgFlash-=dt;
      if(shake>0)shake-=dt*1.2f;
      winT+=dt;

      update_enemies(dt);

      for(int i=0;i<nitems;i++){
        if(items[i].taken)continue;
        float dx=items[i].x-px,dz=items[i].z-pz;
        if(dx*dx+dz*dz<0.8f*0.8f){
          items[i].taken=1; sfx(V_PICK);
          if(items[i].type==0){ php+=25; if(php>100)php=100; }
          else { pammo+=12; if(pammo>99)pammo=99; }
        }
      }
      { float dx=exitx-px,dz=exitz-pz;
        if(dx*dx+dz*dz<0.9f*0.9f){ gstate=ST_WIN; sfx(V_WIN); SDL_SetRelativeMouseMode(SDL_FALSE); } }
    }
    if(gstate==ST_DEAD)deadT+=dt;

    for(int i=0;i<MAXPART;i++){
      Part*p=&parts[i]; if(p->life<=0)continue;
      p->life-=dt; p->vy-=6.0f*dt;
      p->x+=p->vx*dt; p->y+=p->vy*dt; p->z+=p->vz*dt;
      if(p->y<0.02f){p->y=0.02f;p->vy*=-0.3f;p->vx*=0.7f;p->vz*=0.7f;}
    }
    for(int i=0;i<MAXTEMPL;i++) if(templ_[i].life>0)templ_[i].life-=dt;

    /* render */
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    { float zn=0.08f, t=zn*tanf(35*PI/180), a=(float)WINW/WINH;
      glFrustum(-t*a,t*a,-t,t,zn,80.0f); }
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    float shx=shake>0?(frand()-0.5f)*shake*6:0;
    float shy=shake>0?(frand()-0.5f)*shake*6:0;
    float camy=EYE + (gstate==ST_PLAY?sinf(bobT*7.0f)*0.03f:0);
    glRotatef(ppitch+shy,1,0,0);
    glRotatef(pyaw+shx,0,1,0);
    glTranslatef(-px,-camy,-pz);

    draw_world(px,camy,pz);
    if(gstate==ST_PLAY)draw_gun();
    draw_hud();

    SDL_GL_SwapWindow(win);
  }

  if(adev)SDL_CloseAudioDevice(adev);
  SDL_GL_DeleteContext(ctx); SDL_DestroyWindow(win); SDL_Quit();
  return 0;
}
