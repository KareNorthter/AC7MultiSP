-- 出击与补给共用的双选草稿规则。草稿不直接改配置、实体挂载或任务武器状态。
local Flow={}
local function copy(p)return {a=p.a,b=p.b,ratio=p.ratio}end
-- 出击与补给共用双选规则，最终确认文字由原生页面用途决定。
function Flow.new(profile,options,reviewLabel,mode,translate)
    local tr=assert(translate,"缺少双选草稿语言提供器") -- 不改变草稿协议，只提供用户文字。
    assert(#options==3,"Native loadout requires the aircraft's three SP options")
    local self={stage="sp1",cursor=1,draft=copy(profile),options=options,message=nil}
    local takeoffReview=mode=="resupply" -- 补给保留 YES/NO；出击确认 SP2 后直接交接。
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
            rows[#rows+1]={weapon=-1,enabled=true,code=tr("无兵装","NO WEAPON")}
        elseif self.stage=="review"then
            if takeoffReview then
                rows={{action="takeoff",code=tr("是","YES"),enabled=true},{action="reselect",code=tr("否","NO"),enabled=true}}
            end -- 出击没有额外确认菜单；review 只作为双选已完成的内部状态。
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
        assert(self.stage~="review"or takeoffReview,"双 SP 已确认，正在返回 STANDBY")
        local row=self:rows()[self.cursor]
        if not row.enabled then self.message=tr("该兵装尚未购买","This weapon has not been purchased");return "blocked"end
        self.message=nil
        if self.stage=="sp1"then
            -- 每次确认 SP1 都重新选择 SP2，旧配置不能冒充本轮已经确认的兵装。
            self.draft.a=row.weapon;self.draft.b=-1;self.stage="sp2"
            self:selectWeapon(-1)
            return "sp2"
        elseif self.stage=="sp2"then
            self.draft.b=row.weapon;self.stage="review";self.cursor=takeoffReview and 2 or 1
            return takeoffReview and "review"or "continue"
        elseif self.stage=="review"then
            if row.action=="takeoff"then return row.action end
            self.stage="sp1";self:selectWeapon(self.draft.a);return self.stage
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
