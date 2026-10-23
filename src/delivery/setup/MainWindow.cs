using System;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace AC7MultiSP.Setup {
    // 保留标准按钮的键盘行为，按实际文字高度排列标题与说明。
    internal sealed class ChoiceButton : Button {
        readonly string heading;
        readonly string description;
        internal ChoiceButton(string title, string detail) {
            heading = title; description = detail;
            AccessibleName = title; AccessibleDescription = detail; Text = "";
            Dock = DockStyle.Fill; AutoSize = true; MinimumSize = new Size(0, 112);
            FlatStyle = FlatStyle.Flat; BackColor = Color.White; ForeColor = Color.FromArgb(35, 55, 75);
            FlatAppearance.BorderColor = Color.FromArgb(165, 183, 200); Cursor = Cursors.Hand;
        }
        public override Size GetPreferredSize(Size proposedSize) {
            int width = proposedSize.Width > 80 && proposedSize.Width < 10000 ? proposedSize.Width : Math.Max(200, Width);
            using (var titleFont = new Font(Font.FontFamily, 14F, FontStyle.Bold)) {
                int titleHeight = TextRenderer.MeasureText(heading, titleFont, new Size(Math.Max(80, width - 44), 0), TextFormatFlags.WordBreak).Height;
                int detailHeight = TextRenderer.MeasureText(description, Font, new Size(Math.Max(80, width - 44), 0), TextFormatFlags.WordBreak).Height;
                return new Size(width, Math.Max(MinimumSize.Height, titleHeight + detailHeight + 48));
            }
        }
        protected override void OnPaint(PaintEventArgs e) {
            base.OnPaint(e);
            using (var titleFont = new Font(Font.FontFamily, 14F, FontStyle.Bold)) {
                int pad = 20;
                int titleHeight = TextRenderer.MeasureText(heading, titleFont, new Size(Math.Max(80, Width - pad * 2), 0), TextFormatFlags.WordBreak).Height;
                TextRenderer.DrawText(e.Graphics, heading, titleFont, new Rectangle(pad, pad, Width - pad * 2, titleHeight), ForeColor, TextFormatFlags.Left | TextFormatFlags.WordBreak);
                TextRenderer.DrawText(e.Graphics, description, Font, new Rectangle(pad, pad + titleHeight + 8, Width - pad * 2, Height - pad * 2 - titleHeight - 8),
                    Color.FromArgb(85, 100, 115), TextFormatFlags.Left | TextFormatFlags.WordBreak);
            }
        }
    }

    internal sealed class MainWindow : Form {
        readonly Bundle bundle;
        readonly bool preview;
        readonly string snapshot;
        readonly TableLayoutPanel shell = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 2, Padding = new Padding(22) };
        readonly Panel content = new Panel { Dock = DockStyle.Fill };
        Control header;
        ComboBox language;
        LinkLabel credits;
        Label localState;
        Label feedback;
        TextBox location;
        Button install;
        Button uninstall;
        ProgressBar progress;
        TabControl documents;
        string game;
        string currentPage = "home";
        string selectedTopic = "features";
        string waitingKey = "working.checkUpdates";
        string feedbackKey;
        object[] feedbackArgs = new object[0];
        Exception feedbackFailure;
        UpdateInfo availableUpdate;
        string exported;
        bool busy;
        bool closing;
        bool userInteracted;
        internal Exception PreviewFailure { get; private set; }

        internal MainWindow(Bundle value, bool isPreview, string initialPage, string snapshotPath) {
            bundle = value; preview = isPreview; snapshot = snapshotPath;
            Font = new Font("Microsoft YaHei UI", 10F);
            ClientSize = new Size(980, 740); MinimumSize = new Size(840, 660);
            AutoScaleMode = AutoScaleMode.Dpi; StartPosition = FormStartPosition.CenterScreen;
            BackColor = Color.FromArgb(245, 248, 251);
            shell.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            shell.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            shell.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            shell.Controls.Add(content, 0, 1); Controls.Add(shell);
            currentPage = preview ? initialPage : "home";
            if (preview && initialPage == "direct") selectedTopic = "installation";
            Render();
            FormClosing += delegate(object sender, FormClosingEventArgs e) {
                if (busy) { e.Cancel = true; return; }
                closing = true;
            };
            Shown += async delegate {
                if (preview) {
                    // 预览仅绘制现有控件；不联网、不搜索或写入游戏目录。
                    var timer = new Timer { Interval = 200 };
                    timer.Tick += delegate {
                        timer.Stop(); timer.Dispose();
                        try {
                            SavePreviewSnapshot();
                        } catch (Exception failure) { PreviewFailure = failure; }
                        Close();
                    };
                    timer.Start();
                } else await Start();
            };
        }
        [StructLayout(LayoutKind.Sequential)]
        struct NativeRect { internal int Left, Top, Right, Bottom; }
        [StructLayout(LayoutKind.Sequential)]
        struct CharacterRange { internal int First, Last; }
        [StructLayout(LayoutKind.Sequential)]
        struct FormatRange {
            internal IntPtr Device, TargetDevice;
            internal NativeRect Area, Page;
            internal CharacterRange Characters;
        }
        [DllImport("user32.dll", EntryPoint = "SendMessageW")]
        static extern IntPtr RenderRichText(IntPtr window, int message, IntPtr render, ref FormatRange range);
        [DllImport("user32.dll", EntryPoint = "SendMessageW")]
        static extern IntPtr ReleaseRichText(IntPtr window, int message, IntPtr unused, IntPtr range);
        const int EmFormatRange = 0x0439;

        void SavePreviewSnapshot() {
            PerformLayout();
            Directory.CreateDirectory(Path.GetDirectoryName(snapshot));
            using (var bitmap = new Bitmap(Width, Height)) {
                DrawToBitmap(bitmap, new Rectangle(0, 0, Width, Height));
                using (var graphics = Graphics.FromImage(bitmap)) DrawNativePreview(this, graphics);
                bitmap.Save(snapshot, ImageFormat.Png);
            }
        }
        Rectangle PreviewClip(Control control) {
            Rectangle visible = control.RectangleToScreen(control.ClientRectangle);
            for (Control ancestor = control.Parent; ancestor != null; ancestor = ancestor.Parent)
                visible.Intersect(ancestor.RectangleToScreen(ancestor.ClientRectangle));
            visible.Offset(-Left, -Top);
            return visible;
        }
        void DrawNativePreview(Control parent, Graphics graphics) {
            foreach (Control child in parent.Controls) {
                if (!child.Visible || child.Width < 1 || child.Height < 1) continue;
                var reader = child as RichTextBox;
                var picker = child as ComboBox;
                var saved = graphics.Save();
                try {
                    graphics.SetClip(PreviewClip(child));
                    Point origin = child.PointToScreen(Point.Empty); origin.Offset(-Left, -Top);
                    if (reader != null) {
                        // RichTextBox 不支持 DrawToBitmap，改用其原生排版引擎画入独立内存位图。
                        if (reader.ClientSize.Width < 1 || reader.ClientSize.Height < 1) continue;
                        using (var textBitmap = new Bitmap(reader.ClientSize.Width, reader.ClientSize.Height)) {
                            textBitmap.SetResolution(graphics.DpiX, graphics.DpiY);
                            using (var textGraphics = Graphics.FromImage(textBitmap)) {
                                textGraphics.Clear(reader.BackColor);
                                var page = new NativeRect {
                                    Right = (int)Math.Round(reader.ClientSize.Width * 1440D / textGraphics.DpiX),
                                    Bottom = (int)Math.Round(reader.ClientSize.Height * 1440D / textGraphics.DpiY) };
                                var area = page; area.Left = (int)Math.Round(2 * 1440D / textGraphics.DpiX); area.Top = (int)Math.Round(2 * 1440D / textGraphics.DpiY);
                                IntPtr device = textGraphics.GetHdc();
                                try {
                                    var range = new FormatRange { Device = device, TargetDevice = device, Area = area, Page = page,
                                        Characters = new CharacterRange { First = 0, Last = -1 } };
                                    RenderRichText(reader.Handle, EmFormatRange, new IntPtr(1), ref range);
                                } finally {
                                    // 原生控件会缓存输出设备信息，使用后必须显式释放。
                                    ReleaseRichText(reader.Handle, EmFormatRange, IntPtr.Zero, IntPtr.Zero);
                                    textGraphics.ReleaseHdc(device);
                                }
                                graphics.DrawImageUnscaled(textBitmap, origin);
                            }
                        }
                    } else if (picker != null) {
                        // 下拉框的选中项由原生子窗口绘制，单独补入当前实际选中值。
                        var area = new Rectangle(origin.X + 3, origin.Y + 1,
                            Math.Max(1, picker.ClientSize.Width - SystemInformation.VerticalScrollBarWidth - 6), Math.Max(1, picker.ClientSize.Height - 2));
                        TextRenderer.DrawText(graphics, picker.Text, picker.Font, area, picker.ForeColor,
                            TextFormatFlags.Left | TextFormatFlags.VerticalCenter | TextFormatFlags.SingleLine | TextFormatFlags.EndEllipsis | TextFormatFlags.PreserveGraphicsClipping);
                    }
                } finally { graphics.Restore(saved); }
                DrawNativePreview(child, graphics);
            }
        }
        static Label TextLabel(string text) { return new Label { Text = text, Dock = DockStyle.Fill, AutoSize = true, Margin = new Padding(0, 4, 0, 6) }; }
        static Button ActionButton(string text) { return new Button { Text = text, AutoSize = true, MinimumSize = new Size(130, 40), Padding = new Padding(10, 2, 10, 2), Margin = new Padding(0, 0, 10, 6) }; }
        void Render() {
            Text = UiText.T("app.title");
            if (header != null) { shell.Controls.Remove(header); header.Dispose(); }
            var bar = new TableLayoutPanel { Dock = DockStyle.Fill, AutoSize = true, ColumnCount = 2, Margin = new Padding(0, 0, 0, 18) };
            bar.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100)); bar.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            var titles = new TableLayoutPanel { Dock = DockStyle.Fill, AutoSize = true, ColumnCount = 1, Margin = Padding.Empty };
            titles.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            titles.Controls.Add(new Label { Text = UiText.T("app.heading"), AutoSize = true, Dock = DockStyle.Fill, Font = new Font(Font.FontFamily, 19F, FontStyle.Bold), Margin = Padding.Empty });
            titles.Controls.Add(TextLabel(UiText.T("app.subtitle", bundle.Info.version, bundle.Info.delivery_version)));
            var tools = new FlowLayoutPanel { AutoSize = true, FlowDirection = FlowDirection.TopDown, WrapContents = false, Anchor = AnchorStyles.Top | AnchorStyles.Right, Margin = Padding.Empty };
            language = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Width = 150, AccessibleName = UiText.T("language.label"), Enabled = !busy };
            language.Items.Add(UiText.For("zh-CN", "language.name")); language.Items.Add(UiText.For("en", "language.name"));
            language.SelectedIndex = UiText.Language == "zh-CN" ? 0 : 1;
            language.DropDown += delegate { userInteracted = true; };
            language.SelectedIndexChanged += delegate { if (busy) return; userInteracted = true; UiText.Language = language.SelectedIndex == 0 ? "zh-CN" : "en"; Render(); };
            credits = new LinkLabel { Text = UiText.T("document.credits"), AutoSize = true, Margin = new Padding(3, 8, 0, 0), Enabled = !busy && currentPage != "waiting" };
            credits.LinkClicked += delegate {
                userInteracted = true;
                if (documents == null || documents.IsDisposed) { currentPage = "home"; Render(); }
                if (documents != null) documents.SelectedTab = documents.TabPages["credits"];
            };
            tools.Controls.Add(language); tools.Controls.Add(credits); bar.Controls.Add(titles, 0, 0); bar.Controls.Add(tools, 1, 0);
            header = bar; shell.Controls.Add(header, 0, 0);
            foreach (Control child in content.Controls.Cast<Control>().ToArray()) { content.Controls.Remove(child); child.Dispose(); }
            documents = null; localState = null; feedback = null; install = null; uninstall = null; location = null; progress = null;
            if (currentPage == "direct") RenderDirect();
            else if (currentPage == "update") RenderUpdate();
            else if (currentPage == "exported") RenderExported();
            else if (currentPage == "waiting") ShowContent(new Label { Text = UiText.T(waitingKey), Dock = DockStyle.Fill, TextAlign = ContentAlignment.MiddleCenter, Font = new Font(Font.FontFamily, 13F) });
            else RenderChoices();
            content.Enabled = !busy;
        }
        void ShowContent(Control page) { page.Dock = DockStyle.Fill; content.Controls.Add(page); }
        void ShowChoices() { currentPage = "home"; Render(); }
        void ShowWaiting(string key) { waitingKey = key; currentPage = "waiting"; Render(); }
        async Task Start() {
            try {
                var update = await Task.Run(() => UpdateFeed.Fetch(Convert.ToString(bundle.Info.policy["updates_url"]), bundle.Info.delivery_version));
                if (closing || userInteracted || currentPage != "home") return;
                if (update.status == "ready" && new Version(update.latest_version) > new Version(bundle.Info.version)) { availableUpdate = update; currentPage = "update"; Render(); return; }
            } catch (Exception failure) { Debug.WriteLine(failure); }
        }
        void RenderChoices() {
            var page = new TableLayoutPanel { ColumnCount = 1, RowCount = 3, AutoScroll = true };
            page.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            page.RowStyles.Add(new RowStyle(SizeType.AutoSize)); page.RowStyles.Add(new RowStyle(SizeType.AutoSize)); page.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            var direct = new ChoiceButton(UiText.T("home.direct.title"), UiText.T("home.direct.description")) { Margin = new Padding(0, 0, 0, 12) };
            var zip = new ChoiceButton(UiText.T("home.zip.title"), UiText.T("home.zip.description")) { Margin = new Padding(0, 0, 0, 16) };
            direct.Click += async delegate { await EnterDirect(); }; zip.Click += async delegate { await Export(); };
            page.Controls.Add(direct, 0, 0); page.Controls.Add(zip, 0, 1); page.Controls.Add(CreateDocuments(), 0, 2); ShowContent(page);
        }
        void RenderUpdate() {
            var page = new TableLayoutPanel { ColumnCount = 1, RowCount = 3, AutoScroll = true };
            page.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            page.RowStyles.Add(new RowStyle(SizeType.AutoSize)); page.RowStyles.Add(new RowStyle(SizeType.AutoSize)); page.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            var message = TextLabel(UiText.T("update.available", availableUpdate.latest_version, bundle.Info.version));
            message.Font = new Font(Font.FontFamily, 13F, FontStyle.Bold); message.Margin = new Padding(0, 20, 0, 20);
            var actions = new FlowLayoutPanel { AutoSize = true, Dock = DockStyle.Fill };
            var latest = ActionButton(UiText.T("update.download")); var ignore = ActionButton(UiText.T("update.continue"));
            latest.Click += delegate {
                string url = availableUpdate.installer_downloads != null && availableUpdate.installer_downloads.Count != 0
                    ? availableUpdate.installer_downloads[0].url : Convert.ToString(bundle.Info.policy["nexus_url"]);
                Open(url);
            };
            ignore.Click += delegate { ShowChoices(); }; actions.Controls.Add(latest); actions.Controls.Add(ignore);
            page.Controls.Add(message, 0, 0); page.Controls.Add(actions, 0, 1); page.Controls.Add(CreateDocuments(), 0, 2); ShowContent(page);
        }
        async Task EnterDirect() {
            if (busy) return;
            userInteracted = true;
            feedbackKey = null; feedbackFailure = null;
            if (!preview && String.IsNullOrEmpty(game)) {
                ShowWaiting("working.findGame");
                language.Enabled = false; credits.Enabled = false;
                try {
                    var games = await Task.Run(() => Bundle.FindGames().ToArray());
                    if (closing) return;
                    foreach (string candidate in games) { try { game = Bundle.GameRoot(candidate); break; } catch (IOException) {} }
                } catch (Exception failure) { Debug.WriteLine(failure); }
                if (closing) return;
            }
            selectedTopic = "installation"; currentPage = "direct"; Render();
            if (!preview && String.IsNullOrEmpty(game) && SelectGame()) RefreshLocal();
        }
        void RenderDirect() {
            var page = new TableLayoutPanel { ColumnCount = 1, RowCount = 7, AutoScroll = true };
            page.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            for (int i = 0; i < 6; ++i) page.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            page.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            var navigation = new FlowLayoutPanel { Dock = DockStyle.Fill, AutoSize = true, Margin = Padding.Empty };
            var back = ActionButton(UiText.T("navigation.back")); back.Click += delegate { ShowChoices(); }; navigation.Controls.Add(back);
            page.Controls.Add(navigation, 0, 0);
            var path = new TableLayoutPanel { ColumnCount = 2, RowCount = 2, Dock = DockStyle.Fill, AutoSize = true, Margin = new Padding(0, 0, 0, 8) };
            path.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100)); path.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            path.Controls.Add(TextLabel(UiText.T("game.label")), 0, 0);
            location = new TextBox { ReadOnly = true, Dock = DockStyle.Fill, Margin = new Padding(0, 4, 10, 4), AccessibleName = UiText.T("game.label"), BackColor = Color.White };
            var browse = ActionButton(UiText.T("game.browse")); browse.Click += delegate { if (SelectGame()) { feedbackKey = null; feedbackFailure = null; RefreshLocal(); } };
            path.Controls.Add(location, 0, 1); path.Controls.Add(browse, 1, 1); page.Controls.Add(path, 0, 1);
            localState = TextLabel(""); page.Controls.Add(localState, 0, 2);
            var actions = new FlowLayoutPanel { Dock = DockStyle.Fill, AutoSize = true, Margin = new Padding(0, 6, 0, 0) };
            install = ActionButton(UiText.T("action.install")); uninstall = ActionButton(UiText.T("action.uninstall"));
            install.Click += async delegate { await Work(false); }; uninstall.Click += async delegate { await Work(true); };
            actions.Controls.Add(install); actions.Controls.Add(uninstall); page.Controls.Add(actions, 0, 3);
            feedback = TextLabel(""); feedback.ForeColor = Color.FromArgb(65, 88, 105); page.Controls.Add(feedback, 0, 4);
            progress = new ProgressBar { Dock = DockStyle.Fill, Style = ProgressBarStyle.Marquee, Height = 10, Visible = busy, Margin = new Padding(0, 2, 0, 10) };
            page.Controls.Add(progress, 0, 5); page.Controls.Add(CreateDocuments(), 0, 6); ShowContent(page); RefreshLocal();
        }
        bool SelectGame() {
            if (preview) return false;
            using (var dialog = new FolderBrowserDialog { Description = UiText.T("game.dialog"), ShowNewFolderButton = false }) {
                if (dialog.ShowDialog(this) != DialogResult.OK) return false;
                try { game = Bundle.GameRoot(dialog.SelectedPath); return true; }
                catch (Exception failure) { UiDialog.Show(this, UiText.Explain(failure), UiText.T("game.dialogError")); return false; }
            }
        }
        void RefreshLocal() {
            if (install == null) return;
            install.Enabled = false; uninstall.Enabled = false;
            location.Text = String.IsNullOrEmpty(game) ? UiText.T("game.none") : game;
            try {
                if (String.IsNullOrEmpty(game)) localState.Text = UiText.T("local.choose", bundle.Info.version);
                else {
                    game = Bundle.GameRoot(game); bundle.CheckManager(game);
                    string file = Bundle.Safe(game, Bundle.Mod + "VERSION");
                    string current = File.Exists(file) ? File.ReadAllText(file).Trim() : null;
                    localState.Text = UiText.T(current == null ? "local.absent" : "local.installed", bundle.Info.version, current);
                    install.Text = UiText.T(current == null ? "action.install" : current != bundle.Info.version ? "action.update" : "action.reinstall");
                    var previous = bundle.Previous(game);
                    install.Enabled = !busy; uninstall.Enabled = !busy && previous != null;
                }
            } catch (Exception failure) { localState.Text = UiText.T("local.blocked", bundle.Info.version); feedbackFailure = failure; }
            UpdateFeedback();
        }
        void UpdateFeedback() {
            if (feedback != null) feedback.Text = feedbackFailure != null ? UiText.Explain(feedbackFailure) : feedbackKey == null ? "" : UiText.T(feedbackKey, feedbackArgs);
        }
        void SetFeedback(string key, params object[] args) { feedbackFailure = null; feedbackKey = key; feedbackArgs = args; UpdateFeedback(); }
        void SetBusy(bool value) {
            busy = value; content.Enabled = !value; language.Enabled = !value; credits.Enabled = !value;
            if (progress != null) progress.Visible = value;
        }
        async Task Work(bool remove) {
            if (busy || preview || String.IsNullOrEmpty(game)) return;
            if (remove && UiDialog.Show(this, UiText.T("uninstall.confirm"), UiText.T("action.uninstall"), true) != DialogResult.OK) return;
            SetBusy(true); SetFeedback(remove ? "working.uninstall" : "working.install");
            var report = new Progress<string>(message => { if (busy && !closing && feedback != null && !feedback.IsDisposed) feedback.Text = message; });
            try {
                string selected = game;
                await Task.Run(delegate {
                    Bundle.Closed(); bundle.CheckManager(selected);
                    // 仍在独占锁中自动恢复；不改变安装事务和配置保护规则。
                    using (var locked = new FileStream(Bundle.Safe(selected, "AC7MultiSP-install.lock"), FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.None)) {
                        var pending = bundle.Incomplete(selected);
                        if (pending.Count > 1) throw UiText.Error("error.multipleTransactions");
                        if (pending.Count == 1) bundle.Restore(selected, pending[0]);
                    }
                    bundle.Apply(bundle.MakePlan(selected, remove), message => ((IProgress<string>)report).Report(message));
                });
                SetBusy(false); SetFeedback(remove ? "result.uninstall" : "result.install", bundle.Info.version); RefreshLocal();
            } catch (Exception failure) {
                SetBusy(false); feedbackFailure = failure; RefreshLocal();
                UiDialog.Show(this, UiText.Explain(failure), UiText.T("error.operationTitle"));
            }
        }
        async Task Export() {
            if (busy || preview) return;
            userInteracted = true;
            string target;
            using (var dialog = new SaveFileDialog { Title = UiText.T("export.dialog"), Filter = UiText.T("export.filter"), FileName = bundle.Info.zip_name,
                DefaultExt = "zip", AddExtension = true, CheckPathExists = true, OverwritePrompt = false }) {
                if (dialog.ShowDialog(this) != DialogResult.OK) return;
                target = dialog.FileName;
            }
            if (File.Exists(target)) { UiDialog.Show(this, UiText.T("error.exportExists"), UiText.T("error.exportTitle")); return; }
            busy = true; ShowWaiting("working.export");
            try {
                await Task.Run(() => bundle.Export(target)); busy = false; exported = target; currentPage = "exported"; Render();
            } catch (Exception failure) { busy = false; ShowChoices(); UiDialog.Show(this, UiText.Explain(failure), UiText.T("error.exportTitle")); }
        }
        void RenderExported() {
            var page = new TableLayoutPanel { ColumnCount = 1, RowCount = 3, AutoScroll = true };
            page.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            page.RowStyles.Add(new RowStyle(SizeType.AutoSize)); page.RowStyles.Add(new RowStyle(SizeType.AutoSize)); page.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            var message = TextLabel(UiText.T("result.export", exported)); message.Margin = new Padding(0, 20, 0, 20);
            var actions = new FlowLayoutPanel { Dock = DockStyle.Fill, AutoSize = true };
            var open = ActionButton(UiText.T("export.openFolder")); var back = ActionButton(UiText.T("navigation.back"));
            open.Click += delegate {
                try { Process.Start(new ProcessStartInfo(Path.GetDirectoryName(exported)) { UseShellExecute = true }); }
                catch (Exception failure) { UiDialog.Show(this, UiText.Explain(failure), UiText.T("error.openFolderTitle")); }
            };
            back.Click += delegate { ShowChoices(); }; actions.Controls.Add(open); actions.Controls.Add(back);
            page.Controls.Add(message, 0, 0); page.Controls.Add(actions, 0, 1); page.Controls.Add(CreateDocuments(), 0, 2); ShowContent(page);
        }
        TabControl CreateDocuments() {
            documents = new TabControl { Dock = DockStyle.Fill, MinimumSize = new Size(0, 160), Margin = new Padding(0, 12, 0, 0) };
            foreach (string topic in new[] { "features", "installation", "changelog", "credits" }) {
                string text = bundle.Document(topic, UiText.Language);
                if (topic == "credits") {
                    var licenses = bundle.Info.files.Keys.Where(n => n.StartsWith(Bundle.Mod + "licenses/", StringComparison.Ordinal)).OrderBy(n => n, StringComparer.Ordinal).ToArray();
                    if (licenses.Length != 0) text += "\r\n\r\n" + UiText.T("document.thirdParty") + "\r\n\r\n" + String.Join("\r\n\r\n", licenses.Select(n => Path.GetFileName(n) + "\r\n" + Bundle.Utf8.GetString(bundle.Bytes(n))));
                }
                var tab = new TabPage(UiText.T("document." + topic)) { Name = topic, Padding = new Padding(6) };
                var reader = new RichTextBox { Text = text.Replace("\r\n", "\n").Replace("\n", "\r\n"), ReadOnly = true, DetectUrls = true,
                    ScrollBars = RichTextBoxScrollBars.Vertical, Dock = DockStyle.Fill, BackColor = Color.White, BorderStyle = BorderStyle.None, WordWrap = true };
                string original = text;
                reader.LinkClicked += delegate(object sender, LinkClickedEventArgs e) { OpenDocumentLink(e.LinkText, original); };
                reader.MouseDown += delegate { userInteracted = true; }; reader.KeyDown += delegate { userInteracted = true; };
                tab.Controls.Add(reader);
                documents.TabPages.Add(tab);
            }
            if (documents.TabPages.ContainsKey(selectedTopic)) documents.SelectedTab = documents.TabPages[selectedTopic];
            documents.MouseDown += delegate { userInteracted = true; }; documents.KeyDown += delegate { userInteracted = true; };
            documents.SelectedIndexChanged += delegate { userInteracted = true; if (documents.SelectedTab != null) selectedTopic = documents.SelectedTab.Name; };
            return documents;
        }
        void OpenDocumentLink(string link, string original) {
            if (String.IsNullOrEmpty(link)) return;
            string value = link;
            if (value.IndexOf('@') > 0 && value.IndexOf(':') < 0) value = "mailto:" + value;
            string embedded = value.StartsWith("mailto:", StringComparison.OrdinalIgnoreCase) ? value.Substring(7) : value;
            Uri uri;
            if (!original.Contains(embedded) || !Uri.TryCreate(value, UriKind.Absolute, out uri) || (uri.Scheme != "https" && uri.Scheme != "mailto")) return;
            try { Process.Start(new ProcessStartInfo(uri.AbsoluteUri) { UseShellExecute = true }); }
            catch (Exception failure) { UiDialog.Show(this, UiText.Explain(failure), UiText.T("error.documentLinkTitle")); }
        }
        bool Open(string value) {
            Uri uri;
            if (!Uri.TryCreate(value, UriKind.Absolute, out uri) || !UpdateFeed.Allowed(uri)) {
                UiDialog.Show(this, UiText.T("error.openUrl"), UiText.T("error.openTitle")); return false;
            }
            try { Process.Start(new ProcessStartInfo(uri.AbsoluteUri) { UseShellExecute = true }); return true; }
            catch (Exception failure) { UiDialog.Show(this, UiText.Explain(failure), UiText.T("error.openTitle")); return false; }
        }
    }
}
