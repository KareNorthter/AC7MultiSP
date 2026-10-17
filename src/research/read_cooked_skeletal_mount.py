"""读取本批AC7 cooked骨架挂架几何和权重；不生成或修改游戏资源。

结构依据：UEViewer/Unreal/UnrealMesh/UnMesh4.cpp（UE4.18旧LOD分支）。
只接受本批明确读出的布局；不搜索浮点数据猜顶点起点，不支持布料。
"""
import base64
import math
import struct
from summarize_mount_assets import resolve
from survey_aircraft_mounts import multiply, rotate

FORMAT_SOURCE = 'https://github.com/gildor2/UEViewer/blob/master/Unreal/UnrealMesh/UnMesh4.cpp'


class Reader:
    def __init__(self, raw):
        self.raw, self.offset = raw, 0

    def take(self, size):
        if size < 0 or self.offset + size > len(self.raw):
            raise ValueError('骨架网格数据越界')
        start = self.offset
        self.offset += size
        return self.raw[start:self.offset]

    def unpack(self, fmt):
        return struct.unpack('<'+fmt, self.take(struct.calcsize('<'+fmt)))

    def integer(self, low=0, high=1_000_000):
        value, = self.unpack('i')
        if not low <= value <= high:
            raise ValueError(f'骨架网格整数越界：{value} at {self.offset-4}')
        return value

    def array(self, fmt):
        count = self.integer()
        return list(self.unpack(fmt*count))

    def strip(self):
        if self.take(2) != b'\x01\x00':
            raise ValueError('仅支持去编辑器、保留渲染数据的本批骨架网格')

    def bulk(self, stride, expected):
        if self.integer() != stride or self.integer() != expected:
            raise ValueError('骨架网格批量数据尺寸不符')
        return self.take(stride*expected)


def read_skeletal_mesh(document, name):
    export = next(e for e in document['Exports'] if e['ObjectName'].casefold() == name.casefold())
    if resolve(document, export['ClassIndex']) != 'SkeletalMesh':
        raise ValueError('要求原生SkeletalMesh导出')
    raw = base64.b64decode(export['Extras'], validate=True)
    r = Reader(raw)
    r.strip()
    mesh_bounds = r.unpack('7f')
    materials = []
    for _ in range(r.integer(1,64)):
        material, slot, suffix = r.unpack('iii')
        initialized, override = r.unpack('ii')
        densities = r.unpack('4f')
        if initialized not in (0,1) or override not in (0,1):
            raise ValueError('材质UV元数据不符')
        materials.append({'material':resolve(document,material), 'slot':document['NameMap'][slot], 'suffix':suffix})
    bone_count = r.integer(1,1000)
    bones = []
    for index in range(bone_count):
        name_index, number, parent = r.unpack('iii')
        if not 0 <= name_index < len(document['NameMap']) or number < 0 or not -1 <= parent < index:
            raise ValueError('网格参考骨架名称或父序不符')
        bone_name = document['NameMap'][name_index]+(f'_{number-1}' if number else '')
        bones.append({'name':bone_name, 'parent':parent})
    if len({b['name'] for b in bones}) != bone_count or bones[0]['parent'] != -1 or any(b['parent']<0 for b in bones[1:]):
        raise ValueError('要求唯一骨名与单根参考骨架')
    if r.integer() != bone_count:
        raise ValueError('网格参考姿态数量不符')
    for bone in bones:
        transform = r.unpack('10f')
        if not all(math.isfinite(v) for v in transform) or any(abs(v-1)>1e-5 for v in transform[7:]) or abs(sum(v*v for v in transform[:4])-1)>.001:
            raise ValueError('网格参考变换不支持')
        bone.update(quaternion=transform[:4],position=transform[4:7])
    if r.integer() != bone_count:
        raise ValueError('网格骨骼名称映射数量不符')
    seen = set()
    for _ in range(bone_count):
        ni, number, index = r.unpack('iii')
        mapped_name = document['NameMap'][ni]+(f'_{number-1}' if number else '')
        if not 0 <= index < bone_count or index in seen or mapped_name != bones[index]['name'] or number < 0:
            raise ValueError('网格骨骼映射不是双射')
        seen.add(index)
    lods = []
    for _ in range(r.integer(1,8)):
        r.strip()
        sections = []
        for _ in range(r.integer(1,64)):
            r.strip()
            material, first, triangles = r.unpack('Hii')
            sorting, disabled, cloth_section, cloth_lod = r.unpack('BihB')
            recompute, shadow, start = r.unpack('iii')
            bone_map = r.array('H')
            vertices, influences = r.unpack('ii')
            # 未启用布料的三个数组必须为空；遗留布料索引/Guid原样读过。
            if any(r.integer()!=0 for _ in range(3)):
                raise ValueError('不支持布料/物理网格映射')
            r.take(2+16+4)
            if disabled or sorting or recompute not in (0,1) or shadow not in (0,1) or not 1 <= influences <= 8:
                raise ValueError('section标记不支持')
            if material >= len(materials) or not bone_map or any(b >= bone_count for b in bone_map) or min(first,triangles,start,vertices)<0:
                raise ValueError('section材质或骨骼映射越界')
            sections.append(dict(material=material, first=first, triangles=triangles, start=start,
                                 vertices=vertices, influences=influences, bone_map=bone_map))
        indices = read_indices(r)
        active = r.array('H')
        size, vertex_count = r.unpack('ii')
        required = r.array('H')
        import_map = r.array('i')
        max_import = r.integer()
        uvs = r.integer(1,8)
        r.strip()
        if r.integer(1,8) != uvs:
            raise ValueError('UV通道数不符')
        full_uv = r.integer(0,1)
        extension_origin = r.unpack('6f')
        stride = 20 + uvs*(8 if full_uv else 4)
        vertices_raw = r.bulk(stride,vertex_count)
        points = [struct.unpack_from('<3f',vertices_raw,i*stride+8) for i in range(vertex_count)]
        if not all(math.isfinite(v) for point in points for v in point):
            raise ValueError('顶点不是有限数')
        r.strip()
        extra = r.integer(0,1)
        if r.integer() != vertex_count:
            raise ValueError('权重与顶点数不符')
        width = 8 if extra else 4
        weights_raw = r.bulk(width*2,vertex_count)
        weights = [tuple(zip(weights_raw[i*width*2:i*width*2+width],weights_raw[i*width*2+width:(i+1)*width*2])) for i in range(vertex_count)]
        adjacency = read_indices(r)
        if len(indices) != sum(s['triangles'] for s in sections)*3 or any(i >= vertex_count for i in indices+adjacency):
            raise ValueError('三角面范围/计数不符')
        if any(i >= bone_count for i in active+required) or len(import_map) not in (0,vertex_count) or any(not 0<=i<=max_import for i in import_map):
            raise ValueError('LOD骨骼或导入映射数量不符')
        if sum(s['vertices'] for s in sections) != vertex_count:
            raise ValueError('section顶点未完整覆盖')
        vertex_cursor, index_cursor = 0, 0
        for section in sections:
            if section['start'] != vertex_cursor or section['first'] != index_cursor:
                raise ValueError('section范围存在空洞或重叠')
            vertex_end = vertex_cursor+section['vertices']
            index_end = index_cursor+section['triangles']*3
            if any(not vertex_cursor <= i < vertex_end for i in indices[index_cursor:index_end]):
                raise ValueError('三角面跨越自身section顶点区')
            for i in range(section['start'],section['start']+section['vertices']):
                if sum(w for _,w in weights[i]) != 255 or any(b >= len(section['bone_map']) for b,w in weights[i] if w):
                    raise ValueError('蒙皮权重/section局部骨号无效')
            vertex_cursor, index_cursor = vertex_end, index_end
        lods.append(dict(sections=sections,points=points,indices=indices,weights=weights,adjacency_count=len(adjacency)))
    tail = r.take(len(raw)-r.offset)
    if tail != b'\0'*4:
        raise ValueError('未识别骨架网格尾部，不交付部分解析')
    return dict(bones=bones,materials=materials,bounds=mesh_bounds,lods=lods,bytes=len(raw),complete=True)


def reference_component(bones):
    """明确单位缩放下，由局部参考姿态计算组件空间绑定变换。"""
    result = []
    for bone in bones:
        position, quaternion = bone['position'], bone['quaternion']
        if bone['parent'] >= 0:
            parent_position, parent_quaternion = result[bone['parent']]
            position = tuple(a+b for a,b in zip(parent_position,rotate(parent_quaternion,position)))
            quaternion = multiply(parent_quaternion,quaternion)
        result.append((position,quaternion))
    return result


def skin_points(model, lod, pose_by_name):
    """组件空间动画姿态×逆参考姿态；保留原section局部骨映射及全部字节权重。"""
    reference = reference_component(model['bones'])
    output = [None]*len(lod['points'])
    for section in lod['sections']:
        for index in range(section['start'],section['start']+section['vertices']):
            vertex = lod['points'][index]
            result = [0.0,0.0,0.0]
            for local_bone, weight in lod['weights'][index]:
                if not weight:
                    continue
                bone = section['bone_map'][local_bone]
                reference_position, reference_quaternion = reference[bone]
                quaternion_inverse = tuple(-v for v in reference_quaternion[:3])+(reference_quaternion[3],)
                local = rotate(quaternion_inverse,tuple(a-b for a,b in zip(vertex,reference_position)))
                position, quaternion = pose_by_name[model['bones'][bone]['name']]
                transformed = rotate(quaternion,local)
                for axis in range(3):
                    result[axis] += (position[axis]+transformed[axis])*weight/255.0
            output[index] = tuple(result)
    return output


def read_indices(r):
    width, = r.unpack('B')
    if width not in (2,4):
        raise ValueError('索引宽度不支持')
    if r.integer() != width:
        raise ValueError('索引元素大小不符')
    count = r.integer(0,12_000_000)
    return list(struct.unpack('<'+('H' if width==2 else 'I')*count,r.take(count*width)))
