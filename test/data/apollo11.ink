# Apollo 11's translunar coast after its one midcourse correction, to lunar
# orbit insertion, for translunar.ink. Apollo 11 Mission Report, MSC-00171
# (1969): table 7-II gives the states, 7-III the pericynthion each maneuver
# aimed at, 5-IV the radius of Landing Site 2 that altitudes are counted
# from. The Moon and the Sun are JPL Horizons' (DE441), geocentric, ICRF
# axes, km and km/s, every 6 h from 1969-07-17 12:00 TDB: row k at
# tau = 21600*(k-1) s.
moon = [~-336966.9204551392, ~194138.639202848, ~101206.5456161044, ~-0.5090478454659974, ~-0.7334248916578737, ~-0.4043985312284881;
    ~-347468.3459501225, ~178028.0864345774, ~92331.10754938112, ~-0.4630302355861332, ~-0.7579536794873108, ~-0.4172132743102397;
    ~-356958.2748683357, ~161409.9908672872, ~83191.21056428293, ~-0.4154083771549952, ~-0.7804060865596396, ~-0.4288782932708598;
    ~-365403.215001162, ~144330.067390838, ~73812.14778003379, ~-0.3662904547401858, ~-0.8007024956007376, ~-0.4393513543173205;
    ~-372772.0640426911, ~126835.7189295099, ~64220.10656654455, ~-0.3157895769050064, ~-0.8187662481288425, ~-0.448591903399813;
    ~-379036.2163750459, ~108975.9709532086, ~54442.13138701682, ~-0.2640238179976807, ~-0.8345237926855141, ~-0.4565611483592346;
    ~-384169.6709027858, ~90801.40264087786, ~44506.08478509077, ~-0.2111162735029981, ~-0.8479048472420869, ~-0.4632221489381124;
    ~-388149.1402268055, ~72364.07434612936, ~34440.60632290784, ~-0.1571951285934454, ~-0.8588425798946688, ~-0.4685399168864909;
    ~-390954.1614320979, ~53717.45092257198, ~24275.06922635386, ~-0.1023937387550788, ~-0.8672738122972123, ~-0.4724815285679934;
    ~-392567.2087303545, ~34916.32036713131, ~14039.53444023099, ~-0.04685072049381468, ~-0.8731392505523979, ~-0.4750162526074573]
sun = [~-64610270.10871621, ~126270291.1939656, ~54754835.77829718, ~-26.4860819307226, ~-11.52175316352981, ~-4.997412308953734;
    ~-65181792.65935338, ~126020306.4524352, ~54646407.93863039, ~-26.43259131800017, ~-11.62494487262339, ~-5.042185807111537;
    ~-65752154.54307725, ~125768095.1962684, ~54537014.07609903, ~-26.37861312742023, ~-11.72791147182009, ~-5.086858657475141;
    ~-66321345.25147204, ~125513662.317145, ~54426656.37842337, ~-26.32414952436759, ~-11.83065025390374, ~-5.13142959245903;
    ~-66889354.32361712, ~125257012.7648063, ~54315337.06047218, ~-26.26920274123054, ~-11.93315855031614, ~-5.175897366060471;
    ~-67456171.34753948, ~124998151.5462005, ~54203058.36378577, ~-26.21377507790298, ~-12.03543373297979, ~-5.220260754867006;
    ~-68021785.96167868, ~124737083.7245876, ~54089822.55607639, ~-26.15786890245132, ~-12.13747321629272, ~-5.264518559159455;
    ~-68586187.85636815, ~124473814.4185979, ~53975631.93070345, ~-26.10148665194189, ~-12.23927445934551, ~-5.308669604137547;
    ~-69149366.7753358, ~124208348.8012405, ~53860488.80612063, ~-26.0446308334152, ~-12.34083496841445, ~-5.352712741297422;
    ~-69711312.51722704, ~123940692.0988545, ~53744395.52529146, ~-25.98730402498271, ~-12.44215229978823, ~-5.396646849991855]

nmi = 1852/1000
ft = 3048/10000000
deg = pi/180
asec = deg/3600
muE = 398600.435436
muM = 4902.800066
muS = 132712440041.279419

# GET 0 is 1969-07-16 13:32:00 UTC. TT - UTC is 32.184 s + TAI - UTC, whose
# 1968-1972 rule is 4.21317 s + 0.002592 s a day past MJD 39126; TDB - TT
# (2 ms) is left out, and UT1 is taken as UTC.
mjd0 = 40418 + (13*3600 + 32*60)/86400
taiutc = 4.21317 + (mjd0 - 39126)*0.002592
tau(g) = g - 80880 + 32.184 + taiutc
hms(h, m, s) = 3600*h + 60*m + s

# Cubic Hermite on the table's positions and velocities.
H = 21600
row(t) = floor(t/H) + 1
pos(T, k) = [T[k,1]; T[k,2]; T[k,3]]
vel(T, k) = [T[k,4]; T[k,5]; T[k,6]]
hermite(p0, v0, p1, v1, u) = (2*u^3 - 3*u^2 + 1)*p0 + (u^3 - 2*u^2 + u)*H*v0 + (3*u^2 - 2*u^3)*p1 + (u^3 - u^2)*H*v1
dhermite(p0, v0, p1, v1, u) = ((6*u^2 - 6*u)*p0 + (3*u^2 - 4*u + 1)*H*v0 + (6*u - 6*u^2)*p1 + (3*u^2 - 2*u)*H*v1)/H
at(T, t) = hermite(pos(T, row(t)), vel(T, row(t)), pos(T, row(t)+1), vel(T, row(t)+1), t/H - row(t) + 1)
vat(T, t) = dhermite(pos(T, row(t)), vel(T, row(t)), pos(T, row(t)+1), vel(T, row(t)+1), t/H - row(t) + 1)

# Point masses, in the frame of the Earth, which the Moon and the Sun pull too.
nrm(v) = (v'*v)^(1/2)
grav(mu, d) = -mu*d/nrm(d)^3
pull(r, rm, rs) = grav(muE, r) + grav(muM, r - rm) + grav(muM, rm) + grav(muS, r - rs) + grav(muS, rs)
r3(y) = [y[1]; y[2]; y[3]]
v3(y) = [y[4]; y[5]; y[6]]
f(y, t) = [v3(y); pull(r3(y), at(moon, t), at(sun, t))]

# Table 7-II is Earth-fixed: geodetic latitude on the Fischer 1960 ellipsoid,
# speed, flight path angle and heading in the geocentric horizontal. To ICRF
# by Earth rotation (GMST, Meeus 12.4, and the equation of the equinoxes),
# nutation (its four largest terms, Meeus 22) and IAU 1976 precession.
aF = 6378.166
fF = 1/298.3
e2 = fF*(2 - fF)
R1(a) = [1 0 0; 0 cos(a) sin(a); 0 -sin(a) cos(a)]
R2(a) = [cos(a) 0 -sin(a); 0 1 0; sin(a) 0 cos(a)]
R3(a) = [cos(a) sin(a) 0; -sin(a) cos(a) 0; 0 0 1]
jdu(g) = 2440418.5 + (13*3600 + 32*60 + g)/86400
cent(g) = (jdu(g) + (32.184 + taiutc)/86400 - 2451545)/36525
gmst(g) = mod(280.46061837 + 360.98564736629*(jdu(g) - 2451545) + 0.000387933*cent(g)^2 - cent(g)^3/38710000, 360)*deg
om(T) = mod(125.04452 - 1934.136261*T, 360)*deg
ls(T) = mod(280.4665 + 36000.7698*T, 360)*deg
lm(T) = mod(218.3165 + 481267.8813*T, 360)*deg
dpsi(T) = (-17.2*sin(om(T)) - 1.32*sin(2*ls(T)) - 0.23*sin(2*lm(T)) + 0.21*sin(2*om(T)))*asec
deps(T) = (9.2*cos(om(T)) + 0.57*cos(2*ls(T)) + 0.1*cos(2*lm(T)) - 0.09*cos(2*om(T)))*asec
eps0(T) = (84381.448 - 46.815*T - 0.00059*T^2 + 0.001813*T^3)*asec
prec(T) = R3(-(2306.2181*T + 1.09468*T^2 + 0.018203*T^3)*asec)*R2((2004.3109*T - 0.42665*T^2 - 0.041833*T^3)*asec)*R3(-(2306.2181*T + 0.30188*T^2 + 0.017998*T^3)*asec)
nut(T) = R1(-(eps0(T) + deps(T)))*R3(-dpsi(T))*R1(eps0(T))
fixed(g) = R3(gmst(g) + dpsi(cent(g))*cos(eps0(cent(g))))*nut(cent(g))*prec(cent(g))
Nrad(lat) = aF/(1 - e2*sin(lat*deg)^2)^(1/2)
ecef(lat, lon, h) = [(Nrad(lat) + h)*cos(lat*deg)*cos(lon*deg); (Nrad(lat) + h)*cos(lat*deg)*sin(lon*deg); (Nrad(lat)*(1 - e2) + h)*sin(lat*deg)]
up(lat, lon, h) = ecef(lat, lon, h)/nrm(ecef(lat, lon, h))
east(lon) = [-sin(lon*deg); cos(lon*deg); 0]
north(lat, lon, h) = ([0; 0; 1] - up(lat, lon, h)[3]*up(lat, lon, h))/(1 - up(lat, lon, h)[3]^2)^(1/2)
vecef(lat, lon, h, v, fpa, hdg) = v*(sin(fpa*deg)*up(lat, lon, h) + cos(fpa*deg)*(cos(hdg*deg)*north(lat, lon, h) + sin(hdg*deg)*east(lon)))
state(g, lat, lon, h, v, fpa, hdg) = [fixed(g)'*ecef(lat, lon, h*nmi); fixed(g)'*vecef(lat, lon, h*nmi, v*ft, fpa, hdg)]

# Table 7-II: the first midcourse correction's cutoff, and lunar orbit
# insertion's ignition, where the coast ends.
mcc = state(hms(26, 45, 1.8), ~6.00, ~-11.17, ~109477.2, ~5010.0, ~76.88, ~120.87)
loi = hms(75, 49, 50.4)

# What the prelude lacks (DESIGN.md, next in line): asin by Newton's
# method, acosh and sinh from log and exp. And table 5-IV's radius.
asn(x)_0 = x
asn(x)_k = asn(x)_(k-1) - (sin(asn(x)_(k-1)) - x)/cos(asn(x)_(k-1))
asin(x) = lim asn(x)
acosh(x) = log(x + (x^2 - 1)^(1/2))
sinh(x) = (exp(x) - exp(-x))/2
cross(a, b) = [a[2]*b[3] - a[3]*b[2]; a[3]*b[1] - a[1]*b[3]; a[1]*b[2] - a[2]*b[1]]
sgn(x) = (x > 0) - (x < 0)
rls = 937.1*nmi

# Runge-Kutta 4 in N steps from g0 to g1 (GET, s); then, about the Moon, the
# altitude (nmi), speed (ft/s) and flight path angle (deg), and the
# pericynthion of the osculating hyperbola, its altitude (nmi) and GET (s).
coast(N = 1000, g0 = hms(26, 45, 1.8), g1 = loi, y0 = mcc) = {
    h = (tau(g1) - tau(g0))/N
    t_n = tau(g0) + n*h
    y_0 = y0
    k1_n = f(y_(n-1), t_(n-1))
    k2_n = f(y_(n-1) + h/2*k1_n, t_(n-1) + h/2)
    k3_n = f(y_(n-1) + h/2*k2_n, t_(n-1) + h/2)
    k4_n = f(y_(n-1) + h*k3_n, t_n)
    y_n = y_(n-1) + h/6*(k1_n + 2*k2_n + 2*k3_n + k4_n)
    d_n = r3(y_n) - at(moon, t_n)
    w_n = v3(y_n) - vat(moon, t_n)
    alt_n = (nrm(d_n) - rls)/nmi
    speed_n = nrm(w_n)/ft
    fpa_n = asin(d_n'*w_n/(nrm(d_n)*nrm(w_n)))/deg
    en_n = w_n'*w_n/2 - muM/nrm(d_n)
    a_n = -muM/(2*en_n)
    ecc_n = (1 + 2*en_n*nrm(cross(d_n, w_n))^2/muM^2)^(1/2)
    peri_n = (a_n*(1 - ecc_n) - rls)/nmi
    F_n = acosh((1 - nrm(d_n)/a_n)/ecc_n)*sgn(d_n'*w_n)
    tperi_n = t_n - (ecc_n*sinh(F_n) - F_n)/(muM/(-a_n)^3)^(1/2) - tau(0)
}
