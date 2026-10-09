# Apollo 11's translunar coast, apollo11.ink, from the first midcourse
# correction's cutoff to lunar orbit insertion's ignition, both of table
# 7-II of the Apollo 11 Mission Report, MSC-00171 (1969), through JPL
# Horizons' Moon and Sun (DE441). Every expected value was worked out apart
# from the interpreter and none was recorded: the same coast in numpy,
# Runge-Kutta in as many steps on the same table, its frames ERFA's IAU
# 2006/2000A where apollo11.ink takes Meeus's sidereal time, four terms of
# nutation and IAU 1976 precession; and with scipy's DOP853 at 1e-13. Each
# answer is shown to the digits where numpy's and this one agree.
#
# In 400 steps, to keep the test short. The showcase took 1000, where numpy
# gives 8260.217 ft/s, -10.3806 deg, 84.8045 nmi, 56.6302 nmi and 36.529 s
# for the answers below, shown as they are 8260, -10.38, 84.8, 56.6 and 37:
# the step's own error at 400, 0.3 ft/s and 0.07 nmi, moves the last digit
# of three of them.
>> use apollo11 (coast, hms)
use apollo11 (coast, hms)

>> c = coast(N = 400)
c = coast(N = 400)

# At the ignition, about the Moon: speed (ft/s) and flight path angle
# (deg), numpy's 8260.522 and -10.3829, then the altitude above Landing
# Site 2 (nmi), numpy's 84.7339; DOP853's 8260.197, -10.3805 and 84.8070.
>> digits = 4
digits = 4

>> [c.speed_400, c.fpa_400]
[~8261, ~-10.38]

>> digits = 3
digits = 3

>> c.alt_400
~84.7

# The pericynthion the coast was aimed at, numpy's 56.5494 nmi and 36.552 s
# past 75:53:00. Table 7-III gives 61.5 nmi at 75:53:35, which this meets
# as closely as table 7-II's rounding allows: drawn within half its last
# digit, 0.005 deg, 0.05 nmi, 0.05 ft/s and 0.05 s, 200 times and coasted
# by DOP853, the cutoff's row moves the pericynthion by 5.7 nmi and its
# time by 21 s, one standard deviation each.
>> c.peri_400
~56.5

>> digits = 2
digits = 2

>> c.tperi_400 - hms(75, 53, 0)
~37
