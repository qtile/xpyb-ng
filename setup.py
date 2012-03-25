import shutil, os, sys, errno
from collections import namedtuple
from distutils.core import setup, Extension
from distutils.command.build_ext import build_ext as _build_ext
from distutils.command.install_headers import install_headers
from distutils.sysconfig import get_config_var, PREFIX
from shutil import copyfile
from tools import py_client

VERSION="1.3"

CFLAGS=["-Wall"]

XCB_PATHS = [
    "/usr/share/xcb",
    "/usr/local/share/xcb",
]

def find_xcb():
    for i in XCB_PATHS:
        if os.path.isdir(i):
            return i
    raise ValueError("Could not detect xcb protocol definition location...")

xmlfiles = [
    "bigreq", "composite", "damage", "dpms", "glx",
    "randr", "record", "render", "res", "screensaver",
    "shape", "shm", "sync", "xc_misc", "xevie",
    "xf86dri", "xfixes", "xinerama", "xprint", "xproto",
    "xtest", "xvmc", "xv"
]
extensions = [
    "conn", "constant", "cookie",
    "except", "ext", "extkey", "list", "module",
    "protobj", "void"
]
ext_modules = [
    Extension(
        "xcb.xcb",
        sources = ["xcb/%s.c" % i for i in extensions],
        libraries = ["xcb"],
        extra_compile_args=CFLAGS
    )
]

class build_ext(_build_ext):
    def run(self):
        for i in xmlfiles:
            py_client.build(os.path.join(find_xcb(), "%s.xml"%i))
        return _build_ext.run(self)

### XXX: The following is a hack. py2cairo looks for xpyb.pc when configuring.
### However (as near as I can find) there is no way to get it to install with
### distutils, so we fake it here.
# make xpyb.pc file
class PCOpt(object):
    def __init__(self, replace_str, val):
        self.replace_str = replace_str
        self.val = val

def gen_pc():
    # --root is provided when doing maintainer type things, so set the prefix
    # to local in that case, or /usr/local when doing normal things.
    rootarg = filter(lambda a: a.startswith('--root'), sys.argv)
    try:
        rootarg = rootarg[0].split('=')[1]
    except IndexError:
        rootarg = ''
    if rootarg:
        prefix = '/usr'
    else:
        prefix = '/usr/local'

    pc_opts = {
        '--prefix': PCOpt('@prefix@', prefix),
        '--exec-prefix': PCOpt('@exec_prefix@', '${prefix}'),
        '--install-lib': PCOpt('@libdir@', '${exec_prefix}/lib'),
        '--install-headers': PCOpt('@includedir@', '${prefix}/include/python2.7/xpyb'),
        # please don't actually use this :-)
        '--dont-set-the-xcb-version': PCOpt('@PACKAGE_VERSION@', VERSION),
    }
    def override_arg(arg):
        for (k, v) in pc_opts.items():
            if arg.startswith(k):
                try:
                    v.val = arg.split('=')[1]
                except IndexError:
                    pass
    for arg in sys.argv:
        override_arg(arg)

    with open('xpyb.pc.in') as in_:
        pc = in_.read()
    with open('xpyb.pc', 'w') as out:
        for opt in pc_opts.values():
            pc = pc.replace(opt.replace_str, opt.val)
        out.write(pc)

    # if we're not installing, don't install the .pc
    if 'install' not in sys.argv:
        return

    def resolve(path):
        """ Resolve a path through pkgconfig variables. """
        for opt in pc_opts.values():
            name = '${' + opt.replace_str[1:-1] + '}' # strip off @s
            if name in path:
                path = path.replace(name, opt.val)
                path = resolve(path)
        return path

    # XXX: moar haxxx: here we strip off the leading slash to keep join() happy
    install_path = resolve(pc_opts['--install-lib'].val).strip('/')
    pkgconfig = os.path.join(rootarg, install_path, 'pkgconfig')
    try:
        os.makedirs(pkgconfig)
    except OSError as e:
        if e.errno == errno.EEXIST:
            pass
        else:
            raise
    copyfile('xpyb.pc', os.path.join(pkgconfig, 'xpyb.pc'))

setup(
    name = 'xpyb',
    version = VERSION,
    ext_modules = ext_modules,
    packages = ["xcb"],
    cmdclass = {
        "build_ext": build_ext
    },
    headers=['xcb/xpyb.h'],
)
gen_pc()
