-- 1 至 3 个 SP 共用方案规则；逻辑槽与原生配置编号分开。
local Profile={fields={"a","b","c"}}
-- 校验连续槽位和不重复配置；空槽只允许出现在末尾。
function Profile.count(profile)
    local count=0 -- 实际搭载数决定采用单、双或三 SP 的配弹参数。
    local seen={} -- 已占用的原生配置编号。
    local empty=false -- 空槽后不能继续搭载。
    for index,field in ipairs(Profile.fields)do
        local weapon=profile[field] -- 0/1/2 为原生配置，-1 为空。
        assert(type(weapon)=="number"and weapon%1==0 and weapon>=-1 and weapon<=2,"SP 配置编号无效")
        if weapon<0 then assert(index>1,"SP1 必须搭载兵装");empty=true
        else
            assert(not empty and not seen[weapon],"SP 必须连续且不能重复")
            seen[weapon]=true;count=count+1
        end
    end
    return count
end
-- 百分数只接受整数；快照保留当时比例，检查点不受后续管理设置影响。
function Profile.percent(count,ratio)
    assert(ratio==nil or type(ratio)=="number","弹药比例须为数字")
    local minimum=count==1 and 100 or count==2 and 50 or 33
    local percent=ratio==nil and minimum or math.floor(ratio*100+.5)
    assert(type(ratio)=="nil"or(type(ratio)=="number"and math.abs(ratio*100-percent)<.000001),"弹药比例须为整数百分数")
    assert(percent>=minimum and percent<=100,"弹药比例超出允许范围")
    return percent
end
function Profile.copy(profile)
    local count=Profile.count(profile)
    return {a=profile.a,b=profile.b,c=profile.c,ratio=Profile.percent(count,profile.ratio)/100}
end
-- 只在新草稿、出击或调试时套用管理设置，不能改写当前任务库存。
function Profile.withSettings(profile,settings)
    local count=Profile.count(profile)
    local percent=count==1 and 100 or count==2 and settings.ammo_percent_two or settings.ammo_percent_three
    return Profile.copy{a=profile.a,b=profile.b,c=profile.c,ratio=percent/100}
end
-- 每个已选项均须购买；UAV/ECM 等限制仅在多挂时生效。
function Profile.validate(profile,options)
    local count=Profile.count(profile) -- 单挂不限制武器类别。
    assert(#options==3,"当前机体必须有三个原生兵装选项")
    for index=1,count do
        local option=options[profile[Profile.fields[index]]+1] -- 按实际配置核对。
        assert(option and option.owned,"所选兵装尚未购买")
        assert(count==1 or option.available,"当前兵装只支持单挂")
    end
    return count
end
-- 部件先计入完整载量；整数百分数向上取整，避免浮点误差多出一发。
-- 调试按集合轮切，不将ABC的不同排列视为新组合；跳过未购买及仅单挂限制。
function Profile.nextCombination(current,options,settings)
    local masks={1,2,4,3,5,6,7}
    local currentMask=0
    for _,field in ipairs(Profile.fields)do if current[field]>=0 then currentMask=currentMask | (1 << current[field])end end
    local start=0
    for index,mask in ipairs(masks)do if mask==currentMask then start=index;break end end
    for step=1,#masks do
        local mask=masks[(start+step-1)%#masks+1]
        if mask~=currentMask then
            local candidate={a=-1,b=-1,c=-1};local slot=1
            for index=0,2 do if mask & (1 << index)~=0 then candidate[Profile.fields[slot]]=index;slot=slot+1 end end
            if pcall(Profile.validate,candidate,options)then return Profile.withSettings(candidate,settings)end
        end
    end
    return nil
end
function Profile.ammo(full,count,ratio)
    assert(type(full)=="number"and full>=0 and full%1==0,"完整载量无效")
    assert(count>=1 and count<=3,"SP 数量无效")
    return math.floor((full*Profile.percent(count,ratio)+99)/100)
end
return Profile
