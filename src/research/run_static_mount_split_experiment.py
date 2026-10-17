"""对明确提供的本地资源副本执行挂架分离实验，不编译、打包、安装或连接游戏。"""

import argparse
from datetime import datetime
import json
from pathlib import Path
import subprocess
import sys

from split_static_mount_asset import ROOT, digest, load_mesh


def entries(path):
    """接受既有提取 manifest 或 manifest 路径清单，不扫描其他工作区。"""
    for item in json.loads(path.read_text(encoding='utf-8')):
        if isinstance(item, str):
            nested = Path(item)
            yield from entries(nested if nested.is_absolute() else path.parent/nested)
        elif item.get('json'):
            item = dict(item)
            parsed = Path(item['json'])
            item['json'] = str(parsed if parsed.is_absolute() else path.parent/parsed)
            yield item


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, action='append', required=True)
    parser.add_argument('--recipes', type=Path, default=ROOT/'src/research/static_mount_split_recipes.json')
    parser.add_argument('--tools-root', type=Path, required=True)
    parser.add_argument('--summary-output', type=Path, required=True, help='新的汇总 JSON 路径，不覆盖既有证据')
    args = parser.parse_args()
    if args.summary_output.exists():
        raise ValueError('汇总文件已存在，请选择新路径')
    sources = {}
    for manifest in args.manifest:
        for item in entries(manifest):
            sources.setdefault(Path(item['asset']).stem, []).append(item)
    recipes = json.loads(args.recipes.read_text(encoding='utf-8'))
    selected = []
    for recipe in recipes['variants']:
        candidates = sources[recipe['source_asset']]
        preferred = sorted(candidates, key=lambda e: '_0_P.' in e['pak'], reverse=True)[0]
        path = Path(preferred['json'])
        _, _, raw, _ = load_mesh(path)
        if digest(raw) != recipe['source_extras_sha256']:
            raise ValueError('资源更新或覆盖与配方不符：'+recipe['source_asset'])
        selected.append((recipe, preferred))
    # 在全部来源通过身份核对后才写实验目录，任何转换失败立即停止。
    run = ROOT/'Temp'/('static-mount-split-'+datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
    run.mkdir(parents=True, exist_ok=False)
    results = []
    for recipe, source in selected:
        target = run/recipe['name']
        subprocess.run([sys.executable, '-X', 'utf8', str(ROOT/'src/research/split_static_mount_asset.py'),
                        '--source-json', source['json'], '--recipe', str(args.recipes.resolve()), '--variant', recipe['name'],
                        '--output', str(target), '--tools-root', str(args.tools_root.resolve())], check=True)
        report = json.loads((target/'report.json').read_text(encoding='utf-8'))
        report['source_pak'] = source['pak']
        report['source_pak_asset'] = source['asset']
        report['source_encrypted_uasset_sha256'] = source['encrypted_sha256']
        for suffix in ('.uasset', '.uexp'):
            original = Path(source['json']).with_suffix(suffix)
            report['source_decoded'+suffix.replace('.', '_')+'_sha256'] = digest(original.read_bytes())
        results.append(report)
    summary = {'schema': 1, 'baseline': 'v2.4.0', 'steam_build': recipes['steam_build'],
               'status': 'offline_asset_roundtrip_only', 'method': recipes['method'],
               'compiled': False, 'tests_run': False, 'installed': False, 'game_process_access': False,
               'installable_package': False, 'integration_allowed': False,
               'recipes_sha256': digest(args.recipes.read_bytes()), 'variant_count': len(results),
               'local_artifacts_relative': run.relative_to(ROOT).as_posix(), 'variants': results}
    args.summary_output.parent.mkdir(parents=True, exist_ok=True)
    with args.summary_output.open('x', encoding='utf-8', newline='\n') as stream:
        json.dump(summary, stream, ensure_ascii=False, indent=2)
        stream.write('\n')
    print(json.dumps({'summary': str(args.summary_output), 'variant_count': len(results)}, ensure_ascii=False))


if __name__ == '__main__':
    main()
