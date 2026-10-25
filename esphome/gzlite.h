#pragma once
// Small gzip writer (RFC 1951/1952): LZ77 with hash chains + fixed Huffman codes, one block.
// No zlib needed; memory comes from the caller-provided allocator (PSRAM on the treadmill).
// Typical CSV ratio ~3.5–4× (gzip -9 does ~4–5×). Decoded by any gzip / browser.
#include <cstdint>
#include <cstddef>
#include <cstring>
namespace gzlite {
struct Bits { uint8_t *out; size_t cap, n=0; uint32_t acc=0; int cnt=0; bool over=false;
  void put(uint32_t v,int len){acc|=v<<cnt;cnt+=len;while(cnt>=8){if(n<cap)out[n++]=uint8_t(acc);else over=true;acc>>=8;cnt-=8;}}
  void flush(){if(cnt>0){if(n<cap)out[n++]=uint8_t(acc);else over=true;}acc=0;cnt=0;}
  void byte(uint8_t b){if(n<cap)out[n++]=b;else over=true;} };
inline uint32_t rev(uint32_t v,int len){uint32_t r=0;for(int i=0;i<len;i++){r=(r<<1)|(v&1);v>>=1;}return r;}
// fixed literal/length code: 0-143 8b(0x30+), 144-255 9b(0x190+), 256-279 7b, 280-287 8b(0xC0+)
inline void lit(Bits &b,int c){if(c<144)b.put(rev(0x30+c,8),8);else if(c<256)b.put(rev(0x190+c-144,9),9);else if(c<280)b.put(rev(c-256,7),7);else b.put(rev(0xC0+c-280,8),8);}
inline void match(Bits &b,int len,int dist){
  static const uint16_t LB[29]={3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
  static const uint8_t LE[29]={0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
  static const uint16_t DB[30]={1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
  static const uint8_t DE[30]={0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
  int i=28;while(LB[i]>len)i--;lit(b,257+i);if(LE[i])b.put(len-LB[i],LE[i]);
  int j=29;while(DB[j]>dist)j--;b.put(rev(j,5),5);if(DE[j])b.put(dist-DB[j],DE[j]);}
inline uint32_t crc32(const uint8_t *p,size_t n){uint32_t c=0xFFFFFFFFu;for(size_t i=0;i<n;i++){c^=p[i];for(int k=0;k<8;k++)c=(c>>1)^(0xEDB88320u&(0u-(c&1)));}return ~c;}
// Compress in[0..n) into out (cap bytes). work: >= (65536+32768)*4 bytes. yield(): called now and then (vTaskDelay).
// Returns gzip size, or 0 if out is too small.
template<typename Y> size_t gzip(const uint8_t *in,size_t n,uint8_t *out,size_t cap,uint32_t *work,Y yield){
  const uint32_t HB=16,HS=1u<<HB,W=32768,NONE=0xFFFFFFFFu;uint32_t *head=work,*prev=work+HS;
  for(uint32_t i=0;i<HS;i++)head[i]=NONE;
  Bits b{out,cap};static const uint8_t H[10]={0x1f,0x8b,8,0,0,0,0,0,0,3};for(auto x:H)b.byte(x);
  b.put(1,1);b.put(1,2); // BFINAL=1, BTYPE=01 (fixed)
  auto hash=[&](size_t i){return ((uint32_t(in[i])<<10)^(uint32_t(in[i+1])<<5)^in[i+2])&(HS-1);};
  size_t i=0;
  while(i<n){
    if((i&4095)==0)yield();
    int best=0,bd=0;
    if(i+2<n){uint32_t h=hash(i),c=head[h];int chain=48;
      while(c!=NONE&&i-c<=W&&chain--){size_t l=0,mx=n-i<258?n-i:258;while(l<mx&&in[c+l]==in[i+l])l++;if(int(l)>best){best=int(l);bd=int(i-c);if(l==mx)break;}c=prev[c%W];}
      prev[i%W]=head[h];head[h]=uint32_t(i);}
    if(best>=3){match(b,best,bd);for(int k=1;k<best;k++){size_t p=i+k;if(p+2<n){uint32_t h=hash(p);prev[p%W]=head[h];head[h]=uint32_t(p);}}i+=best;}
    else{lit(b,in[i]);i++;}
    if(b.over)return 0;
  }
  lit(b,256);b.flush();uint32_t crc=crc32(in,n);for(int k=0;k<4;k++)b.byte(uint8_t(crc>>(8*k)));for(int k=0;k<4;k++)b.byte(uint8_t(uint32_t(n)>>(8*k)));
  return b.over?0:b.n;}
} // namespace gzlite
