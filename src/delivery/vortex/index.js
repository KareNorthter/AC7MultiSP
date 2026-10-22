'use strict';
// 此扩展只处理本产品及已核验的 AC7 UE4SS，不接管其他游戏或普通 PAK。
const path = require('path');
const nativeFs = require('fs');
const fsp = nativeFs.promises;
const crypto = require('crypto');
const { execFile } = require('child_process');
const { promisify } = require('util');
const { actions, selectors, util } = require('vortex-api');
const policy = require('./settings.json');
const official = require('./ue4ss-files.json');
const GAME = policy.game_id;
const MAIN_TYPE = 'ac7multisp-root';
const LOADER_TYPE = 'ac7multisp-ue4ss';
const MAIN_MARKER = 'ac7multisp-package.json';
const MOD = policy.mod_prefix;
const LOADER_MARKER = 'ue4ss/ac7multisp-ue4ss.json';
const runFile = promisify(execFile);
const installs = new Map();
let requestedProfile;
let checking = false;

function stateOf(api) { return api.getState(); }
function profileOf(api, id) {
  const s = stateOf(api);
  const key = id || selectors.lastActiveProfileForGame(s, GAME);
  return s.persistent.profiles[key];
}
function modsOf(api) { return stateOf(api).persistent.mods[GAME] || {}; }
function enabled(profile, id) { return !!profile?.modState?.[id]?.enabled; }
function gameRoot(api) { return selectors.discoveryByGame(stateOf(api), GAME)?.path; }
function active(api) { return selectors.activeGameId(stateOf(api)) === GAME; }
function role(mod) {
  if (mod.type === MAIN_TYPE || Number(mod.attributes?.modId) === policy.nexus_mod_id) return 'main';
  if (mod.type === LOADER_TYPE || Number(mod.attributes?.modId) === policy.ue4ss.nexus_mod_id) return 'loader';
  return mod.attributes?.ac7multispRole;
}
function canonical(name) {
  const n = name.replace(/\\/g, '/');
  if (!n || n.startsWith('/') || /[:\x00-\x1f]/.test(n) || n.split('/').some(p =>
    !p || p === '.' || p === '..' || /[. ]$/.test(p) || /^(con|prn|aux|nul|com[0-9]|lpt[0-9])(\.|$)/i.test(p))) {
    throw new util.ProcessCanceled('压缩包中有无效路径：' + name);
  }
  return n;
}
function filesOnly(files) {
  const result = files.filter(n => !/[\\/]$/.test(n)).map(source => ({ source, name: canonical(source) }));
  const names = result.map(f => f.name.toLowerCase());
  if (new Set(names).size !== names.length) throw new util.ProcessCanceled('压缩包含重复或大小写重名文件。');
  return result;
}
async function safe(root, relative, allowLeafLink = false) {
  const rel = canonical(relative);
  const base = path.resolve(root);
  // 不跟随目录联接或符号链接；也核对游戏根目录的父链。
  let current = path.parse(base).root;
  const parts = path.relative(current, base).split(path.sep).filter(Boolean).concat(rel.split('/'));
  for (const [index, part] of parts.entries()) {
    current = path.join(current, part);
    try { if ((await fsp.lstat(current)).isSymbolicLink() && !(allowLeafLink && index === parts.length - 1)) throw new util.ProcessCanceled('路径由其他工具链接管理，请先迁移：' + current); }
    catch (err) { if (err.code !== 'ENOENT') throw err; }
  }
  return path.join(base, ...rel.split('/'));
}
async function hash(file) {
  try {
    const digest = crypto.createHash('sha256');
    for await (const chunk of nativeFs.createReadStream(file)) digest.update(chunk);
    return digest.digest('hex');
  } catch (err) { if (err.code === 'ENOENT') return null; throw err; }
}
async function json(file) {
  const stat = await fsp.stat(file);
  if (stat.size > 1024 * 1024) throw new util.ProcessCanceled('清单过大。');
  return JSON.parse((await fsp.readFile(file, 'utf8')).replace(/^\uFEFF/, ''));
}
async function exists(file) { try { await fsp.stat(file); return true; } catch (err) { if (err.code === 'ENOENT') return false; throw err; } }
async function assertClosed() {
  // 使用固定命令，不把游戏路径或网络内容拼成命令。检查 Steam/外部启动的进程。
  const command = "@(Get-Process -Name 'Ace7Game','Ace7Game-Win64-Shipping' -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Id) | ConvertTo-Json -Compress";
  let out;
  try { out = await runFile('powershell.exe', ['-NoProfile', '-NonInteractive', '-Command', command], { windowsHide: true, timeout: 12000 }); }
  catch (_) { throw new util.ProcessCanceled('无法检查游戏进程，请关闭游戏并重试；仍失败时保留 Vortex 日志。'); }
  if (out.stdout.trim()) throw new util.ProcessCanceled('请先完整退出皇牌空战7，再部署或停用 Mod。');
}
async function loaderStatus(root) {
  if (!root) return 'missing';
  const values = [];
  for (const [relative, expected] of Object.entries(policy.ue4ss.files)) {
    const target = await safe(root, relative, true);
    values.push([await hash(target), expected]);
  }
  if (values.every(([value]) => value === null)) {
    if (await exists(await safe(root, 'ue4ss/UE4SS-settings.ini', true)) || await exists(await safe(root, 'UE4SS.dll', true))) return 'unknown';
    return 'missing';
  }
  if (!values.every(([value, expected]) => value === expected)) return 'unknown';
  const settings = await safe(root, 'ue4ss/UE4SS-settings.ini', true);
  if (!await exists(settings)) return 'unknown';
  const contents = await fsp.readFile(settings, 'utf8');
  if (/^\s*(ModsFolderPaths|ModsPath|ModPath)\s*=\s*[^;\s]/mi.test(contents)) return 'unknown';
  return 'compatible';
}
function loaderBase(files) {
  const found = files.filter(f => /(^|\/)dwmapi\.dll$/i.test(f.name));
  for (const item of found) {
    const base = item.name.slice(0, -'dwmapi.dll'.length);
    if (files.some(f => f.name.toLowerCase() === (base + 'ue4ss/UE4SS.dll').toLowerCase())) return base;
  }
  return undefined;
}
async function supportMain(files, gameId) {
  if (gameId !== GAME) return { supported: false, requiredFiles: [] };
  const marker = filesOnly(files).find(f => f.name === MAIN_MARKER);
  return { supported: !!marker, requiredFiles: marker ? [marker.source] : [] };
}
async function supportLoader(files, gameId) {
  if (gameId !== GAME) return { supported: false, requiredFiles: [] };
  const entries = filesOnly(files), base = loaderBase(entries);
  return { supported: base !== undefined, requiredFiles: base === undefined ? [] : entries.filter(f =>
    f.name.toLowerCase() === (base + 'dwmapi.dll').toLowerCase() ||
    f.name.toLowerCase() === (base + 'ue4ss/UE4SS.dll').toLowerCase()).map(f => f.source) };
}
async function installMain(api, files, destination) {
  const entries = filesOnly(files);
  const data = await json(await safe(destination, MAIN_MARKER));
  if (data.schema !== 1 || data.product !== policy.product || data.distribution !== 'universal' ||
      !/^\d+\.\d+\.\d+$/.test(data.version) || !data.files) throw new util.ProcessCanceled('无法识别 MultiSP 主包清单。');
  const expected = Object.keys(data.files);
  const available = new Map(entries.map(f => [f.name, f.source]));
  const controls = [MAIN_MARKER, 'vortex_override_instructions.json', '安装说明.txt'];
  if (entries.some(f => !expected.includes(f.name) && !controls.includes(f.name))) throw new util.ProcessCanceled('主包含未登记文件。');
  const instructions = [];
  for (const relative of expected) {
    canonical(relative);
    if (!relative.startsWith(MOD) && relative !== policy.pak) throw new util.ProcessCanceled('主包不能携带共享前置或其他 Mod。');
    if (/\/(profiles[^/]*\.json|ui-state\.json|catalogue[^/]*\.json)$/i.test(relative)) throw new util.ProcessCanceled('主包不能携带个人状态。');
    const record = data.files[relative];
    if (!available.has(relative) || !/^[0-9a-f]{64}$/.test(record.sha256) ||
        await hash(await safe(destination, relative)) !== record.sha256) throw new util.ProcessCanceled('主包文件不完整：' + relative);
    instructions.push({ type: 'copy', source: available.get(relative), destination: relative.replace(/\//g, path.sep) });
  }
  if (!expected.includes(policy.pak) || !expected.includes(MOD + 'Scripts/main.lua') || !expected.includes(MOD + 'enabled.txt')) throw new util.ProcessCanceled('主包缺少成套组件。');
  instructions.push({ type: 'setmodtype', value: MAIN_TYPE });
  // 使用独立 source 标识，避免包内覆盖映射把多个 attribute 合并成同一项。
  for (const [key, value] of Object.entries({ ac7multispRole: 'main', version: data.version,
    source: 'nexus', modId: String(policy.nexus_mod_id), downloadGame: GAME }))
    instructions.push({ type: 'attribute', source: 'ac7multisp-' + key, key, value });
  return { instructions };
}
async function installLoader(api, files, destination) {
  const entries = filesOnly(files), base = loaderBase(entries), root = gameRoot(api);
  if (base === undefined || !root) throw new util.ProcessCanceled('先在 Vortex 中管理 AC7 并确认游戏位置。');
  await assertClosed();
  if (await loaderStatus(root) !== 'missing') throw new util.ProcessCanceled('已有加载器，请共用兼容版本；不要重复安装。未知或不完整环境需先核对。');
  const instructions = [];
  for (const [relative, expected] of Object.entries(policy.ue4ss.files)) {
    const item = entries.find(f => f.name.toLowerCase() === (base + relative).toLowerCase());
    if (!item || await hash(await safe(destination, item.name)) !== expected) throw new util.ProcessCanceled('不是已核验的 AC7 UE4SS 3.0.1.7，请使用 Nexus 2474 条目的文件 8453。');
  }
  // 官方包剥掉外层目录，仅部署登记运行文件；日志和替代二进制不部署，样例调试模块不启用。
  for (const item of entries) {
    if (!item.name.startsWith(base)) continue;
    const relative = item.name.slice(base.length);
    const record = official.files[relative];
    if (!record) continue;
    if (await hash(await safe(destination, item.name)) !== record.sha256) throw new util.ProcessCanceled('官方前置文件摘要不符：' + relative);
    if (await exists(await safe(root, relative))) throw new util.ProcessCanceled('前置目标已有文件，已保留：' + relative);
    // 首次配置由本项目已使用的无调试控制台设置生成；共享配置已存在时上方检查会停止。
    if (!['ue4ss/UE4SS-settings.ini', 'ue4ss/Mods/mods.txt', 'ue4ss/Mods/mods.json'].includes(relative))
      instructions.push({ type: 'copy', source: item.source, destination: relative.replace(/\//g, path.sep) });
  }
  for (const required of Object.keys(official.files)) {
    if (!entries.some(f => f.name === base + required)) throw new util.ProcessCanceled('官方包缺少运行文件：' + required);
  }
  instructions.push({ type: 'generatefile', destination: path.join('ue4ss', 'UE4SS-settings.ini'), data: await fsp.readFile(path.join(__dirname, 'first-install-settings.ini'), 'utf8') });
  instructions.push({ type: 'generatefile', destination: path.join('ue4ss', 'Mods', 'mods.txt'), data: 'BPML_GenericFunctions : 1\r\nBPModLoaderMod : 1\r\n\r\n; Built-in keybinds, do not move up!\r\nKeybinds : 1\r\n' });
  instructions.push({ type: 'generatefile', destination: path.join('ue4ss', 'Mods', 'mods.json'), data: JSON.stringify(['BPML_GenericFunctions', 'BPModLoaderMod', 'Keybinds'].map(mod_name => ({ mod_name, mod_enabled: true }))) });
  instructions.push({ type: 'generatefile', destination: LOADER_MARKER.replace(/\//g, path.sep), data: JSON.stringify({ schema: 1, source: policy.ue4ss.page, version: policy.ue4ss.version }) });
  instructions.push({ type: 'setmodtype', value: LOADER_TYPE }, { type: 'attribute', key: 'ac7multispRole', value: 'loader' });
  return { instructions };
}
function downloadIds(download) {
  const ids = download?.modInfo?.nexus?.ids;
  return !!ids && Number(ids.modId) === policy.ue4ss.nexus_mod_id && Number(ids.fileId) === policy.ue4ss.nexus_file_id &&
    (ids.gameId === GAME || (Array.isArray(download.game) ? download.game.includes(GAME) : download.game === GAME));
}
async function installDownload(api, downloadId, profileId) {
  if (installs.has(downloadId)) return installs.get(downloadId);
  const work = (async () => {
    const profile = profileOf(api, profileId);
    if (!profile || profile.gameId !== GAME || !active(api) || profileOf(api)?.id !== profileId) {
      api.sendNotification({ type: 'info', message: 'UE4SS 已下载。请切回原 AC7 配置档后安装。' }); return;
    }
    const existing = Object.entries(modsOf(api)).find(([, mod]) => mod.archiveId === downloadId);
    if (existing) {
      if (role(existing[1]) !== 'loader') throw new util.ProcessCanceled('下载已安装，但未识别为 AC7 UE4SS；请重新安装该前置使扩展接管目录映射。');
      await actions.setModsEnabled(api, profileId, [existing[0]], true, { allowAutoDeploy: false });
      return;
    }
    const modId = await new Promise((resolve, reject) => api.events.emit('start-install-download', downloadId,
      { allowAutoEnable: false }, (err, id) => err ? reject(err) : resolve(id)));
    await actions.setModsEnabled(api, profileId, [modId], true, { allowAutoDeploy: false, installed: true });
    api.sendNotification({ id: 'ac7multisp-loader-ready', type: 'success', message: 'AC7 UE4SS 已安装并启用，请退出游戏后部署。' });
  })();
  installs.set(downloadId, work);
  try { await work; } finally { installs.delete(downloadId); }
}
async function requestLoader(api) {
  const profile = profileOf(api);
  if (!profile || !active(api)) return;
  requestedProfile = profile.id;
  const downloads = stateOf(api).persistent.downloads.files || {};
  const downloaded = Object.entries(downloads).find(([, value]) => value.state === 'finished' && downloadIds(value));
  if (downloaded) return installDownload(api, downloaded[0], profile.id);
  // 免费账户走正常的 Nexus 网页授权；不抓取 Cookie，不改下载链接、不绕过确认。
  await util.opn(policy.ue4ss.download_page);
  api.sendNotification({ id: 'ac7multisp-waiting-download', type: 'info', message: '请在 Nexus 点击 Mod Manager Download。下载完成后将由 Vortex 接续安装。' });
}
async function inspect(api, includeInstalled = false) {
  if (!active(api) || checking) return;
  const profile = profileOf(api);
  const main = Object.entries(modsOf(api)).some(([id, mod]) => role(mod) === 'main' && (includeInstalled || enabled(profile, id)));
  if (!main) return;
  checking = true;
  try {
    const status = await loaderStatus(gameRoot(api));
    const staged = Object.entries(modsOf(api)).find(([, mod]) => role(mod) === 'loader');
    if (status === 'compatible' && (!staged || enabled(profile, staged[0]))) {
      api.dismissNotification('ac7multisp-prerequisite');
      api.dismissNotification('ac7multisp-waiting-download');
      return;
    }
    if (status === 'unknown') {
      api.sendNotification({ id: 'ac7multisp-prerequisite', type: 'warning', message: '已有未知或不完整的 UE4SS，已保留；请核对加载器后再部署 MultiSP。' }); return;
    }
    if (staged && enabled(profile, staged[0])) {
      api.sendNotification({ id: 'ac7multisp-prerequisite', type: 'info', message: 'AC7 UE4SS 已安装并启用，请退出游戏后点击 Deploy（部署）。' });
      return;
    }
    api.sendNotification({ id: 'ac7multisp-prerequisite', type: 'warning', message: staged ? 'AC7 UE4SS 尚未启用。' : 'MultiSP 需要 AC7 专用 UE4SS，点击补齐前置。也可浏览器下载 ZIP 后从 Install From File 导入。',
      actions: [{ title: staged ? '启用前置' : '下载并安装', action: async () => {
        try {
          if (staged) await actions.setModsEnabled(api, profile.id, [staged[0]], true, { allowAutoDeploy: false });
          else await requestLoader(api);
        } catch (err) { api.showErrorNotification('前置安装未完成', err, { allowReport: false }); }
      } }] });
  } finally { checking = false; }
}
async function validateStagedLoader(api, profile) {
  const list = Object.entries(modsOf(api)).filter(([id, mod]) => role(mod) === 'loader' && enabled(profile, id));
  if (list.length !== 1) return false;
  const staging = selectors.installPathForGame(stateOf(api), GAME);
  const root = path.join(staging, list[0][1].installationPath);
  for (const [relative, expected] of Object.entries(policy.ue4ss.files)) if (await hash(await safe(root, relative)) !== expected) return false;
  return true;
}
async function trackedMain(root) {
  const names = await fsp.readdir(root);
  for (const name of names.filter(n => /^vortex\.deployment\..+\.json$/i.test(n))) {
    const record = await json(await safe(root, name));
    if (Array.isArray(record.files) && record.files.some(file => {
      const relative = String(file.relPath || '').replace(/\\/g, '/').toLowerCase();
      return relative.startsWith(MOD.toLowerCase()) || relative === policy.pak.toLowerCase();
    })) return true;
  }
  return false;
}
async function unmanagedProgram(root) {
  // 卸载后保留的个人配置目录可以继续使用；只有程序或启用标记残留才拦截迁移。
  for (const relative of [policy.pak, MOD + 'enabled.txt']) if (await exists(await safe(root, relative))) return true;
  async function hasFiles(relative) {
    const folder = await safe(root, relative);
    if (!await exists(folder)) return false;
    for (const item of await fsp.readdir(folder, { withFileTypes: true })) {
      const child = relative + '/' + item.name;
      await safe(root, child);
      if (!item.isDirectory() || await hasFiles(child)) return true;
    }
    return false;
  }
  return await hasFiles(MOD + 'Scripts') || await hasFiles(MOD + 'bin');
}
async function beforeDeploy(api, profileId) {
  const profile = profileOf(api, profileId);
  if (profile?.gameId !== GAME) return;
  const hasMain = Object.entries(modsOf(api)).some(([id, mod]) => role(mod) === 'main' && enabled(profile, id));
  const hasLoader = Object.entries(modsOf(api)).some(([id, mod]) => role(mod) === 'loader' && enabled(profile, id));
  if (!Object.values(modsOf(api)).some(mod => !!role(mod))) return;
  await assertClosed();
  if (!hasMain && !hasLoader) return;
  const root = gameRoot(api), status = await loaderStatus(root);
  if (!root) throw new util.ProcessCanceled('请先确认 AC7 游戏目录。');
  const existingManagedLoader = Object.entries(modsOf(api)).some(([, mod]) => role(mod) === 'loader');
  if (hasMain && existingManagedLoader && !hasLoader) throw new util.ProcessCanceled('MultiSP 仍在启用，请同时启用公共 UE4SS 前置，再部署。');
  if (status === 'unknown') throw new util.ProcessCanceled('已有未知或不完整加载器，已停止部署，避免覆盖。');
  if (hasLoader && status === 'compatible' && !await exists(path.join(root, LOADER_MARKER)))
    throw new util.ProcessCanceled('已有兼容的外部 UE4SS，请共用现有前置，不要重复部署新下载的公共前置。');
  if (hasMain && status !== 'compatible' && !await validateStagedLoader(api, profile)) {
    await inspect(api);
    throw new util.ProcessCanceled('请先补齐 AC7 专用 UE4SS，然后再部署 MultiSP。');
  }
  // 已有旧手装不自动认领，防止停用时 Vortex 恢复旧程序。已有 Vortex 标记允许接续。
  if (hasMain && await unmanagedProgram(root) && !await trackedMain(root))
    throw new util.ProcessCanceled('发现未被 Vortex 记录的旧手装 MultiSP，请先备份并迁移；直接叠加会使停用后旧版恢复。');
}
function main(context) {
  const api = context.api;
  const root = () => gameRoot(api);
  context.registerModType(MAIN_TYPE, 20, id => id === GAME, root, async instructions =>
    instructions.some(i => i.type === 'attribute' && i.key === 'ac7multispRole' && i.value === 'main'), { name: 'AC7MultiSP', mergeMods: true });
  context.registerModType(LOADER_TYPE, 19, id => id === GAME, root, async instructions =>
    instructions.some(i => i.type === 'attribute' && i.key === 'ac7multispRole' && i.value === 'loader'), { name: 'AC7 UE4SS 公共前置', mergeMods: true });
  context.registerInstaller('ac7multisp-main', 10, supportMain, (files, dest) => installMain(api, files, dest));
  context.registerInstaller('ac7multisp-loader', 11, supportLoader, (files, dest) => installLoader(api, files, dest));
  context.registerAction('mods-action-icons', 205, 'download', {}, '检查 AC7MultiSP 前置', () => inspect(api, true), () => active(api));
  context.once(() => {
    api.events.on('gamemode-activated', id => { if (id === GAME) inspect(api).catch(err => api.showErrorNotification('MultiSP 前置检测失败', err)); });
    api.events.on('did-install-mod', (gameId, archiveId, modId) => {
      if (gameId === GAME) inspect(api, role(modsOf(api)[modId] || {}) === 'main').catch(err => api.showErrorNotification('MultiSP 前置检测失败', err));
    });
    api.events.on('profile-did-change', () => { if (active(api)) inspect(api).catch(err => api.showErrorNotification('MultiSP 前置检测失败', err)); });
    api.events.on('mods-enabled', () => { if (active(api)) inspect(api).catch(err => api.showErrorNotification('MultiSP 前置检测失败', err)); });
    api.events.on('did-finish-download', (downloadId, result) => {
      const download = stateOf(api).persistent.downloads.files[downloadId];
      if (result === 'finished' && requestedProfile && downloadIds(download)) {
        const requested = requestedProfile; requestedProfile = undefined;
        installDownload(api, downloadId, requested).catch(err => api.showErrorNotification('AC7 UE4SS 安装未完成', err, { allowReport: false }));
      }
    });
    api.onAsync('will-deploy', profileId => beforeDeploy(api, profileId));
    api.onAsync('did-deploy', profileId => {
      if (profileOf(api, profileId)?.gameId === GAME) return inspect(api);
    });
    api.onAsync('will-purge', async profileId => { if (profileOf(api, profileId)?.gameId === GAME) await assertClosed(); });
  });
  return true;
}
module.exports = { default: main };
