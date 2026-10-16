-- 个人配置的加载、迁移、校验和保存；不持有玩家、页面或武器状态。
local Config={}
local function writeText(path,text)
    local file=assert(io.open(path,"w"))
    local written,writeError=file:write(text)
    local closed,closeError=file:close() -- 写失败也要释放句柄；缓冲写入可能直到关闭才报错。
    assert(written,writeError);assert(closed,closeError)
end
function Config.writeJson(path,value,json)
    local text=json.encode(value) -- 编码成功后才打开目标，避免编码错误清空旧文件。
    writeText(path,text)
end
local function readAll(file)
    local text,readError=file:read("*a")
    local closed,closeError=file:close()
    assert(text,readError);assert(closed,closeError)
    return text
end
local function supportedKey(key,managementKey)
    return type(key)=="number"and key%1==0 and
        ((key>=48 and key<=57)or(key>=65 and key<=90)or(key>=96 and key<=111)
        or(key>=112 and key<=135 and key~=managementKey)or(key>=186 and key<=192)or(key>=219 and key<=222)
        or key==8 or key==9 or key==13 or key==32 or(key>=33 and key<=40)or key==45 or key==46)
end
-- 所有管理设置先完整验证，再写盘，不允许部分保存或静默替换错误键位。
local function validateSettings(settings)
    local managementKey=settings.management_key -- 管理入口仅支持功能键，关闭游戏后修改配置。
    assert(type(managementKey)=="number"and managementKey%1==0 and managementKey>=112 and managementKey<=135,"管理快捷键须为 F1 至 F24 的整数键码")
    assert(settings.language=="auto"or settings.language=="zh"or settings.language=="en","Language must be auto, zh or en")
    assert(type(settings.central_hint)=="boolean","中央简称开关必须为布尔值")
    assert(settings.hud==nil and settings.direct_select==nil and settings.ignore_unlocks==nil,"旧功能开关已移除")
    assert(type(settings.keyboard_keys)=="table"and #settings.keyboard_keys==4,"需要 MSL/SP1/SP2/SP3 四个键位")
    local used={}
    for _,key in ipairs(settings.keyboard_keys)do
        assert(supportedKey(key,managementKey),"不支持此键位，管理快捷键和 Esc 为保留键")
        assert(not used[key],"四个武器选择键不能重复");used[key]=true
    end
    assert(settings.ammo_ratio==nil,"三 SP 已改为按数量均分，不接受旧比例滑块")
end

function Config.new(options)
    local root,note,json,Profile=options.root,options.note,options.json,options.profile
    local readJson=options.readJson
    local writeJson=options.writeJson or function(path,value)Config.writeJson(path,value,json)end
    local self={}
    function self:save(value)
        -- 覆盖配置前保留上一份，读写失败仍向调用者报告。
        local old=io.open(root.."profiles.json","r")
        if old then writeText(root.."profiles.previous.json",readAll(old))end
        writeJson(root.."profiles.json",value)
    end
    function self:load()
        local config=readJson(root.."profiles.json")or {schema=2,management_revision=3,profiles={},language="auto",central_hint=true,keyboard_keys={49,50,51,52},management_key=117}
        local migrateSettings=config.schema==1 -- 旧版方案只补空 SP3，比例统一重算。
        assert(type(config.profiles)=="table"and (migrateSettings or config.schema==2),"不支持的配置版本")
        if migrateSettings then
            assert(config.management_revision==nil or config.management_revision==2,"旧管理参数版本无效")
            for id,saved in pairs(config.profiles)do
                assert(saved.c==nil,"旧版方案含未知第三槽，停止迁移")
                saved.c=-1;config.profiles[id]=Profile.copy(saved)
            end
            config.keyboard_keys=config.keyboard_keys or {49,50,51}
            assert(#config.keyboard_keys==3,"旧版键位数量无效")
            -- 默认 4 与旧自定义键冲突时明确要求处理，不能偷偷改掉用户键位。
            for _,key in ipairs(config.keyboard_keys)do assert(key~=52,"旧键位已占用 4，请先在 1.0.2 调整后迁移")end
            config.keyboard_keys[4]=52
            config.schema=2;config.management_revision=3;config.ammo_ratio=nil
            config.language=config.language or "auto";config.central_hint=config.central_hint~=false
            config.management_key=config.management_key or 117
            config.hud=nil;config.direct_select=nil;config.ignore_unlocks=nil;config.hud_scale=nil;config.native_loadout=nil
            note("THREE_SP_MIGRATION schema=2 management=3 ammo=equal_share")
        end
        assert(config.management_revision==3,"三 SP 需要管理参数第 3 版")
        for _,saved in pairs(config.profiles)do Profile.count(saved)end
        validateSettings(config)
        if migrateSettings then
            -- 迁移前保存永久副本，避免后续普通保存覆盖可回退的 1.0.2 配置。
            local bytes=readAll(assert(io.open(root.."profiles.json","r")))
            local existing=io.open(root.."profiles.before-three-sp.json","r")
            if existing then existing:close();error("三 SP 迁移备份已存在，请核对上次迁移状态")end
            writeText(root.."profiles.before-three-sp.json",bytes)
            self:save(config);note("MANAGEMENT_MIGRATION_SAVED revision=3")
        end
        return config
    end
    function self:applySettings(config,action)
        assert(type(action.request)=="number"and action.request%1==0 and action.request>0,"管理保存请求编号无效")
        local nextConfig=json.decode(json.encode(config))
        assert(action.management_protocol==3,"管理界面协议不匹配，请成套安装")
        for _,field in ipairs({"language","central_hint","keyboard_keys"})do
            assert(action[field]~=nil,"管理设置缺少字段："..field);nextConfig[field]=action[field]
        end
        validateSettings(nextConfig)
        self:save(nextConfig)
        return nextConfig -- 写盘成功后调用者才替换正在使用的配置。
    end
    return self
end
return Config
