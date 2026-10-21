-- 机炮齐射只使用同一方案中的一个已核对SP；不把快速轮切当成并发。
local Salvo={}
local modulePath=debug.getinfo(1,"S").source:gsub("^@",""):gsub("\\","/")
local Profile=dofile(assert(modulePath:match("^(.*)/[^/]+$")).."/loadout_profile.lua")
function Salvo.new(options)
    local self={available=false,ownedCall=false,seen="0",bound=false}
    local function invoke(c,action,resume)
        local ok,held,main,press,reason=options.invoke(c.valid and c.plane or 0,c.valid and c.activator or 0,
            c.operable and 1 or 0,action or -1,resume and 1 or 0)
        assert(ok,reason or "机炮齐射核心调用失败")
        return held,main,press
    end
    local function choose(target)
        self.ownedCall=true
        local called,result=pcall(options.select,target)
        self.ownedCall=false
        assert(called,result)
        return result
    end
    function self:cancel(reason)
        options.clearPending()
        if self.bound then
            local called,err=pcall(function()return invoke(options.context(),0,false)end)
            if not called then self.error=tostring(err);options.note("GUN_SALVO_RELEASE_ERROR "..self.error)end
        end
        self.active=nil;self.reason=reason
    end
    function self:release()
        options.clearPending()
        local called,ok,_,_,_,reason=pcall(options.invoke,0,0,0,0,0)
        if not called or not ok then options.note("GUN_SALVO_RELEASE_ERROR "..tostring(called and reason or ok))end
        self.active=nil;self.reason="context_changed";self.bound=false;self.available=false;self.seen="0"
    end
    local function update()
        local c=options.context()
        if not c.valid then if self.bound then self:release()end;return end
        if not c.enabled and not self.bound then return end
        local held,main,press=invoke(c)
        self.bound=true;self.available=c.enabled and c.operable
        local a=self.active
        if a and a.key~=c.key then self:cancel("context_changed");a=nil end
        if not c.operable then if a then self:cancel("unavailable")end;self.seen=press;return end
        if not c.enabled and a and a.phase~="restoring"then
            if c.pending and not c.pendingOwned then self:cancel("superseded");return end
            options.clearPending();c.pending=false
            a.phase="restoring";a.target=c.logical;a.revision=c.revision;a.untilTime=c.time+1
        end
        if not c.enabled and not a then self:release();return end
        if a then
            if c.time<a.started then self:cancel("time_rewound");return end
            if press~=a.press then self:cancel("new_input");self.seen=press;return end
            if a.phase=="waiting" then
                if c.time-a.started>5 then self:cancel("selection_expired");return end
                if not held then
                    if c.pending and not c.pendingOwned then self:cancel("superseded");return end
                    options.clearPending();c.pending=false
                    a.phase="restoring";a.target=c.logical;a.revision=c.revision;a.untilTime=c.time+1
                elseif c.logical==a.target and not c.pending then a.phase="firing";a.revision=c.revision;a.resume=true
                elseif not c.pending or not c.pendingOwned then self:cancel("selection_cancelled");return
                else a.revision=c.revision end
            end
            if a.phase=="firing" then
                if c.revision~=a.revision or c.logical~=a.target then self:cancel("manual_selection");return end
                if not held or not c.enabled or not Profile.hasAmmo(c.ammo[a.target]or 0)then
                    invoke(c,0);a.phase="restoring";a.untilTime=c.time+1
                else
                    invoke(c,1,a.resume);a.resume=false;return
                end
            end
            if a.phase=="restoring" then
                invoke(c,0)
                if c.logical==a.original then invoke(c,0,true);self.active=nil;return end
                if c.revision~=a.revision or c.logical~=a.target or c.time>a.untilTime then self:cancel("restore_expired");return end
                if main or c.pending then return end -- 不把玩家仍按着的SP输入带回旧武器。
                if choose(a.original)then invoke(c,0,true);self.active=nil end
                return
            end
        end
        if press==self.seen then return end
        self.seen=press
        if not c.enabled or not held or main or c.pending then return end
        if #c.eligible~=1 then self.reason=#c.eligible>1 and "multiple_supported_sp"or "no_supported_sp";return end
        local target=c.eligible[1]
        if not Profile.hasAmmo(c.ammo[target]or 0)then self.reason="empty_ammo";return end
        self.reason=nil
        self.active={key=c.key,press=press,target=target,original=c.logical,revision=c.revision,
            phase=c.logical==target and "firing"or "waiting",started=c.time}
        if c.logical==target then invoke(c,1)
        else choose(target)end -- 成功后也等下一次调度，先让原生松键动作有机会取消。
    end
    function self:update()
        if self.failed then return end
        local ok,err=xpcall(update,debug.traceback)
        if not ok then
            self:release();self.failed=true;self.error=tostring(err)
            options.note("GUN_SALVO_ERROR "..self.error)
        end
    end
    return self
end
return Salvo
