-- 1 至 3 个 SP 共用方案规则；逻辑槽与原生配置编号分开。
local Profile={fields={"a","b","c"}}
-- 校验连续槽位和不重复配置；空槽只允许出现在末尾。
function Profile.count(profile)
    local count=0 -- 实际搭载数决定均分比例。
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
-- 比例由数量唯一确定，不沿用旧滑块设置。
function Profile.copy(profile)
    local count=Profile.count(profile) -- 同时校验完整结构。
    return {a=profile.a,b=profile.b,c=profile.c,ratio=1/count}
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
-- 部件先计入完整载量；向下取整，正载量至少保留一发，零载量保持零。
function Profile.ammo(full,count)
    assert(type(full)=="number"and full>=0 and full%1==0,"完整载量无效")
    assert(count>=1 and count<=3,"SP 数量无效")
    return full==0 and 0 or math.max(1,math.floor(full/count))
end
return Profile
