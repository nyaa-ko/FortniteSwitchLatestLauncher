#pragma once
#include <switch.h>
#include <string>
#include <algorithm>
#include <cstdint>

namespace MidnightUI {

struct Color { uint8_t r,g,b,a; };
static constexpr Color BG{18,18,22,255};
static constexpr Color PANEL{27,29,35,255};
static constexpr Color PANEL2{34,37,44,255};
static constexpr Color TEXT{242,244,248,255};
static constexpr Color MUTED{145,150,160,255};
static constexpr Color BLUE{45,132,255,255};
static constexpr Color BLUE_DARK{25,83,170,255};
static constexpr Color LINE{60,64,73,255};
static constexpr Color RED{235,70,75,255};
static constexpr Color GREEN{65,195,105,255};
static constexpr Color YELLOW{238,190,55,255};

static inline void Put(u8* fb, u32 stride, int x, int y, Color c) {
    if (x < 0 || y < 0 || x >= 1280 || y >= 720) return;
    u32* p = reinterpret_cast<u32*>(fb + y * stride + x * 4);
    *p = RGBA8(c.r,c.g,c.b,c.a);
}
static inline void Rect(u8* fb, u32 stride, int x,int y,int w,int h,Color c) {
    if (w<=0||h<=0) return;
    int x0=std::max(0,x), y0=std::max(0,y), x1=std::min(1280,x+w), y1=std::min(720,y+h);
    for(int yy=y0;yy<y1;yy++) for(int xx=x0;xx<x1;xx++) Put(fb,stride,xx,yy,c);
}

// Compact built-in 5x7 ASCII font. No external font/SDL dependency.
static inline const uint8_t* Glyph(char ch) {
    static const uint8_t blank[7]={0,0,0,0,0,0,0};
    static const uint8_t q[7]={14,17,1,2,4,0,4};
#define G(name,a,b,c,d,e,f,g) static const uint8_t name[7]={a,b,c,d,e,f,g}
    G(A,14,17,17,31,17,17,17); G(B,30,17,17,30,17,17,30); G(C,14,17,16,16,16,17,14);
    G(D,30,17,17,17,17,17,30); G(E,31,16,16,30,16,16,31); G(F,31,16,16,30,16,16,16);
    G(G,14,17,16,23,17,17,15); G(H,17,17,17,31,17,17,17); G(I,31,4,4,4,4,4,31);
    G(J,7,2,2,2,18,18,12); G(K,17,18,20,24,20,18,17); G(L,16,16,16,16,16,16,31);
    G(M,17,27,21,21,17,17,17); G(N,17,25,21,19,17,17,17); G(O,14,17,17,17,17,17,14);
    G(P,30,17,17,30,16,16,16); G(Q,14,17,17,17,21,18,13); G(R,30,17,17,30,20,18,17);
    G(S,15,16,16,14,1,1,30); G(T,31,4,4,4,4,4,4); G(U,17,17,17,17,17,17,14);
    G(V,17,17,17,17,17,10,4); G(W,17,17,17,21,21,21,10); G(X,17,17,10,4,10,17,17);
    G(Y,17,17,10,4,4,4,4); G(Z,31,1,2,4,8,16,31);
    G(N0,14,17,19,21,25,17,14); G(N1,4,12,4,4,4,4,14); G(N2,14,17,1,2,4,8,31);
    G(N3,30,1,1,14,1,1,30); G(N4,2,6,10,18,31,2,2); G(N5,31,16,16,30,1,1,30);
    G(N6,14,16,16,30,17,17,14); G(N7,31,1,2,4,8,8,8); G(N8,14,17,17,14,17,17,14);
    G(N9,14,17,17,15,1,1,14); G(DOT,0,0,0,0,0,12,12); G(DASH,0,0,0,31,0,0,0);
    G(COLON,0,12,12,0,12,12,0); G(SLASH,1,2,2,4,8,8,16); G(LP,2,4,8,8,8,4,2); G(RP,8,4,2,2,2,4,8);
    switch(ch){
        case 'A':return A;case'B':return B;case'C':return C;case'D':return D;case'E':return E;case'F':return F;case'G':return G;case'H':return H;case'I':return I;case'J':return J;case'K':return K;case'L':return L;case'M':return M;case'N':return N;case'O':return O;case'P':return P;case'Q':return Q;case'R':return R;case'S':return S;case'T':return T;case'U':return U;case'V':return V;case'W':return W;case'X':return X;case'Y':return Y;case'Z':return Z;
        case '0':return N0;case'1':return N1;case'2':return N2;case'3':return N3;case'4':return N4;case'5':return N5;case'6':return N6;case'7':return N7;case'8':return N8;case'9':return N9;
        case '.':return DOT;case '-':return DASH;case ':':return COLON;case '/':return SLASH;case '(':return LP;case ')':return RP;case '?':return q; default:return blank;
    }
#undef G
}

static inline void Text(u8* fb,u32 stride,int x,int y,const std::string& s,Color c,int scale=3) {
    int cx=x;
    for(char raw:s){ char ch=raw; if(ch>='a'&&ch<='z') ch=char(ch-'a'+'A'); if(ch==' '){cx+=6*scale;continue;} const uint8_t* g=Glyph(ch); for(int gy=0;gy<7;gy++) for(int gx=0;gx<5;gx++) if(g[gy]&(1<<(4-gx))) Rect(fb,stride,cx+gx*scale,y+gy*scale,scale,scale,c); cx+=6*scale; }
}

static inline void ButtonHint(u8* fb,u32 stride,int x,int y,const std::string& key,const std::string& label){
    Rect(fb,stride,x,y,34,28,PANEL2); Text(fb,stride,x+10,y+7,key,TEXT,2); Text(fb,stride,x+45,y+7,label,MUTED,2);
}

class Screen {
public:
    u8* fb=nullptr; u32 stride=0;
    void Begin(){ fb=gfxGetFramebuffer(&width,&height); stride=width*4; Rect(fb,stride,0,0,width,height,BG); }
    void End(){ gfxFlushBuffers(); gfxSwapBuffers(); gfxWaitForVsync(); }
    u32 width=1280,height=720;
    void Header(const std::string& title,const std::string& right="HOME"){
        Rect(fb,stride,0,0,1280,74,PANEL); Rect(fb,stride,0,72,1280,2,BLUE);
        Text(fb,stride,36,23,"MIDNIGHT LAUNCHER",TEXT,3); Text(fb,stride,1040,27,right,MUTED,2); Text(fb,stride,36,50,title,MUTED,2);
    }
    void Footer(bool back=false){
        Rect(fb,stride,0,670,1280,50,PANEL); Rect(fb,stride,0,670,1280,1,LINE);
        ButtonHint(fb,stride,36,681,"A","SELECT"); int x=180; if(back){ButtonHint(fb,stride,x,681,"B","BACK");x+=145;} ButtonHint(fb,stride,x,681,"+","EXIT"); ButtonHint(fb,stride,370,681,"UP/DOWN","MOVE");
    }
    void Row(int index,bool selected,const std::string& label,const std::string& value=""){
        int y=120+index*66; Color bg=selected?BLUE:PANEL; Rect(fb,stride,48,y,620,54,bg); if(selected) Rect(fb,stride,48,y,5,54,TEXT);
        Text(fb,stride,76,y+17,label,selected?TEXT:TEXT,3); if(!value.empty()) Text(fb,stride,520,y+19,value,selected?TEXT:MUTED,2);
    }
};
}
