"""Generate plane, sphere, probe, cube, cylinder, cone, torus and water meshes.

Run from any directory: python scripts/gen_meshes.py
Use --output-dir to generate into another directory for inspection.
"""
import argparse
import json
import math
from pathlib import Path


def grid(cells, material):
    width = cells + 1
    mesh = {
        "material": material,
        "aabb": [-0.5, 0, -0.5, 0.5, 0, 0.5],
        "positions": [], "normals": [], "tangents": [],
        "uv_0": [], "indices": [],
    }
    for z in range(width):
        for x in range(width):
            mesh["positions"].extend([x / cells - 0.5, 0, z / cells - 0.5])
            mesh["normals"].extend([0, 1, 0])
            mesh["tangents"].extend([1, 0, 0, -1])
            mesh["uv_0"].extend([x / cells, z / cells])
    for z in range(cells):
        for x in range(cells):
            a = z * width + x
            mesh["indices"].extend([a, a + width, a + 1,
                                    a + 1, a + width, a + width + 1])
    return mesh


def sphere(radius, bands, material):
    """Y-up UV sphere, matching the renderer's radii and band counts."""
    mesh = {
        "material": material,
        "aabb": [-radius] * 3 + [radius] * 3,
        "positions": [], "normals": [], "tangents": [],
        "uv_0": [], "indices": [],
    }
    for latitude in range(bands + 1):
        theta = math.pi * latitude / bands
        sin_theta = 0.0 if latitude in (0, bands) else math.sin(theta)
        for longitude in range(bands + 1):
            # Duplicate the seam exactly, with separate UVs.
            phi = math.tau * (longitude % bands) / bands
            normal = [math.cos(phi) * sin_theta, math.cos(theta),
                      math.sin(phi) * sin_theta]
            mesh["positions"].extend(component * radius for component in normal)
            mesh["normals"].extend(normal)
            mesh["tangents"].extend([-math.sin(phi), 0, math.cos(phi), 1])
            mesh["uv_0"].extend([longitude / bands, latitude / bands])
    width = bands + 1
    for latitude in range(bands):
        for longitude in range(bands):
            first = latitude * width + longitude
            second = first + width
            # Outward winding; omit the collapsed triangles at either pole.
            if latitude > 0:
                mesh["indices"].extend([first + 1, second, first])
            if latitude < bands - 1:
                mesh["indices"].extend([first + 1, second + 1, second])
    return mesh


def island(material, rings=24, segments=96):
    """Irregular radial heightfield: submerged perimeter, beaches and low hills.

    Unit-scale footprint, sea level y=0. Shared smooth normals keep material
    boundaries continuous when the caller partitions triangles into submeshes.
    """
    mesh = primitive(material, [0]*6)
    positions = [(0, 0.22, 0)]
    for ring in range(1, rings+1):
        r = ring/rings
        for sector in range(segments):
            a = math.tau*sector/segments
            outline = 1 + 0.16*math.sin(3*a+0.6) + 0.09*math.cos(5*a-0.4) + 0.04*math.sin(9*a)
            x, z = r*outline*math.cos(a), r*outline*math.sin(a)
            # A gentle, uneven inland ridge falls through a broad shore into sea.
            h = 0.22 - 0.48*r**2.8
            h += 0.085*math.sin(x*5+z*2)*math.sin(z*4-x)*math.sin(math.pi*r)
            positions.append((x,h,z))
    indices = []
    for j in range(segments):
        indices.extend((0, 1+(j+1)%segments, 1+j))
    for ring in range(1,rings):
        inner, outer = 1+(ring-1)*segments, 1+ring*segments
        for j in range(segments):
            k=(j+1)%segments
            indices.extend((inner+j, inner+k, outer+j, inner+k, outer+k, outer+j))
    normals = [[0.,0.,0.] for _ in positions]
    for i in range(0,len(indices),3):
        a,b,c = indices[i:i+3]
        u = [positions[b][k]-positions[a][k] for k in range(3)]
        v = [positions[c][k]-positions[a][k] for k in range(3)]
        n = [u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0]]
        for j in (a,b,c):
            for k in range(3): normals[j][k] += n[k]
    for p,n in zip(positions,normals):
        length=math.sqrt(sum(v*v for v in n)); n=[v/length for v in n]
        t=[n[1],-n[0],0]; length=math.sqrt(sum(v*v for v in t)); t=[v/length for v in t]
        vertex(mesh,p,n,(*t,-1),((p[0]+1.4)/2.8,(p[2]+1.4)/2.8))
    mesh['indices']=indices
    mesh['aabb']=[min(p[k] for p in positions) for k in range(3)]+[max(p[k] for p in positions) for k in range(3)]
    return mesh


def ocean(material, inner_cells=128, outer_cells=32, inner_extent=128, extent=65536):
    """One continuous ocean grid, dense near islands and coarse at the horizon."""
    positive = [inner_extent*i/(inner_cells//2) for i in range(inner_cells//2+1)]
    positive += [inner_extent*(extent/inner_extent)**(i/outer_cells) for i in range(1,outer_cells+1)]
    axis = [-v for v in reversed(positive[1:])] + positive
    cells=len(axis)-1
    mesh=grid(cells,material)
    for z,pz in enumerate(axis):
        for x,px in enumerate(axis):
            i=z*len(axis)+x
            mesh['positions'][3*i:3*i+3]=[px,0,pz]
            mesh['uv_0'][2*i:2*i+2]=[px/(2*inner_extent)+0.5,pz/(2*inner_extent)+0.5]
    mesh['aabb']=[-extent,0,-extent,extent,0,extent]
    return mesh


def write_mesh(output, mesh):
    data = {"format": "plmesh", "version": 1,
            "aabb": mesh["aabb"], "submeshes": [mesh]}
    output.write_text(json.dumps(data, separators=(",", ":")) + "\n", encoding="utf-8")
    print(f"Wrote {output}: {len(mesh['positions']) // 3} vertices, "
          f"{len(mesh['indices']) // 3} triangles")


def primitive(material, bounds):
    return {"material": material, "aabb": bounds,
            "positions": [], "normals": [], "tangents": [],
            "uv_0": [], "indices": []}


def vertex(mesh, position, normal, tangent, uv):
    index = len(mesh["positions"]) // 3
    for stream, values in (("positions", position), ("normals", normal),
                           ("tangents", tangent), ("uv_0", uv)):
        mesh[stream].extend(values)
    return index


def cube(material):
    """2-unit cube matching resources/cube.stl, with hard face normals."""
    mesh = primitive(material, [-1, -1, -1, 1, 1, 1])
    for normal, tangent in (((1, 0, 0), (0, 0, -1)),
                            ((-1, 0, 0), (0, 0, 1)),
                            ((0, 1, 0), (1, 0, 0)),
                            ((0, -1, 0), (1, 0, 0)),
                            ((0, 0, 1), (1, 0, 0)),
                            ((0, 0, -1), (-1, 0, 0))):
        nx, ny, nz = normal
        tx, ty, tz = tangent
        bitangent = (ny * tz - nz * ty, nz * tx - nx * tz, nx * ty - ny * tx)
        base = len(mesh["positions"]) // 3
        for u, v in ((0, 0), (1, 0), (1, 1), (0, 1)):
            position = [normal[i] + (2*u-1)*tangent[i] + (2*v-1)*bitangent[i]
                        for i in range(3)]
            vertex(mesh, position, normal, (*tangent, 1), (u, v))
        mesh["indices"].extend(base + i for i in (0, 1, 2, 0, 2, 3))
    return mesh


def cylinder(material, segments=32):
    """Y-up cylinder: radius 1, height 2; separate cap and side vertices."""
    mesh = primitive(material, [-1, -1, -1, 1, 1, 1])
    for y in (-1, 1):
        for i in range(segments + 1):
            angle = math.tau * (i % segments) / segments
            c, s = math.cos(angle), math.sin(angle)
            vertex(mesh, (c, y, s), (c, 0, s), (-s, 0, c, -1),
                   (i / segments, (y + 1) / 2))
    for i in range(segments):
        upper = i + segments + 1
        mesh["indices"].extend((i, upper, i + 1, i + 1, upper, upper + 1))
    for y in (-1, 1):
        center = vertex(mesh, (0, y, 0), (0, y, 0), (1, 0, 0, -y), (0.5, 0.5))
        for i in range(segments):
            angle = math.tau * i / segments
            c, s = math.cos(angle), math.sin(angle)
            vertex(mesh, (c, y, s), (0, y, 0), (1, 0, 0, -y),
                   ((c + 1) / 2, (s + 1) / 2))
        for i in range(segments):
            a, b = center + 1 + i, center + 1 + (i + 1) % segments
            mesh["indices"].extend((center, b, a) if y > 0 else (center, a, b))
    return mesh


def cone(material, segments=32):
    """Y-up cone: base radius 1 at y=-1, apex at y=1, with a hard base rim."""
    mesh = primitive(material, [-1, -1, -1, 1, 1, 1])
    length = math.sqrt(5)
    for i in range(segments + 1):
        angle = math.tau * (i % segments) / segments
        c, s = math.cos(angle), math.sin(angle)
        vertex(mesh, (c, -1, s), (2*c/length, 1/length, 2*s/length),
               (-s, 0, c, -1), (i / segments, 0))
    # Each triangle has its own apex UV and normal to avoid wrapping the seam
    # or inventing a single normal for the cone's singular tip.
    for i in range(segments):
        angle = math.tau * (i + 0.5) / segments
        c, s = math.cos(angle), math.sin(angle)
        tip = vertex(mesh, (0, 1, 0), (2*c/length, 1/length, 2*s/length),
                     (-s, 0, c, -1), ((i + 0.5) / segments, 1))
        mesh["indices"].extend((i, tip, i + 1))
    center = vertex(mesh, (0, -1, 0), (0, -1, 0), (1, 0, 0, 1), (0.5, 0.5))
    for i in range(segments):
        angle = math.tau * i / segments
        c, s = math.cos(angle), math.sin(angle)
        vertex(mesh, (c, -1, s), (0, -1, 0), (1, 0, 0, 1),
               ((c + 1) / 2, (s + 1) / 2))
    for i in range(segments):
        mesh["indices"].extend((center, center + 1 + i, center + 1 + (i + 1) % segments))
    return mesh


def torus(material, segments=48, tube_segments=12):
    """Y-up torus: major radius 1, tube radius 0.25, matching the STL."""
    mesh = primitive(material, [-1.25, -0.25, -1.25, 1.25, 0.25, 1.25])
    for i in range(segments + 1):
        phi = math.tau * (i % segments) / segments
        cp, sp = math.cos(phi), math.sin(phi)
        for j in range(tube_segments + 1):
            theta = math.tau * (j % tube_segments) / tube_segments
            ct, st = math.cos(theta), math.sin(theta)
            radius = 1 + 0.25 * ct
            vertex(mesh, (radius * cp, 0.25 * st, radius * sp),
                   (ct * cp, st, ct * sp), (-sp, 0, cp, -1),
                   (i / segments, j / tube_segments))
    width = tube_segments + 1
    for i in range(segments):
        for j in range(tube_segments):
            a = i * width + j
            b = a + width
            mesh["indices"].extend((a, a + 1, b, a + 1, b + 1, b))
    return mesh


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path,
                        default=Path(__file__).resolve().parents[1] / "assets/meshes")
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    default_material = "/assets/materials/default.plmaterial"
    meshes = {
        "plane": grid(1, default_material),
        "sphere": sphere(1.0, 16, default_material),
        "probe": sphere(0.25, 32, "/assets/materials/probe.plmaterial"),
        "cube": cube(default_material),
        "cylinder": cylinder(default_material),
        "cone": cone(default_material),
        "torus": torus(default_material),
        # Unit grid scaled to 40960 units by the terrain scene: 160-unit cells.
        "water_grid": grid(256, "/assets/materials/water.plmaterial"),
    }
    for name, mesh in meshes.items():
        write_mesh(args.output_dir / f"{name}.plmesh", mesh)


if __name__ == "__main__":
    main()
