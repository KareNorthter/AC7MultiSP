using System;
using System.Text.RegularExpressions;

namespace AC7MultiSP.Setup {
    // 同版本依次比较 alpha、beta（无编号为 beta1）和正式版。
    internal sealed class ReleaseVersion {
        readonly Version number;
        readonly int stage, sequence;
        ReleaseVersion(Version number, int stage, int sequence) { this.number = number; this.stage = stage; this.sequence = sequence; }
        static ReleaseVersion Parse(string text) {
            var match = Regex.Match(text ?? "", @"^([0-9]+\.[0-9]+\.[0-9]+)(?: alpha([1-9][0-9]*)|( beta([1-9][0-9]*)?))?$");
            if (!match.Success) throw new FormatException();
            var number = new Version(match.Groups[1].Value);
            int stage = match.Groups[2].Success ? 0 : match.Groups[3].Success ? 1 : 2;
            int sequence = stage == 0 ? Int32.Parse(match.Groups[2].Value) : stage == 1 ? (match.Groups[4].Success ? Int32.Parse(match.Groups[4].Value) : 1) : 0;
            if (number.Major > 65534 || number.Minor > 65534 || number.Build > 65534 || sequence > 65534) throw new FormatException();
            return new ReleaseVersion(number, stage, sequence);
        }
        internal static bool Valid(string text) {
            try { Parse(text); return true; } catch (FormatException) { return false; }
            catch (OverflowException) { return false; } catch (ArgumentException) { return false; }
        }
        internal static int Compare(string left, string right) {
            var a = Parse(left); var b = Parse(right);
            int order = a.number.CompareTo(b.number);
            if (order != 0) return order;
            order = a.stage.CompareTo(b.stage);
            return order != 0 ? order : a.sequence.CompareTo(b.sequence);
        }
    }
}
