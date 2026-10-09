# 'i' is the imaginary unit, and a name, as 'pi' and 'e' are (DESIGN.md, next
# in line). This was the specification, and every entry passes as it was
# written. A name bound in a scope of its own -- a sum's index, a cell's row
# or column, a parameter, a sequence's index -- shadows it there, as any local
# shadows a global, so a paper's sum over i reads as it is written. Unlike
# 'pi', it cannot be defined again: an answer is printed outside every such
# scope, and its 'i' must always be the unit.
>> i^2
~-1

>> 3 + i
~(3+i)

>> ?i
i

>> sum_(i=1)^4 i
10

>> p[i<=3] = i^2
p[i<=3] = i^2

>> p
[1;
 4;
 9]

>> f(i) = 2*i
f(i) = 2*i

>> f(3)
6

>> x_i = i/2
x_i = i/2

>> x_4
2

# Outside the scope that binds it, it is the unit again.
>> (sum_(i=1)^2 i) + i
~(3+i)

# Inside one, the unit is reached through a name defined outside it, as an
# engineer's j would be.
>> j = i
j = i

>> sum_(i=1)^2 i*j
~(i*3)

# A definition of it, in any of its forms, is refused: the answers printed
# would then mean something other than what they say.
>> i = 7
error: i is the imaginary unit, so it cannot be defined

>> i_n = n
error: i is the imaginary unit, so it cannot be defined

>> i(x) = x
error: i is the imaginary unit, so it cannot be defined

# A number written against it is not a product, here as anywhere: on paper
# '2i' in a sum over i is twice the index, and it was twice the unit.
>> 2i
error: unexpected 'i' -- the operator '*' is probably missing

>> sum_(i=1)^3 2i
error: unexpected 'i' -- the operator '*' is probably missing

>> sum_(i=1)^3 2*i
12
