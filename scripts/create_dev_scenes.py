"""Build the self-contained procedural renderer suite in assets.

python scripts/create_dev_scenes.py [--regressions] [--output-dir PATH]
Generated scenes reference /assets, the local asset mount.
Generated assets use showcase_* filenames directly inside the standard asset folders.
No models, HDRs, textures or scene files from assets/ are needed to build the suite.
"""
import argparse
import copy
import json
import math
from pathlib import Path
import struct
import zlib

import create_meshes as meshes

ROOT = Path(__file__).resolve().parents[1]
PREFIX = '/assets/'
FILES = {}


def ref(kind, name, suffix):
    return PREFIX + f'{kind}/showcase_{name}.{suffix}'


def matref(name):
    return ref('materials', name, 'plmaterial')


def emit(path, data):
    FILES[path] = (json.dumps(data, indent=2, allow_nan=False) + '\n').encode()


def material(color, roughness=0.6, metalness=0, **extra):
    return dict(format='plmaterial', version=1, model='pbr_metallic_roughness',
                base_color=list(color) if len(color) == 4 else [*color, 1],
                roughness=roughness, metalness=metalness, ior=1.5,
                normal_map_strength=1, occlusion_strength=1,
                alpha={'mode': 'opaque', 'cutoff': 0.5}, **extra)


def quat(axis, angle):
    return [*(x * math.sin(angle/2) for x in axis), math.cos(angle/2)]


def camera(position, target):
    delta = [b-a for a, b in zip(position, target)]
    return dict(projection='perspective', position=position, near=0.1, far=500,
                yaw=math.atan2(delta[0], delta[2]),
                pitch=math.atan2(delta[1], math.hypot(delta[0], delta[2])),
                roll=0, yfov=math.pi/3)


class Scene:
    def __init__(self, name, title, environment='day', settings='quality',
                 position=(13, 10, 22), target=(0, 1, 0)):
        self.name, self.title = name, title
        self.entities = []
        self.add('Scene settings', environment={'environment': ref('environments', environment, 'plenvironment')},
                 renderer={'renderer': ref('settings', settings, 'plrenderer')})
        self.add('Main Camera', camera=camera(list(position), target),
                 script={'file': 'pl_script_camera', 'playing': True,
                         'play_once': False, 'reloadable': True})

    def add(self, name, **components):
        entity = dict(id=len(self.entities)+1, tag={'name': name}, **components)
        self.entities.append(entity)
        return entity

    def obj(self, name, mesh, mat, pos, scale=(1, 1, 1), rotation=(0, 0, 0, 1), casts=True):
        entity = self.add(name, transform={'translation': list(pos), 'scale': list(scale), 'rotation': list(rotation)})
        materials = mat if isinstance(mat, (list,tuple)) else [mat]
        entity['object'] = dict(mesh=ref('meshes', mesh, 'plmesh'), first_submesh=0, submesh_count=len(materials),
                                transform=entity['id'], casts_shadow=casts, receives_shadow=True,
                                renderable=True, material_overrides=[matref(m) for m in materials])
        return entity

    def light(self, name, pos, color, intensity=30, kind='point', direction=(0, -1, 0), shadows=False, reach=9):
        self.add(name, light=dict(type=kind, position=list(pos), direction=list(direction), color=list(color),
                                 intensity=intensity, range=reach, radius=0.05, shadow_resolution=512,
                                 inner_cone_angle=0.35, outer_cone_angle=0.75,
                                 casts_shadow=shadows, contact_shadows=True, visualizer=False))

    def probe(self, name, pos, radius=8):
        self.add(name, transform={'translation': list(pos), 'rotation': [0, 0, 0, 1], 'scale': [1, 1, 1]},
                 probe=dict(resolution=128, samples=128, interval=1, range=radius, realtime=False, include_sky=True))

    def floor(self, size=(12, 8)):
        # The inset ends at y=0; keep the surrounding foundation below it.
        self.obj('Basalt foundation', 'cube', 'basalt', (0, -0.4, 0), (size[0], 0.3, size[1]))
        self.obj('Ivory inset', 'cube', 'ivory', (0, -0.04, 0), (size[0]-0.3, 0.04, size[1]-0.3))

    def finish(self):
        emit(f'scenes/showcase_{self.name}.plscene', dict(format='plscene', version=1, entities=self.entities))
        return self


def png(name, pixel, size=64):
    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag+data))
    raw = b''.join(b'\0' + bytes(c for x in range(size) for c in pixel(x, y, size)) for y in range(size))
    FILES[f'textures/showcase_{name}.png'] = (b'\x89PNG\r\n\x1a\n' +
        chunk(b'IHDR', struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0)) +
        chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))
    emit(f'textures/showcase_{name}.pltexture', dict(file_format='pltexture', version=1, type='2d',
         format='PL_FORMAT_R8G8B8A8_UNORM', color_space='srgb', generate_mips=True, compress=False,
         source=ref('textures', name, 'png')))
    return {'base_color': dict(resource=ref('textures', name, 'pltexture'), uv_set=0,
                              scale=[1, 1], offset=[0, 0], rotation=0)}


def assets():
    palette = {'ivory': (0.72, 0.72, 0.65), 'basalt': (0.055, 0.075, 0.10),
               'coral': (0.75, 0.09, 0.035), 'teal': (0.025, 0.38, 0.42),
               'sand': (0.58, 0.42, 0.23), 'white': (0.9, 0.9, 0.9), 'green': (0.04, 0.55, 0.12)}
    materials = {name: material(color) for name, color in palette.items()}
    materials['forward'] = material(palette['ivory'], clearcoat={'factor': 0, 'roughness': 0.2})
    materials['green_forward'] = material(palette['green'], clearcoat={'factor': 0, 'roughness': 0.2})
    materials['chrome'] = material((0.9, 0.9, 0.9), 0.12, 1)
    materials['coast_grass'] = material((0.19, 0.27, 0.085), 0.95)
    materials['coast_rock'] = material((0.25, 0.27, 0.24), 0.92)
    materials['copper'] = material((0.85, 0.38, 0.16), 0.25, 1)
    materials['brushed'] = material((0.85, 0.38, 0.16), 0.3, 1,
                                    anisotropy={'strength': 0.8, 'rotation': 0.6})
    materials['coat'] = material(palette['coral'], 0.28, clearcoat={'factor': 1, 'roughness': 0.08})
    materials['sheen'] = material(palette['teal'], 0.65, sheen={'color': [0.8, 0.9, 1], 'roughness': 0.45})
    materials['iridescent'] = material((0.7, 0.7, 0.7), 0.18, 0.75,
        iridescence={'factor': 1, 'ior': 1.3, 'thickness_min': 100, 'thickness_max': 400})
    materials['glass'] = material((0.75, 0.95, 1), 0.08, transmission={'factor': 1},
        volume={'thickness': 0.5, 'attenuation_color': [0.7, 0.95, 1], 'attenuation_distance': 2})
    materials['glass_rough'] = copy.deepcopy(materials['glass'])
    materials['glass_rough']['roughness'] = 0.4
    materials['glass_amber'] = material((1,0.8,0.4),0.08,transmission={'factor':1},
        volume={'thickness':1.5,'attenuation_color':[1,0.45,0.08],'attenuation_distance':1})
    materials['blend'] = material((0.1, 0.65, 0.8, 0.35), 0.25)
    materials['blend']['alpha']['mode'] = 'blend'
    materials['glow'] = material((1, 0.4, 0.1), emissive={'color': [1, 0.24, 0.04], 'strength': 8})
    cutout = png('cutout', lambda x, y, n: (255, 255, 255, 255 if (x//8+y//8)%2 else 0))
    mark = png('mark', lambda x, y, n: (255, 255, 255, 255 if
        0.17 < math.hypot(x/n-0.5, y/n-0.5) < 0.43 or abs(x-y) < 3 else 0))
    materials['masked'] = material(palette['teal'], textures=cutout, double_sided=True)
    materials['masked']['alpha']['mode'] = 'mask'
    materials['foliage'] = copy.deepcopy(materials['masked'])
    materials['foliage']['model'] = 'pbr_metallic_roughness_foliage'
    materials['masked_forward'] = copy.deepcopy(materials['masked'])
    materials['masked_forward']['clearcoat'] = {'factor': 0, 'roughness': 0.2}
    for name, color in [('decal_coral', palette['coral']), ('decal_teal', palette['teal'])]:
        materials[name] = material(color, textures=mark, double_sided=True)
    for i, roughness in enumerate((0.08, 0.25, 0.5, 0.8)):
        materials[f'metal_{i}'] = material((0.8, 0.65, 0.35), roughness, 1)
        materials[f'ceramic_{i}'] = material(palette['teal'], roughness)
    water = dict(wave_mode='gerstner', fft_resolution=128, fft_seed=1, gerstner_steepness=0.65,
        fft_patch_size=32, fft_height_limit=1.5, fft_wind_speed=6, fft_wind_heading_degrees=35,
        fft_against_wind=0.07, fft_short_wave_damping=0.18, fft_choppiness=1.2, fft_speed=1,
        swell0_amplitude=0.18, swell0_wavelength=9, swell0_speed=0.8, swell0_heading_degrees=25,
        swell1_amplitude=0.07, swell1_wavelength=4, swell1_speed=0.6, swell1_heading_degrees=115,
        detail_scale=0.06, detail_speed=0.4, foam_speed=0.3, foam_scale=0.06,
        shore_strength=1, shore_depth=0.4, shore_ray_fade_start=1, shore_ray_fade_end=2,
        whitecap_strength=0.7, crest_start=0.5, crest_full=0.9, slope_start=0.01, slope_full=0.09,
        minimum_roughness=0.08, foam_roughness=0.75, foam_normal_flatten=0.65,
        refraction_max_distance=10, opacity_distance=5, refraction_edge_fade_pixels=24,
        refraction_intersection_fade=0.15, absorption_red=0.35, absorption_green=0.8,
        absorption_blue=0.9, absorption_distance=5, scattering_red=0.2,
        scattering_green=0.55, scattering_blue=0.65, scattering_strength=0.08,
        scattering_direction=0.35, refraction_strength=1, refraction_max_pixels=12)
    for name, mode, scattering in [('water', 'gerstner', 0.08), ('water_absorption', 'gerstner', 0),
                                   ('ocean', 'fft', 0.08)]:
        m = material((0.015, 0.09, 0.12, 0.65), 0.16, double_sided=True)
        m.update(model='water', ior=1.333, normal_map_strength=0.45,
                 alpha={'mode': 'blend', 'cutoff': 0.1}, water=dict(water, wave_mode=mode, scattering_strength=scattering))
        materials[name] = m
    materials['coast_water'] = copy.deepcopy(materials['ocean'])
    materials['coast_water']['water'].update(shore_strength=0.45, shore_depth=0.22)
    for name, data in materials.items():
        emit(f'materials/showcase_{name}.plmaterial', data)
    primitive_meshes = dict(plane=meshes.grid(1, matref('ivory')), sphere=meshes.sphere(1, 24, matref('ivory')),
        cube=meshes.cube(matref('ivory')), cylinder=meshes.cylinder(matref('ivory')),
        cone=meshes.cone(matref('ivory')), torus=meshes.torus(matref('ivory')),
        water_patch=meshes.grid(128, matref('water')))
    for name, mesh in primitive_meshes.items():
        FILES[f'meshes/showcase_{name}.plmesh'] = (json.dumps(dict(format='plmesh', version=1,
            aabb=mesh['aabb'], submeshes=[mesh]), separators=(',', ':'))+'\n').encode()
    terrain = meshes.island(matref('sand'))
    submeshes = []
    for mat in ('sand','coast_rock','coast_grass'):
        part = dict(terrain, material=matref(mat), indices=[])
        submeshes.append(part)
    # Partition one surface, rather than stacking overlapping island shells.
    for i in range(0,len(terrain['indices']),3):
        tri=terrain['indices'][i:i+3]
        x,y,z = [sum(terrain['positions'][3*j+k] for j in tri)/3 for k in range(3)]
        up=sum(terrain['normals'][3*j+1] for j in tri)/3
        beach_height=0.04+0.018*math.sin(7*x+4*z)
        zone=0 if y<beach_height else (1 if up<0.94 or math.sin(x*9-z*5)>0.75 else 2)
        submeshes[zone]['indices'].extend(tri)
    FILES['meshes/showcase_island.plmesh']=(json.dumps(dict(format='plmesh',version=1,
        aabb=terrain['aabb'],submeshes=submeshes),separators=(',',':'))+'\n').encode()
    day = dict(format='plenvironment', version=1,
        sun=dict(color=[1, 0.95, 0.85], direction=[0.425, -1, -0.384], intensity=3,
                 cast_shadows=True, contact_shadows=True),
        sky={'mode': 'realistic', 'realistic': dict(atmosphere_scale=0.001, sun_radius=0.00465,
            planet_radius=6371, atmosphere_height=100, mie_scattering=0.006, mie_extinction=0.00666,
            mie_exponent=0.76, rayleigh_scattering=[0.0058, 0.0135, 0.0331],
            rayleigh_extinction=[0.0058, 0.0135, 0.0331], ozone_extinction=[0.00065, 0.00188, 0.00008],
            multiscatter=True, clouds={'enabled': False},
            aerial_perspective=dict(enabled=True, max_distance=10,
                                    depth_exponent=2, samples_per_slice=8))})
    night = copy.deepcopy(day); night['sun']['intensity'] = 0; night['sky'] = {'mode': 'none'}
    coast = copy.deepcopy(day)
    coast['sky']['realistic']['clouds'] = dict(enabled=True, coverage=0.55, base_height=0.12,
        thickness=0.18, extinction=12, scale=0.1, wind=[0.001, 0.0003], steps=64,
        resolution_divisor=2, multiple_scattering=0.5, temporal_blend=0.9)
    for name, env in [('day', day), ('night', night), ('coast', coast)]:
        emit(f'environments/showcase_{name}.plenvironment', env)
    quality = dict(format='plrenderer', version=1,
        sky=dict(sky_lut_resolution=[200,100], transmission_lut_resolution=[256,256],
                 multiscatter_lut_resolution=[64,64], aerial_lut_resolution=[128,128,32]),
        lighting=dict(image_based=True, normal_mapping=True, punctual_lights=True, shadows=True),
        shadows=dict(max_range=100, cascade_count=3, resolution=2048, pcf=True, contact_shadows=True,
                     contact_length=0.3, contact_thickness=0.05, contact_bias=0.01, contact_steps=16,
                     multiviewport=True, constant_bias=-1, slope_bias=-1.0),
        fxaa={'enabled': True}, ambient_occlusion=dict(mode='ssao', radius=0.5, strength=1, bias=0.05),
        ssgi=dict(enabled=False, strength=1, radius=2, thickness=0.15, ray_count=8, step_count=16),
        bloom=dict(enabled=False, strength=0.05, radius=1.5, chain_length=5), fog={'enabled': False},
        tonemap=dict(mode='pbr_neutral', exposure=1, brightness=0, contrast=1, saturation=1))
    gi = copy.deepcopy(quality); gi['ssgi']['enabled'] = True; gi['lighting']['image_based'] = False
    gi['ambient_occlusion']['mode'] = 'none'
    night_settings = copy.deepcopy(quality); night_settings['bloom']['enabled'] = True
    night_settings['lighting']['image_based'] = False
    water_settings = copy.deepcopy(quality); water_settings['ambient_occlusion']['mode'] = 'none'
    water_settings['shadows']['contact_shadows'] = False
    integrated = copy.deepcopy(quality); integrated['ssgi']['enabled'] = True
    ao_settings = copy.deepcopy(quality)
    # AO attenuates ambient IBL, not direct light. Keep the sky illumination but
    # disable direct sun/local lighting so unshadowed light cannot hide the AO.
    ao_settings['lighting']['punctual_lights'] = False
    ao_settings['lighting']['shadows'] = False
    ao_settings['shadows']['contact_shadows'] = False
    ao_settings['ambient_occlusion'].update(radius=0.6, strength=1.2)
    for name, settings in [('quality', quality), ('gi', gi), ('night', night_settings), ('water', water_settings), ('integrated', integrated), ('ao', ao_settings)]:
        emit(f'settings/showcase_{name}.plrenderer', settings)


def gallery():
    s = Scene('01_materials', 'Material atelier', position=(14, 10, 21), target=(0, 1, 0)); s.floor((10, 7))
    for row, kind in enumerate(('ceramic', 'metal')):
        for col in range(4):
            x, z = (col-1.5)*3, (row-0.5)*3
            s.obj(f'{kind} roughness {col}', 'sphere', f'{kind}_{col}', (x, 1.35, z), (0.95,)*3)
            s.obj('Low basalt plinth', 'cylinder', 'basalt', (x, 0.2, z), (1.1, 0.2, 1.1))
    for i, mat in enumerate(('coat', 'sheen', 'iridescent', 'glass')):
        x = (i-1.5)*3
        s.obj(mat.title(), 'sphere', mat, (x, 1.35, -4.7), (0.95,)*3)
        s.obj('Plinth', 'cylinder', 'basalt', (x, 0.2, -4.7), (1.1, 0.2, 1.1))
    s.obj('Anisotropic copper ring', 'torus', 'brushed', (7.4, 1.7, -1.5), (1.1,)*3, quat((1,0,0), math.pi/2))
    for i in range(12):
        s.obj(f'FXAA diagonal edge {i}', 'cube', 'basalt', (-5.5+i, 0.12, 5.3),
              (0.025, 0.12, 0.8), quat((0,1,0), 0.45))
    s.probe('Atelier reflection capture', (0, 3, 0), 20)
    return s.finish()


def lighting(name='02_lighting', count=24, mode='distributed', isolated=None, forward=None):
    s = Scene(name, 'Lantern courtyard', 'day' if isolated=='sun' else 'night',
              'night', position=(15, 11, 22)); s.floor()
    for x in (-8, -4, 0, 4, 8):
        s.obj('Column', 'cylinder', 'forward' if forward else 'ivory', (x, 1.7, -4), (0.4, 1.7, 0.4))
        s.obj('Lintel', 'cube', 'ivory', (x, 3.6, -4), (1.6, 0.16, 0.5))
    for i, mesh in enumerate(('sphere', 'torus', 'cube', 'cone', 'sphere')):
        s.obj(f'{mesh} / deferred', mesh, 'ivory', ((i-2)*3.4, 1, 0), (0.8,)*3)
        s.obj(f'{mesh} / forward', mesh, 'forward', ((i-2)*3.4, 1, 3.3), (0.8,)*3)
    # Grazing occluders make contact shadow separation easy to inspect.
    for i in range(8):
        s.obj(f'Contact step {i}', 'cube', 'coral', (-7+i*2, 0.12+i*0.035, 5.8), (0.45, 0.08, 0.35))
    if isolated and isolated != 'sun':
        s.light('Isolated '+isolated, (0, 5, 1), (1, 0.9, 0.75), 2 if isolated=='direction' else 100,
                kind=isolated, direction=(-0.4, -1, -0.3), shadows=True, reach=25)
    elif not isolated:
        colors = [(1, 0.16, 0.04), (0.04, 0.5, 1), (0.2, 1, 0.5)]
        for i in range(count):
            if mode=='overflow': pos=(0.2*math.cos(i), 2.5, 0.2*math.sin(i))
            else:
                rows = math.ceil(count/8)
                pos=(-9+(i%8)*2.6, 2.5, -4+(i//8)*8/max(rows-1,1))
            s.light(f'Cluster light {i}', pos, colors[i%3], 0.4 if mode=='overflow' else 16,
                    kind='spot' if forward is not None and i%2 else 'point',
                    reach=12 if mode=='overflow' else 4.5)
            if i<24:
                s.obj('Lantern', 'sphere', 'glow', pos, (0.09,)*3, casts=False)
        s.light('Shadowed warm spot', (-5, 5, 2), (1, 0.6, 0.3), 90, 'spot', (0.2, -1, -0.2), True, 14)
        s.light('Shadowed cool point', (5, 3, 2), (0.3, 0.6, 1), 40, shadows=True, reach=10)
    if forward is not None:
        # Matched benchmark geometry; only the render path of the receivers changes.
        for entity in s.entities:
            obj = entity.get('object')
            if obj and obj['material_overrides'][0] in (matref('ivory'), matref('forward')):
                obj['material_overrides'] = [matref('forward' if forward else 'ivory')]
    return s.finish()


def indirect():
    s = Scene('03_indirect', 'Color-bounce pavilion', 'night', 'gi', position=(12, 8, 18)); s.floor((8, 6))
    s.obj('Coral bounce wall', 'cube', 'coral', (-6, 2.5, -1), (0.15, 2.5, 4))
    s.obj('Green bounce wall / forward', 'cube', 'green_forward', (6, 2.5, -1), (0.15, 2.5, 4))
    s.obj('Back wall', 'cube', 'ivory', (0, 2.5, -5), (6, 2.5, 0.15))
    for i, mat in enumerate(('ivory', 'forward', 'chrome')):
        s.obj(mat+' bounce receiver', 'sphere', mat, ((i-1)*3.2, 1.2, -0.8), (1.2,)*3)
    s.obj('Occlusion bench', 'cube', 'ivory', (0, 0.55, 3), (3, 0.15, 0.6))
    for x in (-2.5, 2.5): s.obj('Bench leg', 'cube', 'ivory', (x, 0.2, 3), (0.15, 0.2, 0.45))
    s.light('Ceiling point', (0, 4.8, 1), (1, 1, 1), 100, shadows=True, reach=20)
    return s.finish()


def decals():
    s = Scene('04_decals', 'Stencil garden', position=(11, 8, 16)); s.floor((9, 7))
    s.obj('Curved receiver', 'sphere', 'ivory', (-4, 1, -2), (1,)*3)
    s.obj('Forward receiver', 'cube', 'forward', (0, 0.6, -2), (1.2, 0.6, 1.2))
    s.obj('Wall receiver', 'cube', 'ivory', (4, 2, -3), (2, 2, 0.2))
    for i, (x, z, order, mat) in enumerate(((-2, 1, 0, 'decal_coral'), (-0.8, 1, 1, 'decal_teal'),
                                           (-4, -2, 0, 'decal_teal'), (0, -2, 0, 'decal_coral'))):
        s.add(f'Floor projector {i} / sort {order}',
              transform=dict(translation=[x, 0.5, z], rotation=quat((1,0,0), -math.pi/2), scale=[3,3,3]),
              decal=dict(material=matref(mat), enabled=True, opacity=1, edge_fade=0.08, normal_cutoff=0.1, sort_order=order))
    s.add('Vertical wall stencil', transform=dict(translation=[4,2,-2.75], rotation=[0,0,0,1], scale=[3,3,0.8]),
          decal=dict(material=matref('decal_teal'), enabled=True, opacity=0.8, edge_fade=0.08, normal_cutoff=0.1, sort_order=2))
    for i, mat in enumerate(('masked', 'masked_forward', 'foliage', 'blend')):
        s.obj(mat+' screen', 'plane', mat, ((i-1.5)*3.2, 1.6, 4.8), (2.3, 1, 2.8), quat((1,0,0), math.pi/2), casts=False)
    s.obj('Corner receiver A','cube','ivory',(-6,1.6,0),(0.15,1.6,1.5))
    s.obj('Corner receiver B / forward','cube','forward',(-4.6,1.6,-1.35),(1.25,1.6,0.15))
    s.add('Oblique corner projector / normal rejection',
          transform=dict(translation=[-5.3,1.6,-0.3],rotation=quat((0,1,0),math.pi/4),scale=[3,3,3]),
          decal=dict(material=matref('decal_coral'),enabled=True,opacity=1,edge_fade=0.08,normal_cutoff=0.25,sort_order=3))
    return s.finish()


def probes():
    s = Scene('05_probes', 'Reflection arcade', position=(15, 10, 24)); s.floor((14, 7))
    for x, mat in ((-8, 'coral'), (0, 'teal'), (8, 'sand')):
        s.obj(mat+' alcove', 'cube', mat, (x, 2, -4.5), (3.3, 2, 0.2))
        s.probe(mat+' local probe', (x, 2, -1), 6)
        s.obj('Reflective torus', 'torus', 'chrome', (x, 1.5, -1), (1.2,)*3, quat((1,0,0), math.pi/2))
    for i in range(13):
        s.obj(f'Probe boundary sample {i}', 'sphere', 'chrome' if i%2==0 else 'coat',
              (-12+i*2, 0.65, 3), (0.6,)*3)
    return s.finish()


def water():
    s = Scene('06_water', 'Water laboratory', settings='water', position=(24, 15, 29), target=(0,0,0))
    # Keep the foundation below every sloped bed: a floor at y=0 would hide it.
    s.obj('Laboratory foundation', 'cube', 'basalt', (0,-2.5,0), (20,0.3,9))
    for x, mat in ((-12, 'water_absorption'), (0, 'water'), (12, 'ocean')):
        s.obj(mat+' surface', 'water_patch', mat, (x, 0.8, 0), (10, 1, 14), casts=False)
        # A tilted simple slab gives a real shallow-to-deep bed without a custom mesh.
        s.obj('Sloping sand bed', 'cube', 'sand', (x, -0.1, 0), (4.9, 0.15, 6.9), quat((1,0,0), 0.13))
        for dx in (-5.25, 5.25): s.obj('Pool rim', 'cube', 'basalt', (x+dx,0.3,0), (0.25,0.8,7.4))
        for z in (-7.25,7.25): s.obj('Pool rim', 'cube', 'basalt', (x,0.3,z), (5.5,0.8,0.25))
        for i in range(5):
            z = -5+i*2.5
            s.obj(f'Depth marker {i}', 'cube', 'white' if i%2==0 else 'coral',
                  (x,0.1-z*math.tan(0.13),z), (3.8,0.03,0.12), quat((1,0,0),0.13))
        s.obj('Piercing refraction target', 'sphere', 'coral', (x+1.5,0.7,1.5), (0.8,)*3)
        for i in range(6):
            top=1.2-i*0.22
            s.obj('Submerged staircase','cube','ivory',(x-3,top-0.12,-3+i),(0.7,0.12,0.5))
        s.obj('Pier deck','cube','sand',(x+3,1.6,4.5),(0.65,0.15,2.5))
        for z in (2.5,6):
            s.obj('Pier post / water intersection','cylinder','basalt',(x+3,0.4,z),(0.16,1.4,0.16))
    return s.finish()


def coast():
    s = Scene('07_coast', 'Cloud coast', 'coast', 'water', position=(48,22,70), target=(0,5,-12))
    s.obj('FFT ocean / finite patch', 'water_patch', 'coast_water', (0,0,0), (220,1,220), casts=False)
    for x,z,r,height,angle in ((0,-8,21,20,0),(-35,-29,15,25,1.7),(34,-48,18,15,-0.8)):
        s.obj('Irregular island / beach, rock, grass', 'island', ('sand','coast_rock','coast_grass'),
              (x,0,z), (r,height,r*0.8), quat((0,1,0),angle))
    # The island's central height is 0.22 * 20 = 4.4 world units.
    s.obj('Lighthouse footing', 'cylinder', 'coast_rock', (0,4.5,-8), (2.4,0.3,2.4))
    s.obj('Lighthouse tower', 'cylinder', 'ivory', (0,10.8,-8), (1.7,6,1.7))
    s.obj('Lantern housing', 'cylinder', 'basalt', (0,17.1,-8), (2.2,0.4,2.2))
    s.obj('Lighthouse cap', 'cone', 'coral', (0,18.8,-8), (2.5,1.2,2.5))
    s.obj('Beacon', 'sphere', 'glow', (0,17.8,-8), (0.7,)*3)
    s.probe('Coast reflection capture', (0,10,0), 150)
    return s.finish()


def animate(scene, name, entity, positions, duration=12):
    times = [duration*i/(len(positions)-1) for i in range(len(positions))]
    emit(f'animations/showcase_{name}.planimation', dict(format='planimation', version=1,
        start=0, end=duration, channels=[dict(data=0,target=0,path='translation',mode='linear')],
        data=[dict(times=times,data=[v for p in positions for v in p])]))
    scene.add(name+' / looping motion', animation=dict(animation=ref('animations',name,'planimation'),
        targets=[entity['id']], playing=True, looped=True, speed=1, blend_amount=1))


def shadow_courtyard():
    s=Scene('08_shadows','Shadow courtyard',position=(13,7,32),target=(0,2,-20))
    s.floor((18,90))
    for i,z in enumerate((12,0,-16,-36,-64,-82)):
        for x in (-8,8):
            s.obj(f'Distance station {i} / pillar','cylinder','ivory',(x,4,z),(0.7,4,0.7))
        s.obj(f'Distance station {i} / lintel','cube','ivory',(0,8,z),(8.7,0.45,0.7))
        s.obj(f'Distance station {i} / thin wall','cube','forward',(2,2,z+3),(0.035,2,1.5))
        s.obj(f'Distance station {i} / floating contact target','sphere','coral',(-3,0.7,z+3),(0.6,)*3)
    for i in range(12):
        h=(i+1)*0.15
        s.obj('Stair tread','cube','basalt',(-12,h/2,16-i*0.8),(1.8,h/2,0.4))
    return s.finish()


def glass_lab():
    s=Scene('09_glass','Glass and refraction laboratory',position=(13,8,22),target=(0,2,0))
    s.floor((11,8))
    for i in range(19):
        s.obj('Refraction background stripe','cube',('coral','teal','white')[i%3],
              (-9+i,2.5,-4),(0.35,2.5,0.2))
    for i,mat in enumerate(('glass','glass_rough','glass_amber')):
        x=(i-1)*5
        s.obj(mat+' sphere','sphere',mat,(x,1.5,0),(1.5,)*3)
        s.obj(mat+' slab','cube',mat,(x,1.7,4),(1.6,1.7,0.18))
        s.obj('Object between refractors','torus','copper',(x,1.5,2),(0.65,)*3,quat((1,0,0),math.pi/2))
    s.obj('Overlapping alpha panel A','plane','blend',(-8,2,2),(3,1,3),quat((1,0,0),math.pi/2),casts=False)
    s.obj('Overlapping alpha panel B','plane','blend',(-7,2,3),(3,1,3),quat((1,0,0),math.pi/2),casts=False)
    s.probe('Laboratory capture',(0,3,2),25)
    return s.finish()


def corridor():
    s=Scene('10_corridor','Indoor outdoor transition corridor',settings='integrated',
            position=(0,2.5,15),target=(0,2,-20))
    s.floor((12,30))
    s.obj('Interior west wall','cube','coral',(-4,3,5),(0.2,3,15))
    s.obj('Interior east wall','cube','teal',(4,3,5),(0.2,3,15))
    s.obj('Interior roof','cube','ivory',(0,6.2,5),(4.2,0.2,15))
    for z in (12,4,-4,-12,-20):
        for x,mat in ((-2.5,'chrome'),(2.5,'coat')):
            s.obj('Lighting boundary receiver','sphere',mat,(x,1,z),(0.85,)*3)
    for x in (-8,8):
        s.obj('Courtyard column','cylinder','ivory',(x,4,-20),(0.7,4,0.7))
    s.light('Dim interior lamp',(0,4,10),(1,0.65,0.35),8,reach=12)
    s.probe('Interior capture',(0,2,9),10)
    s.probe('Doorway capture',(0,2,-8),9)
    s.probe('Courtyard capture',(0,3,-23),14)
    return s.finish()


def motion():
    s=Scene('11_motion','Motion and disocclusion',environment='coast',settings='integrated',
            position=(0,5,24),target=(0,3,0))
    s.floor((15,12))
    for i in range(15):
        s.obj('Thin foreground fence','cube','basalt',(-10+i*1.4,3,8),(0.045,3,0.045))
    for i in range(7):
        s.obj('Color behind moving occluder','sphere',('coral','teal','ivory')[i%3],
              (-9+i*3,1.3,-3),(1.3,)*3)
    panel=s.obj('Moving occluder','cube','forward',(-7,3,2),(2,3,0.15))
    animate(s,'moving_occluder',panel,[(-7,3,2),(7,3,2),(-7,3,2)],10)
    cam=s.entities[1]
    cam.pop('script')
    cam['tag']['name']='Automated camera / 40 second loop'
    cam['transform']=dict(translation=[0,5,24],rotation=[0,0,0,1],scale=[1,1,1])
    animate(s,'camera_disocclusion',cam,[(0,5,24),(9,5,24),(-9,5,24),(0,5,24),
        (0,150,24),(0,320,24),(0,150,24),(0,5,24),(0,5,24)],40)
    return s.finish()


def ao_lab():
    s=Scene('12_ao','Ambient occlusion laboratory',settings='ao',
            position=(17,13,24),target=(0,1,0))
    s.obj('Laboratory foundation','cube','basalt',(0,-0.4,0),(13,0.3,9))
    # Matched geometry and base color; the zero-strength coat selects forward shading.
    for center,mat,label in ((-6,'ivory','Deferred'),(6,'forward','Forward')):
        s.obj(label+' floor receiver','cube',mat,(center,-0.04,0),(5.6,0.04,8.7))
        s.obj(label+' back wall','cube',mat,(center,2,-5),(4,2,0.15))
        s.obj(label+' corner wall','cube',mat,(center-3.85,2,-2.5),(0.15,2,2.35))
        s.obj(label+' floor contact sphere','sphere',mat,(center-1.7,1,-1.8),(1,)*3)
        s.obj(label+' contact cube','cube',mat,(center+1.2,0.7,-2.5),(0.7,)*3)
        s.obj(label+' stacked cube','cube',mat,(center+1.4,1.85,-2.5),(0.45,)*3)
        s.obj(label+' narrow-gap bench','cube',mat,(center,0.45,0.8),(2.5,0.15,0.6))
        for dx in (-2.1,2.1):
            s.obj(label+' bench leg','cube',mat,(center+dx,0.15,0.8),(0.15,)*3)
        for i,gap in enumerate((0,0.05,0.2,0.6)):
            s.obj(f'{label} sphere / floor clearance {gap:g}','sphere',mat,
                  (center-2.4+i*1.6,0.45+gap,3.3),(0.45,)*3)
        for i in range(5):
            height=0.15*(i+1)
            s.obj(label+' stair crevice','cube',mat,(center+2.8,height/2,-0.8+i*0.6),
                  (0.5,height/2,0.3))
    # Alpha cases remain distinct from the matched opaque rows.
    for i,mat in enumerate(('masked','masked_forward','foliage','blend')):
        s.obj(mat+' AO edge case','plane',mat,(-5.4+i*3.6,1.25,6.5),(2.3,1,2.3),
              quat((1,0,0),math.pi/2),casts=False)
    return s.finish()


def validate():
    documents = {p: json.loads(b) for p,b in FILES.items() if p.endswith(('.plscene','.plmesh','.plmaterial','.pltexture','.plrenderer','.plenvironment','.planimation'))}
    def walk(value):
        if isinstance(value,dict):
            for v in value.values(): yield from walk(v)
        elif isinstance(value,list):
            for v in value: yield from walk(v)
        elif isinstance(value,str): yield value
    for path, data in documents.items():
        for value in walk(data):
            if value.startswith('/'):
                if not value.startswith(PREFIX) or value[len(PREFIX):] not in FILES:
                    raise ValueError(f'{path}: external or missing dependency {value}')
        if path.endswith('.plscene'):
            ids = [e['id'] for e in data['entities']]
            assert len(ids)==len(set(ids)), path
            assert sum('camera' in e for e in data['entities'])==1, path
            for e in data['entities']:
                if 'animation' in e:
                    animation=e['animation']
                    assert all(t in ids for t in animation['targets']), path
                    clip=documents[animation['animation'][len(PREFIX):]]
                    assert all(c['target']<len(animation['targets']) for c in clip['channels']), path
                if 'object' in e:
                    assert e['object']['transform']==e['id'] and 'transform' in e, path
                if 'transform' in e:
                    assert all(v>0 for v in e['transform']['scale']), path
                    assert abs(sum(v*v for v in e['transform']['rotation'])-1)<1e-6, path
        if path.endswith('.planimation'):
            assert data['end']>data['start'], path
            for channel in data['channels']:
                track=data['data'][channel['data']]
                assert all(a<b for a,b in zip(track['times'],track['times'][1:])), path
                assert len(track['data'])==len(track['times'])*3, path
                assert track['data'][:3]==track['data'][-3:], path
        if path.endswith('.plrenderer'):
            assert all(v>0 for extent in data['sky'].values() for v in extent), path
            assert data['shadows']['cascade_count']==3 and data['shadows']['slope_bias']==-1, path
        if path.endswith('.plmesh'):
            for m in data['submeshes']:
                count=len(m['positions'])//3
                assert len(m['normals'])==3*count and len(m['tangents'])==4*count and len(m['uv_0'])==2*count
                assert len(m['indices'])%3==0 and all(0<=i<count for i in m['indices'])
                for i,v in enumerate(m['positions']):
                    assert math.isfinite(v) and m['aabb'][i%3]-1e-6<=v<=m['aabb'][i%3+3]+1e-6
    return documents


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir',type=Path,default=ROOT/'assets',
                        help='Output root (default: assets); references remain rooted at /assets')
    parser.add_argument('--regressions',action='store_true',help='Also create focused stress/isolation scenes')
    args=parser.parse_args()
    FILES.clear()
    assets()
    scenes=[gallery(),lighting(),indirect(),decals(),probes(),water(),coast(),
            shadow_courtyard(),glass_lab(),corridor(),motion(),ao_lab()]
    if args.regressions:
        for kind in ('sun','point','spot','direction'):
            scenes.append(lighting('regression_shadow_'+kind,isolated=kind))
        for forward in (False,True):
            for mode,count in (('sparse',4),('distributed',192),('overflow',192)):
                scenes.append(lighting('regression_cluster_'+('forward_' if forward else 'deferred_')+mode,count,mode,forward=forward))
    documents=validate()
    for path,data in FILES.items():
        output=args.output_dir/path
        output.parent.mkdir(parents=True,exist_ok=True)
        output.write_bytes(data)
    print(f'Generated {len(scenes)} scenes and {len(FILES)} files in {args.output_dir}')
    print(f'Validated {len(documents)} asset documents; all asset references resolve within the suite.')


if __name__=='__main__':
    main()
