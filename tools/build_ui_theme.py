"""Cache the installed game's paper/button artwork and native UI font.

Reads original resources.gpak only. It does not patch ui.swf, add DoABC tags,
or depend on another mod. Generated pixels/font files stay outside Git.
"""
from pathlib import Path
import io
import math
import struct
import zlib
from PIL import Image, ImageDraw, ImageFont
from game_archive import GameArchive
from build_sidebar_swf import read_tags, tag_start, symbol_entries
from build_marker_icons import Reader, matrix, point, compose
from swf_font import make_ttf

ROOT=Path(__file__).resolve().parents[1]
IDENTITY=(1,0,0,1,0,0)


def read_swf(archive,name):
    offset,size=archive.entries[name]
    with archive.path.open('rb') as stream:
        stream.seek(archive.base+offset); data=stream.read(size)
    if data[:3]==b'CWS':data=b'FWS'+data[3:8]+zlib.decompress(data[8:])
    if data[:3]!=b'FWS' or len(data)!=struct.unpack_from('<I',data,4)[0]:raise ValueError('Invalid SWF: '+name)
    return list(read_tags(data,tag_start(data)))


def placement(data):
    r=Reader(data);flags=r.byte();depth=r.word()
    child=r.word() if flags&2 else None
    transform=matrix(r) if flags&4 else None
    tint=None
    if flags&8:
        add,mul,bits=r.u(1),r.u(1),r.u(4)
        factors=tuple(r.s(bits)/256 for _ in range(4)) if mul else (1,1,1,1)
        offsets=tuple(r.s(bits) for _ in range(4)) if add else (0,0,0,0)
        r.align();tint=factors,offsets
    if flags&16:r.word()
    name=data[r.bit//8:].split(b'\0')[0].decode('ascii') if flags&32 else ''
    return flags,depth,child,transform,tint,name


def shapes(kind,data):
    r=Reader(data,2);bits=r.u(5)
    for _ in range(4):r.s(bits)
    if kind==83:
        r.align();bits=r.u(5)
        for _ in range(4):r.s(bits)
        r.byte()
    colors=4 if kind in (32,83) else 3
    def fill():
        style=r.byte()
        if style==0:
            c=tuple(r.byte() for _ in range(colors))
            return ('solid',c if colors==4 else c+(255,))
        if style in (0x40,0x41,0x42,0x43):return ('bitmap',r.word(),matrix(r))
        raise ValueError('Unsupported native UI fill: '+str(style))
    def styles():
        count=r.byte()
        if count==255 and kind!=2:count=r.word()
        styles=[fill() for _ in range(count)]
        lines=r.byte()
        if lines==255 and kind!=2:lines=r.word()
        for _ in range(lines):
            r.word()
            if kind==83:
                r.u(2);joint=r.u(2);has_fill=r.u(1);r.u(1);r.u(1);r.u(1);r.u(5);r.u(1);r.u(2)
                if joint==2:r.word()
                if has_fill:fill()
                else:
                    for _ in range(4):r.byte()
            else:
                for _ in range(colors):r.byte()
        return styles
    fills=styles();groups=[[] for _ in fills];fill_bits,line_bits=r.u(4),r.u(4)
    current=[0,0];offset=0;pos=(0,0)
    while True:
        if r.u(1):
            straight,n=r.u(1),r.u(4)+2;start=pos
            if straight:
                if r.u(1):dx,dy=r.s(n),r.s(n)
                elif r.u(1):dx,dy=0,r.s(n)
                else:dx,dy=r.s(n),0
                pos=(start[0]+dx,start[1]+dy);pts=[start,pos]
            else:
                control=(start[0]+r.s(n),start[1]+r.s(n));pos=(control[0]+r.s(n),control[1]+r.s(n))
                pts=[tuple((1-t)**2*start[j]+2*(1-t)*t*control[j]+t*t*pos[j] for j in (0,1)) for t in (i/16 for i in range(17))]
            for side,f in enumerate(current):
                if f:groups[offset+f-1].extend((a,b) if side else (b,a) for a,b in zip(pts,pts[1:]))
        else:
            flags=r.u(5)
            if not flags:break
            if flags&1:
                n=r.u(5);pos=r.s(n),r.s(n)
            if flags&2:current[0]=r.u(fill_bits)
            if flags&4:current[1]=r.u(fill_bits)
            if flags&8:r.u(line_bits)
            if flags&16:
                offset=len(fills);new=styles();fills+=new;groups.extend([] for _ in new)
                fill_bits,line_bits=r.u(4),r.u(4)
    return [(style,group) for style,group in zip(fills,groups) if group]


class NativeUi:
    def __init__(self,archive):
        self._load(read_swf(archive,'swfs/ui.swf'))
    @classmethod
    def from_tags(cls,tags):
        result=cls.__new__(cls);result._load(tags);return result
    def _load(self,tags):
        self.tags=tags
        kinds=(2,22,32,39,83,20,36,37)
        self.defs={struct.unpack_from('<H',b)[0]:(k,b) for k,b,_ in self.tags if k in kinds}
        self.symbols={name:i for k,b,_ in self.tags if k==76 for i,name in symbol_entries(b)}
        self.bitmaps={}
    def bitmap(self,ident):
        if ident not in self.bitmaps:
            kind,b=self.defs[ident];_,fmt,w,h=struct.unpack_from('<HBHH',b)
            if kind!=36 or fmt!=5:raise ValueError('Expected native ARGB bitmap')
            raw=zlib.decompress(b[7:])
            if len(raw)!=w*h*4:raise ValueError('Invalid bitmap length')
            image=Image.frombytes('RGBA',(w,h),raw,'raw','ARGB')
            # SWF lossless pixels are premultiplied; atlas textures are straight alpha.
            pixels=bytearray(image.tobytes())
            for i in range(0,len(pixels),4):
                a=pixels[i+3]
                if 0<a<255:
                    for c in range(3):pixels[i+c]=min(255,round(pixels[i+c]*255/a))
            self.bitmaps[ident]=Image.frombytes('RGBA',(w,h),bytes(pixels))
        return self.bitmaps[ident]
    def frame(self,ident,index=None,label=None):
        kind,body=self.defs[ident]
        if kind!=39:raise ValueError('Expected native MovieClip')
        if index is None:index=min(8,struct.unpack_from('<H',body,2)[0]-1)
        display={};at=0;found=label is None
        for k,b,_ in read_tags(body,4):
            if k==43 and label==b.split(b'\0')[0].decode('ascii'):found=True;index=at
            elif k==26:
                flags,depth,child,m,tint,name=placement(b)
                old=display.get(depth,(None,IDENTITY,((1,1,1,1),(0,0,0,0)),''))
                display[depth]=(child if child is not None else old[0],m or old[1],tint or old[2],name or old[3])
            elif k==28:display.pop(struct.unpack_from('<H',b)[0],None)
            elif k==1:
                if found and at==index:return [display[d] for d in sorted(display)]
                at+=1
        raise ValueError('Native frame missing')
    def collect(self,ident,transform=IDENTITY,tint=((1,1,1,1),(0,0,0,0)),label=None,depth=0):
        if depth>16:raise ValueError('Cyclic UI artwork')
        kind,body=self.defs[ident]
        if kind==37:return [] # Dynamic label is rendered separately with the native font.
        if kind!=39:
            return [(style,[(point(transform,a),point(transform,b)) for a,b in edges],transform,tint) for style,edges in shapes(kind,body)]
        result=[]
        for child,m,c,name in self.frame(ident,label=label):
            if not child:continue
            combined=(tuple(a*b for a,b in zip(tint[0],c[0])),tuple(a*b+o for a,b,o in zip(tint[0],c[1],tint[1])))
            result+=self.collect(child,compose(transform,m),combined,depth=depth+1)
        return result


def edge_mask(edges,size):
    mask=Image.new('L',size);draw=ImageDraw.Draw(mask)
    for y in range(size[1]):
        crosses=[]
        for a,b in edges:
            if min(a[1],b[1])<=y+.5<max(a[1],b[1]):
                crosses.append((a[0]+(y+.5-a[1])*(b[0]-a[0])/(b[1]-a[1]),1 if b[1]>a[1] else -1))
        winding=0;previous=0
        for x,delta in sorted(crosses):
            if winding:
                start,end=math.ceil(previous-.5),math.floor(x-.5)
                if start<=end:draw.line((start,y,end,y),fill=255)
            winding+=delta;previous=x
    return mask


def inverse(m):
    a,b,c,d,x,y=m;det=a*d-b*c
    return d/det,-c/det,(c*y-d*x)/det,-b/det,a/det,(b*x-a*y)/det


def render(ui,ident,label,width=280,height=88):
    groups=ui.collect(ident,label=label)
    groups=[g for g in groups if not (g[0][0]=='solid' and g[0][1][3]==0)]
    pts=[p for _,edges,_,_ in groups for e in edges for p in e]
    left,right=min(p[0] for p in pts),max(p[0] for p in pts)
    top,bottom=min(p[1] for p in pts),max(p[1] for p in pts)
    size=(width*2,height*2);sx,sy=(size[0]-4)/(right-left),(size[1]-4)/(bottom-top)
    screen=(sx,0,0,sy,2-left*sx,2-top*sy)
    out=Image.new('RGBA',size)
    for fill,edges,transform,tint in groups:
        mask=edge_mask([(point(screen,a),point(screen,b)) for a,b in edges],size)
        if fill[0]=='solid':layer=Image.new('RGBA',size,fill[1])
        elif fill[1]==65535:continue
        else:layer=ui.bitmap(fill[1]).transform(size,Image.Transform.AFFINE,inverse(compose(screen,compose(transform,fill[2]))),Image.Resampling.BILINEAR)
        channels=layer.split()
        channels=[ch.point([max(0,min(255,round(v*tint[0][i]+tint[1][i]))) for v in range(256)]) for i,ch in enumerate(channels)]
        from PIL import ImageChops
        channels[3]=ImageChops.multiply(channels[3],mask)
        out.alpha_composite(Image.merge('RGBA',channels))
    return out.resize((width,height),Image.Resampling.LANCZOS)


def build():
    archive=GameArchive(ROOT.parents[1]/'resources.gpak');ui=NativeUi(archive)
    prompt=ui.symbols['ConfirmationBox']
    button=next(child for child,_,_,name in ui.frame(prompt,index=0) if name=='yes')
    paper=next(fill[1] for fill,*_ in ui.collect(prompt) if fill[0]=='bitmap' and fill[1]!=65535)
    tooltip=next(fill[1] for fill,*_ in ui.collect(ui.symbols['MutationTooltip']) if fill[0]=='bitmap' and fill[1]!=65535)
    images=[('Paper',ui.bitmap(paper).resize((384,384),Image.Resampling.LANCZOS))]
    images += [(state.capitalize(),render(ui,button,state)) for state in ('up','over','down','disabled')]
    images += [('Tooltip',ui.bitmap(tooltip).resize((384,234),Image.Resampling.LANCZOS))]
    # Cache grayscale + alpha, preserving native paper grain without a PNG decoder.
    data=bytearray();records=[]
    for name,image in images:
        records.append((name,image.width,image.height,len(data)))
        data+=image.convert('LA').tobytes()
    out=['// Generated from installed game artwork; do not redistribute.','#pragma once',
         '#include "ui_skin_data.hpp"',
         'const NativeUiSkinImage kNativeUiSkinImages[6] = {']
    out += ['    {%d,%d,%d}, // %s'%(w,h,offset,name) for name,w,h,offset in records]
    out += ['};','const unsigned char kNativeUiSkinPixels[] = {']
    out += ['    '+','.join(str(x) for x in data[i:i+64])+',' for i in range(0,len(data),64)]
    out+=['};','']
    (ROOT/'src/ui_skin.generated.hpp').write_text('\n'.join(out),encoding='utf-8')
    fonts=[]
    for file,symbol in (('swfs/fonts.swf','TikaFont'),('swfs/international_fonts.swf','TikaFontCN')):
        tags=read_swf(archive,file);ident=next(i for k,b,_ in tags if k==76 for i,name in symbol_entries(b) if name==symbol)
        fonts.append(next(b for k,b,_ in tags if k==75 and struct.unpack_from('<H',b)[0]==ident))
    ttf,count=make_ttf(fonts)
    target=ROOT/'build/assets/native-ui.ttf';target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(ttf)
    font=ImageFont.truetype(str(target),24)
    preview=Image.new('RGB',(980,600),(86,88,86));preview.paste(images[0][1].resize((960,580)),(10,10),images[0][1].resize((960,580)))
    draw=ImageDraw.Draw(preview)
    draw.text((35,28),'猫咪列表  新生猫筛选  老猫数量控制',font=font,fill=(20,20,18))
    for i,(name,image) in enumerate(images[1:5]):
        x,y=38+(i%2)*470,95+(i//2)*130
        preview.paste(image,(x,y),image);draw.text((x+55,y+25),'确定处理',font=font,fill=(25,25,23));draw.text((x+300,y+25),name,font=font,fill=(30,30,27))
    draw.text((38,398),'名字 / 职业   年龄   真实(遗传)   好突变   坏突变',font=font,fill=(25,25,23))
    draw.text((38,456),'桑葚   无   1      54(49)       7        1',font=font,fill=(25,25,23))
    draw.text((38,514),'滚轮上下 · Shift+滚轮左右 · 点击表头排序',font=font,fill=(55,55,50))
    preview.save(ROOT/'build/native-ui-assets-preview.png')
    print(f'Native UI cached: {len(images)} paper/button skins, {count} native font glyphs, {len(data)} LA bytes. Original SWFs unchanged.')


if __name__=='__main__':build()
