using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace AC7MultiSP.Setup {
    // 保留标准按钮的键盘操作，标题与说明分开绘制。
    internal sealed class ChoiceButton : Button {
        internal string Heading;
        internal string Description;
        internal ChoiceButton(string heading, string description) {
            Heading = heading; Description = description; AccessibleName = heading;
            AccessibleDescription = description; Text = ""; Dock = DockStyle.Fill;
            FlatStyle = FlatStyle.Flat; BackColor = Color.White; ForeColor = Color.FromArgb(35, 55, 75);
            FlatAppearance.BorderColor = Color.FromArgb(165, 183, 200); Cursor = Cursors.Hand;
        }
        protected override void OnPaint(PaintEventArgs e) {
            base.OnPaint(e);
            using (var headingFont = new Font(Font.FontFamily, 15F, FontStyle.Bold))
            using (var descriptionFont = new Font(Font.FontFamily, 10F)) {
                int pad = Math.Max(16, Font.Height);
                TextRenderer.DrawText(e.Graphics, Heading, headingFont,
                    new Rectangle(pad, pad, Width - pad * 2, headingFont.Height + 10), ForeColor,
                    TextFormatFlags.Left | TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis);
                TextRenderer.DrawText(e.Graphics, Description, descriptionFont,
                    new Rectangle(pad, pad + headingFont.Height + 16, Width - pad * 2, Height - pad * 2 - headingFont.Height - 16),
                    Color.FromArgb(85, 100, 115), TextFormatFlags.Left | TextFormatFlags.WordBreak);
            }
        }
    }

    internal sealed class MainWindow : Form {
        readonly Bundle bundle;
        readonly Panel content = new Panel { Dock = DockStyle.Fill };
        readonly Label state = new Label { Dock = DockStyle.Fill, AutoSize = true };
        readonly LinkLabel location = new LinkLabel { Dock = DockStyle.Fill, AutoSize = true };
        readonly Button install = new Button { Text = "安装", AutoSize = true, MinimumSize = new Size(140, 42) };
        readonly Button uninstall = new Button { Text = "卸载", AutoSize = true, MinimumSize = new Size(140, 42) };
        readonly ProgressBar progress = new ProgressBar { Dock = DockStyle.Fill, Style = ProgressBarStyle.Marquee, Visible = false };
        string game;
        bool busy;
        bool closing;

        internal MainWindow(Bundle value) {
            bundle = value;
            Text = "AC7MultiSP/皇牌空战7三SP武器模组";
            Font = new Font("Microsoft YaHei UI", 10F);
            ClientSize = new Size(900, 640); MinimumSize = new Size(760, 580);
            AutoScaleMode = AutoScaleMode.Dpi; StartPosition = FormStartPosition.CenterScreen;
            BackColor = Color.FromArgb(245, 248, 251);
            var layout = new TableLayoutPanel { Dock = DockStyle.Fill, Padding = new Padding(24), ColumnCount = 1, RowCount = 2 };
            layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 100));
            layout.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            var header = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2, RowCount = 1 };
            header.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 52));
            header.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 48));
            header.Controls.Add(new Label { Text = "AC7MultiSP\r\n皇牌空战7三SP武器模组", Dock = DockStyle.Fill,
                Font = new Font(Font.FontFamily, 17F, FontStyle.Bold), AutoSize = true }, 0, 0);
            header.Controls.Add(new Label { Text = "NexusMods：Northter  /  bilibili：班德慕\r\n" +
                "karenorthterofchina@gmail.com\r\n反馈QQ群：1072924682", TextAlign = ContentAlignment.TopRight,
                Dock = DockStyle.Fill, Font = new Font(Font.FontFamily, 9F), AutoSize = true }, 1, 0);
            layout.Controls.Add(header, 0, 0); layout.Controls.Add(content, 0, 1); Controls.Add(layout);
            install.Click += async delegate { await Work(false); };
            uninstall.Click += async delegate { await Work(true); };
            location.LinkClicked += delegate { if (!busy && SelectGame()) RefreshLocal(); };
            FormClosing += delegate(object sender, FormClosingEventArgs e) {
                if (busy) { e.Cancel = true; return; }
                closing = true;
            };
            ShowWaiting("正在检查更新…");
            Shown += async delegate { await Start(); };
        }

        void ShowScreen(Control page) {
            foreach (Control child in content.Controls.Cast<Control>().ToArray()) { content.Controls.Remove(child); child.Dispose(); }
            page.Dock = DockStyle.Fill; content.Controls.Add(page);
        }
        void ShowWaiting(string message) {
            ShowScreen(new Label { Text = message, TextAlign = ContentAlignment.MiddleCenter,
                Font = new Font(Font.FontFamily, 14F) });
        }
        async Task Start() {
            try {
                var update = await Task.Run(() => FetchUpdate(Convert.ToString(bundle.Info.policy["updates_url"])));
                if (closing) return;
                if (update.status == "ready" && new Version(update.latest_version) > new Version(bundle.Info.version)) {
                    ShowUpdate(update); return;
                }
            } catch (Exception failure) { Debug.WriteLine(failure); }
            if (!closing) ShowChoices();
        }
        // 更新读取失败时继续使用已内嵌资源，入口页仍保持两个选项。
        static UpdateInfo FetchUpdate(string url) { return UpdateFeed.Fetch(url); }
        void ShowUpdate(UpdateInfo update) {
            var page = new TableLayoutPanel { ColumnCount = 1, RowCount = 3, Padding = new Padding(30, 80, 30, 40) };
            page.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            page.RowStyles.Add(new RowStyle(SizeType.Absolute, 64));
            page.RowStyles.Add(new RowStyle(SizeType.Absolute, 64));
            page.Controls.Add(new Label { Text = "发现新版本 " + update.latest_version + "\r\n当前安装包：" + bundle.Info.version,
                Dock = DockStyle.Fill, TextAlign = ContentAlignment.MiddleCenter, Font = new Font(Font.FontFamily, 14F) }, 0, 0);
            var latest = new Button { Text = "下载最新版本", Dock = DockStyle.Fill, Margin = new Padding(70, 8, 70, 8) };
            var ignore = new Button { Text = "忽略此版本", Dock = DockStyle.Fill, Margin = new Padding(70, 8, 70, 8) };
            latest.Click += delegate {
                // 清单尚无 EXE 时前往真实发布页，不能将旧 ZIP 冒充 EXE。
                string url = update.installer_downloads != null && update.installer_downloads.Count != 0
                    ? update.installer_downloads[0].url : Convert.ToString(bundle.Info.policy["nexus_url"]);
                if (Open(url)) Close();
            };
            ignore.Click += delegate { ShowChoices(); };
            page.Controls.Add(latest, 0, 1); page.Controls.Add(ignore, 0, 2); ShowScreen(page);
        }
        void ShowChoices() {
            var page = new TableLayoutPanel { ColumnCount = 1, RowCount = 4, Padding = new Padding(38, 24, 38, 24) };
            page.RowStyles.Add(new RowStyle(SizeType.Percent, 50));
            page.RowStyles.Add(new RowStyle(SizeType.Absolute, 140));
            page.RowStyles.Add(new RowStyle(SizeType.Absolute, 140));
            page.RowStyles.Add(new RowStyle(SizeType.Percent, 50));
            var direct = new ChoiceButton("直接安装（推荐）", "直接安装到皇牌空战7游戏目录中，自动处理兼容的 UE4SS 环境和本 Mod 文件；保留其他 Mod 与个人设置。");
            var vortex = new ChoiceButton("通过 Vortex 安装和更新", "导出本 Mod 的 ZIP 资源文件，在 Vortex 中安装，由 Vortex 进行管理和更新。");
            direct.Margin = new Padding(0, 0, 0, 16); vortex.Margin = new Padding(0, 0, 0, 0);
            direct.Click += async delegate { await EnterDirect(); };
            vortex.Click += async delegate { await Export(); };
            page.Controls.Add(direct, 0, 1); page.Controls.Add(vortex, 0, 2); ShowScreen(page);
        }
        async Task EnterDirect() {
            ShowWaiting("正在查找游戏目录…");
            try {
                var games = await Task.Run(() => Bundle.FindGames().ToArray());
                if (closing) return;
                foreach (string candidate in games) {
                    try { game = Bundle.GameRoot(candidate); break; } catch (IOException) {}
                }
            } catch (Exception failure) { Debug.WriteLine(failure); }
            if (closing) return;
            if (String.IsNullOrEmpty(game)) SelectGame();
            var page = new TableLayoutPanel { ColumnCount = 1, RowCount = 5 };
            page.RowStyles.Add(new RowStyle(SizeType.Absolute, 42));
            page.RowStyles.Add(new RowStyle(SizeType.Absolute, 68));
            page.RowStyles.Add(new RowStyle(SizeType.Absolute, 60));
            page.RowStyles.Add(new RowStyle(SizeType.Absolute, 20));
            page.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            page.Controls.Add(location, 0, 0); page.Controls.Add(state, 0, 1);
            var actions = new FlowLayoutPanel { Dock = DockStyle.Fill };
            actions.Controls.Add(install); actions.Controls.Add(uninstall); page.Controls.Add(actions, 0, 2);
            page.Controls.Add(progress, 0, 3);
            var tabs = new TabControl { Dock = DockStyle.Fill };
            AddTextTab(tabs, "更新日志", bundle.Document("版本说明.txt"));
            AddTextTab(tabs, "授权信息", Credits());
            page.Controls.Add(tabs, 0, 4); ShowScreen(page); RefreshLocal();
        }
        bool SelectGame() {
            using (var dialog = new FolderBrowserDialog { Description = "请选择含 Ace7Game.exe 的皇牌空战7游戏目录", ShowNewFolderButton = false }) {
                if (dialog.ShowDialog(this) != DialogResult.OK) return false;
                try { game = Bundle.GameRoot(dialog.SelectedPath); return true; }
                catch (Exception failure) { MessageBox.Show(this, failure.Message, "请选择游戏目录"); return false; }
            }
        }
        void RefreshLocal() {
            install.Enabled = false; uninstall.Enabled = false;
            location.Text = String.IsNullOrEmpty(game) ? "选择游戏目录" : "游戏目录：" + game + "　（更改）";
            try {
                if (String.IsNullOrEmpty(game)) { state.Text = "请选择游戏目录。"; return; }
                game = Bundle.GameRoot(game); bundle.CheckManager(game);
                string file = Bundle.Safe(game, Bundle.Mod + "VERSION");
                string current = File.Exists(file) ? File.ReadAllText(file).Trim() : null;
                state.Text = "本安装包：" + bundle.Info.version + "\r\n" + (current == null ? "尚未安装，点击“安装”即可。" : "已安装：" + current);
                install.Text = current != null && current != bundle.Info.version ? "更新" : "安装";
                install.Enabled = true; uninstall.Enabled = bundle.Previous(game) != null;
            } catch (Exception failure) { state.Text = failure.Message; }
        }
        void SetBusy(bool value) {
            busy = value; install.Enabled = !value; uninstall.Enabled = !value;
            location.Enabled = !value; progress.Visible = value;
        }
        async Task Work(bool remove) {
            if (busy || String.IsNullOrEmpty(game)) return;
            if (remove && MessageBox.Show(this, "卸载本 Mod？个人设置和共享 UE4SS 将保留。", "卸载", MessageBoxButtons.OKCancel) != DialogResult.OK) return;
            SetBusy(true); state.Text = remove ? "正在卸载，请稍候…" : "正在安装，请稍候…";
            try {
                string selected = game;
                await Task.Run(delegate {
                    Bundle.Closed(); bundle.CheckManager(selected);
                    // 将恢复并入主操作，不给用户增加常驻维护按钮。
                    using (var locked = new FileStream(Bundle.Safe(selected, "AC7MultiSP-install.lock"), FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.None)) {
                        var pending = bundle.Incomplete(selected);
                        if (pending.Count > 1) throw new IOException("发现多份未完成安装记录，请保留备份并反馈。");
                        if (pending.Count == 1) bundle.Restore(selected, pending[0]);
                    }
                    bundle.Apply(bundle.MakePlan(selected, remove), message => {});
                });
                SetBusy(false); RefreshLocal();
                state.Text = remove ? "卸载完成，个人设置与共享 UE4SS 已保留。" : "安装完成（" + bundle.Info.version + "），可照常从 Steam 启动游戏。";
            } catch (Exception failure) {
                SetBusy(false); RefreshLocal(); state.Text = failure.Message;
                MessageBox.Show(this, failure.Message, "操作未完成", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
        }
        async Task Export() {
            if (busy) return;
            string target;
            using (var dialog = new FolderBrowserDialog { Description = "选择 ZIP 导出目录", ShowNewFolderButton = true }) {
                if (dialog.ShowDialog(this) != DialogResult.OK) return;
                target = Path.Combine(dialog.SelectedPath, bundle.Info.zip_name);
                for (int n = 2; File.Exists(target); ++n)
                    target = Path.Combine(dialog.SelectedPath, Path.GetFileNameWithoutExtension(bundle.Info.zip_name) + " (" + n + ").zip");
            }
            busy = true; ShowWaiting("正在导出 ZIP…");
            try { await Task.Run(() => bundle.Export(target)); busy = false; Close(); }
            catch (Exception failure) { busy = false; ShowChoices(); MessageBox.Show(this, failure.Message, "导出未完成"); }
        }
        string Credits() {
            return Bundle.Utf8.GetString(bundle.Bytes("installer/授权.txt")).TrimStart('\uFEFF') + "\r\n\r\n" +
                String.Join("\r\n\r\n", bundle.Info.files.Keys.Where(n => n.StartsWith(Bundle.Mod + "licenses/", StringComparison.Ordinal))
                .Select(n => Path.GetFileName(n) + "\r\n" + Bundle.Utf8.GetString(bundle.Bytes(n))));
        }
        static void AddTextTab(TabControl tabs, string title, string text) {
            var tab = new TabPage(title);
            tab.Controls.Add(new TextBox { Text = text.Replace("\r\n", "\n").Replace("\n", "\r\n"), Multiline = true,
                ReadOnly = true, ScrollBars = ScrollBars.Vertical, Dock = DockStyle.Fill, BackColor = Color.White });
            tabs.TabPages.Add(tab);
        }
        bool Open(string value) {
            Uri uri;
            if (!Uri.TryCreate(value, UriKind.Absolute, out uri) || !UpdateFeed.Allowed(uri)) {
                MessageBox.Show(this, "下载地址无效或尚未登记。", "无法打开下载页面"); return false;
            }
            try { Process.Start(new ProcessStartInfo(uri.AbsoluteUri) { UseShellExecute = true }); return true; }
            catch (Exception failure) { MessageBox.Show(this, failure.Message, "无法打开下载页面"); return false; }
        }
    }
}
