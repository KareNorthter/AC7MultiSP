using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Reflection;
using System.Web.Script.Serialization;

namespace AC7MultiSP.Setup {
    // 界面与安全提示只从 docs/user/interface 对应的内嵌资源读取。
    internal static class UiText {
        static readonly Dictionary<string, Dictionary<string, string>> texts = Load();
        static string selection = "system";
        internal static string Selection {
            get { return selection; }
            set { selection = value == "zh-CN" || value == "en" ? value : "system"; }
        }
        internal static string Language {
            get { return selection == "system" ? (CultureInfo.CurrentUICulture.TwoLetterISOLanguageName == "zh" ? "zh-CN" : "en") : selection; }
            set { Selection = value; }
        }
        static Dictionary<string, Dictionary<string, string>> Load() {
            var result = new Dictionary<string, Dictionary<string, string>>(StringComparer.Ordinal);
            foreach (string code in new[] { "zh-CN", "en" }) {
                result[code] = new JavaScriptSerializer { MaxJsonLength = 65536 }.Deserialize<Dictionary<string, string>>(
                    Bundle.Utf8.GetString(AuthorSignature.Bytes("installer/ui/" + code + ".json")).TrimStart('\uFEFF'));
            }
            return result;
        }
        internal static string T(string key, params object[] values) { return For(Language, key, values); }
        internal static string For(string code, string key, params object[] values) {
            string format;
            if (!texts[code].TryGetValue(key, out format)) { Debug.WriteLine("缺少界面资源：" + key); return "[" + key + "]"; }
            var args = new object[values.Length];
            for (int i = 0; i < values.Length; ++i) args[i] = values[i] is Exception ? ((Exception)values[i]).Message : values[i];
            return String.Format(CultureInfo.GetCultureInfo(code), format, args);
        }
        internal static IOException Error(string key, params object[] values) { return new LocalizedIOException(key, null, values); }
        internal static IOException ErrorWithInner(string key, Exception inner, params object[] values) { return new LocalizedIOException(key, inner, values); }
        internal static string Explain(Exception failure) { return failure is LocalizedIOException ? failure.Message : T("error.external", failure.Message); }

        // 切换语言后，已显示的本项目错误也可重新取得对应语言。
        sealed class LocalizedIOException : IOException {
            readonly string key;
            readonly object[] values;
            internal LocalizedIOException(string name, Exception inner, object[] arguments) : base(name, inner) { key = name; values = arguments; }
            public override string Message { get { return T(key, values); } }
        }
    }
}
