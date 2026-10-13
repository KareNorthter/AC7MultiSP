-- Preparation only. Drafts never mutate the equipped profile until the native
-- menu commits. Physical weapon mounting/flight state is outside this module.
local Flow={}
local function copy(p)return {a=p.a,b=p.b,ratio=p.ratio}end
function Flow.new(profile,options)
    assert(#options==3,"Native loadout requires the aircraft's three SP options")
    local self={stage="sp1",cursor=1,draft=copy(profile),options=options,message=nil}
    function self:rows()
        local rows={}
        if self.stage=="sp1" then
            for index,option in ipairs(self.options)do
                rows[#rows+1]={weapon=index-1,enabled=option.owned,code=option.code:upper()}
            end
        elseif self.stage=="sp2"then
            local first=self.options[self.draft.a+1]
            for index,option in ipairs(self.options)do
                if index-1~=self.draft.a and first.available and option.available then
                    rows[#rows+1]={weapon=index-1,enabled=option.owned,code=option.code:upper()}
                end
            end
            rows[#rows+1]={weapon=-1,enabled=true,code="无兵装 / NO WEAPON"}
        elseif self.stage=="review"then
            rows={{code="SP1  "..self.options[self.draft.a+1].code:upper(),enabled=true},
                {code="SP2  "..(self.draft.b<0 and "无兵装"or self.options[self.draft.b+1].code:upper()),enabled=true},
                {code="确认并继续",enabled=true}}
        end
        return rows
    end
    function self:selectWeapon(index)
        self.cursor=1
        for i,row in ipairs(self:rows())do if row.weapon==index then self.cursor=i;return true end end
        return false
    end
    function self:move(delta)
        assert(self.stage~="review","Native menu owns review input")
        local count=#self:rows();self.cursor=(self.cursor-1+delta)%count+1;self.message=nil
    end
    function self:confirm()
        local row=self:rows()[self.cursor]
        if not row.enabled then self.message="该兵装尚未购买";return "blocked"end
        self.message=nil
        if self.stage=="sp1"then
            self.draft.a=row.weapon;self.stage="sp2"
            if not self:selectWeapon(self.draft.b)then self:selectWeapon(-1)end
            -- A previous pair can become incompatible after selecting a new
            -- SP1. Choose the explicit empty option, never duplicate a weapon.
            if self:rows()[self.cursor].weapon==self.draft.a then self:selectWeapon(-1)end
            if not self:rows()[self.cursor].enabled then self:selectWeapon(-1)end
            return "sp2"
        elseif self.stage=="sp2"then
            self.draft.b=row.weapon;self.stage="review";self.cursor=3
            return "review"
        end
        error("Only the native menu can confirm the prepared loadout")
    end
    function self:back()
        if self.stage=="sp2"then self.stage="sp1";self:selectWeapon(self.draft.a);self.message=nil;return "sp1"end
        if self.stage=="sp1"then self.stage="cancelled";return "cancelled"end
        error("Native menu owns review cancellation")
    end
    function self:profile()
        assert(self.stage=="review","Both SP choices must be confirmed")
        return copy(self.draft)
    end
    self:selectWeapon(profile.a)
    return self
end
return Flow
