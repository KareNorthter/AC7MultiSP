"""本机故障测试面板：快速五轮、自动保存上一轮日志，不启动游戏。"""
import argparse
import atexit
import copy
import ctypes
import csv
from datetime import datetime
import json
import os
from pathlib import Path
import queue
import subprocess
import sys
import threading
import tkinter as tk
from tkinter import ttk, messagebox

from aircraft_fault_test_control import CASES, closed, digest, require

PYTHON = str(Path(sys.executable).with_name('python.exe'))

ROUNDS = [
    ('正常基线', 'off', '正常启动至单人菜单：检查加载提示、更新结果、F6和保存设置。\nF-16C选择两种SP出击并轮切；Mirage 2000-5也应正常。\n任务内看F6时先使用游戏原生暂停。完成后退出游戏，回到面板。'),
    ('资料异常与换机恢复', 'T2', '选择F-16C：应报告两个测试兵装缺失，F6可查看槽位和原因。\n退出任务后换Mirage 2000-5，确认多SP正常；再选回F-16C，仍应隔离。\n可以顺便保存一个无关设置，确认保存不会解除隔离。完成后退出游戏。'),
    ('任务内出错与回退', 'T5', '重新启动后，F-16C应可正常配置两种SP。起飞后首次轮切会触发错误。\n按提示退出任务，换Mirage 2000-5确认多SP恢复，再回F-16C仍应隔离。\n检查输入、光标和声音有无残留；完成后退出游戏。'),
    ('严重错误保护', 'T6', '选择F-16C并进入选装页，会模拟一次选装及收尾失败。\n应提示重启，F6仍可查看；换机、保存设置或切换总开关均不能恢复增强。\n不要继续出击；完成观察后完整退出游戏。'),
    ('重启恢复', 'off', '完整重启后，人工异常应消失。\nF-16C和Mirage 2000-5应可重新选择多SP、出击及轮切，输入和暂停无残留。\n新模型外观可在这轮顺便检查。完成后退出游戏，保存本轮结果。'),
]
LABELS = {'off': '关闭人工故障', 'T1': '单槽缺失兵装', 'T2': '两槽缺失兵装',
          'T3-weapon': '重复兵装资料', 'T3-ammo': '无效载量', 'T3-aircraft': '重复机体编号',
          'T4': '选装接入错误', 'T5': '任务内轮切错误', 'T6': '清理失败保护', 'T7': '核心共享故障',
          'U-current': '更新：当前', 'U-newer': '更新：新版', 'U-failed': '更新：失败', 'U-unpublished': '更新：尚未发布'}


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def write(path, data):
    with path.open('x', encoding='utf8', newline='\n') as file:
        json.dump(data, file, ensure_ascii=False, indent=2)
        file.write('\n')


class Panel:
    def __init__(self, root, session):
        self.root, self.session = root, session
        self.control = Path(__file__).with_name('aircraft_fault_test_control.py')
        self.events = queue.Queue()
        self.busy = False
        self.progress_file = session/'quick-progress.json'
        self.progress = read(self.progress_file) if self.progress_file.exists() else {'index': 0, 'armed': False, 'results': []}
        self.status = tk.StringVar(value='先安装开发测试包，然后从第一轮开始。游戏由你正常启动。')
        self.result = tk.StringVar(value='请选择本轮结果')
        root.title('AC7MultiSP alpha2 · 快速测试')
        root.geometry('790x580')
        root.minsize(710, 540)
        root.protocol('WM_DELETE_WINDOW', self.close)
        notebook = ttk.Notebook(root)
        notebook.pack(fill='both', expand=True, padx=12, pady=10)
        quick, extended = ttk.Frame(notebook, padding=12), ttk.Frame(notebook, padding=12)
        notebook.add(quick, text='快速测试 · 5轮')
        notebook.add(extended, text='扩展测试 · 可选')
        ttk.Button(quick, text='安装／更新开发测试包', command=lambda: self.run('install-dev')).pack(anchor='w')
        ttk.Button(quick, text='重测第4轮（保留第1—3轮结果）', command=lambda: self.run('retry-four')).pack(anchor='w', pady=(8, 0))
        self.heading = ttk.Label(quick, font=('Microsoft YaHei UI', 15, 'bold'))
        self.heading.pack(anchor='w', pady=(18, 8))
        self.instructions = ttk.Label(quick, wraplength=720, justify='left')
        self.instructions.pack(fill='x', pady=8)
        self.next_button = ttk.Button(quick, command=self.next_round)
        self.next_button.pack(anchor='w', pady=12)
        row = ttk.Frame(quick)
        row.pack(fill='x')
        ttk.Label(row, text='本轮结果：').pack(side='left')
        ttk.Combobox(row, state='readonly', textvariable=self.result, width=22,
                     values=['请选择本轮结果', '通过', '有问题', '未完成']).pack(side='left')
        ttk.Label(quick, text='每轮只需：启动游戏 → 按上方提示观察 → 退出游戏 → 选结果并点下一轮。', wraplength=720).pack(anchor='w', pady=12)
        ttk.Label(extended, text='快速版未覆盖的细分项目保留在这里，按需要补测；不计作已经通过。', wraplength=700).pack(anchor='w', pady=8)
        self.case = tk.StringVar(value='T7：核心共享故障')
        ttk.Combobox(extended, state='readonly', textvariable=self.case, width=40,
                     values=[key+'：'+LABELS[key] for key in CASES]).pack(anchor='w', pady=12)
        ttk.Button(extended, text='保存现有日志并准备该项', command=self.extended).pack(anchor='w')
        ttk.Button(extended, text='保存本轮日志', command=lambda: self.run('archive')).pack(anchor='w', pady=10)
        ttk.Label(extended, text='U类仅模拟更新响应，不点击测试下载链接。每次选项都在完整重启后生效。', wraplength=700).pack(anchor='w', pady=10)
        bottom = ttk.Frame(root, padding=(12, 0, 12, 10))
        bottom.pack(fill='x')
        ttk.Button(bottom, text='结束测试并安装常规alpha2', command=lambda: self.run('install-normal')).pack(side='left')
        ttk.Button(bottom, text='恢复测试前alpha1', command=lambda: self.run('restore')).pack(side='left', padx=10)
        ttk.Button(bottom, text='打开结果目录', command=lambda: os.startfile(str(session))).pack(side='right')
        ttk.Label(root, textvariable=self.status, wraplength=755, padding=(12, 0, 12, 12)).pack(fill='x')
        self.render()
        root.after(150, self.poll)

    def close(self):
        if self.busy:
            messagebox.showinfo('正在保存', '请等待当前操作完成后再关闭面板。')
            return
        self.root.destroy()

    def render(self):
        index = self.progress['index']
        if index >= len(ROUNDS):
            self.heading.config(text='快速五轮记录已保存')
            self.instructions.config(text='请把出现问题的轮次及现象告诉我。未通过或未完成的轮次仍需处理。\n可以安装常规alpha2继续使用，也可以恢复测试前安装。')
            self.next_button.config(text='快速流程已结束', state='disabled')
        else:
            name, _, instructions = ROUNDS[index]
            self.heading.config(text=f'第{index+1}/5轮：{name}')
            self.instructions.config(text=instructions)
            self.next_button.config(text='保存结果并准备下一轮' if self.progress['armed'] else '准备这一轮', state='normal')

    def invoke(self, action, case=None):
        command = [PYTHON, '-X', 'utf8', str(self.control), '--session', str(self.session), action]
        if case is not None:
            command += ['--case', case]
        result = subprocess.run(command, capture_output=True, text=True, encoding='utf8',
                                creationflags=subprocess.CREATE_NO_WINDOW)
        require(result.returncode == 0, result.stderr.strip() or result.stdout.strip() or '操作未完成。')
        return result.stdout.strip()

    def ensure_session(self):
        if not (self.session/'session.json').exists():
            prepare = self.session.parent/'20261028-alpha2-integration/prepare_environment.py'
            result = subprocess.run([PYTHON, '-X', 'utf8', str(prepare)], capture_output=True,
                                    text=True, encoding='utf8', creationflags=subprocess.CREATE_NO_WINDOW)
            require(result.returncode == 0, result.stderr.strip() or result.stdout.strip())
        return read(self.session/'session.json')

    def ensure_developer(self, data):
        mod = Path(data['game'])/'ue4ss/Mods/DualSP'
        require(digest(mod/'bin/DualSP.dll') == data['development_core_sha256'] and
                digest(mod/'bin/DualSPUI.dll') == data['development_ui_sha256'],
                '请先点击“安装／更新开发测试包”，完成安装后再准备本轮。')
        if data.get('development_main_lua_sha256'):
            require(digest(mod/'Scripts/main.lua') == data['development_main_lua_sha256'],
                    '当前Lua尚未包含T6修正，请先完成开发测试包更新安装。')

    def save_progress(self, progress):
        history = self.session/'progress-history'
        history.mkdir(exist_ok=True)
        stamp = datetime.now().strftime('%Y%m%d-%H%M%S-%f')
        if self.progress_file.exists():
            os.rename(self.progress_file, history/(stamp+'.json'))
        write(self.progress_file, progress)
        summary = self.session/'快速测试结果.csv'
        if summary.exists():
            os.rename(summary, history/(stamp+'.csv'))
        with summary.open('x', encoding='utf-8-sig', newline='') as file:
            writer = csv.writer(file)
            writer.writerow(['轮次', '内容', '结果', '日志目录'])
            records = {item['round']: item for item in progress['results']}
            for index, (name, _, _) in enumerate(ROUNDS, 1):
                item = records.get(index, {})
                writer.writerow([index, name, item.get('result', '未执行'), item.get('directory', '')])

    def archive_result(self, progress, result):
        output = self.invoke('archive')
        latest = max((p for p in (self.session/'results').iterdir() if p.is_dir() and (p/'result.json').exists()), key=lambda p: p.name)
        file = latest/'result.json'
        record = read(file)
        record.update({'quick_round': progress['index']+1, 'name': ROUNDS[progress['index']][0], 'result': result})
        os.rename(file, latest/'result-before-user-entry.json')
        write(file, record)
        progress['results'].append({'round': progress['index']+1, 'result': result, 'directory': str(latest)})
        return output

    def next_round(self):
        if self.progress['armed'] and self.result.get() == '请选择本轮结果':
            messagebox.showinfo('填写结果', '先选择“通过”“有问题”或“未完成”，再继续。')
            return
        self.run('next', self.result.get())

    def extended(self):
        if self.progress['armed']:
            messagebox.showinfo('先完成快速流程', '请先完成快速五轮，再选择扩展项目；也可在开始快速流程前单独选择。')
            return
        self.run('extended', self.case.get().split('：', 1)[0])

    def run(self, action, value=None):
        if self.busy:
            return
        self.busy = True
        self.status.set('正在处理，请稍候；不会自动启动游戏。')
        progress = copy.deepcopy(self.progress)

        def worker():
            try:
                closed()
                data = self.ensure_session()
                if action.startswith('install-'):
                    mode = 'dev' if action == 'install-dev' else 'normal'
                    receipt_path = data.get('development_delivery_receipt') if mode == 'dev' else None
                    receipt = read(Path(receipt_path) if receipt_path else self.session.parent/f'20261028-alpha2-{mode}-build/delivery-receipt.json')
                    exe = Path(data['development_exe'] if mode == 'dev' else data['normal_exe'])
                    require(digest(exe) == receipt['artifact']['sha256'], '安装包摘要不符，请联系维护者。')
                    if mode == 'normal':
                        self.invoke('archive')
                        self.invoke('select', 'off')
                    os.startfile(str(exe))
                    message = '已打开安装器。完成安装后，再准备第一轮。' if mode == 'dev' else '已关闭人工故障并打开常规安装器。'
                elif action == 'retry-four':
                    self.ensure_developer(data)
                    require(all(any(item['round'] == number and item['result'] == '通过' for item in progress['results'])
                                for number in (1, 2, 3)), '第1—3轮尚未全部记录通过，请先完成快速流程。')
                    self.invoke('archive')
                    self.invoke('select', 'T6')
                    progress['index'] = 3
                    progress['armed'] = True
                    self.save_progress(progress)
                    message = '已准备修正版第4轮。前3轮结果保留；现在请启动游戏，进入F-16C选装页。'
                elif action == 'next':
                    self.ensure_developer(data)
                    if progress['armed']:
                        self.archive_result(progress, value)
                        progress['index'] += 1
                        progress['armed'] = False
                        self.save_progress(progress)
                    if progress['index'] < len(ROUNDS):
                        self.invoke('select', ROUNDS[progress['index']][1])
                        progress['armed'] = True
                        self.save_progress(progress)
                        message = '已保存上一轮并准备本轮。现在请正常启动游戏，按面板提示观察。' if progress['index'] else '第一轮已准备。现在请正常启动游戏。'
                    else:
                        self.invoke('select', 'off')
                        message = '五轮记录已保存，人工故障已关闭。请反馈未通过的轮次。'
                elif action == 'extended':
                    self.ensure_developer(data)
                    self.invoke('archive')
                    self.invoke('select', value)
                    message = '扩展用例已准备：'+LABELS[value]+'。现在请正常启动游戏。'
                else:
                    message = self.invoke(action)
                self.events.put((True, message, progress))
            except Exception as error:
                self.events.put((False, str(error), None))
        threading.Thread(target=worker, daemon=True).start()

    def poll(self):
        try:
            success, message, progress = self.events.get_nowait()
            self.busy = False
            self.status.set(message)
            if success:
                self.progress = progress
                self.result.set('请选择本轮结果')
                self.render()
            else:
                # 上一步已经保存时，以磁盘进度为准，不重复登记同一轮结果。
                if self.progress_file.exists():
                    self.progress = read(self.progress_file)
                    self.render()
                messagebox.showerror('操作尚未完成', message)
        except queue.Empty:
            pass
        self.root.after(150, self.poll)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--session', required=True, type=Path)
    args = parser.parse_args()
    root = tk.Tk()
    session = args.session.resolve()
    require(session.is_dir(), '测试目录不存在。')
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.CreateFileW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32,
                                   ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
    kernel.CreateFileW.restype = ctypes.c_void_p
    kernel.CloseHandle.argtypes = [ctypes.c_void_p]
    handle = kernel.CreateFileW(str(session/'panel.lock'), 0xC0000000, 0, None, 4, 0x80, None)
    if handle in (None, ctypes.c_void_p(-1).value):
        messagebox.showinfo('测试面板', '面板已经打开，请使用现有窗口。')
        root.destroy()
        return
    atexit.register(kernel.CloseHandle, handle)
    Panel(root, session)
    root.mainloop()


if __name__ == '__main__':
    main()
