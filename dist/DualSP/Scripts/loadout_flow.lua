-- 出击与补给共用的双选草稿规则。草稿不直接改配置、实体挂载或任务武器状态。
local Flow={}
local function copy(p)return {a=p.a,b=p.b,ratio=p.ratio}end
-- 出击与补给共用双选规则，最终确认文字由原生页面用途决定。
function Flow.new(profile,options,reviewLabel,mode)
    assert(#options==3,"Native loadout requires the aircraft's three SP options")
    local self={stage="sp1",cursor=1,draft=copy(profile),options=options,message=nil}
    local takeoffReview=mode=="resupply" -- 补给最终页显示 YES/NO；出击显示编辑槽位和继续。
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
            if takeoffReview then
                rows={{action="takeoff",code="是 / YES",enabled=true},{action="reselect",code="否 / NO",enabled=true}}
            else rows={{action="edit_sp1",code="修改 SP1  "..self.options[self.draft.a+1].code:upper(),enabled=true},
                {action="edit_sp2",code="修改 SP2  "..(self.draft.b<0 and "无兵装"or self.options[self.draft.b+1].code:upper()),enabled=true},
                {action="continue",code=reviewLabel or "交给原生菜单继续",enabled=true}}end
        end
        return rows
    end
    function self:selectWeapon(index)
        self.cursor=1
        for i,row in ipairs(self:rows())do if row.weapon==index then self.cursor=i;return true end end
        return false
    end
    function self:move(delta)
        local count=#self:rows();self.cursor=(self.cursor-1+delta)%count+1;self.message=nil
    end
    function self:confirm()
        local row=self:rows()[self.cursor]
        if not row.enabled then self.message="该兵装尚未购买";return "blocked"end
        self.message=nil
        if self.stage=="sp1"then
            self.draft.a=row.weapon;self.stage="sp2"
            if not self:selectWeapon(self.draft.b)then self:selectWeapon(-1)end
            -- 更换 SP1 后，旧 SP2 可能不再兼容；游标移到明确的空兵装项。
            if self:rows()[self.cursor].weapon==self.draft.a then self:selectWeapon(-1)end
            if not self:rows()[self.cursor].enabled then self:selectWeapon(-1)end
            return "sp2"
        elseif self.stage=="sp2"then
            self.draft.b=row.weapon;self.stage="review";self.cursor=takeoffReview and 2 or 3
            return "review"
        elseif self.stage=="review"then
            if row.action=="takeoff"or row.action=="continue"then return row.action end
            self.stage=row.action=="edit_sp2"and "sp2"or "sp1"
            self:selectWeapon(self.stage=="sp1"and self.draft.a or self.draft.b);return self.stage
        end
        error("无效的双 SP 草稿阶段")
    end
    function self:back()
        -- 最终页返回上一步，不结束补给或提交草稿。
        if self.stage=="review"then self.stage="sp2";self:selectWeapon(self.draft.b);self.message=nil;return "sp2"end
        if self.stage=="sp2"then self.stage="sp1";self:selectWeapon(self.draft.a);self.message=nil;return "sp1"end
        if self.stage=="sp1"then self.stage="cancelled";return "cancelled"end
        error("无效的双 SP 返回阶段")
    end
    function self:profile()
        assert(self.stage=="review","Both SP choices must be confirmed")
        return copy(self.draft)
    end
    self:selectWeapon(profile.a)
    return self
end
return Flow
