"""
run_geant4_papper1.py - Parallel worker for Geant4 Simulation (2026_10_7_G4_papper1)
Supports 4 sub-projects:
1. mac_be_day_co_dinh
2. mac_be_day_toi_uu
3. msp_range
4. R
Runs on GitHub Actions Swarm (13 active accounts x 10 runners).
Uploads raw 1-to-1 CSV results to Flat Common Pool (02_Flux_Data) and corresponding subfolders.
Uploads child outputs (.root files) to 03_Simulation_Outputs.
"""

import argparse
import os
import sys
import time
import json
import zipfile
import subprocess
from pathlib import Path
import pandas as pd

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")

import gdrive_helper
import job_naming

def parse_args():
    parser = argparse.ArgumentParser(description="Geant4 Parallel Worker for 2026_10_7_G4_papper1")
    parser.add_argument("--runner-id", type=int, required=True, help="ID runner (1..10)")
    parser.add_argument("--num-runners", type=int, default=10, help="Tong so runner tren account nay")
    parser.add_argument("--primaries", type=int, default=1000000, help="So hat moi job")
    parser.add_argument("--gdrive-folder", type=str, default="1w5EhFwwj6uzjT_tWJvNRXKPCbZm_PfA6")
    parser.add_argument("--plan-file", type=str, default="papper1_plan_14acc.json")
    parser.add_argument("--github-user", type=str, default=None)
    parser.add_argument("--timeout-hours", type=float, default=2.0)
    return parser.parse_args()

def ensure_compiled(sub_proj: str):
    """Ensure binary for sub_proj is compiled."""
    base = Path(sub_proj)
    build_dir = base / "build"
    
    if sub_proj == "msp_range":
        target_bin = build_dir / "sim_msp_range"
        if target_bin.exists() and os.access(str(target_bin), os.X_OK):
            return str(target_bin)
        print(f">>> [BUILD] Compiling msp_range...")
        build_dir.mkdir(parents=True, exist_ok=True)
        subprocess.run(["cmake", "..", "-DCMAKE_BUILD_TYPE=Release"], cwd=str(build_dir), check=True)
        subprocess.run(["make", f"-j{os.cpu_count() or 2}", "sim_msp_range", "range_calc", "msp_calc"], cwd=str(build_dir), check=True)
        return str(target_bin)
    else:
        target_bin = build_dir / "Sim"
        if target_bin.exists() and os.access(str(target_bin), os.X_OK):
            return str(target_bin)
        print(f">>> [BUILD] Compiling {sub_proj} Sim...")
        build_dir.mkdir(parents=True, exist_ok=True)
        subprocess.run(["cmake", "..", "-DCMAKE_BUILD_TYPE=Release"], cwd=str(build_dir), check=True)
        subprocess.run(["make", f"-j{os.cpu_count() or 2}", "Sim"], cwd=str(build_dir), check=True)
        return str(target_bin)

def main():
    args = parse_args()
    acc_name = args.github_user or os.environ.get("GITHUB_REPOSITORY_OWNER") or "runner"

    print("=" * 80)
    print(f"🚀 STARTING GEANT4 RUNNER #{args.runner_id} (Account: {acc_name})")
    print(f"Project: 2026_10_7_G4_papper1 | Plan file: {args.plan_file}")
    print("=" * 80)

    # 1. Connect Google Drive and locate folders
    drive_service = None
    flux_folder_id = None
    outputs_folder_id = None
    sub_flux_ids = {}
    existing_files = set()

    try:
        drive_service = gdrive_helper.get_drive_service()
        subfolders = gdrive_helper.get_or_create_subfolders(drive_service, parent_id=args.gdrive_folder)
        flux_folder_id = subfolders.get("02_Flux_Data")
        outputs_folder_id = subfolders.get("03_Simulation_Outputs")

        # Get or locate 4 subfolders in 02_Flux_Data
        for sp in ["mac_be_day_co_dinh", "mac_be_day_toi_uu", "msp_range", "R"]:
            q_sp = f"'{flux_folder_id}' in parents and name = '{sp}' and mimeType = 'application/vnd.google-apps.folder' and trashed = false"
            res_sp = drive_service.files().list(q=q_sp, supportsAllDrives=True, includeItemsFromAllDrives=True, fields="files(id, name)").execute()
            files_sp = res_sp.get("files", [])
            if files_sp:
                sub_flux_ids[sp] = files_sp[0]["id"]
            else:
                body_sp = {"name": sp, "mimeType": "application/vnd.google-apps.folder", "parents": [flux_folder_id]}
                f_sp = drive_service.files().create(body=body_sp, supportsAllDrives=True, fields="id, name").execute()
                sub_flux_ids[sp] = f_sp["id"]

        print(f">>> [DRIVE] Ket noi Google Drive thanh cong!")
        print(f"  • Flat Common Pool (02_Flux_Data):          {flux_folder_id}")
        print(f"  • Raw Archive      (03_Simulation_Outputs): {outputs_folder_id}")

        # Scan existing files in 02_Flux_Data and subfolders
        page_token = None
        while True:
            q = f"'{flux_folder_id}' in parents and trashed = false"
            res_files = drive_service.files().list(
                q=q, supportsAllDrives=True, includeItemsFromAllDrives=True, fields="nextPageToken, files(id, name, mimeType)",
                pageSize=1000, pageToken=page_token
            ).execute()
            for f in res_files.get("files", []):
                existing_files.add(f["name"].lower().strip())
                if f.get("mimeType") == "application/vnd.google-apps.folder":
                    res_sub = drive_service.files().list(
                        q=f"'{f['id']}' in parents and trashed = false",
                        supportsAllDrives=True, includeItemsFromAllDrives=True, fields="files(name)", pageSize=1000
                    ).execute()
                    for sf in res_sub.get("files", []):
                        existing_files.add(sf["name"].lower().strip())
            page_token = res_files.get("nextPageToken")
            if not page_token:
                break
        print(f">>> [DRIVE] Tim thay {len(existing_files)} files da ton tai tren Drive.")
    except Exception as e:
        print(f">>> [WARNING] Ket noi Drive that bai: {e}. Du lieu se duoc luu local.")

    # 2. Load execution plan
    with open(args.plan_file, "r", encoding="utf-8") as pf:
        full_plan = json.load(pf)

    matched_acc = None
    for acc in full_plan:
        if acc.lower() == acc_name.lower():
            matched_acc = acc
            break
    if not matched_acc:
        matched_acc = list(full_plan.keys())[0]

    runner_jobs = full_plan[matched_acc].get(str(args.runner_id), [])
    total_jobs = len(runner_jobs)
    print(f"\nRunner #{args.runner_id} duoc phan cong {total_jobs} jobs tu account '{matched_acc}'.")

    # 3. Pre-compile required binaries
    sub_projects_needed = sorted(list({j["sub_project"] for j in runner_jobs}))
    bin_map = {}
    for sp in sub_projects_needed:
        try:
            bin_map[sp] = ensure_compiled(sp)
            print(f">>> Binary san sang cho {sp}: {bin_map[sp]}")
        except Exception as e:
            print(f"⚠️ Loi bien dich {sp}: {e}")

    # 4. Execute Jobs
    generated_roots = []

    for idx, job in enumerate(runner_jobs, 1):
        jname = job["job_name"]
        sub_proj = job["sub_project"]
        macro_file = job["macro_file"]

        # Check candidate filenames
        candidates = [
            f"{jname}.csv",
            f"hist_{jname}.csv",
            f"{macro_file.replace('.mac', '')}.csv"
        ]

        if any(c.lower().strip() in existing_files for c in candidates):
            print(f"[{idx}/{total_jobs}] SKIP: [{jname}] da co tren Drive.")
            continue

        print(f"\n--- [{idx}/{total_jobs}] RUNNING: {jname} ({sub_proj}) ---")

        if sub_proj == "msp_range":
            sim_bin = bin_map.get("msp_range")
            if not sim_bin or not os.path.exists(sim_bin):
                print(f"    [ERR] Binary sim_msp_range khong ton tai!")
                continue

            # Parse particle and material from macro_file (e.g. 'proton_s1')
            parts = macro_file.split("_")
            particle = parts[0]
            mat = parts[1]

            msp_dir = Path("msp_range")
            energies_file = msp_dir / "energies_kev.txt"
            out_prefix = f"sim_{particle}_{mat}"

            t0 = time.time()
            cmd = [
                str(Path(sim_bin).resolve()),
                particle, mat,
                str(energies_file.resolve()),
                out_prefix,
                str(job.get("primaries", 3000)),
                "200"
            ]
            res = subprocess.run(cmd, cwd=str(msp_dir.resolve()), capture_output=True, text=True)
            elapsed = time.time() - t0

            if res.returncode != 0:
                print(f"    [SIM ERROR] code {res.returncode}: {(res.stderr or res.stdout)[-300:]}")
            else:
                print(f"    [SIM OK] Thoi gian chay: {elapsed:.2f}s")

            # Check generated CSVs
            msp_csv = msp_dir / f"{out_prefix}_msp.csv"
            range_csv = msp_dir / f"{out_prefix}_range.csv"
            target_csv = f"{jname}.csv"

            # Create standard checklist file {jname}.csv
            if msp_csv.exists() and range_csv.exists():
                df_msp = pd.read_csv(msp_csv)
                df_rng = pd.read_csv(range_csv)
                df_merged = pd.merge(df_msp, df_rng, on="Energy_keV", how="outer")
                df_merged.to_csv(target_csv, index=False)
            elif msp_csv.exists():
                pd.read_csv(msp_csv).to_csv(target_csv, index=False)
            else:
                pd.DataFrame([{"job_name": jname, "sim_sec": round(elapsed, 2)}]).to_csv(target_csv, index=False)

            # Upload to 02_Flux_Data and subfolder
            if drive_service and flux_folder_id:
                try:
                    gdrive_helper.upload_file_to_folder(drive_service, target_csv, flux_folder_id)
                    sp_id = sub_flux_ids.get("msp_range", flux_folder_id)
                    gdrive_helper.upload_file_to_folder(drive_service, target_csv, sp_id)
                    if msp_csv.exists():
                        gdrive_helper.upload_file_to_folder(drive_service, str(msp_csv), sp_id)
                    if range_csv.exists():
                        gdrive_helper.upload_file_to_folder(drive_service, str(range_csv), sp_id)
                    existing_files.add(target_csv.lower())
                    print(f"    ✅ Uploaded {target_csv} -> 02_Flux_Data")
                except Exception as e:
                    print(f"    ⚠️ Loi upload {target_csv}: {e}")

        else:
            # mac_be_day_co_dinh, mac_be_day_toi_uu, R
            sim_bin = bin_map.get(sub_proj)
            if not sim_bin or not os.path.exists(sim_bin):
                print(f"    [ERR] Binary {sim_bin} khong ton tai!")
                continue

            sub_dir = Path(sub_proj)
            macro_candidates = [
                sub_dir / "macros" / macro_file,
                sub_dir / "macros" / "macro" / macro_file,
                sub_dir / "build" / "macros" / macro_file,
                sub_dir / macro_file
            ]
            macro_path = None
            for mc in macro_candidates:
                if mc.exists():
                    macro_path = mc
                    break

            if not macro_path:
                print(f"    [ERR] Khong tim thay macro file {macro_file} trong {sub_proj}!")
                continue

            proc_csv = sub_dir / "process_results.csv"
            if proc_csv.exists():
                try:
                    proc_csv.unlink()
                except Exception:
                    pass

            t0 = time.time()
            cmd = [str(Path(sim_bin).resolve()), str(macro_path.resolve())]
            res = subprocess.run(cmd, cwd=str(sub_dir.resolve()), capture_output=True, text=True)
            elapsed = time.time() - t0

            if res.returncode != 0:
                print(f"    [SIM ERROR] code {res.returncode}: {(res.stderr or res.stdout)[-300:]}")
            else:
                print(f"    [SIM OK] Thoi gian chay: {elapsed:.2f}s")

            out_csv_name = f"{jname}.csv"
            if proc_csv.exists():
                try:
                    df_res = pd.read_csv(proc_csv)
                    if len(df_res) > 0:
                        df_res.tail(1).to_csv(out_csv_name, index=False)
                    else:
                        df_res.to_csv(out_csv_name, index=False)
                except Exception as e:
                    print(f"    ⚠️ Loi doc {proc_csv}: {e}")
            else:
                rec = {"job_name": jname, "sub_project": sub_proj, "sim_sec": round(elapsed, 2)}
                pd.DataFrame([rec]).to_csv(out_csv_name, index=False)

            # Upload to 02_Flux_Data and subfolder
            if drive_service and flux_folder_id and os.path.exists(out_csv_name):
                try:
                    gdrive_helper.upload_file_to_folder(drive_service, out_csv_name, flux_folder_id)
                    sp_id = sub_flux_ids.get(sub_proj, flux_folder_id)
                    gdrive_helper.upload_file_to_folder(drive_service, out_csv_name, sp_id)
                    existing_files.add(out_csv_name.lower())
                    print(f"    ✅ Uploaded {out_csv_name} -> 02_Flux_Data/{sub_proj}")
                except Exception as e:
                    print(f"    ⚠️ Loi upload {out_csv_name}: {e}")

            # Collect root files
            for rf in list(sub_dir.glob("*.root")) + list((sub_dir / "build").glob("*.root")):
                if rf not in generated_roots:
                    generated_roots.append(rf)

    # 5. Pack and upload child outputs to 03_Simulation_Outputs
    if generated_roots and drive_service and outputs_folder_id:
        zip_name = f"outputs_{acc_name}_runner_{args.runner_id:02d}.zip"
        print(f"\n>>> [ZIP] Dong goi {len(generated_roots)} root files vao {zip_name}...")
        with zipfile.ZipFile(zip_name, "w", zipfile.ZIP_DEFLATED) as z:
            for rf in generated_roots:
                z.write(rf, arcname=rf.name)
        gdrive_helper.upload_file_to_folder(drive_service, zip_name, outputs_folder_id)
        print(f"    ✅ Uploaded {zip_name} -> 03_Simulation_Outputs")

    print("\n" + "=" * 80)
    print(f"🎉 RUNNER #{args.runner_id} HOAN TAT MO PHONG PAPPER1!")
    print("=" * 80)

if __name__ == "__main__":
    main()
