-- 第三方机体只读适配：真实引用、涂装上下文和当前兵装展示资料。
-- 不按短名/任意后缀放行身份，也不授予核心写入权限。
local ModAircraft={}
local fields={"SpecialWeaponConfig1","SpecialWeaponConfig2","SpecialWeaponConfig3"}
local function valid(o)return o and o:IsValid()end
local function classPath(reference)
    if type(reference)~="string"then return nil end
    local path=reference:match("^%w+'([^']+)'$")or reference
    if path:sub(1,6)~="/Game/"or path:find(":",1,true)then return nil end
    if not path:find(".",1,true)then path=path.."."..(path:match("([^/]+)$")or "")end
    if path:sub(-2)~="_C"then path=path.."_C"end
    return path
end
function ModAircraft.new(note)
    local self={warnings={},lastMenu=nil}
    function self:warn(key,message)
        if self.warnings[key]then return end
        self.warnings[key]=true;note("MOD_AIRCRAFT_FALLBACK "..message)
    end
    function self:skin(instance)
        if self.skinAPI==nil then
            self.skinAPI=valid(StaticFindObject("/Script/Nimbus.NimbusGameInstance:GetSelectedPlaneSkinNo"))==true
            if not self.skinAPI then self:warn("skin_api","缺少涂装编号接口，仅使用基础类严格核对")end
        end
        if not self.skinAPI then return nil end
        local value=instance:GetSelectedPlaneSkinNo()
        if type(value)=="number"and value<0 then return nil end -- 尚无选择的菜单/载入阶段不构造假涂装编号。
        assert(type(value)=="number"and value>=0 and value%1==0,"当前涂装编号无效")
        return value
    end
    function self:selection(instance)
        return table.concat({instance:GetAddress(),instance:GetWorld():GetAddress(),instance:GetSelectedPlaneID(),self:skin(instance)or "unknown"},"|")
    end
    function self:token(instance)
        local parts={self:selection(instance)}
        local hangar=instance.HangarManager
        local actor=valid(hangar)and hangar.LoadedPlayerPlane or nil
        if valid(actor)then
            parts[#parts+1]=actor:GetAddress();parts[#parts+1]=actor:GetClass():GetFullName()
            local activator=actor.PlayerWeaponActivator
            if valid(activator)then
                parts[#parts+1]=activator:GetAddress()
                for _,field in ipairs(fields)do
                    local weapon=activator[field].WeaponClass
                    parts[#parts+1]=valid(weapon)and weapon:GetAddress()or "missing"
                    if valid(weapon)then
                        local default=weapon:GetCDO()
                        parts[#parts+1]=valid(default)and tostring(default.WeaponID)or "missing_default"
                    end
                end
            end
        end
        return table.concat(parts,"|"),valid(actor)==true
    end
    function self:reference(info,instance)
        local skin=self:skin(instance)
        if skin~=nil then
            local expected=nil
            for _,row in ipairs(info.skins or {})do
                if row.skin_no==skin then
                    assert(not row.reference_error,"当前涂装引用不可用："..tostring(row.reference_error))
                    local path=assert(classPath(row.reference),"当前涂装的机体引用无效")
                    assert(not expected or expected:lower()==path:lower(),"当前涂装存在冲突的机体引用")
                    expected=path
                end
            end
            if expected then return expected end
        end
        -- 没有对应涂装行时只承认明确的基础引用及其真正子类，不尝试猜s09/type02。
        return classPath(info.reference)or classPath("/Game/Blueprint/Player/Pawn/AcePlayerPawn_"..info.code)
    end
    function self:matches(actor,info,instance)
        local expected=self:reference(info,instance):lower()
        local actual=actor:GetClass()
        for _=1,64 do
            if not valid(actual)then return false end
            local path=actual:GetFullName():match("^[^ ]+ (.+)$")
            if path and path:lower()==expected then return true end
            actual=actual:GetSuperStruct()
        end
        error("机体类继承链过长或成环")
    end
    -- 只有基础蓝图同一槽的实际WeaponClass一致，才保留其自定义PWDT代号（如2eml）。
    function self:baseWeapons(info)
        local path=classPath(info.reference)
        local class=path and StaticFindObject(path)or nil
        local default=valid(class)and class:GetCDO()or nil
        local activator=valid(default)and default.PlayerWeaponActivator or nil
        if not valid(activator)then return nil end
        local result={}
        for i,field in ipairs(fields)do
            local weapon=activator[field].WeaponClass
            if not valid(weapon)then return nil end
            result[i]=weapon:GetAddress()
        end
        return result
    end
    function self:weapon(catalogue,info,index,weaponClass,weaponID,baseWeapons)
        local item=info.weapons[index]
        if baseWeapons and baseWeapons[index]==weaponClass:GetAddress()then return item end
        -- 基础类未加载时，仅运行时编号与PWDT编号一致的资料可以直接沿用。
        if not baseWeapons and item.weapon_id==weaponID then return item end
        local found=nil
        for code,data in pairs(catalogue._compat.weapons)do
            if data.weapon_id==weaponID then
                if found then found=false;break end -- 同编号多行可能是不同Mod武器，不任选其一。
                found={code=code,stats=data.stats,category=data.category,type_key=data.type_key}
            end
        end
        if found then
            self:warn(info.code..":"..index..":"..weaponClass:GetAddress(),"plane="..info.code.." slot="..index.." weapon_id="..weaponID.." 使用编号对应的展示资料；自定义性能仍以Mod本身为准")
            return found
        end
        local code=weaponClass:GetFullName():match("([^/. ]+)_C$")or ("weapon_"..weaponID)
        self:warn(info.code..":"..index..":"..code,"plane="..info.code.." slot="..index.." class="..code.." weapon_id="..weaponID.." 展示资料不唯一，使用类代号和OTHER")
        return {code=code,stats={0,0,0,0,0,0},category=0,type_key="HangarWeaponType_Name_Other",generic=true}
    end
    return self
end
return ModAircraft
