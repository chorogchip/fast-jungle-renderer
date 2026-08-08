# FastJungle LOD Audit

- Baseline commit: `0e9b2b5`
- Audit ratios: `100 / 50 / 25 / 12 / 6 / 3 / 1%`
- Cook mode: sequential simplification from the preceding LOD, no minimum
  triangle count, no minimum reduction gate, maximum relative error budget 1.0
- Captures: 640×640, full materials/textures, three views per mesh and one
  diagnostic view per submesh

Open `report/index.html` to review the results. Each mesh page contains a
three-view contact sheet, an A/B LOD viewer with a display-size slider, geometry
metrics, and links to its submesh contact sheets.

`selection.csv` contains a conservative automatic draft and empty
`user_selected_lod` / `user_notes` columns for the final review. The automatic
draft is only a starting point: it selects the coarsest LOD whose three
screen-filling views all have normalized MAE <= 0.04 and silhouette IoU >= 0.97.

## Result inventory

- 137 meshes
- 204 base submeshes
- 4,305 source captures
- 137 mesh contact sheets
- 204 submesh contact sheets
- 138 HTML pages
- 959 mesh/LOD geometry records
- 1,428 submesh/LOD geometry records

Total scene triangle counts by LOD:

| Retention target | Triangles |
|---:|---:|
| 100% | 15,555,338 |
| 50% | 7,753,542 |
| 25% | 3,851,365 |
| 12% | 1,842,267 |
| 6% | 918,935 |
| 3% | 460,138 |
| 1% | 151,700 |

## Important files

- `report/index.html`: review UI
- `selection.csv`: final user-selection worksheet
- `mesh_lod_metrics.csv`: mesh-level geometry/error/distance data
- `submesh_lod_metrics.csv`: submesh-level cook method and error data
- `image_metrics.csv`: LOD0-relative image and silhouette measurements
- `contact_sheets/`: compact visual comparisons
- `charts/`: global distributions and cost/error plots
- `cook_summary.json` and `report_summary.json`: machine-readable summaries

The audit scene is
`assets/cooked/lod-audit/JungleRuins-lod-audit.fjscene`. Its companion `.fjtex`
is a hard link to the original texture file, so it does not consume a second
2.59 GB of disk space. The baseline scene remains untouched.

Temporary audit code has intentionally not been rolled back yet. Keep it until
the LOD choices are finalized; restore tracked files to commit `0e9b2b5` only
after preserving this directory and `selection.csv`.
