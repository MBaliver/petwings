import pefile, sys, os, struct
pe = pefile.PE(sys.argv[1]); out=sys.argv[2]
for t in pe.DIRECTORY_ENTRY_RESOURCE.entries:
    tname = str(t.name) if t.name else pefile.RESOURCE_TYPE.get(t.id, str(t.id))
    for e in t.directory.entries:
        n = str(e.name) if e.name else e.id
        for l in e.directory.entries:
            d = pe.get_data(l.data.struct.OffsetToData, l.data.struct.Size)
            ext={'RT_BITMAP':'bmp','WAVE':'wav','RT_RCDATA':'txt'}.get(tname,'bin')
            if tname=='RT_BITMAP':
                # prepend BITMAPFILEHEADER
                hs=struct.unpack('<I',d[:4])[0]; bpp=struct.unpack('<H',d[14:16])[0]; clr=struct.unpack('<I',d[32:36])[0]
                pal=(clr or (1<<bpp if bpp<=8 else 0))*4
                d=b'BM'+struct.pack('<IHHI',14+len(d),0,0,14+hs+pal)+d
            open(f"{out}/{tname}_{n}.{ext}",'wb').write(d)
