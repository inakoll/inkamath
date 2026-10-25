"""A model from a header built as a shared library with -D<NAME>_SHARED, read
from its tables alone (docs/compile.md, Hosting from C and Python)."""
import ctypes
import itertools
import re

_C = {"double": ctypes.c_double, "float": ctypes.c_float,
      "long long": ctypes.c_longlong, "int": ctypes.c_int}


def _cells(base, count, values):
    if len(values) != count:
        raise ValueError(f"{count} cells, not {len(values)}")
    return (base * count)(*values)


class Model:
    def __init__(self, path, name):
        lib = ctypes.CDLL(path)
        count = ctypes.c_size_t.in_dll(lib, name + "_count_").value
        strings = lambda table: [
            s.decode() for s in (ctypes.c_char_p * count).in_dll(lib, name + table)]
        offsets = (ctypes.c_size_t * count).in_dll(lib, name + "_offsets_")
        size = ctypes.c_size_t.in_dll(lib, name + "_size_").value
        # Of doubles, so as aligned as any member.
        self._state = (ctypes.c_double * ((size + 7) // 8))()
        self._views, self._bases = {}, {}
        for key, offset, ctype in zip(strings("_members_"), offsets, strings("_types_")):
            base, dims = re.fullmatch(r"([a-z ]+?)((?:\[\d+\])*)", ctype).groups()
            view = self._bases[key] = _C[base]
            for d in reversed(re.findall(r"\d+", dims)):
                view = view * int(d)
            self._views[key] = view.from_buffer(self._state, offset)
        # Not counted, the inputs end at their null pointer.
        given = ctypes.cast(ctypes.addressof(ctypes.c_char_p.in_dll(lib, name + "_inputs_")),
                            ctypes.POINTER(ctypes.c_char_p))
        self._inputs, args = [], [ctypes.c_void_p]
        for key in itertools.takewhile(bool, given):
            key, optional = key.decode().rstrip("?"), key.endswith(b"?")
            real, view = self._bases[key], self._views[key]
            cells = ctypes.sizeof(view) // len(view) // ctypes.sizeof(real)
            args += [real if cells == 1 else ctypes.POINTER(real)] + [ctypes.c_int] * optional
            self._inputs.append((key, real, cells, optional))
        self._init, self._update, self._step = (
            getattr(lib, name + f) for f in ("_init", "_update", "_step"))
        self._init.argtypes = self._update.argtypes = [ctypes.c_void_p]
        self._step.argtypes = args
        self._init(self._state)

    def __getitem__(self, key):
        """The member as the struct holds it: m["u"][0] is u_n, m["kp"].value kp."""
        return self._views[key]

    def __setitem__(self, key, value):
        """A parameter, a number or its cells in C order, and what derives from it."""
        view, base = self._views[key], self._bases[key]
        if isinstance(view, base):
            view.value = value
        else:
            cells = _cells(base, ctypes.sizeof(view) // ctypes.sizeof(base), value)
            ctypes.memmove(view, cells, ctypes.sizeof(view))
        self._update(self._state)

    def step(self, *values):
        """One value an input, a number or its cells in C order; None for an optional one absent."""
        args = [self._state]
        for (key, real, cells, optional), value in zip(self._inputs, values, strict=True):
            present = value is not None
            if not present and not optional:
                raise ValueError(f"{key} is not optional")
            value = value if present else 0.0 if cells == 1 else [0.0] * cells
            args.append(value if cells == 1 else _cells(real, cells, value))
            args += [int(present)] * optional
        self._step(*args)
