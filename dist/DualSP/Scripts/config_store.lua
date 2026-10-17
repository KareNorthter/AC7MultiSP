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
local function supportedKey(key)
    return type(key)=="number"and key%1==0 and
        ((key>=48 and key<=57)or(key>=65 and key<=90)or(key>=96 and key<=111)
        or(key>=112 and key<=135)or(key>=186 and key<=192)or(key>=219 and key<=222)
        or key==8 or key==9 or key==13 or key==32 or(key>=33 and key<=40)or key==45 or key==46)
end
-- 所有管理设置先完整验证，再写盘，不允许部分保存或静默替换错误键位。
local function validateSettings(settings)
    assert(type(settings.shortcuts)=="table","缺少三个快捷键设置")
    local chords={}
    for _,name in ipairs({"management","cycle","mount"})do
        local chord=settings.shortcuts[name]
        assert(type(chord)=="table"and supportedKey(chord.key),"快捷键需要一个常用键盘键，Esc和独立修饰键不可绑定")
        assert(type(chord.mods)=="number"and chord.mods%1==0 and chord.mods>=0 and chord.mods<=7,"快捷键修饰键无效")
        assert(not(chord.mods==3 and chord.key==46),"Ctrl+Alt+Delete不能用作模组快捷键")
        assert(not(chord.mods==2 and chord.key==115),"Alt+F4不能用作模组快捷键")
        local identity=chord.mods..":"..chord.key
        assert(not chords[identity],"三个快捷键不能重复");chords[identity]=true
    end
    assert(settings.management_key==settings.shortcuts.management.key,"管理入口旧字段与快捷键不一致")
    assert(settings.language=="auto"or settings.language=="zh"or settings.language=="en","Language must be auto, zh or en")
    assert(type(settings.mod_enabled)=="boolean","模组总开关必须为布尔值")
    assert(type(settings.fixed_mount_enabled)=="boolean","多 SP 挂载模型开关必须为布尔值")
    assert(type(settings.target_gaze_pad_x)=="boolean","手柄X注视开关必须为布尔值")
    local gazeKey=settings.target_gaze_key
    assert(gazeKey==0 or supportedKey(gazeKey),"目标注视键位无效")
    assert(gazeKey==0 or not chords["0:"..gazeKey],"目标注视键不能与模组快捷键重复")
    for field,minimum in pairs({ammo_percent_two=50,ammo_percent_three=33})do
        local value=settings[field]
        assert(type(value)=="number"and value%1==0 and value>=minimum and value<=100,"SP 弹药百分比必须为范围内的整数："..field)
    end
    assert(settings.central_hint==nil,"中央武器提示已固定开启")
    assert(settings.hud==nil and settings.direct_select==nil and settings.ignore_unlocks==nil,"旧功能开关已移除")
    assert(type(settings.keyboard_keys)=="table"and #settings.keyboard_keys==4,"需要 MSL/SP1/SP2/SP3 四个键位")
    local used={}
    for _,key in ipairs(settings.keyboard_keys)do
        assert(supportedKey(key),"不支持此武器选择键位")
        assert(not chords["0:"..key],"单键快捷键与武器选择键不能重复")
        assert(gazeKey==0 or key~=gazeKey,"目标注视键不能与武器选择键重复")
        assert(not used[key],"四个武器选择键不能重复");used[key]=true
    end
    assert(settings.ammo_ratio==nil,"旧弹药比例字段已移除")
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
        local config=readJson(root.."profiles.json")or {schema=2,management_revision=7,profiles={},language="auto",mod_enabled=true,fixed_mount_enabled=true,target_gaze_pad_x=false,target_gaze_key=0,ammo_percent_two=50,ammo_percent_three=33,keyboard_keys={49,50,51,52},management_key=117,
            shortcuts={management={key=117,mods=0},cycle={key=117,mods=3},mount={key=118,mods=3}}}
        local migrateSettings=config.schema==1 -- 旧版方案只补空 SP3，弹药参数迁至新版默认值。
        local migrateOptions=migrateSettings or config.management_revision==3 or config.management_revision==4
        assert(type(config.profiles)=="table"and (migrateSettings or config.schema==2),"不支持的配置版本")
        if migrateSettings then
            assert(config.management_revision==nil or config.management_revision==2,"旧管理参数版本无效")
            for id,saved in pairs(config.profiles)do
                assert(saved.c==nil,"旧版方案含未知第三槽，停止迁移")
                saved.c=-1;saved.ratio=nil;config.profiles[id]=Profile.copy(saved)
            end
            config.keyboard_keys=config.keyboard_keys or {49,50,51}
            assert(#config.keyboard_keys==3,"旧版键位数量无效")
            -- 默认 4 与旧自定义键冲突时明确要求处理，不能偷偷改掉用户键位。
            for _,key in ipairs(config.keyboard_keys)do assert(key~=52,"旧键位已占用 4，请先在 1.0.2 调整后迁移")end
            config.keyboard_keys[4]=52
            config.schema=2;config.management_revision=3;config.ammo_ratio=nil
            config.language=config.language or "auto"
            config.management_key=config.management_key or 117
            config.hud=nil;config.direct_select=nil;config.ignore_unlocks=nil;config.hud_scale=nil;config.native_loadout=nil
            note("THREE_SP_MIGRATION schema=2 management=5 ammo=percentage_ceil")
        end
        if migrateOptions then
            config.management_revision=5;config.central_hint=nil
            config.mod_enabled=true;config.fixed_mount_enabled=true
            config.ammo_percent_two=50;config.ammo_percent_three=33
            -- 旧三挂的 1/3 改为明确的 33%；原始配置字节由迁移备份保留。
            for id,saved in pairs(config.profiles)do config.profiles[id]=Profile.withSettings(saved,config)end
        end
        local migrateShortcuts=config.management_revision==5
        if migrateShortcuts then
            assert(config.shortcuts==nil,"旧配置包含未知快捷键字段")
            local key=config.management_key
            assert(type(key)=="number"and key%1==0 and key>=112 and key<=135,"旧管理快捷键须为F1至F24")
            config.shortcuts={management={key=key,mods=0},cycle={key=117,mods=3},mount={key=118,mods=3}}
            config.management_revision=6
        end
        local migrateGaze=config.management_revision==6
        if migrateGaze then
            assert(config.target_gaze_pad_x==nil and config.target_gaze_key==nil,"旧配置含未知目标注视设置")
            config.target_gaze_pad_x=false;config.target_gaze_key=0;config.management_revision=7
        end
        assert(config.management_revision==7,"需要管理参数第 7 版")
        for _,saved in pairs(config.profiles)do Profile.count(saved)end
        validateSettings(config)
        if migrateOptions or migrateShortcuts or migrateGaze then
            -- 独立永久备份保留原始字节；备份失败时禁止覆盖原配置。
            local bytes=readAll(assert(io.open(root.."profiles.json","r")))
            local backups={}
            if migrateOptions then backups[#backups+1]="profiles.before-management-options.json"end
            if migrateShortcuts then backups[#backups+1]="profiles.before-custom-shortcuts.json"end
            if migrateGaze then backups[#backups+1]="profiles.before-target-gaze.json"end
            if migrateSettings then backups[#backups+1]="profiles.before-three-sp.json"end
            for _,name in ipairs(backups)do
                local existing=io.open(root..name,"r")
                if existing then existing:close();error("迁移备份已存在，请核对上次迁移状态："..name)end
            end
            for _,name in ipairs(backups)do writeText(root..name,bytes)end
            self:save(config);note("MANAGEMENT_MIGRATION_SAVED revision=7")
        end
        return config
    end
    function self:applySettings(config,action)
        assert(type(action.request)=="number"and action.request%1==0 and action.request>0,"管理保存请求编号无效")
        local nextConfig=json.decode(json.encode(config))
        assert(action.management_protocol==8,"管理界面协议不匹配，请成套安装")
        assert(action.central_hint==nil,"中央武器提示已固定开启")
        for _,field in ipairs({"language","mod_enabled","fixed_mount_enabled","target_gaze_pad_x","target_gaze_key","ammo_percent_two","ammo_percent_three","keyboard_keys","shortcuts"})do
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
