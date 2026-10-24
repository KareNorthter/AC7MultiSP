'use strict';
// 仅验证扩展的实际安装器和事件逻辑；接口替身不能代替 Vortex 实际部署验收。
const fs = require('fs');
const fsp = fs.promises;
const path = require('path');
const vm = require('vm');
const assert = require('assert/strict');
const [extension, mainArchive, loaderArchive, output] = process.argv.slice(2).map(p => path.resolve(p));
const policy = require(path.join(extension, 'settings.json'));
const game = policy.game_id;
const root = path.join(output, 'game');
const staging = path.join(output, 'staging');
const installed = {}, notifications = [], events = {}, hooks = {}, installers = {}, passed = [];
const state = { persistent: { mods: { [game]: installed }, downloads: { files: {} }, profiles: { p: { id: 'p', gameId: game, modState: {} } } } };
class ProcessCanceled extends Error {}
const api = {
  getState: () => state,
  events: { on: (name, fn) => { events[name] = fn; }, emit: () => { throw Error('此验证不发起真实下载'); } },
  onAsync: (name, fn) => { hooks[name] = fn; },
  sendNotification: n => notifications.push(n), dismissNotification: () => {},
  showErrorNotification: (name, err) => { throw Error(name + ': ' + err.message); },
};
const context = {
  api, once: fn => fn(), registerModType: () => {}, registerAction: () => {},
  registerInstaller: (id, priority, support, install) => { installers[id] = { support, install }; },
};
const vortex = {
  actions: { setModsEnabled: async (_, id, mods, enabled) => {
    for (const mod of mods) state.persistent.profiles[id].modState[mod] = { enabled };
  } },
  selectors: { lastActiveProfileForGame: () => 'p', activeGameId: () => game, discoveryByGame: () => ({ path: root }), installPathForGame: () => staging },
  util: { ProcessCanceled, opn: () => { throw Error('此验证不打开真实网页'); } },
};
const moduleObject = { exports: {} };
const source = fs.readFileSync(path.join(extension, 'index.js'), 'utf8');
vm.runInNewContext(source, { require: name => name === 'vortex-api' ? vortex : name.startsWith('./') ? require(path.join(extension, name)) : require(name),
  module: moduleObject, exports: moduleObject.exports, __dirname: extension, Buffer, process, console, setTimeout, clearTimeout }, { filename: path.join(extension, 'index.js') });
function pass(name) { passed.push(name); console.log('通过：' + name); }
async function files(folder) {
  let result = [];
  for (const item of await fsp.readdir(folder, { withFileTypes: true })) {
    const full = path.join(folder, item.name);
    if (item.isDirectory()) result = result.concat(await files(full)); else result.push(full);
  }
  return result;
}
async function names(folder) { return (await files(folder)).map(f => path.relative(folder, f)); }
async function apply(instructions, source, dest) {
  for (const i of instructions) {
    if (i.type !== 'copy' && i.type !== 'generatefile') continue;
    const target = path.join(dest, i.destination);
    await fsp.mkdir(path.dirname(target), { recursive: true });
    if (i.type === 'copy') await fsp.copyFile(path.join(source, i.source), target);
    else await fsp.writeFile(target, i.data);
  }
}
async function reject(action, match, name) { await assert.rejects(action, match); pass(name); }
(async () => {
  await fsp.mkdir(output); await fsp.mkdir(root); await fsp.mkdir(staging);
  assert.equal(moduleObject.exports.default(context), true); pass('扩展注册与事件挂接');
  const main = installers['ac7multisp-main'], loader = installers['ac7multisp-loader'];
  const mainFiles = await names(mainArchive), loaderFiles = await names(loaderArchive);
  assert.equal((await main.support(mainFiles, game)).supported, true);
  assert.equal((await main.support(mainFiles, 'othergame')).supported, false); pass('限定 AC7 通用主包识别');
  assert.equal((await loader.support(loaderFiles, game)).supported, true); pass('识别 Nexus UE4SS 原包外层目录');
  const mainResult = await main.install(mainFiles, mainArchive);
  const loaderResult = await loader.install(loaderFiles, loaderArchive);
  assert.equal(mainResult.instructions.filter(i => i.type === 'copy').length, 31);
  const attributes = Object.fromEntries(mainResult.instructions.filter(i => i.type === 'attribute').map(i => [i.key, i.value]));
  assert.equal(attributes.source, 'nexus'); assert.equal(attributes.modId, '3797'); assert.equal(attributes.version, '2.24.1');
  const overrides = JSON.parse(await fsp.readFile(path.join(mainArchive, 'vortex_override_instructions.json'), 'utf8'));
  const attributeOverrides = overrides.filter(i => i.type === 'attribute');
  assert.equal(new Set(attributeOverrides.map(i => i.source)).size, attributeOverrides.length);
  assert.equal(attributeOverrides.find(i => i.key === 'source').value, 'nexus');
  pass('本地导入的主包登记 Nexus 来源与版本，多个属性不相互覆盖');
  assert(!loaderResult.instructions.some(i => i.type === 'copy' && /UE4SS.log|zz-binaryalts/.test(i.destination)));
  assert(loaderResult.instructions.find(i => i.type === 'generatefile' && i.destination.endsWith('mods.txt')).data.includes('BPModLoaderMod : 1'));
  pass('主包映射、前置摘要和首次配置生成');
  await apply(loaderResult.instructions, loaderArchive, path.join(staging, 'loader'));
  installed.main = { type: 'ac7multisp-root', installationPath: 'main' };
  installed.loader = { type: 'ac7multisp-ue4ss', installationPath: 'loader' };
  state.persistent.profiles.p.modState = { main: { enabled: true }, loader: { enabled: true } };
  await hooks['will-deploy']('p'); pass('缺前置但已安装并启用正确暂存前置时允许部署');
  await apply(loaderResult.instructions, loaderArchive, root);
  await fsp.mkdir(path.join(root, 'ue4ss/Mods/DualSP'), { recursive: true });
  await fsp.writeFile(path.join(root, 'ue4ss/Mods/DualSP/profiles.json'), '{"保留个人配置":true}');
  await hooks['will-deploy']('p'); pass('卸载后仅剩个人配置不阻止 Vortex 迁移');
  await apply(mainResult.instructions, mainArchive, root);
  await reject(() => hooks['will-deploy']('p'), /旧手装/, '旧手装程序残留阻止直接覆盖');
  await fsp.writeFile(path.join(root, 'vortex.deployment.ac7multisp-root.json'), JSON.stringify({ files: [{ relPath: 'ue4ss\\Mods\\DualSP\\Scripts\\main.lua' }] }));
  await hooks['will-deploy']('p'); pass('已有 Vortex 管理记录允许后续部署');
  state.persistent.profiles.p.modState.loader.enabled = false;
  await reject(() => hooks['will-deploy']('p'), /同时启用/, '主包启用时阻止单独停用共享前置');
  state.persistent.profiles.p.modState.loader.enabled = true;
  await fsp.copyFile(path.join(root, 'dwmapi.dll'), path.join(output, 'dwmapi.dll.before'));
  await fsp.writeFile(path.join(root, 'dwmapi.dll'), 'unknown');
  await reject(() => hooks['will-deploy']('p'), /未知或不完整/, '未知加载器阻止部署');
  await reject(() => main.support(['../ac7multisp-package.json'], game), /无效路径/, '阻止归档路径越界');
  await reject(() => main.support(['X', 'x'], game), /重复或大小写/, '阻止大小写重复成员');
  await fsp.writeFile(path.join(output, '验证回执.json'), JSON.stringify({ passed, scope: '实际源码、接口替身和临时文件；未加载 Vortex 扩展，未验证真实部署或网页下载' }, null, 2));
})().catch(err => { console.error(err); process.exitCode = 1; });
