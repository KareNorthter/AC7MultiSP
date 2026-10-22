using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Web.Script.Serialization;
using Microsoft.Win32;

namespace AC7MultiSP.Setup {
    // 每个资源原文件独立嵌入，安装不接收外部包；ZIP 只在导出时生成。
    internal sealed class ResourceFile {
        public string kind { get; set; }
        public string resource { get; set; }
        public long size { get; set; }
        public string sha256 { get; set; }
    }
    internal sealed class BundleInfo {
        public int schema { get; set; }
        public string product { get; set; }
        public string version { get; set; }
        public string delivery_version { get; set; }
        public string zip_name { get; set; }
        public Dictionary<string, ResourceFile> files { get; set; }
        public Dictionary<string, object> policy { get; set; }
    }
    internal sealed class OwnedState {
        public int schema { get; set; }
        public string product { get; set; }
        public string version { get; set; }
        public string manager { get; set; }
        public Dictionary<string, string> files { get; set; }
    }
    internal sealed class Entry {
        public string relative { get; set; }
        public string before { get; set; }
        public string after { get; set; }
    }
    internal sealed class Journal {
        public int schema { get; set; }
        public string game { get; set; }
        public string status { get; set; }
        public List<Entry> entries { get; set; }
    }
    internal sealed class Plan {
        internal string Game;
        internal Dictionary<string, byte[]> Changes = new Dictionary<string, byte[]>(StringComparer.OrdinalIgnoreCase);
        internal Dictionary<string, string> Before = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        internal bool SharedInstalled;
    }
    internal sealed class Bundle {
        internal const string Mod = "ue4ss/Mods/DualSP/";
        internal const string Pak = "Game/Content/Paks/~mods/AC7MultiSP_Mounts_P.pak";
        internal const string StateName = "AC7MultiSP-install.json";
        internal static readonly UTF8Encoding Utf8 = new UTF8Encoding(false, true);
        internal readonly BundleInfo Info;
        internal readonly JavaScriptSerializer Json = new JavaScriptSerializer { MaxJsonLength = 1048576, RecursionLimit = 30 };
        internal Bundle() {
            using (var stream = Assembly.GetExecutingAssembly().GetManifestResourceStream("bundle.json")) {
                if (stream == null) throw new InvalidDataException("安装器缺少内置清单。");
                using (var reader = new StreamReader(stream, Utf8)) Info = Json.Deserialize<BundleInfo>(reader.ReadToEnd());
            }
            if (Info.schema != 1 || Info.product != "AC7MultiSP" || !Regex.IsMatch(Info.version, @"^\d+\.\d+\.\d+$"))
                throw new InvalidDataException("安装器内置清单无效。");
        }
        internal static string Digest(byte[] data) {
            using (var sha = SHA256.Create()) return BitConverter.ToString(sha.ComputeHash(data)).Replace("-", "").ToLowerInvariant();
        }
        internal static string Hash(string file) {
            if (!File.Exists(file)) return null;
            using (var stream = File.OpenRead(file)) using (var sha = SHA256.Create())
                return BitConverter.ToString(sha.ComputeHash(stream)).Replace("-", "").ToLowerInvariant();
        }
        internal byte[] Bytes(string relative) {
            var entry = Info.files[relative];
            using (var stream = Assembly.GetExecutingAssembly().GetManifestResourceStream(entry.resource)) {
                if (stream == null) throw new InvalidDataException("缺少内置文件：" + relative);
                using (var output = new MemoryStream()) {
                    stream.CopyTo(output); var data = output.ToArray();
                    if (data.LongLength != entry.size || Digest(data) != entry.sha256) throw new InvalidDataException("内置文件校验失败：" + relative);
                    return data;
                }
            }
        }
        internal string Document(string name) { return Utf8.GetString(Bytes(Mod + "docs/" + name)).TrimStart('\uFEFF'); }
        internal static string Safe(string root, string relative, bool readLeaf = false) {
            if (String.IsNullOrEmpty(relative) || relative.Contains('\\') || relative.Contains(':') || relative.StartsWith("/"))
                throw new IOException("路径无效。");
            foreach (var part in relative.Split('/')) if (part.Length == 0 || part == "." || part == ".." ||
                part.EndsWith(".") || part.EndsWith(" ") || Regex.IsMatch(part, @"^(CON|PRN|AUX|NUL|COM[0-9]|LPT[0-9])(\.|$)", RegexOptions.IgnoreCase))
                throw new IOException("路径无效：" + relative);
            string basePath = Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar);
            string target = Path.GetFullPath(Path.Combine(basePath, relative.Replace('/', Path.DirectorySeparatorChar)));
            if (!target.StartsWith(basePath + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase)) throw new IOException("路径越界。");
            for (string probe = target; !String.IsNullOrEmpty(probe); probe = Path.GetDirectoryName(probe)) {
                try { if ((File.GetAttributes(probe) & FileAttributes.ReparsePoint) != 0 && !(readLeaf && probe == target)) throw new IOException("此路径由其他工具链接管理，请使用原工具更新：" + probe); }
                catch (FileNotFoundException) {} catch (DirectoryNotFoundException) {}
            }
            return target;
        }
        internal static void Closed() {
            foreach (string name in new[] { "Ace7Game", "Ace7Game-Win64-Shipping" })
                foreach (var process in Process.GetProcessesByName(name)) using (process) throw new IOException("请先完整退出皇牌空战7。");
        }
        internal static string GameRoot(string value) {
            string current = Path.GetFullPath(value.Trim().Trim('"'));
            if (File.Exists(current)) current = Path.GetDirectoryName(current);
            for (int i = 0; i < 4 && !String.IsNullOrEmpty(current); ++i) {
                if (File.Exists(Safe(current, "Ace7Game.exe")) && Directory.Exists(Safe(current, "Game/Content/Paks"))) return current.TrimEnd('\\');
                current = Path.GetDirectoryName(current);
            }
            throw new IOException("请选择含 Ace7Game.exe 的皇牌空战7游戏目录。");
        }
        internal static IEnumerable<string> FindGames() {
            var roots = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (var tuple in new[] {
                new { Hive = Registry.CurrentUser, Key = @"Software\Valve\Steam" },
                new { Hive = Registry.LocalMachine, Key = @"SOFTWARE\WOW6432Node\Valve\Steam" },
                new { Hive = Registry.LocalMachine, Key = @"SOFTWARE\Valve\Steam" } }) {
                using (var key = tuple.Hive.OpenSubKey(tuple.Key)) if (key != null) foreach (string name in new[] { "SteamPath", "InstallPath" }) {
                    string value = key.GetValue(name) as string;
                    if (!String.IsNullOrEmpty(value)) roots.Add(value);
                }
            }
            foreach (string root in roots.ToArray()) {
                string file = Path.Combine(root, "steamapps", "libraryfolders.vdf");
                if (File.Exists(file)) foreach (Match m in Regex.Matches(File.ReadAllText(file), "\"(?:path|[0-9]+)\"\\s+\"([^\"\\r\\n]+)\"")) {
                    string library = m.Groups[1].Value.Replace(@"\\", @"\");
                    if (Path.IsPathRooted(library)) roots.Add(library);
                }
            }
            var games = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (string root in roots) {
                string acf = Path.Combine(root, "steamapps", "appmanifest_502500.acf");
                if (!File.Exists(acf)) continue;
                var match = Regex.Match(File.ReadAllText(acf), "\"installdir\"\\s+\"([^\"\\r\\n]+)\"");
                if (!match.Success || match.Groups[1].Value.IndexOfAny(new[] { '/', '\\', ':' }) >= 0) continue;
                string game = Path.Combine(root, "steamapps", "common", match.Groups[1].Value);
                if (File.Exists(Path.Combine(game, "Ace7Game.exe"))) games.Add(game);
            }
            return games;
        }
        internal static string Dependencies(string game = null) {
            // 系统组件的实际 DLL 能否由 64 位加载器找到；不把 DX12 版本号视为旧组件齐全。
            var missing = new List<string>();
            foreach (string name in new[] { "vcruntime140.dll", "vcruntime140_1.dll", "msvcp140.dll", "xinput1_3.dll" })
                if (!File.Exists(Path.Combine(Environment.SystemDirectory, name)) && (game == null ||
                    !File.Exists(Path.Combine(game, name)) && !File.Exists(Path.Combine(game, "ue4ss", name)))) missing.Add(name);
            return String.Join("、", missing.ToArray());
        }
        internal static bool OwnedPath(string relative, bool shared) {
            bool program = relative.StartsWith(Mod + "Scripts/", StringComparison.Ordinal) || relative.StartsWith(Mod + "bin/", StringComparison.Ordinal) ||
                relative.StartsWith(Mod + "licenses/", StringComparison.Ordinal) || relative.StartsWith(Mod + "docs/", StringComparison.Ordinal) ||
                new[] { Mod + "VERSION", Mod + "enabled.txt", Mod + "install-receipt.json", Mod + "vortex-package.json", Mod + "Audio/SP2.wav" }.Contains(relative);
            return program || relative == Pak || relative == StateName ||
                (shared && new[] { "dwmapi.dll", "ue4ss/UE4SS.dll", "ue4ss/UE4SS-settings.ini" }.Contains(relative));
        }
        internal T Read<T>(string file) { return Json.Deserialize<T>(File.ReadAllText(file, Utf8).TrimStart('\uFEFF')); }
        internal void Save(string path, object value) {
            Directory.CreateDirectory(Path.GetDirectoryName(path));
            string temporary = path + "." + Guid.NewGuid().ToString("N") + ".new";
            File.WriteAllText(temporary, Json.Serialize(value), Utf8);
            if (File.Exists(path)) File.Replace(temporary, path, null); else File.Move(temporary, path);
        }
        internal void CheckManager(string game) {
            // 不读取或修改 Vortex 数据库，只检查游戏中已有的部署记录。
            foreach (string file in Directory.GetFiles(game, "vortex.deployment.*.json")) {
                var text = File.ReadAllText(Safe(game, Path.GetFileName(file)), Utf8);
                if (text.IndexOf("DualSP", StringComparison.OrdinalIgnoreCase) >= 0 || text.IndexOf("AC7MultiSP_Mounts_P.pak", StringComparison.OrdinalIgnoreCase) >= 0)
                    throw new IOException("此 Mod 已由 Vortex 管理，请在 Vortex 更新，或使用“导出通用 ZIP”。");
            }
            string extras = Safe(game, "ue4ss/AC7-ExtraMods.json");
            if (File.Exists(extras)) {
                var record = Read<Dictionary<string, object>>(extras);
                object mods;
                if (record.TryGetValue("Mods", out mods)) foreach (Dictionary<string, object> item in (System.Collections.IEnumerable)mods)
                    if (String.Equals(Convert.ToString(item["Name"]), "DualSP", StringComparison.OrdinalIgnoreCase))
                        throw new IOException("此 Mod 由鼠标飞控启动器管理，请先在原工具迁移后再使用本安装器。");
            }
        }
        internal List<string> Incomplete(string game) {
            var result = new List<string>();
            string folder = Safe(game, "AC7MultiSP-backup");
            if (!Directory.Exists(folder)) return result;
            foreach (string directory in Directory.GetDirectories(folder)) {
                string relative = "AC7MultiSP-backup/" + Path.GetFileName(directory) + "/transaction.json";
                string file = Safe(game, relative);
                if (File.Exists(file)) { var j = Read<Journal>(file); if (j.status == "prepared" || j.status == "applying" || j.status == "rollback-failed") result.Add(file); }
            }
            return result;
        }
        internal OwnedState Previous(string game) {
            string file = Safe(game, StateName);
            if (!File.Exists(file)) return null;
            var value = Read<OwnedState>(file);
            if (value.schema != 1 || value.product != Info.product || value.manager != "exe" || value.files == null)
                throw new IOException("已有安装记录无法识别，请保留记录并反馈。");
            foreach (var pair in value.files) if (!OwnedPath(pair.Key, false) || !Regex.IsMatch(pair.Value, "^[0-9a-f]{64}$")) throw new IOException("安装记录包含异常路径。");
            return value;
        }
        internal Plan MakePlan(string input, bool uninstall) {
            Closed();
            if (!Environment.Is64BitOperatingSystem || Environment.OSVersion.Version.Major < 10) throw new IOException("需要 Windows 10/11 64位。");
            string game = GameRoot(input);
            CheckManager(game);
            if (Incomplete(game).Count != 0) throw new IOException("存在上次中断的操作，请先点击“恢复上次中断”。");
            var plan = new Plan { Game = game };
            var previous = Previous(game);
            if (uninstall && previous == null) throw new IOException("没有本安装器的安装记录，不能自动卸载其他方式安装的 Mod。");
            if (uninstall) {
                foreach (var pair in previous.files) {
                    var target = Safe(game, pair.Key);
                    string current = Hash(target);
                    if (current != null) AddChange(plan, pair.Key, null);
                    // 改过的已登记程序同样移入备份，防止停用后继续被加载；个人数据和共享前置保留。
                }
                AddChange(plan, StateName, null);
                return plan;
            }
            string missing = Dependencies(game);
            if (missing.Length != 0) throw new IOException("缺少运行组件：" + missing + "。请用“运行库下载”补齐后重新检测。");
            string oldVersionFile = Safe(game, Mod + "VERSION");
            if (File.Exists(oldVersionFile)) {
                string old = File.ReadAllText(oldVersionFile, Utf8).Trim();
                var match = Regex.Match(old, @"^(\d+\.\d+\.\d+)(?: alpha[1-9][0-9]*)?$");
                if (!match.Success) throw new IOException("原 Mod 版本无法识别，请先核对。");
                if (new Version(match.Groups[1].Value) > new Version(Info.version)) throw new IOException("本包版本低于已安装版本，请下载新版安装器。");
            }
            var loaderFiles = Info.files.Where(p => p.Value.kind == "loader").ToArray();
            bool anyLoader = loaderFiles.Any(p => File.Exists(Safe(game, p.Key, true))) || File.Exists(Safe(game, "UE4SS.dll", true));
            if (anyLoader) {
                foreach (var item in loaderFiles.Where(p => p.Key.EndsWith(".dll", StringComparison.OrdinalIgnoreCase)))
                    if (Hash(Safe(game, item.Key, true)) != item.Value.sha256) throw new IOException("已有未知或不完整的 UE4SS，已保留。请核对 AC7 专用前置。");
                string settings = Safe(game, "ue4ss/UE4SS-settings.ini", true);
                if (!File.Exists(settings) || Regex.IsMatch(File.ReadAllText(settings), @"^\s*(ModsFolderPaths|ModsPath|ModPath)\s*=\s*[^;\s]", RegexOptions.Multiline | RegexOptions.IgnoreCase))
                    throw new IOException("现有 UE4SS 设置缺失或使用自定义 Mod 路径，请先核对。");
                foreach (var item in loaderFiles) plan.Before[item.Key] = Hash(Safe(game, item.Key, true));
            } else {
                foreach (var item in loaderFiles) AddChange(plan, item.Key, Bytes(item.Key));
                plan.SharedInstalled = true;
            }
            var owned = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            foreach (var item in Info.files.Where(p => p.Value.kind == "mod" && OwnedPath(p.Key, false))) {
                string target = Safe(game, item.Key);
                if (item.Key == Mod + "Audio/SP2.wav" && File.Exists(target) && Hash(target) != item.Value.sha256) continue;
                owned.Add(item.Key, item.Value.sha256);
                if (Hash(target) != item.Value.sha256) AddChange(plan, item.Key, Bytes(item.Key));
                else plan.Before[item.Key] = item.Value.sha256;
            }
            // 仅按旧来源清单移走新版本已废弃的程序，不递归清空 Mod 目录。
            var oldFiles = previous == null ? LegacyFiles(game) : previous.files;
            foreach (var pair in oldFiles) if (!owned.ContainsKey(pair.Key) && (pair.Key.StartsWith(Mod + "Scripts/") || pair.Key.StartsWith(Mod + "bin/"))) {
                string current = Hash(Safe(game, pair.Key));
                if (current != null && current != pair.Value) throw new IOException("旧程序已被外部修改，请先核对：" + pair.Key);
                if (current != null) AddChange(plan, pair.Key, null);
            }
            foreach (string sub in new[] { "Scripts", "bin" }) {
                string folder = Safe(game, Mod + sub);
                if (!Directory.Exists(folder)) continue;
                CheckTree(game, folder, owned.Keys.Concat(oldFiles.Keys).ToArray());
            }
            var state = new OwnedState { schema = 1, product = Info.product, version = Info.version, manager = "exe", files = owned };
            AddChange(plan, StateName, Utf8.GetBytes(Json.Serialize(state)));
            plan.Before[Mod + "profiles.json"] = Hash(Safe(game, Mod + "profiles.json"));
            return plan;
        }
        private Dictionary<string, string> LegacyFiles(string game) {
            var result = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            string file = Safe(game, Mod + "install-receipt.json");
            if (!File.Exists(file)) return result;
            var value = Read<Dictionary<string, object>>(file);
            object name, runtime;
            if (!value.TryGetValue("Name", out name) || Convert.ToString(name) != Info.product || !value.TryGetValue("runtime_files", out runtime)) return result;
            foreach (var pair in (Dictionary<string, object>)runtime) {
                string relative = Mod + pair.Key;
                if (!OwnedPath(relative, false) || !Regex.IsMatch(Convert.ToString(pair.Value), "^[0-9a-fA-F]{64}$")) throw new IOException("旧运行回执异常。");
                result.Add(relative, Convert.ToString(pair.Value).ToLowerInvariant());
            }
            return result;
        }
        private static void CheckTree(string game, string folder, string[] known) {
            foreach (string entry in Directory.GetFileSystemEntries(folder)) {
                string rel = entry.Substring(game.Length + 1).Replace('\\', '/');
                Safe(game, rel);
                if (Directory.Exists(entry)) CheckTree(game, entry, known);
                else if (!known.Contains(rel, StringComparer.OrdinalIgnoreCase)) throw new IOException("旧程序目录有额外文件，已保留，请先核对：" + rel);
            }
        }
        private static void AddChange(Plan plan, string relative, byte[] data) {
            plan.Before[relative] = Hash(Safe(plan.Game, relative));
            plan.Changes.Add(relative, data);
        }
        internal void Restore(string game, string journalFile) {
            Closed();
            string file = Safe(game, "AC7MultiSP-backup/" + Path.GetFileName(Path.GetDirectoryName(journalFile)) + "/transaction.json");
            var journal = Read<Journal>(file);
            if (journal.schema != 1 || !String.Equals(journal.game, game, StringComparison.OrdinalIgnoreCase) || journal.entries == null ||
                !new[] { "prepared", "applying", "rollback-failed" }.Contains(journal.status)) throw new IOException("恢复记录无效。");
            string baseRel = "AC7MultiSP-backup/" + Path.GetFileName(Path.GetDirectoryName(file));
            foreach (var entry in journal.entries.AsEnumerable().Reverse()) {
                if (!OwnedPath(entry.relative, true)) throw new IOException("恢复记录越界。");
                string target = Safe(game, entry.relative), current = Hash(target);
                if (current == entry.before) continue;
                if (current != entry.after) throw new IOException("恢复目标被其他程序修改：" + entry.relative);
                if (entry.before != null) {
                    string backup = Safe(game, baseRel + "/original/" + entry.relative);
                    if (Hash(backup) != entry.before) throw new IOException("备份损坏：" + entry.relative);
                    string temporary = Safe(game, entry.relative + ".restore-" + Guid.NewGuid().ToString("N"));
                    Directory.CreateDirectory(Path.GetDirectoryName(target)); File.Copy(backup, temporary);
                    if (File.Exists(target)) File.Replace(temporary, target, null); else File.Move(temporary, target);
                } else if (File.Exists(target)) {
                    string moved = Safe(game, baseRel + "/rollback-removed/" + entry.relative);
                    Directory.CreateDirectory(Path.GetDirectoryName(moved)); File.Move(target, moved);
                }
                if (Hash(target) != entry.before) throw new IOException("恢复后校验失败。");
            }
            journal.status = "rolled-back"; Save(file, journal);
        }
        internal string Apply(Plan plan, Action<string> progress) {
            Closed(); CheckManager(plan.Game);
            using (var locked = new FileStream(Safe(plan.Game, "AC7MultiSP-install.lock"), FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.None)) {
                foreach (var snapshot in plan.Before) if (Hash(Safe(plan.Game, snapshot.Key, !plan.Changes.ContainsKey(snapshot.Key))) != snapshot.Value) throw new IOException("检测后目标有变化，请重新检测：" + snapshot.Key);
                string baseRel = "AC7MultiSP-backup/" + DateTime.Now.ToString("yyyyMMdd-HHmmss") + "-" + Guid.NewGuid().ToString("N");
                string journalFile = Safe(plan.Game, baseRel + "/transaction.json");
                var journal = new Journal { schema = 1, game = plan.Game, status = "prepared", entries = new List<Entry>() };
                // 暂存、备份全部成功后，才写持久日志并修改游戏文件。
                foreach (var pair in plan.Changes) {
                    Closed(); string target = Safe(plan.Game, pair.Key);
                    if (Hash(target) != plan.Before[pair.Key]) throw new IOException("检测后目标有变化，请重新检测：" + pair.Key);
                    if (pair.Value != null) {
                        string staged = Safe(plan.Game, baseRel + "/staged/" + pair.Key);
                        Directory.CreateDirectory(Path.GetDirectoryName(staged)); File.WriteAllBytes(staged, pair.Value);
                        if (Hash(staged) != Digest(pair.Value)) throw new IOException("暂存校验失败。");
                    }
                    if (plan.Before[pair.Key] != null) {
                        string original = Safe(plan.Game, baseRel + "/original/" + pair.Key);
                        Directory.CreateDirectory(Path.GetDirectoryName(original)); File.Copy(target, original);
                        if (Hash(original) != plan.Before[pair.Key]) throw new IOException("备份校验失败。");
                    }
                    journal.entries.Add(new Entry { relative = pair.Key, before = plan.Before[pair.Key], after = pair.Value == null ? null : Digest(pair.Value) });
                }
                string config = Safe(plan.Game, Mod + "profiles.json");
                if (File.Exists(config)) {
                    string backup = Safe(plan.Game, baseRel + "/configuration/profiles.json");
                    Directory.CreateDirectory(Path.GetDirectoryName(backup)); string expected = Hash(config); File.Copy(config, backup);
                    if (Hash(backup) != expected || Hash(config) != expected) throw new IOException("个人配置备份校验失败。");
                }
                Save(journalFile, journal);
                try {
                    journal.status = "applying"; Save(journalFile, journal);
                    foreach (var entry in journal.entries) {
                        Closed(); progress("正在处理：" + entry.relative);
                        string target = Safe(plan.Game, entry.relative);
                        if (Hash(target) != entry.before) throw new IOException("写入前目标有变化。");
                        Directory.CreateDirectory(Path.GetDirectoryName(target));
                        if (entry.after == null) {
                            string removed = Safe(plan.Game, baseRel + "/removed/" + entry.relative);
                            Directory.CreateDirectory(Path.GetDirectoryName(removed)); if (File.Exists(target)) File.Move(target, removed);
                        } else {
                            string staged = Safe(plan.Game, baseRel + "/staged/" + entry.relative);
                            if (File.Exists(target)) File.Replace(staged, target, null); else File.Move(staged, target);
                        }
                        if (Hash(target) != entry.after) throw new IOException("写入后校验失败：" + entry.relative);
                    }
                    foreach (var entry in journal.entries) if (Hash(Safe(plan.Game, entry.relative)) != entry.after) throw new IOException("最终校验失败。");
                    foreach (var snapshot in plan.Before) if (!plan.Changes.ContainsKey(snapshot.Key) && Hash(Safe(plan.Game, snapshot.Key, true)) != snapshot.Value) throw new IOException("应保留文件发生变化：" + snapshot.Key);
                    journal.status = "completed"; Save(journalFile, journal);
                    return Path.GetDirectoryName(journalFile);
                } catch (Exception failure) {
                    try { Restore(plan.Game, journalFile); }
                    catch (Exception restore) { journal.status = "rollback-failed"; Save(journalFile, journal); throw new IOException("操作未完成，自动恢复也未完成。请保留备份：" + journalFile + "\r\n" + restore.Message, failure); }
                    throw new IOException("操作未完成，已恢复本次写入：" + failure.Message, failure);
                }
            }
        }
        internal void Export(string output) {
            string temporary = output + ".partial-" + Guid.NewGuid().ToString("N");
            using (var stream = new FileStream(temporary, FileMode.CreateNew, FileAccess.ReadWrite)) using (var archive = new ZipArchive(stream, ZipArchiveMode.Create)) {
                foreach (var pair in Info.files.Where(p => p.Value.kind == "mod").OrderBy(p => p.Key, StringComparer.Ordinal)) {
                    var entry = archive.CreateEntry(pair.Key, CompressionLevel.Optimal);
                    using (var target = entry.Open()) { byte[] data = Bytes(pair.Key); target.Write(data, 0, data.Length); }
                }
            }
            File.Move(temporary, output);
        }
    }
}
