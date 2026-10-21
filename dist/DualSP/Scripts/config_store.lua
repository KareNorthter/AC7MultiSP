-- 个人配置的加载、原字节备份、校验和保存；不持有玩家、页面或武器状态。
local Config={}
-- 回中键与组合快捷键可共用主键；按修饰键状态编码，交给游戏线程避开双触发。
function Config.centerShortcutMask(settings)
    local mask=0
    for _,name in ipairs({"management","cycle","mount","salvo","camera_hold"})do
        local chord=settings.shortcuts[name]
        if chord.key==settings.camera_center_key and chord.mods~=0 then mask=mask|(1<<chord.mods) end
    end
    return mask
end
local function writeText(path,text)
    local file=assert(io.open(path,"wb"))
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
local function supportedKey(key)
    return type(key)=="number"and key%1==0 and
        ((key>=48 and key<=57)or(key>=65 and key<=90)or(key>=96 and key<=111)
        or(key>=112 and key<=135)or(key>=186 and key<=192)or(key>=219 and key<=222)
        or key==8 or key==9 or key==13 or key==32 or(key>=33 and key<=40)or key==45 or key==46)
end
-- 所有管理设置先完整验证，再写盘，不允许部分保存或静默替换错误键位。
local function validateSettings(settings)
    assert(type(settings.shortcuts)=="table","缺少五个快捷键设置")
    local chords={}
    for _,name in ipairs({"management","cycle","mount","salvo","camera_hold"})do
        local chord=settings.shortcuts[name]
        assert(type(chord)=="table"and (supportedKey(chord.key)or(name~="management"and chord.key==0 and chord.mods==0)),"快捷键需要一个常用键盘键，Esc和独立修饰键不可绑定")
        assert(type(chord.mods)=="number"and chord.mods%1==0 and chord.mods>=0 and chord.mods<=7,"快捷键修饰键无效")
        assert(not(chord.mods==3 and chord.key==46),"Ctrl+Alt+Delete不能用作模组快捷键")
        assert(not(chord.mods==2 and chord.key==115),"Alt+F4不能用作模组快捷键")
        local identity=chord.mods..":"..chord.key
        if chord.key~=0 then assert(not chords[identity],"五个快捷键不能重复");chords[identity]=true end
    end
    assert(settings.management_key==settings.shortcuts.management.key,"管理入口旧字段与快捷键不一致")
    assert(settings.language=="auto"or settings.language=="zh"or settings.language=="en","Language must be auto, zh or en")
    assert(type(settings.mod_enabled)=="boolean","模组总开关必须为布尔值")
    assert(type(settings.task_mount_enabled)=="boolean"and settings.fixed_mount_enabled==nil,"任务多SP模型开关必须为布尔值，旧总模型开关已退役")
    assert(type(settings.hangar_mount_enabled)=="boolean","机库多SP模型开关必须为布尔值")
    assert(type(settings.dpad_weapon_switch_enabled)=="boolean","手柄十字键切换武器开关必须为布尔值")
    assert(type(settings.gun_salvo_enabled)=="boolean","机炮齐射开关必须为布尔值")
    assert(settings.view_target_mode=="normal"or settings.view_target_mode=="vr","NEXT模式必须为普通或VR模式")
    assert(type(settings.view_center_dot_enabled)=="boolean","视野中心白点开关必须为布尔值")
    assert(type(settings.target_gaze_pad_enabled)=="boolean"and type(settings.target_gaze_keyboard_enabled)=="boolean","手柄和键盘注视开关必须为布尔值")
    assert(settings.target_gaze_enabled==nil and settings.target_gaze_key==nil and settings.camera_r3_enabled==nil and settings.shortcuts.view_target==nil,"旧管理界面字段已退役")
    assert(settings.target_gaze_pad_l3==nil and settings.view_target_pad_l3==nil and settings.view_target_enabled==nil and settings.camera_auto_center_degrees==nil,"旧镜头交互设置已退役")
    assert(type(settings.camera_hold_enabled)=="boolean","镜头保持开关必须为布尔值")
    local centerKey=settings.camera_center_key
    assert(centerKey==0 or centerKey==4 or supportedKey(centerKey),"镜头回正键位无效")
    if settings.camera_hold_enabled and centerKey~=0 then
        assert(not chords["0:"..centerKey],"镜头回正键不能与其他模组键位重复")
    end
    local sensitivity=settings.camera_sensitivity
    assert(type(sensitivity)=="number"and sensitivity%1==0 and sensitivity>=5 and sensitivity<=200,"镜头灵敏度必须为5至200的整数")
    for field,minimum in pairs({ammo_percent_two=50,ammo_percent_three=33})do
        local value=settings[field]
        assert(type(value)=="number"and value%1==0 and value>=minimum and value<=100,"SP 弹药百分比必须为范围内的整数："..field)
    end
    assert(settings.central_hint==nil,"中央武器提示已固定开启")
    assert(settings.central_weapon_display=="always"or settings.central_weapon_display=="switch_1s","中央武器简称显示方式无效")
    assert(settings.hud==nil and settings.direct_select==nil and settings.ignore_unlocks==nil,"旧功能开关已移除")
    assert(type(settings.keyboard_keys)=="table"and #settings.keyboard_keys==4,"需要 MSL/SP1/SP2/SP3 四个键位")
    local used={}
    for _,key in ipairs(settings.keyboard_keys)do
        assert(supportedKey(key),"不支持此武器选择键位")
        assert(not chords["0:"..key],"单键快捷键与武器选择键不能重复")
        assert(not settings.camera_hold_enabled or centerKey==0 or key~=centerKey,"镜头回正键不能与其他模组键位重复")
        assert(not used[key],"四个武器选择键不能重复");used[key]=true
    end
    assert(settings.ammo_ratio==nil,"旧弹药比例字段已移除")
end

function Config.defaults()
    return {schema=2,management_revision=19,task_mount_enabled=true,hangar_mount_enabled=true,dpad_weapon_switch_enabled=true,view_target_mode="normal",view_center_dot_enabled=false,gun_salvo_enabled=true,central_weapon_display="switch_1s",profiles={},language="auto",mod_enabled=true,target_gaze_pad_enabled=false,target_gaze_keyboard_enabled=false,camera_hold_enabled=false,camera_center_key=4,camera_sensitivity=100,ammo_percent_two=50,ammo_percent_three=33,keyboard_keys={49,50,51,52},management_key=117,
            shortcuts={management={key=117,mods=0},cycle={key=0,mods=0},mount={key=0,mods=0},salvo={key=116,mods=3},camera_hold={key=117,mods=3}}}
end

function Config.new(options)
    local root,note,json,Profile=options.root,options.note,options.json,options.profile
    local readJson=options.readJson
    local savedConfig=nil -- 已成功持久化的独立值快照，不持有调用方会继续修改的表。
    local function same(a,b)
        if type(a)~=type(b)then return false end
        if type(a)~="table"then return a==b end
        for key,value in pairs(a)do if not same(value,b[key])then return false end end
        for key in pairs(b)do if a[key]==nil then return false end end
        return true
    end
    local self={}
    function self:save(value)
        local encoded=json.encode(value) -- 先发现编码错误，不能截断旧配置。
        local snapshot=json.decode(encoded)
        if savedConfig and same(snapshot,savedConfig)then return false end
        -- 覆盖配置前保留上一份，读写失败仍向调用者报告。
        local old=io.open(root.."profiles.json","rb")
        if old then writeText(root.."profiles.previous.json",readAll(old))end
        if options.writeJson then options.writeJson(root.."profiles.json",value)
        else writeText(root.."profiles.json",encoded)end
        savedConfig=snapshot;return true
    end
    function self:load()
        local loaded,config=pcall(readJson,root.."profiles.json")
        if loaded and type(config)=="table"and config.schema==2 and config.management_revision==19 then
            assert(type(config.profiles)=="table","不支持的配置版本")
            for _,saved in pairs(config.profiles)do Profile.count(saved)end
            validateSettings(config)
            savedConfig=json.decode(json.encode(config))
            return config
        end
        -- 本轮用户明确不兼容旧配置：先按原字节留档，再使用完整新默认值，不迁移旧字段/方案。
        local old=io.open(root.."profiles.json","rb")
        if old then
            local bytes=readAll(old)
            local stem="profiles.before-2.23.0-reset-"..tostring(os.time())
            local backup=stem..".json";local serial=0
            while true do
                local existing=io.open(root..backup,"rb")
                if not existing then break end
                existing:close();serial=serial+1;backup=stem.."-"..serial..".json"
            end
            writeText(root..backup,bytes)
            note("CONFIGURATION_RESET_BACKUP "..backup)
        end
        config=Config.defaults()
        validateSettings(config)
        self:save(config)
        note("CONFIGURATION_DEFAULTS_SAVED revision=19")
        return config
    end
    function self:setCameraHold(config,enabled)
        local nextConfig=json.decode(json.encode(config));nextConfig.camera_hold_enabled=enabled
        validateSettings(nextConfig) -- 开启后重新校验独立回中键冲突，不绕过管理页约束。
        self:save(nextConfig)
        return nextConfig
    end
    function self:applySettings(config,action)
        assert(type(action.request)=="number"and action.request%1==0 and action.request>0,"管理保存请求编号无效")
        local nextConfig=json.decode(json.encode(config))
        assert(action.management_protocol==21,"管理界面协议不匹配，请成套安装")
        assert(action.central_hint==nil,"中央武器提示已固定开启")
        for _,field in ipairs({"language","mod_enabled","task_mount_enabled","hangar_mount_enabled","dpad_weapon_switch_enabled","gun_salvo_enabled","view_target_mode","view_center_dot_enabled","central_weapon_display","target_gaze_pad_enabled","target_gaze_keyboard_enabled","camera_hold_enabled","camera_center_key","camera_sensitivity","ammo_percent_two","ammo_percent_three","keyboard_keys","shortcuts"})do
            assert(action[field]~=nil,"管理设置缺少字段："..field);nextConfig[field]=action[field]
        end
        nextConfig.management_key=nextConfig.shortcuts.management.key
        validateSettings(nextConfig)
        self:save(nextConfig)
        return nextConfig -- 写盘成功后调用者才替换正在使用的配置。
    end
    return self
end
return Config
