-- 内存命令协议 1；参数与结果只在本次调用内有效，不落盘、不重放。
local Bridge={}
function Bridge.new(options)
    local execute,json,Profile=options.execute,options.json,options.profile
    local requestId=0 -- 跨任务单调递增，旧应答不能冒充当前请求。
    local self={}
    function self:request(command)
        local profile=command.profile
        requestId=requestId+1
        local result=json.decode(assert(execute(command.op,command.plane,command.activator,
            command.target,command.time,Profile.count(profile),profile.a,profile.b,profile.c,
            requestId,Profile.copy(profile).ratio),"原生内存接口没有返回结果"))
        assert(result.request==requestId,"Native response does not match the current request")
        assert(result.ok,result.error)
        return result
    end
    return self
end
return Bridge
