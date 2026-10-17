"""为2.6.0挂架登记表加载失败生成Lua修复候选；只读来源，只写本工作区Temp。"""

import argparse
from datetime import datetime
import difflib
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# 仅替换已经核对的加载段，不改武器事务、布局及成功后的资源身份登记。
BEFORE = '''            local package="/Game/AC7MultiSP/Research/"..entry[2]
            local object=StaticFindObject(package.."."..entry[2])
            if not valid(object)then LoadAsset(package.."."..entry[2]);object=StaticFindObject(package.."."..entry[2])end
'''

AFTER = '''            local package="/Game/AC7MultiSP/Research/"..entry[2]
            local objectPath=package.."."..entry[2]
            local object=StaticFindObject(objectPath)
            if not valid(object)then
                local loaded,registered,didLoad=LoadAsset(objectPath)
                object=valid(loaded)and loaded or StaticFindObject(objectPath)
                if not valid(object)then
                    note("MOUNT_ASSET_REGISTRY_RESULT path="..objectPath.." registered="..tostring(registered).." loaded="..tostring(didLoad))
                    -- LoadAsset先查登记表，新PAK未登记时改用引擎按对象路径加载。
                    -- 用原生工厂建立完整AssetData，再改路径；不手拼软引用或原生结构内存。
                    local helpers=StaticFindObject("/Script/AssetRegistry.Default__AssetRegistryHelpers")
                    assert(valid(helpers),"独立挂架缺少原生AssetRegistryHelpers")
                    local donor=sourcePlane.SWP1.StaticMesh
                    local meshClass=StaticFindObject("/Script/Engine.StaticMesh")
                    assert(valid(donor)and valid(meshClass)and donor:IsA(meshClass),"独立挂架缺少StaticMesh来源对象")
                    local data=helpers:CreateAssetData(donor,false)
                    data.ObjectPath=FName(objectPath)
                    data.PackageName=FName(package)
                    data.PackagePath=FName("/Game/AC7MultiSP/Research")
                    data.AssetName=FName(entry[2])
                    data.AssetClass=FName("StaticMesh")
                    object=helpers:GetAsset(data)
                    if not valid(object)then object=StaticFindObject(objectPath)end
                    note("MOUNT_ASSET_PATH_RESULT path="..objectPath.." valid="..tostring(valid(object)))
                end
            end
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-main', type=Path, required=True)
    parser.add_argument('--output', type=Path, help='本工作区Temp内的新目录')
    args = parser.parse_args()
    source = args.source_main.resolve(strict=True)
    raw = source.read_bytes()
    text = raw.decode('utf-8')
    if '\r' in text or text.count(BEFORE) != 1:
        raise ValueError('来源换行或加载段与已审查的2.6.0不同，请先人工核对，未生成候选')
    output = (args.output or ROOT/'Temp'/('mount-loader-fix-'+datetime.now().strftime('%Y%m%d-%H%M%S-%f'))).resolve()
    if not output.is_relative_to((ROOT/'Temp').resolve()):
        raise ValueError('候选只能写入本研究工作区Temp')
    candidate = text.replace(BEFORE, AFTER, 1)
    output.mkdir(parents=True, exist_ok=False)
    candidate_path = output/'main.lua'
    candidate_path.write_bytes(candidate.encode('utf-8'))
    patch = ''.join(difflib.unified_diff(text.splitlines(True), candidate.splitlines(True),
                                       fromfile='a/dist/DualSP/Scripts/main.lua', tofile='b/dist/DualSP/Scripts/main.lua'))
    (output/'mount-asset-loader.patch').write_bytes(patch.encode('utf-8'))
    proof = {'schema': 1, 'candidate': '2.6.0-registry-bypass-research',
             'source_main_sha256': hashlib.sha256(raw).hexdigest(),
             'candidate_main_sha256': hashlib.sha256(candidate_path.read_bytes()).hexdigest(),
             'patch_sha256': hashlib.sha256(patch.encode('utf-8')).hexdigest(),
             'generator_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
             'output_relative': output.relative_to(ROOT).as_posix(), 'compiled': False,
             'tests_run': False, 'installed': False, 'game_executed': False,
             'source_changed': source.read_bytes()!=raw,
             'limits': ['尚未核对本机CreateAssetData/GetAsset的运行反射与按路径加载结果',
                        '需要整合线程沿用整套来源登记后交付；本文件不改产品源码或安装目录',
                        '不覆盖全局AssetRegistry.bin；仍要求原PAK已经正确挂载',
                        '仅解决加载入口候选，不代表后续模型池、Socket和真实离架验证通过']}
    (output/'receipt.json').write_text(json.dumps(proof,ensure_ascii=False,indent=2)+'\n',encoding='utf-8',newline='\n')
    print(json.dumps(proof,ensure_ascii=False))


if __name__ == '__main__':
    main()
