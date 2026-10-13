-- Logical selections: 0 = MSL, 1 = SP1, 2 = SP2. The native game rejects
-- arming an empty SP, so skip it before asking the original input route to arm.
local cycle={}
function cycle.next(current,ammoA,ammoB)
    assert(current>=0 and current<=2,"Invalid logical weapon selection")
    if current==0 then
        if ammoA>0 then return 1 end
        if ammoB>0 then return 2 end
        return 0
    end
    if current==1 and ammoB>0 then return 2 end
    return 0
end
return cycle
