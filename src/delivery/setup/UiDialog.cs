using System;
using System.Drawing;
using System.Windows.Forms;

namespace AC7MultiSP.Setup {
    // 对话框按钮跟随安装器语言；系统目录/保存选择器仍使用 Windows 提供的界面。
    internal static class UiDialog {
        internal static DialogResult Show(IWin32Window owner, string message, string title, bool confirm = false, string confirmKey = "common.ok") {
            using (var dialog = new Form()) {
                dialog.Text = title; dialog.Font = new Font("Microsoft YaHei UI", 10F);
                dialog.AutoScaleDimensions = new SizeF(96F, 96F); dialog.AutoScaleMode = AutoScaleMode.Dpi;
                dialog.StartPosition = owner == null ? FormStartPosition.CenterScreen : FormStartPosition.CenterParent;
                dialog.FormBorderStyle = FormBorderStyle.FixedDialog; dialog.MaximizeBox = false; dialog.MinimizeBox = false; dialog.ShowInTaskbar = owner == null;
                int height = Math.Max(85, Math.Min(380, TextRenderer.MeasureText(message, dialog.Font, new Size(590, 0), TextFormatFlags.WordBreak).Height + 20));
                dialog.ClientSize = new Size(640, height + 88);
                var layout = new TableLayoutPanel { Dock = DockStyle.Fill, Padding = new Padding(20), ColumnCount = 1, RowCount = 2 };
                layout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
                layout.RowStyles.Add(new RowStyle(SizeType.Percent, 100)); layout.RowStyles.Add(new RowStyle(SizeType.AutoSize));
                layout.Controls.Add(new TextBox { Text = message, Multiline = true, ReadOnly = true, WordWrap = true, ScrollBars = ScrollBars.Vertical,
                    Dock = DockStyle.Fill, BorderStyle = BorderStyle.None, BackColor = dialog.BackColor, AccessibleName = title }, 0, 0);
                var actions = new FlowLayoutPanel { AutoSize = true, Dock = DockStyle.Fill, FlowDirection = FlowDirection.RightToLeft, Padding = new Padding(0, 12, 0, 0) };
                var okay = new Button { Text = UiText.T(confirmKey), DialogResult = DialogResult.OK, AutoSize = true, MinimumSize = new Size(110, 36) };
                actions.Controls.Add(okay); dialog.AcceptButton = okay;
                if (confirm) {
                    var cancel = new Button { Text = UiText.T("common.cancel"), DialogResult = DialogResult.Cancel, AutoSize = true, MinimumSize = new Size(110, 36) };
                    actions.Controls.Add(cancel); dialog.CancelButton = cancel;
                } else dialog.CancelButton = okay;
                layout.Controls.Add(actions, 0, 1); dialog.Controls.Add(layout);
                return owner == null ? dialog.ShowDialog() : dialog.ShowDialog(owner);
            }
        }
    }
}
