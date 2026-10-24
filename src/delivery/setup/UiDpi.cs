using System;
using System.Drawing;
using System.Windows.Forms;

namespace AC7MultiSP.Setup {
    // 清单声明System DPI感知后，直接按实际系统DPI排版，避免Windows位图拉伸。
    internal static class UiDpi {
        internal static readonly float Factor = ReadFactor();
        static float ReadFactor() {
            using (var graphics = Graphics.FromHwnd(IntPtr.Zero)) return graphics.DpiX / 96F;
        }
        internal static int Pixels(int logical) { return (int)Math.Round(logical * Factor); }
        internal static void ScaleNew(Control control) {
            if (Math.Abs(Factor - 1F) > 0.001F) control.Scale(new SizeF(Factor, Factor));
        }
    }
}
