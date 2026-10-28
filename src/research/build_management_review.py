"""生成中文界面审核稿；只读取正式文字源，不写回玩家文案或编译游戏。"""
from pathlib import Path
from copy import deepcopy
import hashlib
import json
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT/'docs/research/management-review-20261028'


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def main():
    OUT.mkdir(exist_ok=True)
    template = (ROOT/'docs/research/management-design/index.html').read_text(encoding='utf8')
    game = read(ROOT/'docs/user/interface/game.zh-CN.json')
    texts = game['texts']
    installer = read(ROOT/'docs/user/interface/installer.zh-CN.json')
    seed = read_seed(template)
    seed.update(storageKey='ac7-review-current-20261028-v2', sourceVersion='2.26.2 alpha2 · 未编译源码',
                sourceCommit=subprocess.check_output(['git', 'rev-parse', '--short', 'HEAD'], cwd=ROOT, text=True).strip(),
                overallNote='', receivedFeedback=[])

    def text(key):
        return texts[key].split('###')[0].replace('\x00', ' / ').rstrip(' / ')

    def item(identity, key, kind='toggle', value='开启', help_key=None, description='', inline=None):
        result = dict(id=identity, label=text(key), kind=kind, value=value,
                      description=text(help_key) if help_key else description,
                      help='help' if help_key else 'visible' if description else 'none', hidden=False, note='',
                      sourceKey=key)
        if inline:
            result['inlineAfter'] = inline
        return result

    def raw(identity, label, kind='text', value='', description=''):
        return dict(id=identity, label=label, kind=kind, value=value, description=description,
                    help='visible' if description else 'none', hidden=False, note='')

    def group(identity, title, items, description=''):
        return dict(id=identity, title=title, items=items, description=description)

    def page(identity, key, groups):
        return dict(id=identity, title=text(key), groups=groups)

    seed['pages'] = [
        page('features', 'general.features.d6795dc0', [
            group('models', text('model.display.0bf20428'), [
                item('task_mount', 'use.multi.sp.models.in.f85ff7cd', help_key='replaces.native.aircraft.models.with.d5984a98'),
                item('hangar_mount', 'use.multi.sp.models.in.b06eb8b1', help_key='uses.multi.sp.models.in.903bf914')]),
            group('features_main', text('feature.settings.540ec6f5'), [
                item('central', 'always.show.the.current.weapon.84c597b0', value='关闭'),
                item('ammo_two', 'ammo.per.weapon.with.two.fe1030b7', 'slider', '50%'),
                item('ammo_three', 'ammo.per.weapon.with.three.3eca43e5', 'slider', '33%'),
                item('salvo', 'gun.salvo.gunsalvo.acda2ffc', help_key='with.an.mgp.hgp.plsl.873e6dbb'),
                item('salvo_key', 'shortcut.51cb85e1', 'binding', 'Ctrl + Alt + F5', inline='salvo')]),
            group('direct', text('additional.weapon.selection.3cf00ef8'), [
                item('dpad', 'switch.weapons.with.the.controller.80495bbe'),
                *[raw('direct_'+str(n), name, 'binding', str(n+1), pad) for n, (name, pad) in enumerate(
                    [('MSL', '手柄：十字键左'), ('SP1', '手柄：十字键上'), ('SP2', '手柄：十字键右'), ('SP3', '手柄：十字键下')])]],
                  text('the.native.weapon.action.cycles.48a65ba7'))]),
        page('camera', 'camera.bindings.291ded16', [
            group('view_target', text('target.selection.8ff0d833'), [
                item('target_mode', 'target.selection.mode.nextmode.160bd5fc', 'select', '原生模式 / 机头附近', 'native.mode.prioritizes.targets.near.e00f5e88'),
                item('center_dot', 'show.the.view.center.dot.80e497f3', value='关闭')]),
            group('tracking', text('latched.target.tracking.11a27755'), [
                item('tracking_pad', 'hold.controller.y.to.latch.8a3a68d5', help_key='when.enabled.hold.controller.y.3329cb01'),
                item('tracking_key', 'press.the.native.keyboard.tracking.21d621bc', help_key='uses.the.game.s.actual.c7dce5e6')]),
            group('camera_hold', text('camera.hold.202916ec'), [
                item('hold', 'keep.camera.direction.camerahold.70008ef1', help_key='when.enabled.the.view.retains.446f23c3'),
                item('hold_key', 'shortcut.51cb85e1', 'binding', '未绑定', inline='hold'),
                item('sensitivity', 'view.sensitivity.camerasensitivity.6d7158ee', 'slider', '100%'),
                item('recenter', 'keyboard.recenter.shortcut.27495edb', 'binding', '未绑定')])]),
        page('debug', 'debug.debug.3211ed36', [
            group('loadout', text('mission.loadout.682b8e9c'), [
                item('infinite', 'infinite.ammo.infiniteammo.6a8db67a', value='关闭'),
                item('fast', 'fast.reload.fastreload.645bd7ab', value='关闭'),
                raw('loadout_slots', 'SP1 / SP2 / SP3', 'select', '从当前机体兵装选择', text('checked.weapons.become.sp1.sp2.4395e57c')),
                item('loadout_apply', 'apply.sp.loadout.and.refill.47b451be', 'action', text('apply.sp.loadout.and.refill.47b451be'))]),
            group('aircraft', text('change.aircraft.6b159313'), [
                item('aircraft_search', 'search.name.code.id.a15a68b4', 'binding', '搜索名称 / 代号 / 编号'),
                raw('aircraft_filter', '机体来源', 'select', '全部机体 / 官方机体 / Mod机体'),
                raw('aircraft_initial', '首字母筛选', 'select', '全部首字母'),
                item('aircraft_reset', 'reset.0f237974', 'action', text('reset.0f237974')),
                raw('aircraft_list', '可用机体列表', 'text', description='列表从游戏目录动态读取；此稿不伪造可用、购买或DLC资格。'),
                item('aircraft_apply', 'change.aircraft.and.restart.mission.cdc35890', 'action', text('change.aircraft.and.restart.mission.cdc35890'), description=text('choose.an.aircraft.and.use.cc75b2be'))]),
            group('quick', text('quick.title'), [
                *[item('quick_'+key, 'quick.'+key, 'select', value) for key, value in [('task', '任务示例'), ('aircraft', '当前机体'), ('skin', '默认皮肤（0）'), ('difficulty', '普通')]],
                raw('quick_sp', 'SP1 / SP2 / SP3', 'select', '请选择 1 至 3 种 SP', text('quick.parts_pending')),
                item('quick_auto', 'quick.auto', value='关闭', help_key='quick.auto_help'),
                item('quick_logo', 'quick.skip_logos', value='关闭'),
                *[item('quick_'+key, 'quick.'+key, 'action', text('quick.'+key)) for key in ('save', 'start', 'cancel_auto')]], text('quick.help')),
            group('logs', text('logs.2c322bb4'), [
                raw('logs_expand', '日志', 'action', '展开'),
                item('logs_refresh', 'refresh.refreshlogs.423e6a9d', 'action', '刷新', 'shows.the.last.64.kib.4ded2bc8'),
                raw('logs_files', 'mod.log / native.log', description='实际日志正文按需读取；此处不嵌入个人日志。')])]),
        page('updates', 'version.and.updates.versionupdates.f6da021a', [
            group('versions', '版本信息', [
                item('startup_update', 'updates.check_on_startup'),
                item('current_version', 'current.version.16c8badf', 'text', description='2.26.2 alpha2'),
                item('latest_version', 'latest.version.51f76219', 'text', description='—（由实际检查结果决定）'),
                item('check_update', 'check.for.updates.840f7526', 'action', text('check.for.updates.840f7526')),
                raw('download', '下载区（发现新版时显示）', 'action', text('primary.download.link.f6096f63'))]),
            group('feedback', '反馈', [raw('feedback_body', '反馈说明', description=feedback_body()),
                item('open_logs', 'open.mod.log.folder.06bdb703', 'action', text('open.mod.log.folder.06bdb703'))]),
            group('changelog', text('changelog.fd31c92d'), [raw('changelog_expand', '主要更新', 'action', '展开'),
                raw('changelog_text', '更新日志正文', description=(ROOT/'docs/user/更新日志.txt').read_text(encoding='utf8'))]),
            group('credits', text('author.and.license.5de4e76d'), [raw('credits_expand', '作者与授权', 'action', '展开'),
                raw('credits_text', '作者与授权正文', description=(ROOT/'docs/user/作者与授权.txt').read_text(encoding='utf8'))])])]
    seed['theme'] = dict(layout='top', density='compact', width=1080, font=15, radius=5, accent='#5ed6c9')
    seed['chrome'].update(title='AC7MultiSP', subtitle='2.26.2 alpha2 · 当前源码结构',
                          footer=text('closing.discards.unsaved.changes.language.585b36b5'), masterEnabled=True,
                          status=text('settings.saved.d104ef7d'))
    seed['chrome']['managementBinding'] = item('management_binding', 'management.6009aa3c', 'binding', 'F6', 'back.y.also.toggles.management.b588258e')
    seed['messages'] = messages(game, installer)
    seed['baseline'] = dict(pages=deepcopy(seed['pages']), theme=deepcopy(seed['theme']), chrome=deepcopy(seed['chrome']), messages=deepcopy(seed['messages']))
    seed['updatedAt'] = '2026-10-28'
    # 保留旧稿与批注，只复用编辑器；替换结构与文案快照。
    template = re.sub(r'(<script id="designSeed" type="application/json">).*?(</script>)',
                      lambda m: m[1]+json.dumps(seed, ensure_ascii=False, indent=2).replace('<', '\\u003c')+m[2], template, flags=re.S)
    template = template.replace('语言/Language', '语言').replace('飞行控制中心 · 2.26.1 界面调整稿', '2.26.2 alpha2 · 当前源码结构')
    template = template.replace('<span>底部说明</span><input id="footerField">', '<span>底部说明</span><input id="footerField"></label><label class="field"><span>底部状态（预览）</span><input id="statusField">')
    template = template.replace("['footerField','footer']", "['footerField','footer'],['statusField','status']")
    template = template.replace("$('panelFooter').replaceChildren(el('small','muted',draft.chrome.footer)", "$('panelFooter').replaceChildren(el('small','muted',draft.chrome.status),el('small','muted',draft.chrome.footer)")
    template = template.replace("$('footerField').value=draft.chrome.footer", "$('statusField').value=draft.chrome.status||'设置已保存';$('footerField').value=draft.chrome.footer")
    template = template.replace('<main class="workspace">', '<nav class="review-switch"><button id="showLayout" class="primary">界面排版</button><button id="showMessages">全部弹窗与提示</button><span>只记录中文意见；确认后再同步正式双语资源。</span></nav><section id="messageReview" hidden></section><main class="workspace" id="layoutReview">')
    template = template.replace('</style>', EXTRA_CSS+'\n</style>')
    template = template.replace('function render(){renderPreview();renderInspector()}', 'function render(){renderPreview();renderInspector();renderMessages()}')
    template = template.replace("draft.chrome=base.chrome;draft.overallNote='';", "draft.chrome=base.chrome;draft.messages=clone(base.messages);draft.overallNote='';")
    template = template.replace("return lines.join('\\n')}\n$('exportText')", "for(const m of draft.messages||[])if(m.proposed!==m.original||m.note||m.trigger!==m.originalTrigger)lines.push('','提示：'+m.id,'触发：'+m.trigger,'原文：'+m.original,'修改：'+m.proposed,'意见：'+m.note);return lines.join('\\n')}\n$('exportText')")
    template = template.replace("const incoming=validate(JSON.parse(await file.text()));", "const incoming=validate(JSON.parse(await file.text()));if(!Array.isArray(incoming.messages))throw Error('此方案缺少本轮提示清单。');")
    template = template.replace("render();\n</script>", EXTRA_JS+'\nrender();\n</script>')
    # 当前标题独立一行，控制栏在下一行；底部保留当前署名区域位置。
    template = template.replace("$('modeButton').textContent=", "$('panelFooter').append(el('div','review-credit','本程序仅供学习交流，完全免费，请勿用于其他商业用途 · Northter · 班德慕 · 主页 · 请我喝咖啡'));$('modeButton').textContent=")
    (OUT/'index.html').write_text(template, encoding='utf8')
    (OUT/'提示清单.json').write_text(json.dumps(seed['messages'], ensure_ascii=False, indent=2)+'\n', encoding='utf8')
    print(json.dumps(dict(pages=len(seed['pages']), items=sum(len(g['items']) for p in seed['pages'] for g in p['groups']),
                         messages=len(seed['messages']), output=str(OUT/'index.html')), ensure_ascii=False))


def read_seed(template):
    return json.loads(re.search(r'<script id="designSeed" type="application/json">(.*?)</script>', template, re.S)[1])


def feedback_body():
    source = (ROOT/'docs/user/作者与授权.txt').read_text(encoding='utf8')
    start = source.index('反馈：')
    return source[start:source.index('第三方组件', start)].strip()


def messages(game, installer):
    """保留全部文字供兜底检索；明确区分真正弹窗、帮助、底部状态和普通标签。"""
    files = [p for folder in ('src', 'dist/DualSP/Scripts') for p in (ROOT/folder).rglob('*')
             if p.suffix in ('.hpp', '.cpp', '.cs', '.lua') and 'generated' not in p.parts]
    sources = [(p, p.read_text(encoding='utf8', errors='replace').splitlines()) for p in files]
    explicit = {
        'recovery.loaded': ('启动通知', '首次进入安全的单人任务外菜单，目录已就绪、管理页关闭；每次启动只显示一次，约3秒。'),
        'recovery.catalogue_count': ('异常通知', '目录就绪后发现SP资料异常；并入首次加载通知，或在任务外单独通知一次。'),
        'recovery.aircraft': ('故障通知', '当前机体被隔离；停止增强功能并保留管理界面诊断。'),
        'recovery.leave_mission': ('故障通知', '任务内或补给时发生错误，尚不能确认安全回退；要求退出任务换机。'),
        'recovery.severe': ('故障通知', '严重或共享状态错误、严格收尾失败；整个进程停用增强并要求重启。'),
        'recovery.catalogue_title': ('详情弹窗', '基础功能页存在资料异常时，点击异常数量按钮；打开可滚动的逐条详情。'),
        'recovery.catalogue_help': ('详情弹窗', 'SP资料异常详情窗口打开时显示说明。'),
        'already.up.to.date.9c218466': ('更新通知', '更新检查完成，当前版本已是最新；任务外且管理页关闭后单独显示约3秒。'),
        'unable.to.check.for.updates.f4fdc2e6': ('更新通知', '更新请求、校验或解析失败；任务外单独通知，同时在版本页显示结果。'),
        'update.information.has.not.been.4e698d95': ('更新通知', '服务器返回尚未发布状态；任务外单独通知。'),
        'version.29bc0009': ('更新通知片段', '发现新版通知开头；随后拼接实际版本、管理快捷键和版本页入口。'),
        'is.available.open.e79f34f1': ('更新通知片段', '发现新版通知中段；前接版本，后接当前管理快捷键。'),
        'version.and.updates.and.choose.cd6e7a78': ('更新通知片段', '发现新版通知结尾；提示进入版本和更新页。'),
        'this.sp.is.empty.selection.92f6b119': ('底部提示', '请求直选未配置SP时，保留当前兵装，并在管理页底部显示。'),
        'this.sp.is.depleted.selection.c47a85ad': ('底部提示', '请求直选耗尽SP时，保留当前兵装，并在管理页底部显示。'),
        'settings.saved.ce3bf5a0': ('底部提示', '收到本次保存请求的成功回执。'),
        'save.failed.see.the.error.cfdce700': ('底部提示', '收到保存失败回执；异常详情另列原因。'),
        'management.components.do.not.match.9ae559c4': ('阻断提示', '管理协议不一致或界面状态解析失败，只显示错误与关闭入口。'),
        'quick.skin_unavailable': ('页面提示', '快速入口所选皮肤无有效引用、未在可用目录或DLC许可未确认。'),
        'quick.unavailable': ('页面提示', '快速入口目录尚未就绪或组件协议不匹配。'),
        'loading.multi.sp.models.please.b6555bf0': ('模型加载通知', '任务启动挂载预热/加载进行中，核心通知显示；完成或取消后结束。'),
        'no.multi.sp.model.layout.9b7d944c': ('模型加载通知', '请求多SP模型，但当前机体没有已登记可用布局。'),
    }
    for key in ('gun.salvo.on.ff42b867', 'gun.salvo.off.2b40a562'):
        explicit[key] = ('快捷键通知', '使用机炮齐射快捷键切换功能，游戏线程完成切换后显示实际开启/关闭状态。')
    for key in ('camera.hold.on.43a9820d', 'camera.hold.off.5641503e'):
        explicit[key] = ('快捷键通知', '通过视角保持快捷键或长按R3切换功能后显示实际状态；失败另报原因。')
    for key in ('target.tracking.on.7e430432', 'target.tracking.off.f41f765a', 'no.target.is.selected.2d02fc46', 'target.tracking.is.currently.unavailable.83446d36'):
        explicit[key] = ('快捷键通知', '使用已启用的键盘注视键或手柄长按注视操作；根据目标和许可显示开启、停止、无目标或不可用。')
    for key in ('mission.multi.sp.models.on.79af0eb7', 'mission.multi.sp.models.off.f4fb65f0', 'multi.sp.models.on.10e704f5', 'multi.sp.models.off.ca0b9e2a'):
        explicit[key] = ('快捷键通知', '任务模型显示快捷键请求被处理后提示实际状态；模型应用仍服从安全配装边界。')
    for key in ('mod.enabled.059ed7da', 'mod.disabled.ddc732b7'):
        explicit[key] = ('总开关通知', '保存总开关并到达允许应用的主菜单或单人任务选择页，实际切换完成后提示。')
    result = []
    replacements = set(game['message_replacements'])
    for key, value in game['texts'].items():
        refs, contexts, helps = [], [], False
        match = game.get('message_matches', {}).get(key, {})
        for p, lines in sources:
            for n, line in enumerate(lines):
                literal = any(part and len(part)>5 and part in line for part in (match.get('zh'), match.get('en')))
                if '"'+key+'"' in line or literal:
                    refs.append(p.relative_to(ROOT).as_posix()+':'+str(n+1))
                    contexts.append(' '.join(lines[max(0,n-2):n+1]).strip()[:700])
                    helps = helps or ('panelHelp("'+key+'"' in line)
        if key in explicit:
            category, trigger = explicit[key]
        elif helps:
            category, trigger = '帮助弹窗', '对应选项旁的“?”按钮悬停、获得键盘焦点或点击时显示；点击可供键盘/手柄打开。'
        elif key in replacements:
            category, trigger = '错误映射', '对应功能检测到“'+value.rstrip('。')+'”时报告；按影响范围显示功能警告、隔离机体或要求重启。具体断言/条件见来源。'
        elif any(re.search(r'(?:notice\s*=|showToast|showNotice)', context) for context in contexts):
            category, trigger = '动态提示', '对应功能执行后发布通知或操作回执；具体分支、拼接顺序及错误条件见调用来源。'
        elif key.startswith('recovery.'):
            category, trigger = '异常详情', 'SP资料检查发现对应的缺失、重复或无效条目，在详情列表中逐条显示。'
        else:
            category, trigger = '界面词条补充', '普通标签、状态或动态消息片段，不能单凭词条认定独立弹窗；原始调用位置见来源。'
        result.append(dict(id='game/'+key, category=category, trigger=trigger, originalTrigger=trigger,
                           original=value, proposed=value, note='', sources=list(dict.fromkeys(refs)), contexts=contexts[:5]))
    for key, value in installer.items():
        if not (key.startswith(('error.', 'result.', 'update.')) or key.endswith(('Confirm', '.confirm', '.warning', '.dialogError'))):
            continue
        if key=='install.closeGameConfirm': condition='作者选择关闭游戏后安装，准备结束游戏进程前确认。'
        elif key=='uninstall.confirm': condition='点击卸载后，实际执行前确认。'
        elif key.startswith('result.'): condition='安装、卸载或导出流程成功结束后显示对应结果。'
        elif key.startswith('update.'): condition='安装器启动检查或手动检查更新时显示相应状态或结果。'
        else: condition='安装器遇到“'+value.rstrip('。')+'”所述问题时停止相应操作；具体失败条件见该键调用位置。'
        refs=[]; contexts=[]
        for p, lines in sources:
            if p.suffix!='.cs': continue
            for n,line in enumerate(lines):
                if '"'+key+'"' in line:
                    refs.append(p.relative_to(ROOT).as_posix()+':'+str(n+1));contexts.append(' '.join(lines[max(0,n-2):n+1]).strip()[:700])
        result.append(dict(id='installer/'+key, category='安装器提示', trigger=condition, originalTrigger=condition,
                           original=value, proposed=value, note='', sources=refs, contexts=contexts[:5]))
    for identity, trigger, value, refs in [
        ('startup_with_issues', '首次安全单人菜单加载完成且有SP资料异常；两行组合为同一条加载通知。',
         game['texts']['recovery.loaded']+'\n'+game['texts']['recovery.catalogue_count'], ['src/overlay.cpp:1087']),
        ('newer_version', '更新检查发现新版，等待任务外、管理页关闭且无重要通知遮挡后显示；动态版本和快捷键由当前状态替换。',
         game['texts']['version.29bc0009']+'{最新版本}'+game['texts']['is.available.open.e79f34f1']+'{管理快捷键}'+game['texts']['version.and.updates.and.choose.cd6e7a78'], ['src/overlay.cpp:1102']),
        ('runtime_error', '出现未登记为固定词条的运行错误；从实际异常截取首行并去除Lua路径/堆栈后，通过统一错误通知显示。具体原因无法穷举，原文留日志。',
         'ACMultiSP: {实际错误首行或功能摘要}', ['src/ui_messages.hpp:25', 'src/overlay.cpp:1017']),
    ]:
        result.append(dict(id='runtime/'+identity,category='组合与动态通知',trigger=trigger,originalTrigger=trigger,
                           original=value,proposed=value,note='',sources=refs,contexts=[]))
    return result


EXTRA_CSS = '''
.review-switch{padding:12px 26px;display:flex;gap:12px;align-items:center;border-bottom:1px solid #344256;flex-wrap:wrap}.review-switch span{color:#a7b9c9;font-size:13px}.ac-header{display:block}.ac-header-controls{display:flex;gap:12px;align-items:center;flex-wrap:wrap;margin-top:12px}.ac-group{background:#141d29;border-color:#344256}.ac-row{border-top:0;padding-top:7px;padding-bottom:7px}.review-credit{margin-top:12px;text-align:right;color:#91a4b8;font-size:11px}#messageReview{padding:24px;max-width:1500px;margin:auto}.message-tools{display:flex;gap:12px;flex-wrap:wrap;position:sticky;top:0;background:#111b28;padding:15px;z-index:2}.message-tools input{flex:1;min-width:200px}.message-card{margin:14px 0;padding:20px;border:1px solid #344256;border-radius:6px;background:#141f2e}.message-card h3{font-size:15px;color:#8ce2d2;overflow-wrap:anywhere}.message-grid{display:grid;grid-template-columns:1fr 1fr;gap:18px;margin-top:12px}.message-card textarea{min-height:76px}.message-original{white-space:pre-wrap;padding:12px;background:#0c1520;color:#b5c6d7;overflow-wrap:anywhere}.message-card code{white-space:pre-wrap;font-size:12px;overflow-wrap:anywhere}.message-card summary{cursor:pointer;color:#91b8cc;margin:12px 0}.message-count{color:#afc6d8;padding:12px}.message-preview{padding:22px;border:1px solid #e7a259;max-width:800px;margin:14px auto;text-align:center;font-size:24px;background:#141c28}.message-card label>span{font-size:12px;color:#a4bbcd}.message-empty{padding:40px;text-align:center}.message-pagination{display:flex;gap:10px;align-items:center}@media(max-width:800px){.message-grid{grid-template-columns:1fr}.review-switch{padding:12px}}
'''

EXTRA_JS = r'''
let messagePage=0;
function renderMessages(){
 const root=$('messageReview');if(root.hidden)return;
 const search=root.querySelector('#messageSearch')?.value||'',category=root.querySelector('#messageCategory')?.value||'常用弹窗',only=root.querySelector('#messageChanged')?.checked||false;
 root.replaceChildren();const tools=el('div','message-tools');const input=el('input');input.id='messageSearch';input.placeholder='搜索文字、触发条件或稳定编号';input.value=search;
 const select=el('select');select.id='messageCategory';for(const name of ['常用弹窗','全部（含普通词条）',...new Set(draft.messages.map(m=>m.category))]){const o=el('option','',name);o.value=name;select.append(o)}select.value=category;
 const changed=el('input');changed.type='checkbox';changed.id='messageChanged';changed.checked=only;const changeLabel=el('label');changeLabel.append(changed,document.createTextNode(' 只看已改'));
 tools.append(input,select,changeLabel);root.append(tools);let timer;input.oninput=()=>{clearTimeout(timer);timer=setTimeout(()=>{const cursor=input.selectionStart;messagePage=0;renderMessages();const next=$('messageSearch');next.focus();next.setSelectionRange(cursor,cursor)},300)};select.onchange=changed.onchange=()=>{messagePage=0;renderMessages()};
 const rows=draft.messages.filter(m=>(category==='全部（含普通词条）'||(category==='常用弹窗'?!['界面词条补充','错误映射'].includes(m.category):m.category===category))&&(!only||m.proposed!==m.original||m.trigger!==m.originalTrigger||m.note)&&(!search||[m.id,m.original,m.proposed,m.trigger,m.note].join(' ').toLowerCase().includes(search.toLowerCase())));
 const size=20,total=Math.max(1,Math.ceil(rows.length/size));messagePage=Math.min(messagePage,total-1);
 root.append(el('p','message-count',`共 ${draft.messages.length} 条；当前筛选 ${rows.length} 条。原文和来源只读，触发条件、拟改文字、意见可编辑。### 后为控件标识，空字符为选项分隔，均不作为玩家可见文字。`));
 const pager=el('div','message-pagination');pager.append(button('上一页',()=>{if(messagePage>0){messagePage--;renderMessages()}}),el('span','',`第 ${messagePage+1} / ${total} 页`),button('下一页',()=>{if(messagePage+1<total){messagePage++;renderMessages()}}));root.append(pager);
 for(const m of rows.slice(messagePage*size,(messagePage+1)*size)){
  const card=el('article','message-card');card.append(el('h3','',m.category+' · '+m.id));
  const trigger=el('textarea');trigger.value=m.trigger;trigger.setAttribute('aria-label',m.id+' 触发条件');
  const original=el('div','message-original',m.original);const proposed=el('textarea');proposed.value=m.proposed;proposed.setAttribute('aria-label',m.id+' 拟改文字');
  const note=el('textarea');note.value=m.note;note.placeholder='希望如何调整出现时机、位置、颜色或措辞';note.setAttribute('aria-label',m.id+' 修改意见');
  const label=(name,node)=>{const l=el('label');l.append(el('span','',name),node);return l};
  const grid=el('div','message-grid');grid.append(label('触发条件（可修改）',trigger),label('当前原文（只读）',original),label('拟改文字',proposed),label('修改意见',note));card.append(grid);
  for(const [node,key]of [[trigger,'trigger'],[proposed,'proposed'],[note,'note']]){node.onfocus=()=>{if(!node.dataset.remembered){remember();node.dataset.remembered='1'}};node.oninput=()=>{m[key]=node.value;changedDraft()};node.onblur=()=>delete node.dataset.remembered}
  const details=el('details');details.append(el('summary','',`来源与实际调用上下文（${m.sources.length}处）`),el('code','',m.sources.join('\n')||'动态片段或统一错误映射，按稳定键定位'),el('pre','',m.contexts.join('\n\n')));card.append(details);
  card.append(button('预览这条提示',()=>{const old=card.querySelector('.message-preview');if(old){old.remove();return}card.append(el('div','message-preview',m.proposed.replace(/###.*/g,'').replace(/\0/g,' / ')))}));root.append(card);
 }
 if(!rows.length)root.append(el('p','message-empty','没有符合条件的条目。'));
}
function changedDraft(){changed()}
$('showLayout').onclick=()=>{$('layoutReview').hidden=false;$('messageReview').hidden=true;$('showLayout').classList.add('primary');$('showMessages').classList.remove('primary')};
$('showMessages').onclick=()=>{$('layoutReview').hidden=true;$('messageReview').hidden=false;$('showMessages').classList.add('primary');$('showLayout').classList.remove('primary');renderMessages()};
'''


if __name__ == '__main__':
    main()
