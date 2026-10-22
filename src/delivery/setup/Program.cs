using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Net;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;

namespace AC7MultiSP.Setup {
    internal sealed class UpdateLink {
        public string id { get; set; }
        public string url { get; set; }
        public string format { get; set; }
        public Dictionary<string, string> label { get; set; }
    }
    internal sealed class UpdateInfo {
        public int schema { get; set; }
        public string product { get; set; }
        public string channel { get; set; }
        public string status { get; set; }
        public string latest_version { get; set; }
        public Dictionary<string, string> release_notes { get; set; }
        public List<UpdateLink> downloads { get; set; }
        public List<UpdateLink> installer_downloads { get; set; }
    }
    internal sealed class MainWindow : Form {
        readonly Bundle bundle;
        readonly ComboBox game = new ComboBox { Dock = DockStyle.Fill, DropDownStyle = ComboBoxStyle.DropDown };
        readonly Label summary = new Label { Dock = DockStyle.Fill, AutoSize = true };
        readonly Label updateStatus = new Label { Dock = DockStyle.Fill, AutoSize = true, Text = "正在检查新版，不影响离线安装。" };
        readonly TextBox details = new TextBox { Multiline = true, ReadOnly = true, ScrollBars = ScrollBars.Vertical, Dock = DockStyle.Fill, BackColor = SystemColors.Window };
        readonly Button install = new Button { Text = "安装 / 升级", AutoSize = true, Height = 38 };
        readonly Button check = new Button { Text = "重新检测", AutoSize = true };
        readonly Button browse = new Button { Text = "选择游戏目录…", AutoSize = true };
        readonly Button recover = new Button { Text = "恢复上次中断", AutoSize = true };
        readonly Button remove = new Button { Text = "卸载", AutoSize = true };
        readonly Button export = new Button { Text = "导出通用 ZIP", AutoSize = true };
        readonly Button latest = new Button { Text = "下载最新发布包", AutoSize = true, Enabled = false };
        readonly Button recheck = new Button { Text = "检测更新", AutoSize = true };
        UpdateInfo update;
        bool busy;
        bool closing;
        internal MainWindow(Bundle value) {
            bundle = value;
            Text = "AC7MultiSP 安装与维护（交付预览 " + bundle.Info.delivery_version + "）";
            Font = new Font("Microsoft YaHei UI", 10F);
            ClientSize = new Size(860, 660); MinimumSize = new Size(680, 540);
            StartPosition = FormStartPosition.CenterScreen; AutoScaleMode = AutoScaleMode.Dpi;
            var layout = new TableLayoutPanel { Dock = DockStyle.Fill, Padding = new Padding(20), ColumnCount = 1, RowCount = 7 };
            foreach (var style in new[] { new RowStyle(SizeType.Absolute, 44), new RowStyle(SizeType.Absolute, 40),
                new RowStyle(SizeType.Absolute, 58), new RowStyle(SizeType.Absolute, 42), new RowStyle(SizeType.Absolute, 64),
                new RowStyle(SizeType.Percent, 100), new RowStyle(SizeType.Absolute, 46) }) layout.RowStyles.Add(style);
            layout.Controls.Add(new Label { Text = "AC7MultiSP  " + bundle.Info.version, Font = new Font(Font.FontFamily, 19F, FontStyle.Bold), AutoSize = true }, 0, 0);
            var location = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2, RowCount = 1 };
            location.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100)); location.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            location.Controls.Add(game, 0, 0); location.Controls.Add(browse, 1, 0); layout.Controls.Add(location, 0, 1);
            layout.Controls.Add(summary, 0, 2);
            var action = new FlowLayoutPanel { Dock = DockStyle.Fill };
            action.Controls.Add(install); action.Controls.Add(check); action.Controls.Add(export); action.Controls.Add(remove); action.Controls.Add(recover);
            layout.Controls.Add(action, 0, 3);
            var updateBox = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 2 };
            updateBox.Controls.Add(updateStatus, 0, 0);
            var updateButtons = new FlowLayoutPanel { Dock = DockStyle.Fill };
            updateButtons.Controls.Add(Button("前往 Nexus / Vortex 更新", delegate { Open(Convert.ToString(bundle.Info.policy["nexus_url"])); }));
            updateButtons.Controls.Add(latest); updateButtons.Controls.Add(recheck);
            updateBox.Controls.Add(updateButtons, 0, 1); layout.Controls.Add(updateBox, 0, 4);
            var tabs = new TabControl { Dock = DockStyle.Fill };
            AddTab(tabs, "状态", details);
            AddTextTab(tabs, "功能与操作", bundle.Document("README_zh-CN.txt"));
            AddTextTab(tabs, "版本记录", bundle.Document("版本说明.txt"));
            AddTextTab(tabs, "作者与授权", "作者：NexusMod Northter / bilibili 班德慕\r\n邮箱：karenorthterofchina@gmail.com\r\n测试反馈QQ群：1072924682\r\n\r\n" + Credits());
            layout.Controls.Add(tabs, 0, 5);
            var bottom = new FlowLayoutPanel { Dock = DockStyle.Fill };
            bottom.Controls.Add(Button("VC++ 官方下载", delegate { Open(Dependency("vc_url")); }));
            bottom.Controls.Add(Button("DirectX 官方下载", delegate { Open(Dependency("directx_url")); }));
            bottom.Controls.Add(new Label { Text = "退出游戏后安装；个人设置保留。", AutoSize = true, Padding = new Padding(8) });
            layout.Controls.Add(bottom, 0, 6); Controls.Add(layout);
            browse.Click += delegate { using (var dialog = new FolderBrowserDialog { Description = "选择含 Ace7Game.exe 的游戏目录", ShowNewFolderButton = false })
                if (dialog.ShowDialog(this) == DialogResult.OK) { game.Text = dialog.SelectedPath; RefreshLocal(); } };
            check.Click += delegate { RefreshLocal(); };
            install.Click += async delegate { await Work(false); };
            remove.Click += async delegate { await Work(true); };
            recover.Click += async delegate { await Recover(); };
            export.Click += async delegate { await Export(); };
            latest.Click += delegate { ChooseDownload(); };
            recheck.Click += async delegate { await CheckUpdate(); };
            FormClosing += delegate(object sender, FormClosingEventArgs e) { if (busy) { e.Cancel = true; MessageBox.Show(this, "正在处理文件，请等待操作或恢复完成。", Text); } else closing = true; };
            Shown += async delegate {
                try { foreach (string found in Bundle.FindGames()) game.Items.Add(found); if (game.Items.Count != 0) game.SelectedIndex = 0; RefreshLocal(); }
                catch (Exception failure) { details.Text = failure.Message; }
                await CheckUpdate();
            };
        }
        string Dependency(string name) { return Convert.ToString(((Dictionary<string, object>)bundle.Info.policy["dependencies"])[name]); }
        string Credits() {
            return String.Join("\r\n\r\n", bundle.Info.files.Keys.Where(n => n.StartsWith(Bundle.Mod + "licenses/", StringComparison.Ordinal))
                .Select(n => Path.GetFileName(n) + "\r\n" + Bundle.Utf8.GetString(bundle.Bytes(n))));
        }
        static Button Button(string name, Action action) { var button = new Button { Text = name, AutoSize = true }; button.Click += delegate { action(); }; return button; }
        static void AddTab(TabControl tabs, string name, Control content) { var tab = new TabPage(name); tab.Controls.Add(content); tabs.TabPages.Add(tab); }
        static void AddTextTab(TabControl tabs, string name, string text) { AddTab(tabs, name, new TextBox { Text = text.Replace("\r\n", "\n").Replace("\n", "\r\n"), Multiline = true, ReadOnly = true, ScrollBars = ScrollBars.Vertical, Dock = DockStyle.Fill, BackColor = SystemColors.Window }); }
        void SetBusy(bool value) {
            busy = value;
            foreach (Control control in new Control[] { game, install, check, browse, remove, export, recover }) control.Enabled = !value;
        }
        void RefreshLocal() {
            if (busy) return;
            string current = "未选择游戏目录";
            try {
                string root = Bundle.GameRoot(game.Text);
                string file = Bundle.Safe(root, Bundle.Mod + "VERSION");
                current = File.Exists(file) ? "已安装：" + File.ReadAllText(file).Trim() : "尚未安装 MultiSP";
                install.Text = File.Exists(file) && File.ReadAllText(file).Trim() == bundle.Info.version ? "检查并修复" : "安装 / 升级";
                string missing = Bundle.Dependencies(root);
                summary.Text = current + "　　本包：" + bundle.Info.version + "\r\n" + (missing.Length == 0 ? "运行库初步检测通过。" : "缺少组件：" + missing);
                details.Text = "点击主按钮后自动检测加载器、安装来源及文件状态。\r\n已由 Vortex 管理时，请继续通过 Vortex 更新。\r\n内含完整原始资源，离线也可安装和导出。";
            } catch (Exception failure) { summary.Text = "本包：" + bundle.Info.version + "　　" + current; details.Text = failure.Message; }
        }
        async Task Work(bool uninstall) {
            if (busy) return;
            if (uninstall && MessageBox.Show(this, "卸载本安装器拥有的 Mod 文件？文件将移入备份，个人设置和共享 UE4SS 保留。", "卸载 MultiSP", MessageBoxButtons.OKCancel) != DialogResult.OK) return;
            SetBusy(true);
            try {
                string input = game.Text;
                string backup = await Task.Run(delegate {
                    var plan = bundle.MakePlan(input, uninstall);
                    return bundle.Apply(plan, message => { if (!closing) BeginInvoke((Action)(() => details.Text = message)); });
                });
                details.Text = (uninstall ? "已卸载。" : "安装完成，可照常从 Steam 启动游戏。") + "\r\n备份：" + backup;
                summary.Text = uninstall ? "MultiSP 已卸载，个人设置和共享前置保留。" : "已安装：" + bundle.Info.version;
            } catch (Exception failure) { details.Text = failure.Message; MessageBox.Show(this, failure.Message, "操作未完成", MessageBoxButtons.OK, MessageBoxIcon.Information); }
            finally { SetBusy(false); }
        }
        async Task Recover() {
            if (busy) return; SetBusy(true);
            try {
                string input = game.Text;
                await Task.Run(delegate {
                    string root = Bundle.GameRoot(input); Bundle.Closed(); bundle.CheckManager(root);
                    using (var locked = new FileStream(Bundle.Safe(root, "AC7MultiSP-install.lock"), FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.None)) {
                        var pending = bundle.Incomplete(root);
                        if (pending.Count != 1) throw new IOException(pending.Count == 0 ? "没有未完成操作。" : "发现多份未完成记录，请先人工核对。");
                        bundle.Restore(root, pending[0]);
                    }
                });
                details.Text = "已恢复上次中断前的文件，可以重新安装。";
            } catch (Exception failure) { details.Text = failure.Message; }
            finally { SetBusy(false); }
        }
        async Task Export() {
            if (busy) return;
            using (var dialog = new SaveFileDialog { FileName = bundle.Info.zip_name, Filter = "ZIP 安装包|*.zip", OverwritePrompt = true }) {
                if (dialog.ShowDialog(this) != DialogResult.OK) return;
                if (File.Exists(dialog.FileName)) { MessageBox.Show(this, "请选择新文件名，以保留已有安装包。"); return; }
                SetBusy(true);
                try { string target = dialog.FileName; await Task.Run(() => bundle.Export(target)); details.Text = "已导出本 EXE 内含的 " + bundle.Info.version + " 通用主包。\r\n可手动安装或交给 Vortex；ZIP 不含共享 UE4SS，详见包内说明。"; }
                catch (Exception failure) { details.Text = "导出未完成，请保留现场：" + failure.Message; }
                finally { SetBusy(false); }
            }
        }
        static bool Allowed(Uri uri) {
            return uri.Scheme == "https" && uri.Port == 443 && String.IsNullOrEmpty(uri.UserInfo) && new[] {
                "www.nexusmods.com", "ac7multisp-updates.pages.dev", "downloads.iot-node-service-api.online", "wwbru.lanzoue.com", "learn.microsoft.com", "www.microsoft.com"
            }.Contains(uri.Host.ToLowerInvariant());
        }
        void Open(string value) {
            Uri uri;
            if (!Uri.TryCreate(value, UriKind.Absolute, out uri) || !Allowed(uri)) { MessageBox.Show(this, "下载地址无效或尚未登记。"); return; }
            try { Process.Start(new ProcessStartInfo(uri.AbsoluteUri) { UseShellExecute = true }); }
            catch (Exception failure) { MessageBox.Show(this, "浏览器未打开：" + failure.Message); }
        }
        static UpdateInfo FetchUpdate(string url) {
            ServicePointManager.SecurityProtocol = SecurityProtocolType.Tls12;
            var request = (HttpWebRequest)WebRequest.Create(url);
            request.AllowAutoRedirect = false; request.Timeout = 5000; request.ReadWriteTimeout = 5000;
            request.Accept = "application/json"; request.UserAgent = "AC7MultiSP-Setup/0.1.0";
            request.Credentials = null; request.CookieContainer = null;
            using (var response = (HttpWebResponse)request.GetResponse()) {
                if (response.StatusCode != HttpStatusCode.OK || response.ContentLength > 32768 || response.ContentType == null || !response.ContentType.StartsWith("application/json", StringComparison.OrdinalIgnoreCase)) throw new IOException("更新清单响应无效。");
                using (var stream = response.GetResponseStream()) using (var buffer = new MemoryStream()) {
                    byte[] part = new byte[4096]; int count; var watch = Stopwatch.StartNew();
                    while ((count = stream.Read(part, 0, part.Length)) != 0) {
                        if (watch.ElapsedMilliseconds > 8000 || buffer.Length + count > 32768) throw new IOException("更新清单超时或过大。");
                        buffer.Write(part, 0, count);
                    }
                    var data = new JavaScriptSerializer { MaxJsonLength = 32768, RecursionLimit = 10 }.Deserialize<UpdateInfo>(Bundle.Utf8.GetString(buffer.ToArray()));
                    if (data.schema != 1 || data.product != "AC7MultiSP" || data.channel != "stable") throw new IOException("更新清单不匹配。");
                    if (data.status == "preparing") return data;
                    if (data.status != "ready" || !Regex.IsMatch(data.latest_version ?? "", @"^\d+\.\d+\.\d+$")) throw new IOException("更新版本无效。");
                    if (data.downloads == null || data.downloads.Count == 0 || data.downloads.Count > 4 ||
                        data.installer_downloads != null && (data.installer_downloads.Count > 4 || data.installer_downloads.Any(link => link.format != "exe")))
                        throw new IOException("更新下载类型或数量无效。");
                    foreach (var entry in (data.downloads ?? new List<UpdateLink>()).Concat(data.installer_downloads ?? new List<UpdateLink>())) {
                        Uri uri;
                        if (!Uri.TryCreate(entry.url, UriKind.Absolute, out uri) || !Allowed(uri)) throw new IOException("更新链接无效。");
                    }
                    return data;
                }
            }
        }
        async Task CheckUpdate() {
            if (!recheck.Enabled) return;
            recheck.Enabled = false;
            try {
                var data = await Task.Run(() => FetchUpdate(Convert.ToString(bundle.Info.policy["updates_url"])));
                if (closing) return;
                update = data;
                if (data.status == "preparing") { updateStatus.Text = "更新信息尚未发布，可继续离线安装。"; latest.Enabled = false; return; }
                bool newer = new Version(data.latest_version) > new Version(bundle.Info.version);
                updateStatus.Text = newer ? "发现新版 " + data.latest_version + "：Vortex 用户在 Vortex 更新；其他用户下载最新安装包。" : "最新功能版本：" + data.latest_version;
                bool exe = data.installer_downloads != null && data.installer_downloads.Count != 0;
                latest.Text = exe ? "下载最新安装包" : "下载最新发布 ZIP";
                latest.Enabled = exe || data.downloads != null && data.downloads.Count != 0;
            } catch (Exception failure) { if (!closing) updateStatus.Text = "暂时无法检查更新，可继续离线安装。"; Debug.WriteLine(failure); }
            finally { if (!closing) recheck.Enabled = true; }
        }
        void ChooseDownload() {
            if (update == null) return;
            var links = update.installer_downloads != null && update.installer_downloads.Count != 0 ? update.installer_downloads : update.downloads;
            if (links == null || links.Count == 0) return;
            var menu = new ContextMenuStrip();
            foreach (var link in links) {
                var selected = link;
                string label = link.label != null && link.label.ContainsKey("zh") ? link.label["zh"] : link.id;
                menu.Items.Add(label, null, delegate { Open(selected.url); });
            }
            menu.Show(latest, new Point(0, latest.Height));
        }
    }
    internal static class Program {
        [STAThread] static void Main() {
            Application.EnableVisualStyles(); Application.SetCompatibleTextRenderingDefault(false);
            try { Application.Run(new MainWindow(new Bundle())); }
            catch (Exception failure) { MessageBox.Show("安装器未能启动：" + failure.Message, "AC7MultiSP", MessageBoxButtons.OK, MessageBoxIcon.Error); }
        }
    }
}
