using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Web.Script.Serialization;
using Microsoft.Win32;

namespace AC7MultiSP.Setup {
    // 每个资源独立嵌入；导出使用交付时的通用 ZIP 原始字节。
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
        public bool candidate { get; set; }
        public string zip_name { get; set; }
        public Dictionary<string, ResourceFile> files { get; set; }
        public Dictionary<string, object> policy { get; set; }
    }
    internal sealed class CreditAuthor {
        public string label { get; set; }
        public string url { get; set; }
    }
    internal sealed class CompactCredits {
        public string notice { get; set; }
        public string author_heading { get; set; }
        public List<CreditAuthor> authors { get; set; }
        public string feedback_heading { get; set; }
        public List<string> feedback { get; set; }
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
            Info = AuthorSignature.Info;
            if (Info.schema != 1 || Info.product != "AC7MultiSP" || !ReleaseVersion.Valid(Info.version) ||
                ((Info.version.Contains(" alpha") || Info.version.Contains(" beta")) && !Info.candidate))
                throw new InvalidDataException(UiText.T("error.bundleInvalid"));
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
            return AuthorSignature.Bytes(relative);
        }
        internal string Document(string topic, string language) { return Utf8.GetString(Bytes("installer/docs/" + language + "/" + topic + ".txt")).TrimStart('\uFEFF'); }
        internal CompactCredits Credits(string language) {
            return Json.Deserialize<CompactCredits>(Utf8.GetString(Bytes("installer/credits-compact/" + language + ".json")).TrimStart('\uFEFF'));
        }
        internal static string Safe(string root, string relative, bool readLeaf = false) {
            if (String.IsNullOrEmpty(relative) || relative.Contains('\\') || relative.Contains(':') || relative.StartsWith("/"))
                throw UiText.Error("error.pathInvalid");
            foreach (var part in relative.Split('/')) if (part.Length == 0 || part == "." || part == ".." ||
                part.EndsWith(".") || part.EndsWith(" ") || Regex.IsMatch(part, @"^(CON|PRN|AUX|NUL|COM[0-9]|LPT[0-9])(\.|$)", RegexOptions.IgnoreCase))
                throw UiText.Error("error.pathInvalidDetail", relative);
            string basePath = Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar);
            string target = Path.GetFullPath(Path.Combine(basePath, relative.Replace('/', Path.DirectorySeparatorChar)));
            if (!target.StartsWith(basePath + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase)) throw UiText.Error("error.pathOutside");
            for (string probe = target; !String.IsNullOrEmpty(probe); probe = Path.GetDirectoryName(probe)) {
                try { if ((File.GetAttributes(probe) & FileAttributes.ReparsePoint) != 0 && !(readLeaf && probe == target)) throw UiText.Error("error.linkManaged", probe); }
                catch (FileNotFoundException) {} catch (DirectoryNotFoundException) {}
            }
            return target;
        }
        internal static void Closed() {
            foreach (string name in new[] { "Ace7Game", "Ace7Game-Win64-Shipping" })
                foreach (var process in Process.GetProcessesByName(name)) using (process) throw UiText.Error("error.gameRunning");
        }
        internal static bool GameRunning() {
            bool running = false;
            foreach (string name in new[] { "Ace7Game", "Ace7Game-Win64-Shipping" })
                foreach (var process in Process.GetProcessesByName(name)) using (process) { if (!process.HasExited) running = true; }
            return running;
        }
        internal static void CloseGameForInstall() {
            // 只在用户明确确认后调用；窗口关闭超时再结束这两个已知游戏进程。
            try {
                foreach (string name in new[] { "Ace7Game", "Ace7Game-Win64-Shipping" })
                    foreach (var process in Process.GetProcessesByName(name)) using (process) {
                        if (process.HasExited) continue;
                        try { process.CloseMainWindow(); } catch (InvalidOperationException) { continue; }
                        if (!process.WaitForExit(3000)) {
                            try { process.Kill(); } catch (InvalidOperationException) { continue; }
                            if (!process.WaitForExit(3000)) throw UiText.Error("error.closeGame");
                        }
                    }
                Closed();
            } catch (Exception failure) { throw UiText.ErrorWithInner("error.closeGame", failure); }
        }
        internal static string GameRoot(string value) {
            string current = Path.GetFullPath(value.Trim().Trim('"'));
            if (File.Exists(current)) current = Path.GetDirectoryName(current);
            for (int i = 0; i < 4 && !String.IsNullOrEmpty(current); ++i) {
                if (File.Exists(Safe(current, "Ace7Game.exe")) && Directory.Exists(Safe(current, "Game/Content/Paks"))) return current.TrimEnd('\\');
                current = Path.GetDirectoryName(current);
            }
            throw UiText.Error("error.gameDirectory");
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
        internal static bool OwnedPath(string relative, bool shared) {
            bool program = relative.StartsWith(Mod + "Scripts/", StringComparison.Ordinal) || relative.StartsWith(Mod + "bin/", StringComparison.Ordinal) ||
                relative.StartsWith(Mod + "licenses/", StringComparison.Ordinal) || relative.StartsWith(Mod + "docs/", StringComparison.Ordinal) ||
                new[] { Mod + "VERSION", Mod + "enabled.txt", Mod + "install-receipt.json", Mod + "vortex-package.json", Mod + "Audio/SP2.wav",
                    Mod + "author-content.json", Mod + "author-content.sig.json" }.Contains(relative);
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
                    throw UiText.Error("error.vortexManaged");
            }
            string extras = Safe(game, "ue4ss/AC7-ExtraMods.json");
            if (File.Exists(extras)) {
                var record = Read<Dictionary<string, object>>(extras);
                object mods;
                if (record.TryGetValue("Mods", out mods)) foreach (Dictionary<string, object> item in (System.Collections.IEnumerable)mods)
                    if (String.Equals(Convert.ToString(item["Name"]), "DualSP", StringComparison.OrdinalIgnoreCase))
                        throw UiText.Error("error.launcherManaged");
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
                throw UiText.Error("error.receiptInvalid");
            foreach (var pair in value.files) if (!OwnedPath(pair.Key, false) || !Regex.IsMatch(pair.Value, "^[0-9a-f]{64}$")) throw UiText.Error("error.receiptPath");
            return value;
        }
        internal Plan MakePlan(string input, bool uninstall) {
            // 修改目标或恢复事务前，完整核对可信清单列明的内嵌资源。
            AuthorSignature.VerifyAllResources();
            Closed();
            if (!Environment.Is64BitOperatingSystem || Environment.OSVersion.Version.Major < 10) throw UiText.Error("error.windowsVersion");
            string game = GameRoot(input);
            CheckManager(game);
            if (Incomplete(game).Count != 0) throw UiText.Error("error.incompleteOperation");
            var plan = new Plan { Game = game };
            var previous = Previous(game);
            if (uninstall && previous == null) throw UiText.Error("error.uninstallReceipt");
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
            string oldVersionFile = Safe(game, Mod + "VERSION");
            if (File.Exists(oldVersionFile)) {
                string old = File.ReadAllText(oldVersionFile, Utf8).Trim();
                if (!ReleaseVersion.Valid(old)) throw UiText.Error("error.versionUnknown");
                if (ReleaseVersion.Compare(old, Info.version) > 0) throw UiText.Error("error.downgrade");
            }
            var loaderFiles = Info.files.Where(p => p.Value.kind == "loader").ToArray();
            bool anyLoader = loaderFiles.Any(p => File.Exists(Safe(game, p.Key, true))) || File.Exists(Safe(game, "UE4SS.dll", true));
            if (anyLoader) {
                foreach (var item in loaderFiles.Where(p => p.Key.EndsWith(".dll", StringComparison.OrdinalIgnoreCase)))
                    if (Hash(Safe(game, item.Key, true)) != item.Value.sha256) throw UiText.Error("error.loaderUnknown");
                string settings = Safe(game, "ue4ss/UE4SS-settings.ini", true);
                if (!File.Exists(settings) || Regex.IsMatch(File.ReadAllText(settings), @"^\s*(ModsFolderPaths|ModsPath|ModPath)\s*=\s*[^;\s]", RegexOptions.Multiline | RegexOptions.IgnoreCase))
                    throw UiText.Error("error.loaderSettings");
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
                if (current != null && current != pair.Value) throw UiText.Error("error.legacyModified", pair.Key);
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
                if (!OwnedPath(relative, false) || !Regex.IsMatch(Convert.ToString(pair.Value), "^[0-9a-fA-F]{64}$")) throw UiText.Error("error.legacyReceipt");
                result.Add(relative, Convert.ToString(pair.Value).ToLowerInvariant());
            }
            return result;
        }
        private static void CheckTree(string game, string folder, string[] known) {
            foreach (string entry in Directory.GetFileSystemEntries(folder)) {
                string rel = entry.Substring(game.Length + 1).Replace('\\', '/');
                Safe(game, rel);
                if (Directory.Exists(entry)) CheckTree(game, entry, known);
                else if (!known.Contains(rel, StringComparer.OrdinalIgnoreCase)) throw UiText.Error("error.legacyExtra", rel);
            }
        }
        private static void AddChange(Plan plan, string relative, byte[] data) {
            plan.Before[relative] = Hash(Safe(plan.Game, relative));
            plan.Changes.Add(relative, data);
        }
        internal void Restore(string game, string journalFile) {
            AuthorSignature.VerifyAllResources();
            Closed();
            string file = Safe(game, "AC7MultiSP-backup/" + Path.GetFileName(Path.GetDirectoryName(journalFile)) + "/transaction.json");
            var journal = Read<Journal>(file);
            if (journal.schema != 1 || !String.Equals(journal.game, game, StringComparison.OrdinalIgnoreCase) || journal.entries == null ||
                !new[] { "prepared", "applying", "rollback-failed" }.Contains(journal.status)) throw UiText.Error("error.restoreRecord");
            string baseRel = "AC7MultiSP-backup/" + Path.GetFileName(Path.GetDirectoryName(file));
            foreach (var entry in journal.entries.AsEnumerable().Reverse()) {
                if (!OwnedPath(entry.relative, true)) throw UiText.Error("error.restoreOutside");
                string target = Safe(game, entry.relative), current = Hash(target);
                if (current == entry.before) continue;
                if (current != entry.after) throw UiText.Error("error.restoreModified", entry.relative);
                if (entry.before != null) {
                    string backup = Safe(game, baseRel + "/original/" + entry.relative);
                    if (Hash(backup) != entry.before) throw UiText.Error("error.backupDamaged", entry.relative);
                    string temporary = Safe(game, entry.relative + ".restore-" + Guid.NewGuid().ToString("N"));
                    Directory.CreateDirectory(Path.GetDirectoryName(target)); File.Copy(backup, temporary);
                    if (File.Exists(target)) File.Replace(temporary, target, null); else File.Move(temporary, target);
                } else if (File.Exists(target)) {
                    string moved = Safe(game, baseRel + "/rollback-removed/" + entry.relative);
                    Directory.CreateDirectory(Path.GetDirectoryName(moved)); File.Move(target, moved);
                }
                if (Hash(target) != entry.before) throw UiText.Error("error.restoreHash");
            }
            journal.status = "rolled-back"; Save(file, journal);
        }
        internal string Apply(Plan plan, Action<string> progress) {
            AuthorSignature.VerifyAllResources();
            Closed(); CheckManager(plan.Game);
            using (var locked = new FileStream(Safe(plan.Game, "AC7MultiSP-install.lock"), FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.None)) {
                foreach (var snapshot in plan.Before) if (Hash(Safe(plan.Game, snapshot.Key, !plan.Changes.ContainsKey(snapshot.Key))) != snapshot.Value) throw UiText.Error("error.targetChanged", snapshot.Key);
                string baseRel = "AC7MultiSP-backup/" + DateTime.Now.ToString("yyyyMMdd-HHmmss") + "-" + Guid.NewGuid().ToString("N");
                string journalFile = Safe(plan.Game, baseRel + "/transaction.json");
                var journal = new Journal { schema = 1, game = plan.Game, status = "prepared", entries = new List<Entry>() };
                // 暂存、备份全部成功后，才写持久日志并修改游戏文件。
                foreach (var pair in plan.Changes) {
                    Closed(); string target = Safe(plan.Game, pair.Key);
                    if (Hash(target) != plan.Before[pair.Key]) throw UiText.Error("error.targetChanged", pair.Key);
                    if (pair.Value != null) {
                        string staged = Safe(plan.Game, baseRel + "/staged/" + pair.Key);
                        Directory.CreateDirectory(Path.GetDirectoryName(staged)); File.WriteAllBytes(staged, pair.Value);
                        if (Hash(staged) != Digest(pair.Value)) throw UiText.Error("error.stageHash");
                    }
                    if (plan.Before[pair.Key] != null) {
                        string original = Safe(plan.Game, baseRel + "/original/" + pair.Key);
                        Directory.CreateDirectory(Path.GetDirectoryName(original)); File.Copy(target, original);
                        if (Hash(original) != plan.Before[pair.Key]) throw UiText.Error("error.backupHash");
                    }
                    journal.entries.Add(new Entry { relative = pair.Key, before = plan.Before[pair.Key], after = pair.Value == null ? null : Digest(pair.Value) });
                }
                string config = Safe(plan.Game, Mod + "profiles.json");
                if (File.Exists(config)) {
                    string backup = Safe(plan.Game, baseRel + "/configuration/profiles.json");
                    Directory.CreateDirectory(Path.GetDirectoryName(backup)); string expected = Hash(config); File.Copy(config, backup);
                    if (Hash(backup) != expected || Hash(config) != expected) throw UiText.Error("error.configurationHash");
                }
                Save(journalFile, journal);
                try {
                    journal.status = "applying"; Save(journalFile, journal);
                    foreach (var entry in journal.entries) {
                        Closed(); progress(UiText.T("working.file", entry.relative));
                        string target = Safe(plan.Game, entry.relative);
                        if (Hash(target) != entry.before) throw UiText.Error("error.beforeWriteChanged");
                        Directory.CreateDirectory(Path.GetDirectoryName(target));
                        if (entry.after == null) {
                            string removed = Safe(plan.Game, baseRel + "/removed/" + entry.relative);
                            Directory.CreateDirectory(Path.GetDirectoryName(removed)); if (File.Exists(target)) File.Move(target, removed);
                        } else {
                            string staged = Safe(plan.Game, baseRel + "/staged/" + entry.relative);
                            if (File.Exists(target)) File.Replace(staged, target, null); else File.Move(staged, target);
                        }
                        if (Hash(target) != entry.after) throw UiText.Error("error.writeHash", entry.relative);
                    }
                    foreach (var entry in journal.entries) if (Hash(Safe(plan.Game, entry.relative)) != entry.after) throw UiText.Error("error.finalHash");
                    foreach (var snapshot in plan.Before) if (!plan.Changes.ContainsKey(snapshot.Key) && Hash(Safe(plan.Game, snapshot.Key, true)) != snapshot.Value) throw UiText.Error("error.preservedChanged", snapshot.Key);
                    journal.status = "completed"; Save(journalFile, journal);
                    return Path.GetDirectoryName(journalFile);
                } catch (Exception failure) {
                    try { Restore(plan.Game, journalFile); }
                    catch (Exception restore) { journal.status = "rollback-failed"; Save(journalFile, journal); throw UiText.ErrorWithInner("error.rollbackFailed", failure, journalFile, restore); }
                    throw UiText.ErrorWithInner("error.operationRolledBack", failure, failure);
                }
            }
        }
        internal void Export(string output) {
            AuthorSignature.VerifyAllResources();
            string temporary = output + ".partial-" + Guid.NewGuid().ToString("N");
            byte[] data = Bytes("installer/universal.zip");
            using (var stream = new FileStream(temporary, FileMode.CreateNew, FileAccess.Write)) stream.Write(data, 0, data.Length);
            if (Hash(temporary) != Digest(data)) throw UiText.Error("error.exportHash");
            File.Move(temporary, output);
        }
    }
}
