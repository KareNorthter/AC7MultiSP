using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Web.Script.Serialization;

// 仅在用户授权后执行。调用成品内的实际安装逻辑，不启动游戏；所有旧文件由安装器备份。
internal static class VerifySetup {
    static readonly JavaScriptSerializer Json = new JavaScriptSerializer { MaxJsonLength = 4194304 };
    static readonly BindingFlags Flags = BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.Static;
    static Type type;
    static object bundle;
    static readonly List<string> passed = new List<string>();
    static object Call(string name, params object[] args) {
        try { return type.GetMethod(name, Flags).Invoke(bundle, args); }
        catch (TargetInvocationException e) { throw e.InnerException; }
    }
    static void Assert(bool condition, string name) {
        if (!condition) throw new Exception("核验失败：" + name);
        passed.Add(name); Console.WriteLine("通过：" + name);
    }
    static string Hash(string file) {
        if (!File.Exists(file)) return null;
        using (var stream = File.OpenRead(file)) using (var sha = SHA256.Create())
            return BitConverter.ToString(sha.ComputeHash(stream)).Replace("-", "").ToLowerInvariant();
    }
    static Dictionary<string, string> Files(string root) {
        var result = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        var paths = new[] { "ue4ss/Mods/DualSP", "Game/Content/Paks/~mods/AC7MultiSP_Mounts_P.pak", "AC7MultiSP-install.json",
            "dwmapi.dll", "ue4ss/UE4SS.dll", "ue4ss/UE4SS-settings.ini", "ue4ss/Mods/mods.txt", "ue4ss/Mods/mods.json" };
        foreach (string relative in paths) {
            string path = Path.Combine(root, relative);
            if (Directory.Exists(path)) foreach (string f in Directory.GetFiles(path, "*", SearchOption.AllDirectories))
                result.Add(f.Substring(root.Length + 1).Replace('\\', '/'), Hash(f));
            else if (File.Exists(path)) result.Add(relative, Hash(path));
        }
        return result;
    }
    static string Apply(object plan) { return (string)Call("Apply", plan, (Action<string>)(s => {})); }
    static object Plan(string game, bool uninstall) { return Call("MakePlan", game, uninstall); }
    static void Installed(string game) {
        var state = Json.Deserialize<Dictionary<string, object>>(File.ReadAllText(Path.Combine(game, "AC7MultiSP-install.json")));
        foreach (var item in (Dictionary<string, object>)state["files"])
            if (Hash(Path.Combine(game, item.Key)) != (string)item.Value) throw new Exception("安装摘要不符：" + item.Key);
    }
    static void Block(Action action, string expected, string name) {
        try { action(); } catch (Exception e) { Assert(e.Message.Contains(expected), name + "：" + e.Message); return; }
        throw new Exception("应停止但未停止：" + name);
    }
    [STAThread] static int Main(string[] args) {
        if (args.Length != 4) { Console.Error.WriteLine("参数：安装器.exe 游戏目录 新输出目录 audit|inspect|fresh|upgrade|uninstall|live"); return 2; }
        string output = Path.GetFullPath(args[2]);
        if (Directory.Exists(output)) throw new Exception("输出目录已存在，拒绝覆盖。");
        Directory.CreateDirectory(output);
        try {
            var assembly = Assembly.LoadFrom(Path.GetFullPath(args[0]));
            type = assembly.GetType("AC7MultiSP.Setup.Bundle", true);
            bundle = Activator.CreateInstance(type, true);
            string game = (string)Call("GameRoot", args[1]);
            Console.WriteLine("自动发现：" + String.Join("；", ((IEnumerable<string>)Call("FindGames")).ToArray()));
            Call("Export", Path.Combine(output, "导出通用包.zip"));
            Assert(File.Exists(Path.Combine(output, "导出通用包.zip")), "调用成品内置资源导出 ZIP");
            var before = Files(game);
            File.WriteAllText(Path.Combine(output, "操作前摘要.json"), Json.Serialize(before), new UTF8Encoding(false));
            if (args[3] == "audit") {
                var info = type.GetField("Info", Flags).GetValue(bundle);
                var resources = (IDictionary)info.GetType().GetProperty("files").GetValue(info, null);
                foreach (string name in resources.Keys) Call("Bytes", name);
                Assert(true, "核对全部内嵌资源摘要");
                Installed(game); Assert(true, "当前游戏安装记录与全部已登记文件一致");
                Assert(before.OrderBy(x => x.Key).SequenceEqual(Files(game).OrderBy(x => x.Key)), "成品审计未改游戏文件");
                var window = assembly.GetType("AC7MultiSP.Setup.MainWindow", true);
                try {
                    var update = window.GetMethod("FetchUpdate", Flags).Invoke(null, new object[] { "https://ac7multisp-updates.pages.dev/stable.json" });
                    Console.WriteLine("在线清单状态：" + update.GetType().GetProperty("status").GetValue(update, null));
                    Assert(true, "实际更新清单读取和解析");
                } catch (TargetInvocationException e) { Console.WriteLine("在线更新未通过：" + e.InnerException.Message); }
            } else if (args[3] == "inspect") {
                Block(() => Plan(game, false), "Vortex", "已有 Vortex 部署时拒绝接管");
                Assert(before.OrderBy(x => x.Key).SequenceEqual(Files(game).OrderBy(x => x.Key)), "只读核验未改游戏文件");
            } else if (args[3] == "fresh") {
                string proxy = Path.Combine(game, "dwmapi.dll");
                File.WriteAllText(proxy, "未知加载器，验证结束后保留到输出目录", new UTF8Encoding(false));
                Block(() => Plan(game, false), "未知或不完整", "拒绝覆盖未知加载器");
                File.Move(proxy, Path.Combine(output, "未知加载器样本.txt"));
                Apply(Plan(game, false)); Installed(game); Assert(true, "空目录首装含共享 UE4SS");
                string settings = Path.Combine(game, "ue4ss/UE4SS-settings.ini");
                byte[] original = File.ReadAllBytes(settings);
                try {
                    File.AppendAllText(settings, "\r\nModsFolderPaths = CustomMods\r\n", new UTF8Encoding(false));
                    Block(() => Plan(game, false), "自定义", "自定义 Mod 路径停止覆盖");
                } finally { File.WriteAllBytes(settings, original); }
                string manager = Path.Combine(game, "vortex.deployment.delivery-test.json");
                File.WriteAllText(manager, "{\"files\":[{\"relPath\":\"ue4ss/Mods/DualSP/Scripts/main.lua\"}]}", new UTF8Encoding(false));
                Block(() => Plan(game, false), "Vortex", "已有 Vortex 部署时拒绝接管");
                File.Move(manager, Path.Combine(output, "部署记录样本.json"));
                Apply(Plan(game, true));
                Assert(File.Exists(proxy) && File.Exists(settings) && !File.Exists(Path.Combine(game, "ue4ss/Mods/DualSP/bin/DualSP.dll")), "卸载保留共享 UE4SS");
            } else if (args[3] == "upgrade") {
                string old = File.ReadAllText(Path.Combine(game, "ue4ss/Mods/DualSP/VERSION")).Trim();
                Assert(old == "2.23.1", "旧正式 2.23.1 输入");
                Apply(Plan(game, false)); Installed(game); Assert(true, "真实旧正式版升级到内置 2.24.1");
                Assert(File.ReadAllText(Path.Combine(game, "ue4ss/Mods/DualSP/VERSION")).Trim() == "2.24.1", "升级后版本文件");
            } else if (args[3] == "uninstall") {
                Apply(Plan(game, true));
                Assert(!File.Exists(Path.Combine(game, "AC7MultiSP-install.json")) && !File.Exists(Path.Combine(game, "ue4ss/Mods/DualSP/bin/DualSP.dll")) &&
                    !File.Exists(Path.Combine(game, "Game/Content/Paks/~mods/AC7MultiSP_Mounts_P.pak")), "迁移前移出 EXE 管理的程序和安装记录");
                foreach (var item in before.Where(x => x.Key.StartsWith("ue4ss/Mods/DualSP/profiles") || x.Key.Contains("ui-state") ||
                    x.Key == "dwmapi.dll" || x.Key == "ue4ss/UE4SS.dll" || x.Key == "ue4ss/UE4SS-settings.ini" || x.Key == "ue4ss/Mods/mods.txt"))
                    Assert(Hash(Path.Combine(game, item.Key)) == item.Value, "迁移保留：" + item.Key);
            } else if (args[3] == "live") {
                Apply(Plan(game, false)); Installed(game); Assert(true, "本机安装并逐文件回读");
                string doc = Path.Combine(game, "ue4ss/Mods/DualSP/docs/README_en.txt");
                File.AppendAllText(doc, "\r\n交付验证：模拟损坏说明文件\r\n", new UTF8Encoding(false));
                Apply(Plan(game, false)); Installed(game); Assert(true, "同版修复损坏文件");
                string versionFile = Path.Combine(game, "ue4ss/Mods/DualSP/VERSION");
                byte[] version = File.ReadAllBytes(versionFile);
                try {
                    File.WriteAllText(versionFile, "999.0.0\n", new UTF8Encoding(false));
                    Block(() => Plan(game, false), "低于", "阻止降级");
                } finally { File.WriteAllBytes(versionFile, version); }
                Apply(Plan(game, true));
                Assert(!File.Exists(Path.Combine(game, "ue4ss/Mods/DualSP/enabled.txt")) &&
                    !File.Exists(Path.Combine(game, "ue4ss/Mods/DualSP/bin/DualSP.dll")) &&
                    !File.Exists(Path.Combine(game, "Game/Content/Paks/~mods/AC7MultiSP_Mounts_P.pak")), "卸载移出启用标记、程序和 PAK");
                var uninstalled = Files(game);
                // 第一次写入后锁住该文件，再主动中断；让自动恢复失败，验证持久日志可在解锁后恢复。
                var plan = Plan(game, false);
                var changes = (IDictionary)plan.GetType().GetField("Changes", Flags).GetValue(plan);
                string first = changes.Keys.Cast<string>().First();
                int count = 0; FileStream held = null;
                try {
                    Block(() => Call("Apply", plan, (Action<string>)(s => {
                        if (++count == 2) {
                            held = new FileStream(Path.Combine(game, first), FileMode.Open, FileAccess.Read, FileShare.None);
                            throw new IOException("验证用中断");
                        }
                    })), "自动恢复也未完成", "中断时保留待恢复事务");
                } finally { if (held != null) held.Dispose(); }
                var pending = (List<string>)Call("Incomplete", game);
                Assert(pending.Count == 1, "识别唯一未完成事务");
                Call("Restore", game, pending[0]);
                Assert(uninstalled.OrderBy(x => x.Key).SequenceEqual(Files(game).OrderBy(x => x.Key)), "解锁后恢复全部原文件摘要");
                Apply(Plan(game, false)); Installed(game); Assert(true, "恢复后重新安装");
                foreach (var item in before.Where(x => !x.Key.StartsWith("ue4ss/Mods/DualSP/") &&
                    x.Key != "Game/Content/Paks/~mods/AC7MultiSP_Mounts_P.pak" && x.Key != "AC7MultiSP-install.json" ||
                    x.Key.StartsWith("ue4ss/Mods/DualSP/profiles") || x.Key.Contains("ui-state")))
                    Assert(Hash(Path.Combine(game, item.Key)) == item.Value, "保留：" + item.Key);
            } else throw new Exception("未知模式。");
            File.WriteAllText(Path.Combine(output, "验证回执.json"), Json.Serialize(new { passed = passed, installer_sha256 = Hash(args[0]), mode = args[3], game = game, game_launched = false }), new UTF8Encoding(false));
            return 0;
        } catch (Exception e) {
            File.WriteAllText(Path.Combine(output, "失败.txt"), e.ToString(), new UTF8Encoding(false)); Console.Error.WriteLine(e); return 1;
        }
    }
}
