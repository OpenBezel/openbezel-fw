"""depfile - let a generator report what it read, as a Make-style depfile for
Ninja (meson custom_target(depfile:)).

    import depfile
    depfile.track()                       # first thing in main()
    ...                                   # every file opened for reading is noted
    depfile.write(path, outputs)          # at the end; no-op when path is None

The generator's own Python modules count too, so editing gen_theme.py or
themelib.py reruns it."""
import builtins, os, sys

_read = set()
_open = builtins.open


def _tracking_open(file, mode="r", *a, **k):
    if isinstance(file, (str, bytes, os.PathLike)) and not any(c in mode for c in "wax+"):
        _read.add(os.path.abspath(os.fsdecode(file)))
    return _open(file, mode, *a, **k)


def track():
    builtins.open = _tracking_open


def _esc(p):
    return p.replace(" ", "\\ ")


def write(path, outputs):
    if not path:
        return
    deps = set(_read)
    here = os.path.dirname(os.path.abspath(__file__))
    for m in list(sys.modules.values()):
        f = getattr(m, "__file__", None)
        if f and os.path.abspath(f).startswith(here):
            deps.add(os.path.abspath(f))
    if sys.argv and sys.argv[0].endswith(".py"):
        deps.add(os.path.abspath(sys.argv[0]))
    outs = [outputs] if isinstance(outputs, str) else list(outputs)
    for o in outs:                  # outputs left alone when unchanged still count as
        if os.path.exists(o):       # rebuilt, or Ninja would rerun this every build
            os.utime(o, None)
    with _open(path, "w") as f:
        f.write("%s: %s\n" % (" ".join(_esc(o) for o in outs), " ".join(_esc(d) for d in sorted(deps))))
