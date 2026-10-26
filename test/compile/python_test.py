"""tools/inkamath.py, specified before it is built (DESIGN.md, *Hosting from
Python*): compile_shared's libraries read through Model, the interpreter's
values and the prelude through eval, and .ink files imported, compiled,
stepped and checked. Every expected value is worked out apart from the
interpreter and the compiler: by hand, by Python's fractions and doubles,
from docs/language.md's and docs/compile.md's examples, from the comments of
the fixtures, which say how each was found, or from the paper one cites.

    python_test.py [pid track pidf bank ctl]

The libraries are compile_shared's, in its order; INKAMATH names the
interpreter, else the module finds its own; CC is a cc-compatible C
compiler, without which no .ink is compiled. The fixtures are copied to a
temporary directory, which holds the cache too, so nothing is written
beside the sources."""
import importlib
import math
import os
import pathlib
import shutil
import sys
import tempfile
from fractions import Fraction as F

here = pathlib.Path(__file__).resolve().parent
# Removed at exit, whatever fails, but a DLL Windows holds loaded.
scratch = tempfile.TemporaryDirectory(ignore_cleanup_errors=True)
work = pathlib.Path(scratch.name)
for fixture in here.glob("*.ink"):
    shutil.copy(fixture, work)
os.environ["XDG_CACHE_HOME"] = str(work / "cache")
sys.path[:0] = [str(here.parents[1] / "tools"), str(work)]
import inkamath
from inkamath import Model

failed = 0


def fail(what):
    global failed
    print(what)
    failed = 1


def alike(got, expected):
    if isinstance(expected, list):
        return (isinstance(got, list) and len(got) == len(expected) and
                all(map(alike, got, expected)))
    return type(got) is type(expected) and got == expected


def same(what, got, expected):
    """Equal and of the same type, cell by cell: an exact 1/3 is a Fraction."""
    if not alike(got, expected):
        fail(f"{what} is {got!r}, not {expected!r}")


def close(got, expected, within):
    if isinstance(expected, list):
        return (isinstance(got, list) and len(got) == len(expected) and
                all(close(g, x, within) for g, x in zip(got, expected)))
    return isinstance(got, (float, complex)) and abs(got - expected) <= within


def near(what, got, expected, within):
    """A float or a complex within a distance, cell by cell."""
    if not close(got, expected, within):
        fail(f"{what} is {got!r}, not within {within} of {expected!r}")


def ulps(what, got, expected, n):
    near(what, got, expected, n * math.ulp(expected))


def refused(what, kind, call, *words):
    try:
        call()
    except kind as error:
        for word in words:
            if word not in str(error):
                fail(f"{what} is refused as {error!r}, which does not say {word!r}")
        return
    except Exception as error:
        fail(f"{what} raises {error!r}, not {kind.__name__}")
        return
    fail(f"{what} is not refused")


class Rows:
    """An array's stand-in: numpy's .tolist() without numpy."""
    def __init__(self, rows):
        self.rows = rows

    def tolist(self):
        return self.rows


def model_lines(file, head):
    """A model as the file writes it, from its head to its closing brace."""
    lines = (work / file).read_text().splitlines()
    first = next(k for k, line in enumerate(lines) if line.startswith(head))
    last = next(k for k in range(first, len(lines)) if lines[k] == "}")
    return "\n".join(lines[first:last + 1])


# compile_shared's libraries: shared_test.c's values, the current term read
# whole and the window by history. pid's u_2 = kp*e_2 + ki*s_2 + kd*d_2 =
# 2*0.8 + 0.17 - 0.1, for r = 2 2*1.8 + 0.37 - 0.1 and for kp = 3 as well
# 3*1.8 + 0.37 - 0.1; u_10 = 0 + 0.45 - 0.1. bank at 3, x = 1: smooth's
# v_3 = 1 - (1 - a)^3 for a = 1/4, 2/4, 3/4, y_3 = 105/128 and s_sharp.v_3
# = 2 - 7/8. ctl's u_2 for r = 1 and s = [2; 1]: 1 - 3/2*2 - 1/2.
if len(sys.argv) > 1:
    path = dict(zip(("pid", "track", "pidf", "bank", "ctl"), sys.argv[1:], strict=True))
    pid, raised = Model(path["pid"], "pid"), Model(path["pid"], "pid", r=2)
    for n in range(11):
        pid.step(n / 10)
        raised.step(n / 10)
        if n == 2:
            near("pid's u_2", pid["u"], 1.67, 1e-14)
            near("pid's u_2 for r = 2", raised["u"], 3.87, 1e-14)
            near("pid's [e_2, e_1]", pid.history("e"), [0.8, 0.9], 1e-15)
            near("pid's [s_2, s_1]", pid.history("s"), [0.17, 0.09], 1e-15)
            near("pid's [u_2]", pid.history("u"), [1.67], 1e-14)
            same("pid's index_ at 2", pid["index_"], 2)
    near("pid's u_10", pid["u"], 0.35, 1e-14)
    same("pid's kp", pid["kp"], 2.0)
    both, item = Model(path["pid"], "pid"), Model(path["pid"], "pid")
    both.set(r=2, kp=3)
    item["r"] = 2
    for n in range(3):
        both.step(n / 10)
        item.step(n / 10)
    near("pid's u_2 for r = 2 and kp = 3", both["u"], 5.67, 1e-14)
    near("pid's u_2 for r assigned 2", item["u"], 3.87, 1e-14)

    # optional.ink's comment: x_3 = 25/8, x_15 = 487/32, d_30 = 51/4; z is
    # present at 30 and 27, absent at 28 and 29, its cells NaN there.
    track = Model(path["track"], "track")
    for n in range(31):
        given = n == 30 or (n % 3 == 0 and (n < 9 or n > 14))
        track.step((40.0 if n == 30 else n + 0.25) if given else None)
        if n in (3, 15):
            same(f"track's x_{n}", track["x"], 25 / 8 if n == 3 else 487 / 32)
    same("track's d_30", track["d"], 51 / 4)
    same("track's flags from 30 back", track.history("z_present"), [1, 0, 0, 1])
    z = track.history("z")
    if not (len(z) == 4 and z[0] == 40.0 and math.isnan(z[1]) and math.isnan(z[2]) and
            z[3] == 27.25):
        fail(f"track's z from 30 back is {z!r}, not [40.0, nan, nan, 27.25]")

    pidf = Model(path["pidf"], "pidf")
    for n in range(3):
        pidf.step(n / 10)
    near("pidf's u_2", pidf["u"], 1.67, 1e-6)

    bank = Model(path["bank"], "bank")
    for n in range(4):
        bank.step(1.0)
    same("bank's y_3", bank["y"], 105 / 128)
    same("bank's s_sharp.v_3", bank["s_sharp.v"], 1.125)
    same("bank's bank_3, a column", bank["bank"], [37 / 64, 7 / 8, 63 / 64])
    same("bank's bank_smooth_1.a", bank["bank_smooth_1.a"], 0.25)

    ctl = Model(path["ctl"], "ctl")
    for n in range(3):
        ctl.step(1, [n, 1])
    same("ctl's u_2", ctl["u"], -2.5)
    same("ctl's s_2, a column", ctl["s"], [2.0, 1.0])

    for what, kind, call in (
            ("pid.step(None)", ValueError, lambda: pid.step(None)),
            ("ctl.step(1, [2])", ValueError, lambda: ctl.step(1, [2])),
            ("pid.step()", ValueError, lambda: pid.step()),
            ('pid["e"] = 1, a sequence', ValueError, lambda: pid.__setitem__("e", 1)),
            ('pid["w"]', KeyError, lambda: pid["w"]),
            ('pid.history("kp"), a parameter', ValueError, lambda: pid.history("kp")),
            ("a library without its name", ValueError, lambda: Model(path["pid"])),
            ("a library in float32", ValueError,
             lambda: Model(path["pid"], "pid", float32=True)),
            ("a library's check", ValueError, lambda: pid.check())):
        refused(what, kind, call)

# Values: exact ones as int and Fraction, approximated ones as float at 17
# digits, complex ones as complex, a column a list, a matrix a list of rows,
# a tensor a list of slices (docs/language.md, sections 1 and 2).
ev = inkamath.eval
same("1/3", ev("1/3"), F(1, 3))
same("6/3", ev("6/3"), 2)
same("2^200", ev("2^200"), 2**200)
same("3 > 2", ev("3 > 2"), 1)
same("0.25", ev("0.25"), F(1, 4))
same("~(1/3), the double", ev("~(1/3)"), 1 / 3)
same("~10^30", ev("~10^30"), 1e30)
ulps("2^0.5", ev("2^0.5"), math.sqrt(2), 1)
same("[1; 2/3], a column", ev("[1; 2/3]"), [1, F(2, 3)])
same("[1 2 3], a row", ev("[1 2 3]"), [[1, 2, 3]])
same("[1, 2; 3, 4]", ev("[1, 2; 3, 4]"), [[1, 2], [3, 4]])
same("a tensor", ev("[1, 2; 3, 4;; 5, 6; 7, 8]"), [[[1, 2], [3, 4]], [[5, 6], [7, 8]]])
same("[1;;2], a tensor of 1x1 slices", ev("[1;;2]"), [[[1]], [[2]]])
same("[1/3], a single value", ev("[1/3]"), F(1, 3))
same("2+3*i", ev("2+3*i"), 2 + 3j)
same("-i", ev("-i"), -1j)
near("(1+i)/3", ev("(1+i)/3"), (1 + 1j) / 3, 1e-16)
same("[i, 1; 2, -i]", ev("[i, 1; 2, -i]"), [[1j, 1], [2, -1j]])
# frac refuses a matrix with an approximated cell, so its cells are read as
# shown: a bare one exact, one marked ~ a float, an exact 1/3 among them.
mixed = ev("[1/4, 2^0.5]")
same("[1/4, 2^0.5]'s exact cell", mixed[0], F(1, 4))
ulps("[1/4, 2^0.5]'s root", mixed[1], math.sqrt(2), 1)
same("[1/3, 2^0.5]'s exact third, shown as ~", ev("[1/3, 2^0.5]")[0], 1 / 3)
lost = ev("[~0/~0, 1/~0, -1/~0]")
if not (len(lost) == 3 and math.isnan(lost[0]) and lost[1:] == [math.inf, -math.inf]):
    fail(f"[~0/~0, 1/~0, -1/~0] is {lost!r}, not [nan, inf, -inf]")
same("2^4000, approximated past a thousand digits", ev("2^4000"), math.inf)
refused("undefined", ValueError, lambda: ev("undefined"), "undefined is not defined")
refused("an expression of two lines", ValueError, lambda: ev("1\n2"))

mixfile = str(work / "mix.ink")
same("mix.ink's tau", ev("tau", file=mixfile), F(1, 4))
same("mix's v_1 for u = 1, b = 0: 1/4*1/2", ev("mix(u_n = 1, b_n = 0).v_1", file=mixfile),
     F(1, 8))
same("chain.ink's filters.a0", ev("filters.a0", file=work / "chain.ink"), F(1, 10))
(work / "broken.ink").write_text("a = 1\nb = 1+\n")
refused("a file that fails", ValueError, lambda: ev("a", file=work / "broken.ink"),
        "unexpected end of input after '+'")

# '?name' and 'tex ?name' (docs/language.md, sections 1 and 6).
same('describe("mod")', inkamath.describe("mod"), "mod(a, b) = a - b*floor(a/b)")
same('describe("mix")', inkamath.describe("mix", file=mixfile), model_lines("mix.ink", "mix("))
refused('describe("undefined")', ValueError, lambda: inkamath.describe("undefined"),
        "undefined is not defined")

# The prelude, the built-ins beside it, each from Python. Every value from
# docs/language.md's examples and by hand; the functions on doubles within
# the units docs/language.md gives them, past libm's own.
from inkamath import (pi, e, i, floor, ceil, mod, im, exp, log, tanh, ilogb, sin, cos, atan,
                      atan2, asin, acos, acosh, charpoly, hurwitz, schurcohn, rho, abscissa,
                      eig, smax, hinf, dhinf, chol, conv, polyval, roots)
from inkamath import re as Re, abs as Abs, max as Max, min as Min

same("pi", pi, math.pi)
same("e", e, math.e)
same("i", i, 1j)
same("floor(7/2)", floor(F(7, 2)), 3)
same("ceil(7/2)", ceil(F(7, 2)), 4)
same("mod(-7, 3) = -7 - 3*floor(-7/3)", mod(-7, 3), 2)
same("re(2+3i)", Re(2 + 3j), 2.0)
same("im(2-3i)", im(2 - 3j), -3.0)
same("abs(-7/2)", Abs(F(-7, 2)), F(7, 2))
near("abs(3+4i)", Abs(3 + 4j), 5, 1e-15)
same("max(1/3, 0.25), 0.25 read as 1/4", Max(F(1, 3), 0.25), F(1, 3))
same("min(-2, 3)", Min(-2, 3), -2)
ulps("exp(1)", exp(1), math.e, 3)
ulps("log(2)", log(2), math.log(2), 4)
ulps("tanh(1/2)", tanh(F(1, 2)), math.tanh(0.5), 4)
ulps("sin(1)", sin(1), math.sin(1), 4)
ulps("cos(1)", cos(1), math.cos(1), 4)
ulps("atan(1)", atan(1), math.pi / 4, 3)
ulps("atan2(1, -1)", atan2(1, -1), 3 * math.pi / 4, 3)
ulps("asin(1/2)", asin(F(1, 2)), math.pi / 6, 4)
ulps("acos(0)", acos(0), math.pi / 2, 4)
ulps("acosh(2)", acosh(2), math.acosh(2), 5)
# C's ilogb: the exponent of the power of two at or below.
same("ilogb(10)", ilogb(10), 3)
same("ilogb(1/3)", ilogb(F(1, 3)), -2)

A = [[1, 2], [3, 4]]
same("charpoly(A), l^2 - 5l - 2", charpoly(A), [1, -5, -2])
same("charpoly(A)(A), Cayley-Hamilton", polyval(charpoly(A), A), [[0, 0], [0, 0]])
same("hurwitz(l^2 + l/5 + 1)", hurwitz(charpoly([[0, 1], [-1, F(-1, 5)]])), 1)
same("schurcohn(z - 1/2)", schurcohn([1, F(-1, 2)]), 1)
same("schurcohn(z - 2)", schurcohn((1, -2)), 0)
same("schurcohn(z - 2, r = 3)", schurcohn([1, -2], 3), 1)
near("rho, |1/2 + i|", rho([[F(1, 2), 1], [-1, F(1, 2)]]), math.sqrt(5) / 2, 1e-15)
near("abscissa, -1/10", abscissa([[0, 1], [-1, F(-1, 5)]]), -0.1, 1e-15)
root5 = math.sqrt(5)
near("eig, (5 -+ 5^0.5)/2", eig([[2, 1], [1, 3]]), [(5 - root5) / 2, (5 + root5) / 2], 1e-15)
near("eig of a .tolist()", eig(Rows([[2, 1], [1, 3]])), [(5 - root5) / 2, (5 + root5) / 2],
     1e-15)
refused("eig of a rotation", ValueError, lambda: eig([[0, 1], [-1, 0]]),
        "eig needs a matrix whose eigenvalues are all real")
# A'A = [35, 44; 44, 56], its trace 91 and determinant 24.
near("smax", smax([[1, 2], [3, 4], [5, 6]]), math.sqrt((91 + math.sqrt(8185)) / 2), 1e-14)
# 1/(s^2 + s/2 + 1): |T(iw)|^-2 = (1 - w^2)^2 + w^2/4, least at w^2 = 7/8.
near("hinf", hinf([[0, 1], [-1, F(-1, 2)]], [0, 1], [[1, 0]]), 1 / math.sqrt(15 / 64), 1e-14)
near("dhinf, 1/(z - 1/2) at z = 1", dhinf(F(1, 2), 1, 1), 2.0, 1e-15)
same("chol", chol([[4, 2, -2], [2, 10, F(1, 2)], [-2, F(1, 2), F(9, 4)]]),
     [[2, 0, 0], [1, 3, 0], [-1, F(1, 2), 1]])
L = chol(((4, 2), (2, 3)))
same("chol([4 2; 2 3])'s exact cells", [L[0][0], L[0][1], L[1][0]], [2, 0, 1])
ulps("chol([4 2; 2 3])'s 3 - 1 rooted", L[1][1], math.sqrt(2), 1)
refused("chol of an indefinite matrix", ValueError, lambda: chol([[1, 2], [2, 1]]),
        "chol needs a positive definite matrix")
same("conv((s + 1), (s + 2))", conv([1, 1], [1, 2]), [1, 3, 2])
same("polyval(s^2 + 3s + 2, 2)", polyval([1, 3, 2], 2), 12)
same("roots((s + 1)^2), exact", roots([1, 2, 1]), [-1, -1])
near("roots(s^2 + 2s + 2)", roots([1, 2, 2]), [-1 - 1j, -1 + 1j], 1e-15)

# Python's builtins are not taken by a star import; each is there by name.
star = {}
exec("from inkamath import *", star)
for name in ("eval", "abs", "max", "min", "re"):
    if name in star:
        fail(f"from inkamath import * takes {name}")
    if not hasattr(inkamath, name):
        fail(f"inkamath has no {name}")
for name in ("Model", "check", "load", "describe", "tex", "rho", "pi", "i"):
    if name not in star:
        fail(f"from inkamath import * leaves out {name}")

# Formatted from Python and read back: a 1D sequence is a column, a 2D one
# rows, a float its repr read as written, as data is (docs/language.md,
# section 5).
(work / "shapes.ink").write_text("back(x) = x\nhalfroot = [1/2; 2^0.5]\n")
import shapes
back = shapes.back
same("a column", back([1, 2]), [1, 2])
same("a row", back([[1, 2]]), [[1, 2]])
same("rows of tuples", back(((1, 2), (3, 4))), [[1, 2], [3, 4]])
same("a tensor", back([[[1, 2], [3, 4]], [[5, 6], [7, 8]]]),
     [[[1, 2], [3, 4]], [[5, 6], [7, 8]]])
same("a .tolist()", back(Rows([[1, 2], [3, 4]])), [[1, 2], [3, 4]])
same("-1/3", back(F(-1, 3)), F(-1, 3))
same("0.1, read as written", back(0.1), F(1, 10))
same("1e-05, read as written", back(1e-05), F(1, 100000))
same("2^70", back(2**70), 2**70)
same("True", back(True), 1)
same("1.5+2i", back(1.5 + 2j), 1.5 + 2j)
same("1-2i", back(complex(1, -2)), 1 - 2j)
same("halfroot's exact cell", shapes.halfroot[0], F(1, 2))
ulps("halfroot's root", shapes.halfroot[1], math.sqrt(2), 1)
same("shapes's names", sorted(shapes.__all__), ["back", "halfroot"])
refused("a ragged matrix", ValueError, lambda: back([[1, 2], [3]]))
refused("an empty one", ValueError, lambda: back([]))
refused("a string", TypeError, lambda: back("1"))
same("load", inkamath.load(work / "shapes.ink").back(2), 2)

# The finder comes after Python's own: a .py beside a .ink of its stem wins.
(work / "shadow.py").write_text("found = 'py'\n")
(work / "shadow.ink").write_text("found = 1\n")
# Python's finder may hold work's listing from before shadow.py.
importlib.invalidate_caches()
import shadow
same("a .py beside a .ink", shadow.found, "py")
try:
    import broken
    fail("a file that fails is imported")
except ValueError:
    pass

from mix import mix
same("mix.describe()", mix.describe(), model_lines("mix.ink", "mix("))
# docs/language.md section 6 and tex.ink's gain: its head, then its
# definitions a line each, m_n, v_0 and v_n.
lines = mix.tex().splitlines()
if len(lines) != 4 or not lines[0].startswith(r"\operatorname{mix}("):
    fail(f"mix.tex() is {lines!r}, not a head and three lines")
import pidloop as loopfile
same("pidloop's names", sorted(loopfile.__all__), ["pid", "pidloop", "plant"])
for name in ("model", "check"):
    if hasattr(loopfile, name):
        fail(f"pidloop has a module-level {name}")
same("pidloop's __file__", pathlib.Path(loopfile.__file__).resolve(),
     (work / "pidloop.ink").resolve())
refused('Model("pid.txt")', ValueError, lambda: Model(work / "pid.txt"))

if not os.environ.get("CC"):
    print("CC is not set: no .ink compiled, stepped or checked")
    sys.exit(failed)

# mix with g = 1/4, from (u, b) = (1, 0), (1, 0), (0, 1): m_2 = 3/4, v_1 =
# 1/4*1/4 = 1/16, v_2 = 1/4*3/4 + 3/4*1/16 = 15/64; then g = 1, a = 1/2 and
# (2, 5): v_3 = 1/2*2 + 1/2*15/64 = 143/128. Every term a dyadic.
m = mix(g=F(1, 4))
for u, b in ((1, 0), (1, 0), (0, 1)):
    m.step(u, b)
same("mix's v_2", m["v"], 15 / 64)
same("mix's m_2", m["m"], 0.75)
same("mix's [v_2, v_1]", m.history("v"), [15 / 64, 1 / 16])
same("mix's g and a", [m["g"], m["a"]], [0.25, 0.25])
m.set(g=1, a=F(1, 2))
m.step(2, 5)
same("mix's v_3 for g = 1, a = 1/2", m["v"], 143 / 128)
refused("mix.check(), its inputs not given", ValueError, mix.check,
        "mix in mix.ink takes u, which has no default")

# Instantiated again, with other parameters, the same library is loaded,
# not built again; in float, another.
cache = work / "cache" / "inkamath"
built = {p: p.stat().st_mtime_ns for p in cache.iterdir()}
same("libraries after mix()", len(built), 1)
again = mix(g=F(1, 3))
same("the cache after mix() again", {p: p.stat().st_mtime_ns for p in cache.iterdir()}, built)
lib = Model(mixfile)
for u, b in ((1, 0), (1, 0), (0, 1)):
    lib.step(u, b)
same("Model(mix.ink)'s v_2 for g = 1/2: 1/8 + 3/4*1/8", lib["v"], 7 / 32)
same("the cache after Model(mix.ink)", {p: p.stat().st_mtime_ns for p in cache.iterdir()},
     built)
for single in (Model(mixfile, float32=True), mix(float32=True)):
    for u, b in ((1, 0), (1, 0), (0, 1)):
        single.step(u, b)
    same("mix's v_2 in float", single["v"], 7 / 32)
same("libraries after mix in float, twice", len(list(cache.iterdir())), 2)
refused("a model the file does not define", ValueError, lambda: Model(mixfile, "nomix"),
        "mix.ink defines no model nomix")
compiler = os.environ["CC"]
os.environ["CC"] = f'"{sys.executable}" -c "import sys; sys.exit(\'no compiler here\')"'
refused("a C compiler that fails", RuntimeError, mix, "no compiler here")
os.environ["CC"] = compiler

# README's getting started: pid for r = 2 as above, and the loop closed on
# its plant: x_1 = 0.2, x_2 = 0.2 + 0.1*(1.48 - 0.2) = 0.328, u_0 = 2, u_1 =
# 2*0.8 + 0.08 - 0.2 = 1.48, u_2 = 2*0.672 + 0.1472 - 0.128 = 1.3632.
from pidloop import pid, plant
p = pid(r=2)
for n in range(3):
    p.step(n / 10)
near("pidloop's pid's u_2 for r = 2", p["u"], 3.87, 1e-14)
near("its [e_2, e_1]", p.history("e"), [1.8, 1.9], 1e-15)
closed = loopfile.pidloop()
for n in range(3):
    closed.step()
near("the loop's ctl.u_2", closed["ctl.u"], 1.3632, 1e-14)
near("the loop's plt.x_2", closed["plt.x"], 0.328, 1e-15)
refused("pid.check(), its input not given", ValueError, pid.check,
        "pid in pidloop.ink takes y, which has no default")
report = inkamath.check(loopfile.__file__, "ctl").splitlines()
same("ctl's report's first line", report[0], "ctl: 100 steps from 0, against exact values")
same("ctl's report's terms", [line.split(": within ")[0] for line in report[1:]],
     ["ctl.e", "ctl.d", "ctl.s", "ctl.u"])
refused("an instance given parameters", ValueError,
        lambda: inkamath.check(loopfile.__file__, "ctl", r=2))
# A file's own step reported as checkmodel reports it: after the file, each
# term by its own name.
whole = loopfile.pidloop.check().splitlines()
same("pidloop's own report's first line", whole[0],
     "pidloop.ink: 100 steps from 0, against exact values")
for name in ("ctl.u", "plt.x"):
    if not any(line.startswith(name + ": within ") for line in whole[1:]):
        fail(f"pidloop's own check does not hold {name}: {whole!r}")

# drift.ink's tenfold: for c = 1 each term is 1, exactly; for c = 1/10 what
# rounding puts in, each step multiplies by ten, the step's doubles as
# Python's, until a term is past a billionth of one plus the exact 1/10.
from drift import tenfold, turn
holds = "tenfold: 100 steps from 0, against exact values\ntenfold.d: within 0\n"
same("tenfold.check()", tenfold.check(), holds)
c = d = 0.1
k = 0
while abs(d - 0.1) <= 1e-9 * (1 + 0.1):
    k += 1
    d = 10.0 * d - 9.0 * c
parted = f"d: {d:.17g} at {k}, where the interpreter gives 0.10000000000000001"
refused("tenfold.check(c=1/10)", RuntimeError, lambda: tenfold.check(c=F(1, 10)),
        "tenfold." + parted)
t = tenfold()
t["c"] = F(1, 10)
refused("a Model's check, its c assigned 1/10", RuntimeError, t.check, "tenfold." + parted)
same("a Model's check, its c as given", tenfold().check(), holds)
# turn by 3/5 and 4/5: p_1 = [3/5; 4/5], p_2 = [9/25 - 16/25; 12/25 + 12/25].
spun = turn()
for n in range(3):
    spun.step()
near("turn's [p_2, p_1]", spun.history("p"), [[-0.28, 0.96], [0.6, 0.8]], 1e-15)
if not turn.check(steps=5).startswith("turn: 5 steps from 0, against exact values\nturn.p: "):
    fail("turn.check(steps=5) does not report five steps of turn.p")
if not inkamath.check(work / "drift.ink", "turn", steps=5).startswith("turn: 5 steps from 0"):
    fail('inkamath.check(drift.ink, "turn", steps=5) does not report five steps of turn')
refused("turn.check(steps=0)", ValueError, lambda: turn.check(steps=0),
        "--steps takes a whole number from 1 to 100000")

# A file's own sequences, its value c given as a parameter.
(work / "decade.ink").write_text("c = 1\nd_0 = c\nd_n = 10*d_(n-1) - 9*c\n")
same("decade.ink's own check for c = 1", inkamath.check(work / "decade.ink"),
     "decade.ink: 100 steps from 0, against exact values\nd: within 0\n")
refused("decade.ink's own check for c = 1/10", RuntimeError,
        lambda: inkamath.check(work / "decade.ink", c=F(1, 10)), parted)
import decade
refused("decade.decade.check(c=1/10)", RuntimeError, lambda: decade.decade.check(c=F(1, 10)),
        parted)
refused("decade's Model's check, c = 1/10", RuntimeError,
        Model(work / "decade.ink", c=F(1, 10)).check, parted)

# steps.ink's comment: the coast whole, every term a multiple of 1/64 below
# 2^11, so exact in a double and a float. For N = 8, T = 1, v0 = 4: v_n = 4 -
# n/8 and y_8 = (32 - 36/8)/8 = 55/16, every term exact too.
from steps import fall
exact = "fall: {} steps from 0, against exact values\nfall.v: within 0\nfall.y: within 0\n"
same("fall.check(steps=1001)", fall.check(steps=1001), exact.format(1001))
same("fall.check(steps=1001, float32=True)", fall.check(steps=1001, float32=True),
     "fall: 1001 steps from 0 in float, against exact values\n"
     "fall.v: within 0, 0 units of a float\nfall.y: within 0, 0 units of a float\n")
same("fall.check for N = 8", fall.check(steps=9, N=8, T=1, v0=4), exact.format(9))
short = fall(N=8, T=1, v0=4)
for n in range(9):
    short.step()
same("fall's y_8 for N = 8", short["y"], 55 / 16)
same("fall's [v_8, v_7]", short.history("v"), [3.0, 3.125])
same("fall's index_", short["index_"], 8)
same("fall's Model's check, N = 8 as given", short.check(), exact.format(100))

# Rump (2010), section 1.2, eq. (1.5): exactly -0.827396..., in doubles
# 1.17260394005317..., Python's doubles in the order the step computes.
from rump import example
a, b = 77617.0, 33096.0
g = 21 * b * b - 2 * a * a + 55 * b * b * b * b - 10 * a * a * b * b + a / (2 * b)
refused("Rump's example checked", RuntimeError, example.check,
        f"example.g: {g:.17g} at 0, where the interpreter gives -0.827396")

# lcg.ink's comment: glibc's r parts at 2 where its product passes 2^53, the
# step's mod a - b*floor(a/b) in doubles; Park and Miller's q does not.
from lcg import noise
r1 = (1103515245 * 777 + 12345) % 2**31
step = 1103515245.0 * r1 + 12345.0
r2 = step - 2.0**31 * math.floor(step / 2.0**31)
refused("glibc's LCG checked", RuntimeError, noise.check, "noise.q: within 0",
        f"noise.r: {r2:.17g} at 2, where the interpreter gives {(1103515245 * r1 + 12345) % 2**31}")
gen = noise()
gen.step()
gen.step()
same("noise's r_1", gen["r"], float(r1))
same("noise's q_1", gen["q"], float(16807 * 777 % (2**31 - 1)))

# chain.ink, its use of filters.ink compiled: y_0 = 0 + (1 - 0) + 1/10 and
# y_1 = 1/10*1/2 + (1 - 1/4) + 1/10.
import chain
linked = chain.chain()
linked.step(1)
near("chain's y_0", linked["y"], 1.1, 1e-15)
linked.step(1)
near("chain's y_1", linked["y"], 0.9, 1e-15)

# kalman.ink: Pp_1 = F*10*I*F' + Q = [20.01, 10; 10, 10.01], K_1 = [20.01;
# 10]/21.01, x_1 = K_1*z_1; for Q = 0, [20; 10]/21 and z_1 = 21.
import kalman
kf = kalman.kalman()
kf.step(0)
kf.step(21.01)
near("kalman's x_1", kf["x"], [20.01, 10.0], 1e-12)
same("kalman's H, a row", kf["H"], [1.0, 0.0])
same("kalman's Q", kf["Q"], [[0.01, 0.0], [0.0, 0.01]])
still = kalman.kalman(Q=Rows([[0, 0], [0, 0]]))
still.step(0)
still.step(21)
near("kalman's [x_1, x_0] for Q = 0", still.history("x"), [[20.0, 10.0], [0.0, 0.0]], 1e-12)

sys.exit(failed)
