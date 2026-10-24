"use strict";

(() => {
  const copy = /* SITE_UI_COPY */ {};

  const documents = {
    features: { "zh-CN": "功能说明.txt", en: "Features.txt" },
    install: { "zh-CN": "安装说明.txt", en: "Installation.txt" },
    changelog: { "zh-CN": "更新日志.txt", en: "Changelog.txt" },
    author: { "zh-CN": "作者与授权.txt", en: "Author-and-License.txt" }
  };
  const downloadHosts = new Set([
    "wwbru.lanzoue.com",
    "downloads.iot-node-service-api.online",
    "ac7multisp-updates.pages.dev"
  ]);
  const textCache = new Map();
  const elements = {
    legalNotice: document.getElementById("legalNotice"),
    profileLinks: document.getElementById("profileLinks"),
    feedbackLinks: document.getElementById("feedbackLinks"),
    releaseStatus: document.getElementById("releaseStatus"),
    latestVersion: document.getElementById("latestVersion"),
    downloadHint: document.getElementById("downloadHint"),
    downloadActions: document.getElementById("downloadActions"),
    releaseNotes: document.getElementById("releaseNotes"),
    releaseNotesText: document.getElementById("releaseNotesText"),
    documentPanel: document.getElementById("documentPanel"),
    documentTitle: document.getElementById("documentTitle"),
    documentBody: document.getElementById("documentBody")
  };
  let language = "zh-CN";
  let activeSection = "features";
  let manifest = null;
  let authorRenderRequest = 0;
  let documentRenderRequest = 0;

  function currentCopy() {
    return copy[language];
  }

  function applyLanguage() {
    const strings = currentCopy();
    document.documentElement.lang = language;
    document.querySelectorAll("[data-i18n]").forEach((node) => {
      const key = node.dataset.i18n;
      if (strings[key]) node.textContent = strings[key];
    });
    document.querySelectorAll("[data-i18n-aria]").forEach((node) => {
      const key = node.dataset.i18nAria;
      if (strings[key]) node.setAttribute("aria-label", strings[key]);
    });
    document.querySelector('meta[name="description"]').setAttribute("content", strings.metaDescription);
    document.querySelectorAll("[data-language]").forEach((button) => {
      button.setAttribute("aria-pressed", String(button.dataset.language === language));
    });
    document.querySelectorAll("[data-i18n-alt]").forEach((node) => {
      node.alt = strings[node.dataset.i18nAlt];
    });
    renderManifest();
    renderAuthors();
    renderDocument();
  }

  function validHttpsUrl(raw) {
    if (typeof raw !== "string" ||
        raw.length > 2048 ||
        [...raw].some((character) => {
          const code = character.charCodeAt(0);
          return code <= 32 || code >= 127 || character === '"' || character === "'" || character === "\\";
        })) return false;
    try {
      const url = new URL(raw);
      return url.protocol === "https:" &&
        (url.port === "" || url.port === "443") &&
        !url.username && !url.password &&
        downloadHosts.has(url.hostname.toLowerCase());
    } catch {
      return false;
    }
  }

  function parseManifest(value) {
    if (!value || value.schema !== 1 || value.product !== "AC7MultiSP" || value.channel !== "stable") {
      throw new Error("Unsupported manifest");
    }
    if (value.status === "preparing") return { status: "preparing" };
    if (value.status !== "ready" ||
        typeof value.latest_version !== "string" ||
        !/^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$/.test(value.latest_version) ||
        !value.release_notes ||
        typeof value.release_notes.zh !== "string" ||
        typeof value.release_notes.en !== "string" ||
        !value.release_notes.zh.trim() ||
        !value.release_notes.en.trim() ||
        !Array.isArray(value.downloads) ||
        value.downloads.length < 2 ||
        value.downloads.length > 4) {
      throw new Error("Incomplete manifest");
    }
    const ids = new Set();
    for (const item of value.downloads) {
      if (!item ||
          typeof item.id !== "string" ||
          !/^[a-z0-9_-]{1,32}$/.test(item.id) ||
          ids.has(item.id) ||
          !item.label ||
          typeof item.label.zh !== "string" ||
          typeof item.label.en !== "string" ||
          !item.label.zh.trim() ||
          !item.label.en.trim() ||
          typeof item.url !== "string" ||
          item.url.length > 2048 ||
          !validHttpsUrl(item.url)) {
        throw new Error("Invalid download entry");
      }
      ids.add(item.id);
    }
    const primary = value.downloads.filter((item) => item.id === "r2");
    const backups = value.downloads.filter((item) => item.id !== "r2");
    if (primary.length !== 1 || backups.length < 1 || backups.length > 3 ||
        backups.some((item) => !item.id.startsWith("lanzou") ||
          new URL(item.url).hostname.toLowerCase() !== "wwbru.lanzoue.com")) {
      throw new Error("Manifest must contain one R2 link and one to three Lanzou backups");
    }
    return value;
  }

  function setStatus(kind, label, hint) {
    elements.releaseStatus.className = "status-chip";
    if (kind === "ready") elements.releaseStatus.classList.add("is-ready");
    if (kind === "error") elements.releaseStatus.classList.add("is-error");
    elements.releaseStatus.textContent = label;
    elements.downloadHint.textContent = hint;
  }

  function renderManifest() {
    elements.downloadActions.replaceChildren();
    elements.releaseNotes.hidden = true;
    elements.latestVersion.textContent = manifest && manifest.status === "ready"
      ? manifest.latest_version
      : "—";
    if (!manifest) {
      setStatus("error", currentCopy().invalidStatus, currentCopy().invalidHint);
      return;
    }
    if (manifest.status === "preparing") {
      setStatus("preparing", currentCopy().preparingStatus, currentCopy().preparingHint);
      appendDownloadGroup(currentCopy().mainDownloadTitle, [], currentCopy().preparingMain, true);
      appendDownloadGroup(currentCopy().backupDownloadsTitle, [], currentCopy().preparingBackup, false);
      return;
    }
    setStatus("ready", currentCopy().readyStatus, currentCopy().readyHint);
    const primary = manifest.downloads.filter((item) => item.id === "r2");
    const backups = manifest.downloads.filter((item) => item.id !== "r2");
    appendDownloadGroup(currentCopy().mainDownloadTitle, primary, "", true);
    appendDownloadGroup(currentCopy().backupDownloadsTitle, backups, "", false);
    elements.releaseNotesText.textContent = manifest.release_notes[language === "zh-CN" ? "zh" : "en"];
    elements.releaseNotes.hidden = false;
  }

  function appendDownloadGroup(title, items, pendingText, primary) {
    const group = document.createElement("section");
    group.className = "download-group";
    const heading = document.createElement("h3");
    heading.className = "download-group-title";
    heading.textContent = title;
    group.append(heading);
    const links = document.createElement("div");
    links.className = "download-group-links";
    if (pendingText) {
      const button = document.createElement("button");
      button.type = "button";
      button.disabled = true;
      button.className = "download-pending";
      button.textContent = pendingText;
      links.append(button);
    }
    items.forEach((item) => {
      const link = document.createElement("a");
      link.className = primary ? "download-button" : "download-button secondary";
      link.href = item.url;
      link.target = "_blank";
      link.rel = "noopener noreferrer";
      link.textContent = item.label[language === "zh-CN" ? "zh" : "en"];
      links.append(link);
    });
    group.append(links);
    elements.downloadActions.append(group);
  }

  async function readDocument(key) {
    const filename = documents[key] && documents[key][language];
    if (!filename) throw new Error("Unknown document");
    const cacheKey = language + "/" + filename;
    if (textCache.has(cacheKey)) return textCache.get(cacheKey);
    const response = await fetch("./user-docs/" + encodeURIComponent(filename), { cache: "force-cache" });
    if (!response.ok) throw new Error("Document request failed");
    const text = await response.text();
    textCache.set(cacheKey, text);
    return text;
  }

  async function renderAuthors() {
    const request = ++authorRenderRequest;
    const requestedLanguage = language;
    elements.legalNotice.textContent = currentCopy().loadingAuthor;
    elements.profileLinks.replaceChildren();
    elements.feedbackLinks.replaceChildren();
    try {
      const text = await readDocument("author");
      if (request !== authorRenderRequest || requestedLanguage !== language) return;
      const lines = text.split(/\r?\n/);
      const productTitle = lines.findIndex((line) => line.trim() === "AC7MultiSP");
      const notice = productTitle < 0 ? "" : lines.slice(productTitle + 1).map((line) => line.trim()).find(Boolean);
      const startTitle = language === "zh-CN" ? "作者：" : "Author:";
      const endTitle = language === "zh-CN" ? "反馈：" : "Feedback:";
      const start = lines.findIndex((line) => line.trim() === startTitle);
      const end = lines.findIndex((line, index) => index > start && line.trim() === endTitle);
      if (!notice || start < 0 || end < 0) throw new Error("Author section missing");
      const profiles = [];
      for (let index = start + 1; index < end; index += 1) {
        const line = lines[index].trim();
        if (!line.startsWith("• ")) continue;
        const details = line.slice(2);
        const separator = Math.max(details.indexOf("："), details.indexOf(":"));
        if (separator < 1) continue;
        let next = index + 1;
        while (next < end && !lines[next].trim()) next += 1;
        const url = lines[next] ? lines[next].trim() : "";
        if (!validProfileUrl(url)) continue;
        profiles.push({
          label: details.slice(0, separator).trim(),
          name: details.slice(separator + 1).trim(),
          url
        });
      }
      if (!profiles.length) throw new Error("No author links");
      elements.legalNotice.textContent = notice;
      profiles.forEach((entry) => {
        const link = document.createElement("a");
        link.href = entry.url;
        link.target = "_blank";
        link.rel = "noopener noreferrer";
        link.textContent = entry.label + (language === "zh-CN" ? "：" : ": ") + entry.name;
        elements.profileLinks.append(link);
      });
      const feedbackLines = [];
      for (let index = end + 1; index < lines.length; index += 1) {
        const line = lines[index].trim();
        if (line && !line.startsWith("• ")) break;
        if (line.startsWith("• ")) feedbackLines.push(line.slice(2));
      }
      feedbackLines.forEach((details) => {
        const separator = Math.max(details.indexOf("："), details.indexOf(":"));
        if (separator < 1) return;
        const label = details.slice(0, separator).trim();
        const value = details.slice(separator + 1).trim();
        const entry = document.createElement("span");
        const email = value.match(/^[A-Z0-9._%+-]+@[A-Z0-9.-]+\.[A-Z]{2,}$/i);
        if (email) {
          const link = document.createElement("a");
          link.href = "mailto:" + value;
          link.textContent = value;
          entry.append(document.createTextNode(label + (language === "zh-CN" ? "：" : ": ")), link);
        } else {
          entry.textContent = label + (language === "zh-CN" ? "：" : ": ") + value;
        }
        elements.feedbackLinks.append(entry);
      });
    } catch {
      if (request !== authorRenderRequest || requestedLanguage !== language) return;
      elements.legalNotice.textContent = currentCopy().authorInfoUnavailable;
      [elements.profileLinks, elements.feedbackLinks].forEach((container) => {
        const fallback = document.createElement("span");
        fallback.className = "muted";
        fallback.textContent = currentCopy().authorInfoUnavailable;
        container.replaceChildren(fallback);
      });
    }
  }

  function validProfileUrl(raw) {
    try {
      const url = new URL(raw);
      return url.protocol === "https:" && !url.username && !url.password;
    } catch {
      return false;
    }
  }

  async function renderDocument() {
    const request = ++documentRenderRequest;
    const requestedLanguage = language;
    const requestedSection = activeSection;
    const selected = document.querySelector('[data-section="' + activeSection + '"]');
    if (selected) {
      elements.documentPanel.setAttribute("aria-labelledby", selected.id);
      elements.documentTitle.textContent = currentCopy()[selected.dataset.i18n];
    }
    elements.documentBody.replaceChildren();
    const loading = document.createElement("p");
    loading.className = "document-loading";
    loading.textContent = currentCopy().loadingDocument;
    elements.documentBody.append(loading);
    try {
      const text = await readDocument(requestedSection);
      if (request !== documentRenderRequest ||
          requestedLanguage !== language ||
          requestedSection !== activeSection) return;
      const paragraph = document.createElement("p");
      paragraph.textContent = text.replace(/\r\n/g, "\n").trimEnd();
      elements.documentBody.replaceChildren(paragraph);
    } catch {
      if (request !== documentRenderRequest ||
          requestedLanguage !== language ||
          requestedSection !== activeSection) return;
      const error = document.createElement("p");
      error.textContent = currentCopy().documentLoadError;
      elements.documentBody.replaceChildren(error);
    }
  }

  async function loadManifest() {
    try {
      const response = await fetch("./stable.json", { cache: "no-store" });
      if (!response.ok) throw new Error("Manifest request failed");
      manifest = parseManifest(await response.json());
    } catch {
      manifest = null;
    }
    renderManifest();
  }

  const feedbackDialog = document.getElementById("feedbackDialog");
  const feedbackOpen = document.getElementById("feedbackOpen");
  const feedbackEmail = document.getElementById("feedbackEmail");
  const feedbackSend = document.getElementById("feedbackSend");
  const feedbackCopy = document.getElementById("feedbackCopy");
  const feedbackStatus = document.getElementById("feedbackStatus");
  feedbackOpen.addEventListener("click", async () => {
    feedbackStatus.textContent = "";
    feedbackEmail.value = "";
    feedbackSend.disabled = feedbackCopy.disabled = true;
    feedbackDialog.showModal();
    document.body.classList.add("support-is-open");
    try {
      // 与作者区共用正文邮箱，不另设联系方式副本。
      const author = await readDocument("author");
      const email = author.match(/[A-Z0-9._%+-]+@[A-Z0-9.-]+\.[A-Z]{2,}/i);
      if (!email) throw new Error("Missing feedback email");
      feedbackEmail.value = email[0];
      feedbackSend.disabled = feedbackCopy.disabled = false;
    } catch {
      feedbackStatus.textContent = currentCopy().feedbackEmailError;
    }
  });
  document.getElementById("feedbackClose").addEventListener("click", () => feedbackDialog.close());
  feedbackDialog.addEventListener("click", (event) => {
    const bounds = feedbackDialog.getBoundingClientRect();
    if (event.target === feedbackDialog &&
        (event.clientX < bounds.left || event.clientX > bounds.right ||
         event.clientY < bounds.top || event.clientY > bounds.bottom)) feedbackDialog.close();
  });
  feedbackDialog.addEventListener("close", () => {
    if (!document.querySelector("dialog[open]")) document.body.classList.remove("support-is-open");
    feedbackOpen.focus();
  });
  feedbackSend.addEventListener("click", () => {
    window.location.href = "mailto:" + feedbackEmail.value +
      "?subject=" + encodeURIComponent(currentCopy().feedbackSubject) +
      "&body=" + encodeURIComponent(currentCopy().feedbackBody);
  });
  feedbackCopy.addEventListener("click", async () => {
    try {
      await navigator.clipboard.writeText(feedbackEmail.value);
      feedbackStatus.textContent = currentCopy().feedbackCopied;
    } catch {
      feedbackEmail.focus();
      feedbackEmail.select();
      feedbackStatus.textContent = currentCopy().feedbackCopyFallback;
    }
  });

  const supportDialog = document.getElementById("supportDialog");
  const supportOpen = document.getElementById("supportOpen");
  function syncSupportHash() {
    if (/^#coffee\/?$/.test(window.location.hash)) {
      if (!supportDialog.open) supportDialog.showModal();
      document.body.classList.add("support-is-open");
    } else if (supportDialog.open) {
      supportDialog.close();
    }
  }
  supportOpen.addEventListener("click", () => {
    if (!/^#coffee\/?$/.test(window.location.hash)) window.location.hash = "coffee";
    syncSupportHash();
  });
  document.getElementById("supportClose").addEventListener("click", () => supportDialog.close());
  supportDialog.addEventListener("click", (event) => {
    const bounds = supportDialog.getBoundingClientRect();
    if (event.target === supportDialog &&
        (event.clientX < bounds.left || event.clientX > bounds.right ||
         event.clientY < bounds.top || event.clientY > bounds.bottom)) supportDialog.close();
  });
  supportDialog.addEventListener("close", () => {
    document.body.classList.remove("support-is-open");
    if (/^#coffee\/?$/.test(window.location.hash)) {
      history.replaceState(history.state, "", window.location.pathname + window.location.search);
    }
    supportOpen.focus();
  });
  window.addEventListener("hashchange", syncSupportHash);

  document.querySelectorAll("[data-language]").forEach((button) => {
    button.addEventListener("click", () => {
      language = button.dataset.language;
      try {
        localStorage.setItem("ac7multisp-language", language);
      } catch {
        // 隐私浏览模式下无法保存偏好时仍可切换本页语言。
      }
      applyLanguage();
    });
  });

  document.querySelectorAll("[data-section]").forEach((button) => {
    button.addEventListener("click", () => {
      activeSection = button.dataset.section;
      document.querySelectorAll("[data-section]").forEach((tab) => {
        const selected = tab === button;
        tab.setAttribute("aria-selected", String(selected));
        tab.tabIndex = selected ? 0 : -1;
      });
      renderDocument();
    });
  });

  try {
    const saved = localStorage.getItem("ac7multisp-language");
    if (saved === "zh-CN" || saved === "en") language = saved;
  } catch {
    // 偏好不可用时默认使用中文。
  }
  applyLanguage();
  syncSupportHash();
  loadManifest();
})();
