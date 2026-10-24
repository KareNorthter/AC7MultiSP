using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Web.Script.Serialization;

namespace AC7MultiSP.Setup {
    // 只验证作者发行字节；不检查玩家配置，不联网、不写游戏目录。
    internal static class AuthorSignature {
        const int Limit = 65536;
        const string Algorithm = "RSA-PSS-SHA256";
        const string Content = "ue4ss/Mods/DualSP/author-content.json";
        const string ContentSignature = "ue4ss/Mods/DualSP/author-content.sig.json";
        static readonly byte[] Domain = Encoding.ASCII.GetBytes("AC7MultiSP.signature.v1\0");
        static readonly UTF8Encoding Utf8 = new UTF8Encoding(false, true);
        static readonly object Sync = new object();
        static BundleInfo info;
        static bool ready, allChecked;
        internal static bool Ready { get { return ready; } }
        internal static BundleInfo Info { get { Initialize(); return info; } }
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        struct Pss { [MarshalAs(UnmanagedType.LPWStr)] public string algorithm; public int saltLength; }
        [DllImport("bcrypt.dll", CharSet = CharSet.Unicode)]
        static extern int BCryptOpenAlgorithmProvider(out IntPtr algorithm, string id, string implementation, int flags);
        [DllImport("bcrypt.dll", CharSet = CharSet.Unicode)]
        static extern int BCryptImportKeyPair(IntPtr algorithm, IntPtr importKey, string type, out IntPtr key, byte[] blob, int length, int flags);
        [DllImport("bcrypt.dll")]
        static extern int BCryptVerifySignature(IntPtr key, ref Pss padding, byte[] hash, int hashLength, byte[] signature, int signatureLength, int flags);
        [DllImport("bcrypt.dll")] static extern int BCryptDestroyKey(IntPtr key);
        [DllImport("bcrypt.dll")] static extern int BCryptCloseAlgorithmProvider(IntPtr algorithm, int flags);
        static void Require(bool valid) { if (!valid) throw new InvalidDataException("AUTHOR_SIGNATURE_INVALID"); }
        static JavaScriptSerializer Json() { return new JavaScriptSerializer { MaxJsonLength = Limit, RecursionLimit = 20 }; }
        static string Text(Dictionary<string, object> value, string key) { object found; Require(value.TryGetValue(key, out found) && found is string); return (string)found; }
        static long Number(Dictionary<string, object> value, string key) {
            object found; Require(value.TryGetValue(key, out found) && (found is int || found is long));
            return Convert.ToInt64(found, CultureInfo.InvariantCulture);
        }
        static Dictionary<string, object> Object(object value) { Require(value is Dictionary<string, object>); return (Dictionary<string, object>)value; }
        static void Fields(Dictionary<string, object> value, params string[] names) { Require(value.Count == names.Length && names.All(value.ContainsKey)); }
        static byte[] Hash(byte[] value) { using (var sha = SHA256.Create()) return sha.ComputeHash(value); }
        static string Hex(byte[] value) { return BitConverter.ToString(value).Replace("-", "").ToLowerInvariant(); }
        static string HashText(byte[] value) { return Hex(Hash(value)); }
        internal static void ValidPath(string name) {
            Require(!String.IsNullOrEmpty(name) && name.Length <= 240);
            Require(!name.Any(c => c < 32 || c == 127 || Char.IsSurrogate(c) || "\\:*?\"<>|".Contains(c)));
            foreach (string part in name.Split('/')) Require(part.Length > 0 && part != "." && part != ".." &&
                !part.EndsWith(".") && !part.EndsWith(" ") && !Regex.IsMatch(part, @"^(CON|PRN|AUX|NUL|COM[0-9]|LPT[0-9])(\.|$)", RegexOptions.IgnoreCase));
        }
        static Dictionary<string, object> Parse(byte[] raw, int limit) {
            Require(raw.Length > 0 && raw.Length <= limit);
            string text = Utf8.GetString(raw);
            new JsonGuard(text).Check();
            return Object(Json().DeserializeObject(text));
        }
        static byte[] SmallResource(string name, int maximum) {
            using (var stream = Assembly.GetExecutingAssembly().GetManifestResourceStream(name)) {
                Require(stream != null && stream.Length > 0 && stream.Length <= maximum);
                byte[] bytes = new byte[(int)stream.Length]; int offset = 0;
                while (offset < bytes.Length) { int count = stream.Read(bytes, offset, bytes.Length - offset); Require(count > 0); offset += count; }
                Require(stream.ReadByte() == -1); return bytes;
            }
        }
        static Dictionary<string, object> Verify(byte[] raw, byte[] signature, string purpose, string version) {
            Require(raw.Length > 0 && raw.Length <= Limit);
            var envelope = Parse(signature, 2048);
            Fields(envelope, "schema", "algorithm", "key_id", "signature");
            Require(Number(envelope, "schema") == 1 && Text(envelope, "algorithm") == Algorithm);
            string keyId = Text(envelope, "key_id"); byte[] blob;
            Require(Regex.IsMatch(keyId, @"\A[0-9a-f]{64}\z") && TrustedAuthorKeys.Values.TryGetValue(keyId, out blob));
            // 再取一次避免短路分支造成编译器无法证明out参数已赋值。
            blob = TrustedAuthorKeys.Values[keyId];
            Require(HashText(blob) == keyId && blob.Length == 411 && BitConverter.ToUInt32(blob, 0) == 0x31415352 &&
                BitConverter.ToUInt32(blob, 4) == 3072 && BitConverter.ToUInt32(blob, 8) == 3 && BitConverter.ToUInt32(blob, 12) == 384 &&
                BitConverter.ToUInt32(blob, 16) == 0 && BitConverter.ToUInt32(blob, 20) == 0 && blob[24] == 1 && blob[25] == 0 && blob[26] == 1);
            byte[] signed = Convert.FromBase64String(Text(envelope, "signature")); Require(signed.Length == 384);
            byte[] message = new byte[Domain.Length + raw.Length]; Domain.CopyTo(message, 0); raw.CopyTo(message, Domain.Length);
            IntPtr algorithm = IntPtr.Zero, key = IntPtr.Zero;
            try {
                Require(BCryptOpenAlgorithmProvider(out algorithm, "RSA", null, 0) == 0);
                Require(BCryptImportKeyPair(algorithm, IntPtr.Zero, "RSAPUBLICBLOB", out key, blob, blob.Length, 0) == 0);
                var padding = new Pss { algorithm = "SHA256", saltLength = 32 }; byte[] hash = Hash(message);
                Require(BCryptVerifySignature(key, ref padding, hash, hash.Length, signed, signed.Length, 8) == 0);
            } finally {
                if (key != IntPtr.Zero) BCryptDestroyKey(key);
                if (algorithm != IntPtr.Zero) BCryptCloseAlgorithmProvider(algorithm, 0);
            }
            var manifest = Parse(raw, Limit);
            string[] common = { "schema", "product", "purpose", "version", "channel", "release_id", "source_commit", "files" };
            Fields(manifest, purpose == "bundle" ? common.Concat(new[] { "candidate", "delivery_version", "zip_name", "universal_sha256", "policy" }).ToArray() : common);
            string actualVersion = Text(manifest, "version"), channel = Text(manifest, "channel");
            Require(Number(manifest, "schema") == 1 && Text(manifest, "product") == "AC7MultiSP" && Text(manifest, "purpose") == purpose &&
                ReleaseVersion.Valid(actualVersion) && (version == null || actualVersion == version) &&
                (channel == "stable" || channel == "candidate") && (!actualVersion.Contains(" alpha") || channel == "candidate") &&
                Regex.IsMatch(Text(manifest, "release_id"), @"\A[0-9a-f]{32}\z") && Regex.IsMatch(Text(manifest, "source_commit"), @"\A[0-9a-f]{40}\z"));
            var files = Object(manifest["files"]); Require(files.Count > 0 && files.Count <= 256);
            var paths = new HashSet<string>(StringComparer.OrdinalIgnoreCase); var resources = new HashSet<string>(StringComparer.Ordinal);
            long total = 0;
            foreach (var pair in files) {
                ValidPath(pair.Key); Require(paths.Add(pair.Key)); var record = Object(pair.Value);
                Fields(record, purpose == "bundle" ? new[] { "size", "sha256", "resource", "kind" } : new[] { "size", "sha256" });
                long size = Number(record, "size"); Require(size >= 0 && size <= 268435456); total += size;
                Require(Regex.IsMatch(Text(record, "sha256"), @"\A[0-9a-f]{64}\z"));
                if (purpose == "bundle") Require(Regex.IsMatch(Text(record, "resource"), @"\Apayload\.[0-9a-f]{64}\z") &&
                    resources.Add(Text(record, "resource")) && new[] { "mod", "loader", "document", "archive", "interface" }.Contains(Text(record, "kind")));
            }
            Require(total <= 1073741824);
            return manifest;
        }
        internal static void Initialize() {
            lock (Sync) {
                if (ready) return;
                string version = ((AssemblyInformationalVersionAttribute)Assembly.GetExecutingAssembly().GetCustomAttributes(typeof(AssemblyInformationalVersionAttribute), false).Single()).InformationalVersion;
                byte[] raw = SmallResource("bundle.json", Limit);
                var manifest = Verify(raw, SmallResource("bundle.sig.json", 2048), "bundle", version);
                Require(manifest["candidate"] is bool);
                info = Json().Deserialize<BundleInfo>(Utf8.GetString(raw));
                Require(info.schema == 1 && info.version == version && info.delivery_version == version &&
                    (!version.Contains(" alpha") || info.candidate) && Text(manifest, "channel") == (info.candidate ? "candidate" : "stable") &&
                    info.zip_name == "AC7MultiSP-" + version + "-Universal.zip" &&
                    Regex.IsMatch(Text(manifest, "universal_sha256"), @"\A[0-9a-f]{64}\z"));
                // 初始化期间不调用UiText，失败提示使用同源生成的最小常量。
                var content = Verify(ReadBytes(Content), ReadBytes(ContentSignature), "content", version);
                Require(Text(content, "release_id") == Text(manifest, "release_id") && Text(content, "source_commit") == Text(manifest, "source_commit") &&
                    Text(content, "channel") == Text(manifest, "channel"));
                var contentFiles = Object(content["files"]);
                var modFiles = info.files.Where(p => p.Value.kind == "mod" && p.Key != Content && p.Key != ContentSignature).ToArray();
                Require(contentFiles.Count == modFiles.Length && info.files[Content].kind == "mod" && info.files[ContentSignature].kind == "mod");
                foreach (var pair in modFiles) {
                    object found; Require(contentFiles.TryGetValue(pair.Key, out found)); var entry = Object(found);
                    Require(Number(entry, "size") == pair.Value.size && Text(entry, "sha256") == pair.Value.sha256 && Bundle.OwnedPath(pair.Key, false));
                }
                foreach (string language in new[] { "zh-CN", "en" }) {
                    string path = "installer/ui/" + language + ".json";
                    Require(info.files[path].kind == "interface");
                    byte[] uiBytes = ReadBytes(path);
                    if (uiBytes.Length >= 3 && uiBytes[0] == 239 && uiBytes[1] == 187 && uiBytes[2] == 191) uiBytes = uiBytes.Skip(3).ToArray();
                    var texts = Parse(uiBytes, Limit);
                    Require(texts.Count > 0 && texts.Values.All(v => v is string));
                }
                Require(info.files["installer/universal.zip"].kind == "archive" && info.files["installer/universal.zip"].sha256 == Text(manifest, "universal_sha256"));
                ready = true;
            }
        }
        static Stream Open(ResourceFile entry) {
            var stream = Assembly.GetExecutingAssembly().GetManifestResourceStream(entry.resource);
            if (stream == null) throw new InvalidDataException("AUTHOR_RESOURCE_MISSING");
            if (stream.Length != entry.size) { stream.Dispose(); throw new InvalidDataException("AUTHOR_RESOURCE_SIZE"); }
            return stream;
        }
        static byte[] ReadBytes(string relative) {
            ResourceFile entry; Require(info != null && info.files.TryGetValue(relative, out entry)); entry = info.files[relative];
            Require(entry.size <= 268435456);
            using (var stream = Open(entry)) using (var output = new MemoryStream()) {
                stream.CopyTo(output); byte[] bytes = output.ToArray(); Require(bytes.LongLength == entry.size && HashText(bytes) == entry.sha256); return bytes;
            }
        }
        internal static byte[] Bytes(string relative) { Initialize(); return ReadBytes(relative); }
        internal static void VerifyAllResources() {
            Initialize();
            try {
                lock (Sync) {
                    if (allChecked) return;
                    foreach (var pair in info.files) using (var stream = Open(pair.Value)) using (var sha = SHA256.Create())
                        Require(Hex(sha.ComputeHash(stream)) == pair.Value.sha256);
                    allChecked = true;
                }
            } catch (Exception failure) { throw UiText.ErrorWithInner("error.authorIntegrity", failure); }
        }
        internal static string VerifyReleaseFolder(string folder) {
            folder = Path.GetFullPath(folder);
            var manifest = Verify(ReadFile(Path.Combine(folder, "AC7MultiSP-release.json"), Limit),
                ReadFile(Path.Combine(folder, "AC7MultiSP-release.sig.json"), 2048), "release", null);
            foreach (var pair in Object(manifest["files"])) {
                Require(!pair.Key.Contains('/')); string path = Path.Combine(folder, pair.Key); var record = Object(pair.Value);
                Require((File.GetAttributes(path) & FileAttributes.ReparsePoint) == 0 && new FileInfo(path).Length == Number(record, "size"));
                using (var stream = File.OpenRead(path)) using (var sha = SHA256.Create()) Require(Hex(sha.ComputeHash(stream)) == Text(record, "sha256"));
            }
            return Text(manifest, "version");
        }
        static byte[] ReadFile(string file, int maximum) {
            using (var stream = File.OpenRead(file)) {
                Require(stream.Length > 0 && stream.Length <= maximum); byte[] bytes = new byte[(int)stream.Length]; int offset = 0;
                while (offset < bytes.Length) { int count = stream.Read(bytes, offset, bytes.Length - offset); Require(count > 0); offset += count; }
                Require(stream.ReadByte() == -1); return bytes;
            }
        }
        // JavaScriptSerializer会接受重复键；先按JSON语法逐对象拒绝重复及超深输入。
        sealed class JsonGuard {
            readonly string source; int position;
            internal JsonGuard(string value) { source = value; }
            internal void Check() { Value(0); Space(); Require(position == source.Length); }
            void Space() { while (position < source.Length && " \t\r\n".Contains(source[position])) ++position; }
            bool Take(char value) { Space(); if (position < source.Length && source[position] == value) { ++position; return true; } return false; }
            string Quoted() {
                Space(); int start = position; Require(position < source.Length && source[position++] == '"');
                while (position < source.Length) {
                    char c = source[position++]; Require(c >= 32);
                    if (c == '"') return Json().Deserialize<string>(source.Substring(start, position - start));
                    if (c == '\\') {
                        Require(position < source.Length); char escaped = source[position++];
                        Require("\"\\/bfnrtu".Contains(escaped));
                        if (escaped == 'u') for (int i = 0; i < 4; ++i) { Require(position < source.Length && Uri.IsHexDigit(source[position])); ++position; }
                    }
                }
                Require(false); return null;
            }
            void Value(int depth) {
                Require(depth <= 20); Space(); Require(position < source.Length);
                if (Take('{')) {
                    var keys = new HashSet<string>(StringComparer.Ordinal); if (Take('}')) return;
                    do { Require(keys.Add(Quoted())); Require(Take(':')); Value(depth + 1); } while (Take(','));
                    Require(Take('}')); return;
                }
                if (Take('[')) {
                    if (Take(']')) return; do { Value(depth + 1); } while (Take(',')); Require(Take(']')); return;
                }
                if (source[position] == '"') { Quoted(); return; }
                foreach (string literal in new[] { "true", "false", "null" }) if (source.Substring(position).StartsWith(literal, StringComparison.Ordinal)) { position += literal.Length; return; }
                var number = Regex.Match(source.Substring(position), @"\A-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?");
                Require(number.Success); position += number.Length;
            }
        }
    }
}
