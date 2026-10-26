"""inkamath from Python: values from the interpreter, a model compiled and
loaded, its check, and a .ink file imported as a module (docs/compile.md,
Hosting). Not a stable interface while the language moves."""
import ctypes
import hashlib
import importlib.util
import itertools
import math
import os
import pathlib
import re as _re
import shlex
import shutil
import stat
import subprocess
import sys
import tempfile
from fractions import Fraction

_PRELUDE = ("floor", "ceil", "mod", "re", "im", "abs", "max", "min", "exp", "log", "tanh",
            "ilogb", "sin", "cos", "atan", "atan2", "asin", "acos", "acosh", "charpoly",
            "conv", "polyval", "roots", "hurwitz", "schurcohn", "rho", "abscissa", "eig",
            "chol", "smax", "hinf", "dhinf")
_BUILTINS = ("eval", "abs", "max", "min", "re")  # importable by name only
__all__ = ["Model", "check", "load", "describe", "tex", "pi", "e", "i",
           *(name for name in _PRELUDE if name not in _BUILTINS)]
_EXT = ".dll" if os.name == "nt" else ".dylib" if sys.platform == "darwin" else ".so"
_EXE = ".exe" if os.name == "nt" else ""


def _which(command):
    """An absolute path: a bare name sought on PATH alone, never in the
    working directory, where Windows and Python 3.10's shutil.which look first."""
    if os.sep in command or (os.altsep and os.altsep in command):
        return os.path.abspath(command)
    for folder in os.environ.get("PATH", "").split(os.pathsep):
        for ext in ("", *os.environ.get("PATHEXT", "").split(os.pathsep)) if _EXE else ("",):
            found = os.path.join(folder, command + ext)
            if os.path.isabs(folder) and os.path.isfile(found) and os.access(found, os.X_OK):
                return found
    raise RuntimeError(f"{command} is not on PATH")


def _interpreter():
    if os.environ.get("INKAMATH"):
        return _which(os.environ["INKAMATH"])
    try:
        return _which("inkamath")
    except RuntimeError:
        built = pathlib.Path(__file__).resolve().parents[1] / "build" / ("inkamath" + _EXE)
        if built.is_file():
            return str(built)
        raise RuntimeError("no inkamath: set INKAMATH, or put it on PATH") from None


def _cc():
    command = shlex.split(os.environ.get("CC") or "cc")
    return [_which(command[0]), *command[1:]]


def _run(command, error, cwd=None, given=None, ok=(0,)):
    try:
        done = subprocess.run(command, input=given, capture_output=True, text=True, cwd=cwd)
    except OSError as failure:
        raise RuntimeError(f"cannot run {command[0]}: {failure}") from None
    if ok is not None and done.returncode not in ok:
        raise error("\n".join(line.removeprefix("inkamath: ")
                              for line in (done.stderr + done.stdout).splitlines()
                              if not line.startswith("Try 'inkamath --help'")))
    return done.stdout


def _inkamath(*args, cwd=None, given=None, ok=(0,)):
    return _run([_interpreter(), *args], ValueError, cwd, given, ok)


def _ink(path):
    """An absolute .ink path whose stem `use` reads, as compiling a model by name does."""
    path = os.path.abspath(path)
    stem, suffix = os.path.splitext(os.path.basename(path))
    if suffix != ".ink":
        raise ValueError(f"{path} is neither a .ink file nor a shared library")
    if not _re.fullmatch(r"[A-Za-z]+[0-9]*", stem):
        raise ValueError(f"{path}: use reads a file named as a name is, letters then digits, "
                         f"and {stem} is not one")
    return path


def _names(path):
    return [line.split(" ") for line in _inkamath("--names", path).splitlines()]


# Values: a number formatted for the interpreter, and an answer read back.

def _number(x):
    if isinstance(x, (int, Fraction)):
        return str(int(x) if isinstance(x, bool) else x)
    if isinstance(x, float):
        return repr(x)  # read as written, as data is
    if isinstance(x, complex):
        return f"({x.real!r}+({x.imag!r})*i)"
    raise TypeError(f"{x!r} is not a number")


def _nest(x):
    """Nested lists of formatted cells, and their shape."""
    x = x.tolist() if hasattr(x, "tolist") else x
    if not isinstance(x, (list, tuple)):
        return _number(x), ()
    items = [_nest(item) for item in x]
    if not items or any(shape != items[0][1] for _, shape in items):
        raise ValueError(f"{x!r} is empty or ragged")
    return [cells for cells, _ in items], (len(items), *items[0][1])


def _format(x):
    cells, shape = _nest(x)
    if len(shape) > 3:
        raise ValueError(f"{x!r} has more than three indices")
    joins = ((), ("; ",), ("; ", ", "), (";; ", "; ", ", "))[len(shape)]

    def join(part, k):
        return joins[k].join(join(p, k + 1) for p in part) if k < len(joins) else part
    return f"[{join(cells, 0)}]" if shape else cells


_COMPLEX = _re.compile(r"(.*?)([+-]?)i(?:\*(.+))?")


def _cell(text):
    if "i" in text.replace("inf", ""):  # ~(a+i*b), or a NaN's -nan+i*-nan without ~
        real, sign, size = _COMPLEX.fullmatch(text.removeprefix("~(").removesuffix(")")).groups()
        return complex(float(real or 0), float(sign + "1") * float(size or 1))
    if text.startswith("~") or "nan" in text:
        return float(text.lstrip("~"))
    value = Fraction(text)
    return value.numerator if value.denominator == 1 else value


def _value(text):
    """A 1x1 a value, a column a list, a row or a matrix rows, a tensor slices."""
    text = "".join("".join(line.split("#")[0].split()) for line in text.splitlines())
    try:
        if not text.startswith("["):
            return _cell(text)
        slices = [[[_cell(c) for c in row.split(",")] for row in part.split(";")]
                  for part in text[1:-1].split(";;")]
    except (ValueError, AttributeError):  # AttributeError: no complex matched
        raise ValueError(f"{text} is not a value") from None
    rows = slices[0]
    if len(slices) > 1:
        return slices
    if len(rows) > 1 and all(len(row) == 1 for row in rows):
        return [row[0] for row in rows]
    return rows[0][0] if len(rows) == 1 and len(rows[0]) == 1 else rows


def _ask(line, file):
    """The last answer of a session, after the file's, any error among them raised."""
    if "\n" in line or "\r" in line:
        raise ValueError("an expression of more than one line")
    args = ["--echo"] + ([os.path.abspath(file), "-i"] if file is not None else [])
    out = _inkamath(*args, given=f"digits = 17\n{line}\n", ok=(0, 1))  # 1: an error answered
    answers = []
    for text in out.splitlines():
        if text.startswith(">> "):
            answers.append([])
        elif answers and not text.startswith(".. "):
            answers[-1].append(text)
    answers = ["\n".join(answer).rstrip("\n") for answer in answers]
    for answer in answers[:-1]:
        if answer.startswith("error: "):
            raise ValueError(answer.removeprefix("error: "))
    return answers[-1]


def _said(line, file):
    answer = _ask(line, file)
    if answer.startswith("error: "):
        raise ValueError(answer.removeprefix("error: "))
    return answer


def eval(expr, *, file=None):
    """The value of an expression, exact where it is, after a file's definitions."""
    answer = _ask(f"frac {expr}", file)
    # frac refuses an approximated or complex value: read as shown.
    return _value(_said(expr, file) if answer.startswith("error: ") else answer)


def describe(name, *, file=None):
    """The definition as `?name` prints it."""
    return _said(f"?{name}", file)


def tex(name, *, file=None):
    """The definition as `tex ?name` prints it."""
    return _said(f"tex ?{name}", file)


def _function(name, file=None):
    def call(*args):
        return eval(f"{name}({', '.join(map(_format, args))})", file=file)
    call.__name__ = call.__qualname__ = name
    return call


globals().update((name, _function(name)) for name in _PRELUDE)


def __getattr__(name):
    if name not in ("pi", "e", "i"):
        raise AttributeError(f"module 'inkamath' has no attribute {name!r}")
    value = globals()[name] = eval(name)
    return value


# A model: a library built with -D<NAME>_SHARED read from its tables alone,
# or a .ink compiled to one and cached.

def _cache():
    """Private: on POSIX a directory the user owns that no one else may write."""
    root = os.environ.get("XDG_CACHE_HOME") or os.path.join(os.path.expanduser("~"), ".cache")
    path = os.path.join(root, "inkamath")
    os.makedirs(path, mode=0o700, exist_ok=True)
    found = os.stat(path)
    if os.name == "posix" and (not stat.S_ISDIR(found.st_mode) or found.st_uid != os.getuid()
                               or found.st_mode & 0o022):
        raise RuntimeError(f"{path} is not a directory of the user's own that only they write")
    return path


_known = {}


def _identity(cc):
    """The compiler's first line of --version, read once a process."""
    if tuple(cc) not in _known:
        out = _run([*cc, "--version"], RuntimeError, ok=None)
        _known[tuple(cc)] = out.partition("\n")[0]
    return _known[tuple(cc)]


def _library(path, name, float32):
    """The library of a .ink compiled, built only where the cache has none."""
    with tempfile.TemporaryDirectory() as folder:
        _inkamath("--compile", path, *[name] * bool(name), *["--float"] * float32,
                  "-o", os.path.join(folder, "model.h"))
        header = pathlib.Path(folder, "model.h").read_bytes()
        cc = _cc()
        command = [*cc, "-O2", "-ffp-contract=off", "-shared", "-fPIC", "-x", "c",
                   "-DMODEL_SHARED", "model.h", "-o"]
        key = hashlib.sha256(b"\0".join([header, *map(str.encode, command),
                                         _identity(cc).encode()])).hexdigest()
        cache = _cache()
        library = os.path.join(cache, key + _EXT)
        if not os.path.exists(library):
            handle, built = tempfile.mkstemp(_EXT, dir=cache)
            os.close(handle)
            try:
                _run([*command, built, "-lm"], RuntimeError, folder)
                try:
                    os.replace(built, library)
                except PermissionError:  # Windows: a DLL there is loaded, and taken
                    if not os.path.exists(library):
                        raise
            finally:
                if os.path.exists(built):
                    os.remove(built)
        # The comment unwrapped, which names what the header compiles in.
        return library, " ".join(text.strip(" */") for text in header.decode().splitlines())


_C = {"double": ctypes.c_double, "float": ctypes.c_float,
      "long long": ctypes.c_longlong, "int": ctypes.c_int}


def _flat(x):
    x = x.tolist() if hasattr(x, "tolist") else x
    return [c for item in x for c in _flat(item)] if isinstance(x, (list, tuple)) else [x]


def _term(cells, dims):
    """A 1x1 a value, a row or a column a list, a matrix rows, a tensor slices."""
    rows = lambda part, width: [part[k:k + width] for k in range(0, len(part), width)]
    if len(dims) == 3:
        return [rows(part, dims[2]) for part in rows(cells, len(cells) // dims[0])]
    if len(cells) == 1:
        return cells[0]
    return list(cells) if len(dims) < 2 or 1 in dims else rows(cells, dims[1])


class Model:
    """A model stepped in C: m.step(*inputs), m[name] its current term or a
    parameter, m.history(name) a sequence's window, latest first."""

    def __init__(self, path, name=None, /, *, float32=False, **params):
        self._path, self._name, self._float32, self._given = None, name, float32, {}
        said = ""
        if os.path.splitext(path)[1] in (".so", ".dll", ".dylib"):
            if name is None or float32:
                raise ValueError("a library is loaded by its model's name, and built already")
            prefix, path = name, os.path.abspath(path)
        else:
            self._path = path = _ink(path)
            if name is None and not {"sequence", "instance"} & {k for k, _ in _names(path)}:
                self._name = os.path.splitext(os.path.basename(path))[0]
            path, said = _library(path, self._name, float32)
            prefix = "model"
        fixed = _re.search(r"Compiled in as constants, these cannot change: (.*?)\.(?: |$)", said)
        self._fixed = _re.split(", | and ", fixed[1]) if fixed else []
        lib = ctypes.CDLL(path)
        count = ctypes.c_size_t.in_dll(lib, prefix + "_count_").value
        strings = lambda table: [
            s.decode() for s in (ctypes.c_char_p * count).in_dll(lib, prefix + table)]
        offsets = (ctypes.c_size_t * count).in_dll(lib, prefix + "_offsets_")
        size = ctypes.c_size_t.in_dll(lib, prefix + "_size_").value
        # Of doubles, so as aligned as any member.
        self._state = (ctypes.c_double * ((size + 7) // 8))()
        self._members, held = {}, strings("_members_")
        for key, offset, ctype in zip(held, offsets, strings("_types_")):
            base, dims = _re.fullmatch(r"([a-z ]+?)((?:\[\d+\])*)", ctype).groups()
            dims = [int(d) for d in _re.findall(r"\d+", dims)]
            view = (_C[base] * math.prod(dims)).from_buffer(self._state, offset)
            self._members[key] = (view, dims, held.index(key) < held.index("index_"))
        # Not counted, the inputs end at their null pointer.
        given = ctypes.cast(ctypes.addressof(ctypes.c_char_p.in_dll(lib, prefix + "_inputs_")),
                            ctypes.POINTER(ctypes.c_char_p))
        self._inputs, args = [], [ctypes.c_void_p]
        for key in itertools.takewhile(bool, given):
            key, optional = key.decode().rstrip("?"), key.endswith(b"?")
            view, dims, _ = self._members[key]
            real, cells = view._type_, len(view) // dims[0]
            args += [real if cells == 1 else ctypes.POINTER(real)] + [ctypes.c_int] * optional
            self._inputs.append((key, real, cells, optional))
        self._init, self._update, self._step = (
            getattr(lib, prefix + f) for f in ("_init", "_update", "_step"))
        self._init.argtypes = self._update.argtypes = [ctypes.c_void_p]
        self._step.argtypes = args
        self._init(self._state)
        self.set(**params)

    def _member(self, key):
        if key in self._members:
            return self._members[key]
        if key in self._fixed:
            raise KeyError(f"{key} is compiled into the header as a constant: give it through "
                           ".check(), or " + ("define an instance that gives it" if self._name
                                              else "change it in the file"))
        raise KeyError(f"the model holds no {key}")

    def __getitem__(self, key):
        view, dims, parameter = self._member(key)
        return _term(view[:], dims) if parameter or key == "index_" else self.history(key)[0]

    def history(self, key):
        """The window [key_n, key_(n-1), ...]."""
        view, dims, parameter = self._member(key)
        if parameter or key == "index_":
            raise ValueError(f"{key} is a parameter, which has no history")
        n = len(view) // dims[0]
        return [_term(view[k:k + n], dims[1:]) for k in range(0, len(view), n)]

    def _assign(self, key, value):
        view, _, parameter = self._member(key)
        if not parameter:
            raise ValueError(f"{key} is a sequence, which only a step computes")
        cells = _flat(value)
        if len(cells) != len(view):
            raise ValueError(f"{key} has {len(view)} cells, not {len(cells)}")
        convert = int if view._type_ in (ctypes.c_longlong, ctypes.c_int) else float
        view[:] = [convert(c) for c in cells]
        self._given[key] = value

    def __setitem__(self, key, value):
        """A parameter, a number or its cells in C order, and what derives from it."""
        self.set(**{key: value})

    def set(self, /, **params):
        """Parameters assigned, then what derives from them computed once."""
        for key, value in params.items():
            self._assign(key, value)
        self._update(self._state)

    def step(self, *values):
        """One value an input, a number or its cells in C order; None for an optional one absent."""
        if len(values) != len(self._inputs):
            raise ValueError(f"a step takes {len(self._inputs)} inputs, not {len(values)}: "
                             + ", ".join(key for key, *_ in self._inputs))
        args = [self._state]
        for (key, real, cells, optional), value in zip(self._inputs, values):
            if value is None and not optional:
                raise ValueError(f"{key} is not optional")
            given = [0.0] * cells if value is None else [float(c) for c in _flat(value)]
            if len(given) != cells:
                raise ValueError(f"{key} has {cells} cells, not {len(given)}")
            args += [given[0] if cells == 1 else (real * cells)(*given)]
            args += [int(value is not None)] * optional
        self._step(*args)

    def check(self, steps=100):
        """Its .ink checked with the parameters assigned since it was made."""
        if self._path is None:
            raise ValueError("a library has no .ink to check")
        return _check(self._path, self._name, steps, self._float32, self._given)


def check(path, name=None, /, *, steps=100, float32=False, **params):
    """--check's report on an instance, a model or the file's own step, compiled
    and run; RuntimeError with the report where a term parts."""
    return _check(path, name, steps, float32, params)


def _check(path, name, steps, float32, params):
    path = _ink(path)
    with tempfile.TemporaryDirectory() as folder:
        target = path
        kind = dict((n, k) for k, n in _names(path)).get(name, "") if params and name else None
        if kind == "instance":
            raise ValueError(f"{name} is an instance, checked as the file gives it")
        if params and kind in (None, "model"):  # else --check refuses the name itself
            # What use reads beside the file, in a directory of its own.
            for file in pathlib.Path(path).parent.glob("*.ink"):
                shutil.copy(file, folder)
            given = [f"{key} = {_format(value)}" for key, value in params.items()]
            if name:
                # Read under another stem, so the instance can be named as the model.
                shutil.copy(path, os.path.join(folder, "inkamathsource.ink"))
                target = os.path.join(folder, "inkamathcheck.ink")
                text = f"use inkamathsource\n{name} = inkamathsource.{name}({', '.join(given)})\n"
            else:
                target = os.path.join(folder, os.path.basename(path))
                text = pathlib.Path(path).read_text()
                first = next((t for t in text.splitlines() if t.strip()
                              and not t.lstrip().startswith("#")), "")
                text += "".join(f"\n{'>> ' * first.startswith('>>')}{g}" for g in given) + "\n"
            pathlib.Path(target).write_text(text)
        _inkamath("--check", target, *[name] * bool(name), "--steps", str(steps),
                  *["--float"] * float32, "-o", os.path.join(folder, "check.c"))
        _run([*_cc(), "-O2", "-ffp-contract=off", "check.c", "-o", "check", "-lm"],
             RuntimeError, folder)
        return _run([os.path.join(folder, "check" + _EXE)], RuntimeError, folder)


# A .ink imported: its models, functions and values, and its own step.

class _Maker:
    def __init__(self, path, name):
        self._path, self._name = path, name

    def __call__(self, /, *, float32=False, **params):
        return Model(self._path, self._name, float32=float32, **params)

    def check(self, /, *, steps=100, float32=False, **params):
        return _check(self._path, self._name, steps, float32, params)


class _Named(_Maker):
    def describe(self):
        return describe(self._name, file=self._path)

    def tex(self):
        return tex(self._name, file=self._path)


class _Loader:
    @staticmethod
    def create_module(spec):
        return None

    @staticmethod
    def exec_module(module):
        path = _ink(module.__file__)
        names = _names(path)
        given = {name: _Named(path, name) if kind == "model" else _function(name, path)
                 for kind, name in names if kind in ("model", "function")}
        values = {name for kind, name in names if kind == "value"}
        stem = os.path.splitext(os.path.basename(path))[0]
        if stem not in {name for _, name in names} and {"sequence", "instance"} & {
                kind for kind, _ in names}:
            given[stem] = _Maker(path, None)
        vars(module).update(given, __all__=sorted([*given, *values]))

        def read(name):  # a value read at its first access, and kept
            if name not in values:
                raise AttributeError(f"module {module.__name__!r} has no attribute {name!r}")
            value = vars(module)[name] = eval(name, file=path)
            return value
        module.__getattr__ = read


def load(path):
    """A .ink as a module, outside sys.modules."""
    path = _ink(path)
    spec = importlib.util.spec_from_file_location(
        os.path.splitext(os.path.basename(path))[0], path, loader=_Loader)
    module = importlib.util.module_from_spec(spec)
    _Loader.exec_module(module)
    return module


class _Finder:
    """After Python's own finders: a .py of the same stem wins."""

    @staticmethod
    def find_spec(fullname, path=None, target=None):
        folders = list(sys.path) if path is None else list(path)
        if path is None and not (sys.flags.isolated or getattr(sys.flags, "safe_path", False)):
            folders += [os.path.dirname(os.path.abspath(sys.argv[0] if sys.argv else "")),
                        os.getcwd()]
        for folder in filter(lambda f: isinstance(f, str), folders):
            file = os.path.join(folder or os.getcwd(), fullname.rpartition(".")[2] + ".ink")
            if os.path.isfile(file):
                return importlib.util.spec_from_file_location(fullname, file, loader=_Loader)
        return None


sys.meta_path.append(_Finder)
