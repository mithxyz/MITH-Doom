"""Load the shipped bundle through the FFGL ABI and test actual GPU output."""
import ctypes as C
import pathlib, time, json, os
from pathlib import Path
from PIL import Image, ImageChops, ImageStat
ROOT=pathlib.Path(__file__).resolve().parents[1]
OUT=ROOT/'previews'; OUT.mkdir(exist_ok=True)
G=C.CDLL('/System/Library/Frameworks/OpenGL.framework/OpenGL')
I=C.c_int; U=C.c_uint; F=C.c_float; P=C.c_void_p
def fn(name,rest,args):
    f=getattr(G,name);f.restype=rest;f.argtypes=args;return f
ctx=P();pf=P();num=I()
assert fn('CGLChoosePixelFormat',I,[C.POINTER(I),C.POINTER(P),C.POINTER(I)])((I*3)(99,0x3200,0),C.byref(pf),C.byref(num))==0
assert fn('CGLCreateContext',I,[P,P,C.POINTER(P)])(pf,None,C.byref(ctx))==0
fn('CGLDestroyPixelFormat',I,[P])(pf)
assert fn('CGLSetCurrentContext',I,[P])(ctx)==0

class Mixed(C.Union): _fields_=[('UIntValue',U),('PointerValue',P)]
class VP(C.Structure): _fields_=[('x',U),('y',U),('width',U),('height',U)]
class SetParam(C.Structure): _fields_=[('number',U),('value',Mixed)]
class Process(C.Structure): _fields_=[('count',U),('textures',P),('fbo',U)]
lib=C.CDLL(os.environ.get('DOOM_PLUGIN',str(ROOT/'dist/MITH Doom.bundle/Contents/MacOS/MITH Doom')))
entry=lib.plugMain;entry.restype=Mixed;entry.argtypes=[U,Mixed,P]
def call(op,value=None,instance=None):
    m=Mixed()
    if isinstance(value,int):m.UIntValue=value
    elif value is not None:m.PointerValue=C.cast(C.byref(value),P)
    return entry(op,m,instance)
assert call(34).UIntValue==0
assert call(4).UIntValue==30
assert call(15,0).UIntValue==14
assert call(15,29).UIntValue==100
assert C.string_at(call(6,29).PointerValue).decode()=='https://MITH.xyz'
assert C.string_at(call(0).PointerValue,8)[0:4] # Plugin metadata available.

gen=fn('glGenTextures',None,[I,C.POINTER(U)]); bind=fn('glBindTexture',None,[U,U])
active=fn('glActiveTexture',None,[U]); teximage=fn('glTexImage2D',None,[U,I,I,I,I,I,U,U,P])
texparam=fn('glTexParameteri',None,[U,U,I]); bindfbo=fn('glBindFramebuffer',None,[U,U])
fbo=U(); fn('glGenFramebuffers',None,[I,C.POINTER(U)])(1,C.byref(fbo)); bindfbo(0x8D40,fbo.value)
out=U(); gen(1,C.byref(out)); active(0x84C0+10); bind(0x0DE1,out.value)
texparam(0x0DE1,0x2801,0x2601); texparam(0x0DE1,0x2800,0x2601)
fn('glFramebufferTexture2D',None,[U,U,U,U,I])(0x8D40,0x8CE0,0x0DE1,out.value,0)
viewport=fn('glViewport',None,[I,I,I,I]); geti=fn('glGetIntegerv',None,[U,C.POINTER(I)])
enable=fn('glEnable',None,[U]); disable=fn('glDisable',None,[U]); isenabled=fn('glIsEnabled',C.c_ubyte,[U])
geterror=fn('glGetError',U,[])
w,h=640,480
def size(width,height):
    global w,h
    w,h=width,height; active(0x84C0+10); bind(0x0DE1,out.value)
    teximage(0x0DE1,0,0x8058,w,h,0,0x1908,0x1401,None)
    bindfbo(0x8D40,fbo.value); viewport(0,0,w,h)
    assert fn('glCheckFramebufferStatus',U,[U])(0x8D40)==0x8CD5
size(w,h)
vp=VP(0,0,w,h); instance=call(18,vp).PointerValue
assert instance and instance!=0xffffffff
proc=Process(0,None,fbo.value)
def setfloat(index,value,inst=None):
    v=Mixed(); v.UIntValue=C.cast(C.pointer(F(value)),C.POINTER(U)).contents.value
    assert call(8,SetParam(index,v),inst or instance).UIntValue==0
def setfile(path,inst=None):
    data=C.create_string_buffer(str(path).encode()); v=Mixed(); v.PointerValue=C.cast(data,P)
    assert call(8,SetParam(0,v),inst or instance).UIntValue==0
def status(inst=None):
    p=call(9,2,inst or instance).PointerValue
    return C.string_at(p).decode() if p else ''
def render(inst=None):
    assert call(17,proc,inst or instance).UIntValue==0,status(inst)
    error=geterror(); assert error==0, f'OpenGL error {hex(error)}'
    b=C.create_string_buffer(w*h*4); fn('glReadPixels',None,[I,I,I,I,U,U,P])(0,0,w,h,0x1908,0x1401,b)
    return Image.frombytes('RGBA',(w,h),b.raw).transpose(Image.Transpose.FLIP_TOP_BOTTOM)
def wait_status(prefix,inst=None):
    deadline=time.monotonic()+20
    while time.monotonic()<deadline:
        im=render(inst)
        if status(inst).startswith(prefix):return im
        time.sleep(.01)
    raise AssertionError(status(inst))
def run_for(seconds,inst=None):
    deadline=time.monotonic()+seconds
    while time.monotonic()<deadline: im=render(inst); time.sleep(.015)
    return im

def changed(a,b,box=None):
    d=ImageChops.difference(a,b).convert('RGB')
    if box: d=d.crop(box)
    return d.getbbox() is not None

def freeze(inst=None):
    setfloat(26,1,inst); return run_for(.18,inst)

setfile(''); splash=wait_status('Choose a Doom WAD'); splash.save(OUT/'source-splash.png')
wad=Path(os.environ.get('DOOM_WAD',ROOT/'samples/doom1.wad')).resolve()
assert wad.exists(),wad
setfile(wad); wait_status('Playing:'); base=run_for(2.5); base.save(OUT/'gameplay.png')
assert base.getchannel('A').getextrema()==(255,255)
assert sum(ImageStat.Stat(base.convert('RGB')).stddev)>50,'Visible game image'
# Frozen frame is stable, and movement changes the scene when resumed.
a=freeze(); b=run_for(.3); assert a.tobytes()==b.tobytes(),'Pause'
setfloat(26,0); setfloat(3,1); run_for(.3); setfloat(3,0); moved=freeze()
assert changed(a,moved,(0,0,w,int(h*.83))),'Forward movement'
moved.save(OUT/'moved.png')
setfloat(26,0); setfloat(6,1); run_for(.25); setfloat(6,0); turned=freeze()
assert changed(moved,turned,(0,0,w,int(h*.83))),'Turning'
setfloat(26,0); setfloat(7,1); run_for(.25); setfloat(7,0); strafed=freeze()
assert changed(turned,strafed,(0,0,w,int(h*.83))),'Strafing'
# Restart gives the original level again, independently of current position.
setfloat(26,0); setfloat(1,1); wait_status('Starting'); wait_status('Playing:'); initial=run_for(2.5)
setfloat(9,1); firing=run_for(.25); setfloat(9,0); run_for(.5); shot=freeze()
assert changed(initial,shot,(0,int(h*.83),w,h)),'Fire consumes ammunition in HUD'
firing.save(OUT/'firing.png')
# Opening/closing menus and automap uses actual down/up events.
setfloat(26,0); setfloat(12,1); menu=run_for(.15); setfloat(12,0); menu.save(OUT/'menu.png')
setfloat(12,1); run_for(.15); setfloat(12,0); closed=run_for(.15)
assert changed(menu,closed),'Menu toggle'
setfloat(14,1); automap=run_for(.15); setfloat(14,0); automap.save(OUT/'automap.png')
assert changed(closed,automap),'Automap'
setfloat(14,1); run_for(.15); setfloat(14,0)
# No host render calls => game automatically freezes instead of catching up.
a=run_for(.2); time.sleep(.8); b=run_for(.1); a=freeze(); time.sleep(.3); assert a.tobytes()==render().tobytes()
setfloat(26,0)
# Native WAD parameter failures are contained; Resolume output remains valid.
setfile(ROOT/'samples/missing.wad'); wait_status('WAD file not found')
corrupt=ROOT/'build/corrupt.wad'; corrupt.write_bytes(b'IWAD'+bytes(8)); setfile(corrupt); wait_status('Invalid WAD directory')
# A valid directory with nonsense lumps must fail in the child, not the host.
broken=ROOT/'build/broken.wad'; broken.write_bytes(b'IWAD'+(1).to_bytes(4,'little')+(12).to_bytes(4,'little')+bytes(16))
setfile(broken); wait_status('Engine stopped')
setfile(wad); wait_status('Playing:'); run_for(.2)
# Each instance owns its own helper, file and control state.
other=call(18,vp).PointerValue; assert other and other!=0xffffffff
setfile(wad,other); wait_status('Playing:',other); a=freeze(other)
setfloat(6,1); run_for(.25); setfloat(6,0)
assert a.tobytes()==render(other).tobytes(),'Independent game instances'
assert call(19,instance=other).UIntValue==0
# Shape and scaling options render with predictable pillar/letterboxing.
setfloat(26,1); run_for(.15)
for width,height in [(1920,1080),(3840,2160),(320,640),(1024,128),(257,129)]:
    size(width,height); im=render(); assert im.getchannel('A').getextrema()==(255,255)
    assert im.getpixel((0,0))==(0,0,0,255),'4:3 fit black bars'
    if width==1920:im.save(OUT/'hd.png')
size(640,480); a=render(); setfloat(28,1); assert changed(a,render()),'Smooth scaling'; setfloat(28,0)
size(1280,720); a=render(); setfloat(27,0); b=render(); assert changed(a,b),'Stretch'; setfloat(27,1)
size(640,480)
# All host GL state touched by the plugin must be restored, including upload PBO/stride.
caps=[0x0BE2,0x0B44,0x0B71,0x0C11,0x0B90,0x8DB9,0x8C89,0x0BF2,0x809E]
for cap in caps:enable(cap)
fn('glColorMask',None,[C.c_ubyte]*4)(1,0,1,0); active(0x84C0+3)
store=fn('glPixelStorei',None,[U,I]); store(0x0CF2,777);store(0x0CF3,2);store(0x0CF4,3);store(0x0CF5,8);store(0x0CF0,1)
buffer=U();fn('glGenBuffers',None,[I,C.POINTER(U)])(1,C.byref(buffer));bindbuffer=fn('glBindBuffer',None,[U,U]);bindbuffer(0x88EC,buffer.value)
fn('glBufferData',None,[U,C.c_long,P,U])(0x88EC,64,None,0x88E4)
def snapshot():
    r={str(cap):int(isenabled(cap)) for cap in caps}
    for key,n in [(0x8B8D,1),(0x85B5,1),(0x8CA6,1),(0x84E0,1),(0x0BA2,4),(0x0C23,4),(0x0B40,2),(0x88EF,1),(0x0CF2,1),(0x0CF3,1),(0x0CF4,1),(0x0CF5,1),(0x0CF0,1)]:
        a=(I*n)();geti(key,a);r[str(key)]=list(a)
    return r
before=snapshot(); setfloat(26,0); run_for(.15); assert snapshot()==before,'Host GL state restoration'
for cap in caps:disable(cap)
fn('glColorMask',None,[C.c_ubyte]*4)(1,1,1,1);bindbuffer(0x88EC,0)
for key,value in [(0x0CF2,0),(0x0CF3,0),(0x0CF4,0),(0x0CF5,4),(0x0CF0,0)]:store(key,value)
# Rapid path changes/restarts retain only the final request.
for i in range(5):setfile(ROOT/'samples/missing.wad');render();setfile(wad);setfloat(1,1);render()
wait_status('Playing:'); run_for(.2)
assert call(19,instance=instance).UIntValue==0
assert call(2).UIntValue==0
report={'result':'PASS','version':'0.1.1','checks':['FFGL ABI and native WAD parameter','Shareware E1M1 rendered through actual bundle','Forward, turn, strafe and firing/ammo HUD','Pause, restart, menu, automap','Missing/corrupt/unsupported WAD recovery','Independent instances and rapid restart','HD, 4K, portrait and odd sizes; 4:3 and stretch; smoothing','OpenGL state including PBO and unpack stride'],'gpu':fn('glGetString',C.c_char_p,[U])(0x1F01).decode()}
(OUT/'validation.json').write_text(json.dumps(report,indent=2)); print(json.dumps(report,indent=2))
