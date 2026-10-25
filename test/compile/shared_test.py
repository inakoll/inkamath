"""compile_shared's libraries, loaded by tools/inkamath_model.py against the
values shared_test.c holds them to, and pid's u_2 for r = 2: 2*1.8 + 0.37 - 0.1."""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "tools"))
from inkamath_model import Model

failed = 0


def held(what, got, exact, within=0.0):
    global failed
    if abs(got - exact) > within:
        print(f"{what} is {got!r}, not {exact!r}")
        failed = 1


path = dict(zip(("pid", "track", "pidf", "bank"), sys.argv[1:], strict=True))
pid, raised = Model(path["pid"], "pid"), Model(path["pid"], "pid")
raised["r"] = 2
for n in range(11):
    pid.step(n / 10)
    raised.step(n / 10)
    if n == 2:
        held("pid's u_2", pid["u"][0], 1.67, 1e-14)
        held("pid's u_2 for r = 2", raised["u"][0], 3.87, 1e-14)
held("pid's u_10", pid["u"][0], 0.35, 1e-14)

track = Model(path["track"], "track")
for n in range(31):
    given = n == 30 or (n % 3 == 0 and (n < 9 or n > 14))
    track.step((40.0 if n == 30 else n + 0.25) if given else None)
    if n in (3, 15):
        held(f"track's x_{n}", track["x"][0], 25 / 8 if n == 3 else 487 / 32)
held("track's d_30", track["d"][0], 51 / 4)

pidf = Model(path["pidf"], "pidf")
for n in range(3):
    pidf.step(n / 10)
held("pidf's u_2", pidf["u"][0], 1.67, 1e-6)

bank = Model(path["bank"], "bank")
for n in range(4):
    bank.step(1.0)
held("bank's y_3", bank["y"][0], 105 / 128)
held("bank's s_sharp.v_3", bank["s_sharp.v"][0], 1.125)
sys.exit(failed)
