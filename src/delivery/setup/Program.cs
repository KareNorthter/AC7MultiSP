using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Net;
using System.Reflection;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;

namespace AC7MultiSP.Setup {
    internal sealed class UpdateLink {
        public string id { get; set; }
        public string url { get; set; }
        public string format { get; set; }
        public Dictionary<string, string> label { get; set; }
    }
    internal sealed class UpdateInfo {
        public int schema { get; set; }
        public string product { get; set; }
        public string channel { get; set; }
        public string status { get; set; }
        public string latest_version { get; set; }
        public Dictionary<string, string> release_notes { get; set; }
        public List<UpdateLink> downloads { get; set; }
        public List<UpdateLink> installer_downloads { get; set; }
    }
    internal static class UpdateFeed {
        internal static bool Allowed(Uri uri) {
            return uri.Scheme == "https" && uri.Port == 443 && String.IsNullOrEmpty(uri.UserInfo) && new[] {
                "www.nexusmods.com", "ac7multisp-updates.pages.dev", "downloads.iot-node-service-api.online", "wwbru.lanzoue.com", "learn.microsoft.com", "www.microsoft.com"
            }.Contains(uri.Host.ToLowerInvariant());
        }
        internal static UpdateInfo Fetch(string url, string setupVersion) {
            ServicePointManager.SecurityProtocol = SecurityProtocolType.Tls12;
            var request = (HttpWebRequest)WebRequest.Create(url);
            request.AllowAutoRedirect = false; request.Timeout = 5000; request.ReadWriteTimeout = 5000;
            request.Accept = "application/json"; request.UserAgent = "AC7MultiSP-Setup/" + setupVersion;
            request.Credentials = null; request.CookieContainer = null;
            using (var response = (HttpWebResponse)request.GetResponse()) {
                if (response.StatusCode != HttpStatusCode.OK || response.ContentLength > 32768 || response.ContentType == null || !response.ContentType.StartsWith("application/json", StringComparison.OrdinalIgnoreCase)) throw UiText.Error("error.updateResponse");
                using (var stream = response.GetResponseStream()) using (var buffer = new MemoryStream()) {
                    byte[] part = new byte[4096]; int count; var watch = Stopwatch.StartNew();
                    while ((count = stream.Read(part, 0, part.Length)) != 0) {
                        if (watch.ElapsedMilliseconds > 8000 || buffer.Length + count > 32768) throw UiText.Error("error.updateSize");
                        buffer.Write(part, 0, count);
                    }
                    var data = new JavaScriptSerializer { MaxJsonLength = 32768, RecursionLimit = 10 }.Deserialize<UpdateInfo>(Bundle.Utf8.GetString(buffer.ToArray()));
                    if (data == null || data.schema != 1 || data.product != "AC7MultiSP" || data.channel != "stable") throw UiText.Error("error.updateMismatch");
                    if (data.status == "preparing") return data;
                    if (data.status != "ready" || !Regex.IsMatch(data.latest_version ?? "", @"^\d+\.\d+\.\d+$")) throw UiText.Error("error.updateVersion");
                    if (data.downloads == null || data.downloads.Count == 0 || data.downloads.Count > 4 ||
                        data.installer_downloads != null && (data.installer_downloads.Count > 4 || data.installer_downloads.Any(link => link.format != "exe")))
                        throw UiText.Error("error.updateDownloads");
                    foreach (var entry in (data.downloads ?? new List<UpdateLink>()).Concat(data.installer_downloads ?? new List<UpdateLink>())) {
                        Uri uri;
                        if (entry == null || !Uri.TryCreate(entry.url, UriKind.Absolute, out uri) || !Allowed(uri)) throw UiText.Error("error.updateLink");
                    }
                    return data;
                }
            }
        }
    }
    internal static class Program {
        [STAThread] static void Main(string[] args) {
            Application.EnableVisualStyles(); Application.SetCompatibleTextRenderingDefault(false);
            try {
                if (args.Length == 2 && args[0] == "--export-zip") { new Bundle().Export(Path.GetFullPath(args[1])); return; }
                bool preview = false;
                string snapshot = null, page = "home";
                for (int i = 0; i < args.Length; ++i) {
                    if (i + 1 >= args.Length) throw UiText.Error("error.arguments");
                    string option = args[i], value = args[++i];
                    if (option == "--preview" && (value == "zh-CN" || value == "en")) { preview = true; UiText.Language = value; }
                    else if (option == "--snapshot") snapshot = Path.GetFullPath(value);
                    else if (option == "--preview-page" && (value == "home" || value == "direct")) page = value;
                    else throw UiText.Error("error.arguments");
                }
                if (args.Length != 0 && (!preview || snapshot == null)) throw UiText.Error("error.arguments");
                using (var window = new MainWindow(new Bundle(), preview, page, snapshot)) {
                    Application.Run(window);
                    if (window.PreviewFailure != null) throw window.PreviewFailure;
                }
            } catch (Exception failure) {
                Environment.ExitCode = 1;
                UiDialog.Show(null, UiText.T("error.start", UiText.Explain(failure)), UiText.T("app.title"));
            }
        }
    }
}
