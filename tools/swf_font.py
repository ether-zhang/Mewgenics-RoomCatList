"""Convert installed DefineFont3 outlines into a local, non-redistributed TTF.

The game and the list use the same quadratic glyphs/advances. No fonts are
downloaded; the generated font is only a runtime cache of the installed SWF.
"""
import struct
from build_marker_icons import Reader


def font_glyphs(body):
    ident, flags, language, length = struct.unpack_from('<HBBB', body)
    at = 5 + length
    count = struct.unpack_from('<H', body, at)[0]; at += 2
    wide = bool(flags & 8); step = 4 if wide else 2; fmt = '<I' if wide else '<H'
    offsets = [struct.unpack_from(fmt, body, at + i*step)[0] for i in range(count+1)]
    codes_at = at + offsets[-1]
    codes = struct.unpack_from('<' + ('H' if flags & 4 else 'B')*count, body, codes_at)
    layout_at = codes_at + count*(2 if flags & 4 else 1)
    if not flags & 128:
        raise ValueError('Native font lacks layout metrics')
    ascent, descent, leading = struct.unpack_from('<hhh', body, layout_at)
    advances = struct.unpack_from('<'+'h'*count, body, layout_at+6)
    glyphs = {}
    for i, code in enumerate(codes):
        if not code or 0xD800 <= code <= 0xDFFF or code == 0xFFFF: continue
        r = Reader(body[at+offsets[i]:at+offsets[i+1]])
        fill_bits, line_bits = r.u(4), r.u(4)
        contours = []; contour = []; x = y = 0
        def vertex(px, py, on=True):
            return round(px/20), round(-py/20), on
        while True:
            if r.u(1):
                straight, n = r.u(1), r.u(4)+2
                if not contour: contour.append(vertex(x,y))
                if straight:
                    if r.u(1): dx,dy=r.s(n),r.s(n)
                    elif r.u(1): dx,dy=0,r.s(n)
                    else: dx,dy=r.s(n),0
                    x+=dx; y+=dy; contour.append(vertex(x,y))
                else:
                    cx,cy=x+r.s(n),y+r.s(n)
                    x,y=cx+r.s(n),cy+r.s(n)
                    contour.extend((vertex(cx,cy,False),vertex(x,y)))
            else:
                flags2=r.u(5)
                if not flags2: break
                if flags2 & 16: raise ValueError('Unexpected font style table')
                if flags2 & 1:
                    if contour: contours.append(contour); contour=[]
                    n=r.u(5); x,y=r.s(n),r.s(n)
                if flags2 & 2:r.u(fill_bits)
                if flags2 & 4:r.u(fill_bits)
                if flags2 & 8:r.u(line_bits)
        if contour:contours.append(contour)
        glyphs[code]=(contours,max(0,round(advances[i]/20)))
    return glyphs,round(ascent/20),round(descent/20)


def pack_glyph(contours):
    contours=[c[:-1] if len(c)>1 and c[-1]==c[0] else c for c in contours if c]
    pts=[p for c in contours for p in c]
    if not pts:return struct.pack('>hhhhhH',0,0,0,0,0,0),0,0,0
    xs=[p[0] for p in pts]; ys=[p[1] for p in pts]
    bounds=(min(xs),min(ys),max(xs),max(ys))
    out=bytearray(struct.pack('>hhhhh',len(contours),*bounds));end=0
    for c in contours:end+=len(c);out+=struct.pack('>H',end-1)
    out+=struct.pack('>H',0)+bytes(1 if p[2] else 0 for p in pts)
    for axis in (0,1):
        prev=0
        for p in pts:out+=struct.pack('>h',p[axis]-prev);prev=p[axis]
    return bytes(out),bounds[0],len(pts),len(contours)


def make_ttf(fonts):
    merged={};ascent=descent=0
    for body in fonts:
        glyphs,a,d=font_glyphs(body);ascent=max(ascent,a);descent=max(descent,d)
        for code,glyph in glyphs.items():merged.setdefault(code,glyph)
    codes=sorted(merged); order=[([],512)]+[merged[c] for c in codes]
    glyf=bytearray();loca=[];hmtx=bytearray();max_points=max_contours=0
    for contours,advance in order:
        loca.append(len(glyf));data,left,points,contour_count=pack_glyph(contours)
        glyf+=data+b'\0'*((-len(data))%4)
        hmtx+=struct.pack('>Hh',advance,left)
        max_points=max(max_points,points);max_contours=max(max_contours,contour_count)
    loca.append(len(glyf));count=len(order)
    segments=[]
    for gid,code in enumerate(codes,1):
        delta=(gid-code)&65535
        if segments and code==segments[-1][1]+1 and delta==segments[-1][2]:segments[-1]=(segments[-1][0],code,delta)
        else:segments.append((code,code,delta))
    segments.append((65535,65535,1));n=len(segments);power=1<<(n.bit_length()-1)
    form=bytearray(struct.pack('>HHHHHHH',4,16+8*n,0,2*n,2*power,power.bit_length()-1,2*n-2*power))
    form+=struct.pack('>'+'H'*n,*[s[1] for s in segments])+b'\0\0'
    form+=struct.pack('>'+'H'*n,*[s[0] for s in segments])
    form+=struct.pack('>'+'H'*n,*[s[2] for s in segments])+b'\0\0'*n
    cmap=struct.pack('>HHHHIHHI',0,2,0,3,20,3,1,20)+form
    head=struct.pack('>IIIIHHQQhhhhHHhhh',0x10000,0x10000,0,0x5F0F3CF5,11,1024,0,0,-2048,-2048,2048,2048,0,8,2,1,0)
    hhea=struct.pack('>IhhhHhhhhhhhhhhhH',0x10000,ascent,-descent,0,max(a for _,a in order),-2048,0,2048,1,0,0,0,0,0,0,0,count)
    maxp=struct.pack('>I'+'H'*14,0x10000,count,max_points,max_contours,0,0,2,0,0,0,0,0,0,0,0)
    names=[];strings=bytearray()
    for name_id,value in ((1,'RoomCatList Game UI'),(2,'Regular'),(4,'RoomCatList Game UI'),(6,'RoomCatListGameUI')):
        data=value.encode('utf-16-be');names.append(struct.pack('>HHHHHH',3,1,0x409,name_id,len(data),len(strings)));strings+=data
    name=struct.pack('>HHH',0,len(names),6+12*len(names))+b''.join(names)+strings
    post=struct.pack('>IihhIIIII',0x30000,0,0,0,0,0,0,0,0)
    tables={'cmap':bytes(cmap),'glyf':bytes(glyf),'head':head,'hhea':hhea,'hmtx':bytes(hmtx),
            'loca':struct.pack('>'+'I'*len(loca),*loca),'maxp':maxp,'name':bytes(name),'post':post}
    def checksum(data):
        data+=b'\0'*((-len(data))%4)
        return sum(struct.unpack('>'+'I'*(len(data)//4),data))&0xFFFFFFFF
    total=len(tables);power=1<<(total.bit_length()-1);offset=12+16*total;directory=bytearray();payload=bytearray();head_at=0
    for tag,data in sorted(tables.items()):
        directory+=tag.encode()+struct.pack('>III',checksum(data),offset,len(data))
        if tag=='head':head_at=offset
        padding=b'\0'*((-len(data))%4);payload+=data+padding;offset+=len(data)+len(padding)
    out=bytearray(struct.pack('>IHHHH',0x10000,total,16*power,power.bit_length()-1,16*total-16*power)+directory+payload)
    struct.pack_into('>I',out,head_at+8,(0xB1B0AFBA-checksum(bytes(out)))&0xFFFFFFFF)
    return bytes(out),len(codes)
