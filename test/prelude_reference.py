# Writes data/prelude_reference.txt, the table prelude_test.cpp holds the
# prelude's exp, log, tanh, sin and cos to: python3 prelude_reference.py >
# data/prelude_reference.txt. Needs mpmath; the build needs only the table.
import math
import random
import sys

import mpmath
from mpmath import mp, mpf

PREC = 256
mp.prec = PREC
rng = random.Random(2026)
MAX = sys.float_info.max


def rounded(v):
    """The double nearest v, and v's distance from it in units of the
    spacing of doubles at v."""
    if v == 0:
        return 0.0, 0.0
    e = max(int(mpmath.frexp(v)[1]) - 53, -1074)
    n = int(mpmath.nint(mpmath.ldexp(v, -e)))
    try:
        y = math.ldexp(n, e)
    except OverflowError:
        return math.copysign(math.inf, n), 0.0
    return y, float(mpmath.ldexp(v - mpf(y), -e))


def double(sign, exponent):
    """A double of random mantissa in [2^exponent, 2^(exponent + 1))."""
    if exponent < -1022:
        return sign * math.ldexp(rng.getrandbits(52) | 1, -1074)
    return sign * math.ldexp(1 + rng.getrandbits(52) / 2**52, exponent)


def sign():
    return rng.choice((-1, 1))


def near(x, ulps=3):
    """x and the doubles within ulps of it."""
    out = [x]
    a = b = x
    for _ in range(ulps):
        a, b = math.nextafter(a, -math.inf), math.nextafter(b, math.inf)
        out += [a, b]
    return out


def nearest_multiples(c, kmax, count):
    """The doubles nearest a multiple k c, k <= kmax, in absolute terms."""
    bits = 220
    scaled = int(mpmath.floor(mpmath.ldexp(c, bits)))
    best = []
    for k in range(1, kmax + 1):
        t = k * scaled
        shift = t.bit_length() - 53
        n = (t + (1 << (shift - 1))) >> shift
        r = abs(t - (n << shift))
        best.append((r, math.ldexp(n, shift - bits)))
    best.sort()
    return [x for _, x in best[:count]]


def section(name, f, xs):
    print(name)
    for x in xs:
        y, d = rounded(f(mpf(x)))
        # Again at twice the precision, for a cancellation mpmath missed.
        with mpmath.workprec(2 * PREC):
            again, e = rounded(f(mpf(x)))
        assert again == y and abs(e - d) < 1e-9, x
        print(f"{x.hex()} {y.hex()} {d:.4f}")


# The farthest arguments of the sweep, 10^8 to 4*10^8 random doubles a range
# against a 64-bit long double, each range's seed and size in prelude_test.cpp's
# skipped case 'sweep', which prints them again (C225).
scanned = {
    "exp": "0x1.3da3937ff9cc7p+9 -0x1.3df833788dab4p+9 -0x1.55319e650e697p+9 0x1.3da46d69951efp+9 "
           "0x1.3da3998186431p+9 -0x1.3762ced8dc225p+9 -0x1.6260827acee24p+9 -0x1.625f7cc669fc9p+9",
    "log": "0x1.00fd484476f59p+0 0x1.03f0abf7164f1p+0 0x1.104e830d8d74fp+0 0x1.10736adc19bebp+0 "
           "0x1.105f417fbb6f9p+0 0x1.01fba97cedbadp+0 0x1.471843ece4443p+0 0x1.03b2b8f2fefa5p+0",
    "tanh": "-0x1.aa1d970474cdp-3 0x1.c26e6dd8d13ap-3 0x1.9193048261ed8p-3 0x1.b6ebcc4ce60fp-3 "
            "0x1.b42a281218468p-3 0x1.c2b5d3041cd78p-3 0x1.c167daec7658p-3 -0x1.815458291558p-3",
    "trig": "-0x1.1d2cd4857ce8p+15 0x1.3e4860da84b56p+19 -0x1.4ba395f5ceddcp+19 0x1.a85db8001e4dp+17 "
            "-0x1.0bc91200ff5eep+19 0x1.5893257b0821p+18 -0x1.42ba2f518f172p+19 0x1.9ece5b0520b58p+19 "
            "-0x1.a5610c32f4d4ep+19 -0x1.b4a665f6d622ep+19 0x1.20a7e208068a8p+17 -0x1.231ea8d5160e6p+19 "
            "-0x1.9078703fb7476p-1 -0x1.8c502e4f6ddb7p-1 0x1.8b9b13b99287ep-1 0x1.7e2d6398e78d8p-1 "
            "-0x1.938d4aea09348p-1 -0x1.9d77286ec93p-1 -0x1.2f97d7b99b6f8p+2 0x1.2f97d619b60b4p+2 "
            "0x1.c90a02d1b3414p+9 0x1.c92a0eb1173eap+9 0x1.c92a0cd508d58p+8 -0x1.9f89c9e9be13ap+9",
}
scanned = {k: [float.fromhex(x) for x in v.split()] for k, v in scanned.items()}

ln2 = mpmath.log(2)
half_pi = mpmath.pi / 2

exp_args = [rng.uniform(-745.2, 709.79) for _ in range(100)]
exp_args += [double(sign(), rng.randint(-60, 9)) for _ in range(100)]
exp_args += [double(sign(), rng.randint(-1074, -61)) for _ in range(20)]
exp_args += [rng.uniform(-745.14, -708.39) for _ in range(60)]
exp_args += [709.782712893384 - rng.uniform(0, 1e-3) for _ in range(10)]
exp_args += near(709.782712893384, 4) + near(-708.3964185322641, 4) + near(-745.1332191019411, 4)
exp_args += [x for k in rng.sample(range(-1075, 1025), 15) for x in near(float(k * ln2), 2)]
exp_args += [x for k in rng.sample(range(-1075, 1024), 15) for x in near(float((k + 0.5) * ln2), 2)]
exp_args += [float(k) for k in range(-745, 710, 50)]
exp_args += [0.0, 1.0, -1.0, 0.5, 1000.0, 1e300, -1e300] + scanned["exp"]

log_args = [double(1, rng.randint(-1022, 1023)) for _ in range(150)]
log_args += [double(1, rng.randint(-1074, -1023)) for _ in range(20)]
log_args += [rng.uniform(0.5, 2) for _ in range(100)]
log_args += [1 + sign() * double(1, rng.randint(-53, -2)) for _ in range(100)]
log_args += [x for _ in range(12) for x in near(math.ldexp(math.sqrt(2), rng.randint(-1074, 1023)), 2)]
log_args += [x for _ in range(12) for x in near(math.ldexp(1.0, rng.randint(-1073, 1023)), 1)]
log_args += near(1.0, 6) + [math.ldexp(1, -1074), MAX, float(mpmath.e), 10.0] + scanned["log"]

tanh_args = [rng.uniform(-20, 20) for _ in range(150)]
tanh_args += [double(sign(), rng.randint(-40, 0)) for _ in range(120)]
tanh_args += [double(sign(), rng.randint(-1074, -41)) for _ in range(15)]
tanh_args += [sign() * rng.uniform(19, 25) for _ in range(15)]
tanh_args += [x for k in rng.sample(range(1, 59), 12) for x in near(float((k + 0.5) * ln2 / 2), 2)]
tanh_args += [1e300, -1e300, 0.5, 1.0] + scanned["tanh"]

trig_args = [rng.uniform(-math.pi / 4, math.pi / 4) for _ in range(50)]
trig_args += [double(sign(), rng.randint(-60, -2)) for _ in range(40)]
trig_args += [double(sign(), rng.randint(-1074, -61)) for _ in range(10)]
trig_args += [rng.uniform(-2 * math.pi, 2 * math.pi) for _ in range(50)]
trig_args += [rng.uniform(-1000, 1000) for _ in range(50)]
trig_args += [rng.uniform(-2**20, 2**20) for _ in range(80)]
trig_args += [x for x in nearest_multiples(half_pi, int(2**20 / half_pi), 40) for x in near(x, 1)]
trig_args += [x for k in rng.sample(range(1, 667544), 15) for x in near(float(k * half_pi), 1)]
trig_args += [x for k in rng.sample(range(0, 667544), 15) for x in near(float((k + 0.5) * half_pi), 1)]
trig_args += near(2.0**20, 3)[:5] + near(-2.0**20, 3)[:5] + [10.0, 100.0, 355.0, 1.0] + scanned["trig"]
trig_args = [x for x in trig_args if abs(x) <= 2**20]

print("# The correctly rounded double of each function at each argument, and the")
print("# exact value's distance from it in units of the spacing of doubles there:")
print("# x, then the double, then the distance, x and the double as C's %a.")
print(f"# Written by prelude_reference.py with mpmath {mpmath.__version__} at {PREC} bits,")
print(f"# each value checked at {2 * PREC}.")
section("exp", mpmath.exp, exp_args)
section("log", mpmath.log, log_args)
section("tanh", mpmath.tanh, tanh_args)
section("sin", mpmath.sin, trig_args)
section("cos", mpmath.cos, trig_args)
