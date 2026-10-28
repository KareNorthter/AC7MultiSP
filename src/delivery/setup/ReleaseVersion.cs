using System;
using System.Text.RegularExpressions;

namespace AC7MultiSP.Setup {
    // 同版本按 alpha、beta、正式版排序；alpha 序号按数字排序。
    internal sealed class ReleaseVersion {
        readonly Version number;
        readonly int alpha;
        ReleaseVersion(Version number, int alpha) { this.number = number; this.alpha = alpha; }
        static ReleaseVersion Parse(string text) {
            var match = Regex.Match(text ?? "", @"^([0-9]+\.[0-9]+\.[0-9]+)(?: alpha([1-9][0-9]*)|( beta))?$");
            if (!match.Success) throw new FormatException();
            var number = new Version(match.Groups[1].Value);
            int alpha = match.Groups[3].Success ? 65535 : (match.Groups[2].Success ? Int32.Parse(match.Groups[2].Value) : 0);
            if (number.Major > 65534 || number.Minor > 65534 || number.Build > 65534 || (alpha > 65534 && !match.Groups[3].Success)) throw new FormatException();
            return new ReleaseVersion(number, alpha);
        }
        internal static bool Valid(string text) {
            try { Parse(text); return true; } catch (FormatException) { return false; }
            catch (OverflowException) { return false; } catch (ArgumentException) { return false; }
        }
        internal static int Compare(string left, string right) {
            var a = Parse(left); var b = Parse(right);
            int order = a.number.CompareTo(b.number);
            if (order != 0 || a.alpha == b.alpha) return order;
            if (a.alpha == 0) return 1;
            if (b.alpha == 0) return -1;
            return a.alpha.CompareTo(b.alpha);
        }
    }
}
