-- 原生命令协议 3；只负责请求/应答，不决定游戏线程、玩家身份或 HUD 时序。
local Bridge={}
function Bridge.new(options)
    local root,execute,readJson,Profile=options.root,options.execute,options.readJson,options.profile
    local requestId=0 -- 跨任务单调递增，旧应答不能冒充当前请求。
    local self={}
    function self:request(command)
        local profile=command.profile
        requestId=requestId+1
        local text=string.format("%s %X %X %d %.9f 3 %d %d %d %d %d",
            command.op,command.plane,command.activator,command.target,command.time,
            Profile.count(profile),profile.a,profile.b,profile.c,requestId)
        local file=assert(io.open(root.."native-command.txt","w"))
        local written,writeError=file:write(text)
        local closed,closeError=file:close()
        assert(written,writeError);assert(closed,closeError) -- 完整写入后才允许核心消费命令。
        execute()
        local result=assert(readJson(root.."native-status.json"))
        assert(result.request==requestId,"Native response does not match the current request")
        assert(result.ok,result.error)
        return result
    end
    return self
end
return Bridge
