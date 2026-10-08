# A term far from the base is filled from the base up, each finding the one
# before it remembered, so a recurrence is not limited by how deep references
# nest. This one failed at 256 deep (DESIGN.md, phase 14).
>> g_0=1
g_0=1

>> g_n=g_(n-1)+1
g_n=g_(n-1)+1

>> g_10
11

>> g_500
501

>> g_5000
5001

>> g_600000
600001

# Up to ten million terms from the base, so that a slip of the keyboard costs
# seconds and not the session.
>> g_2000000000
error: g_2000000000 is 2000000000 terms from its base, and a fill stops at 10000000
