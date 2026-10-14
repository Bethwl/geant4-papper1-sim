#include "MyRunAction.hh"
#include "MyDetectorConstruction.hh"   // NEW: to read current sample material name
#include "G4Run.hh"
#include "G4SystemOfUnits.hh"
#include "G4AccumulableManager.hh"
#include "G4AnalysisManager.hh"
#include "G4RunManager.hh"
#include "G4ios.hh"
#include <cmath>
#include <ctime>
#include <fstream>
#include <atomic>
#include <mutex>
#include <cstdio>

namespace {
    std::atomic<G4double> g_primaryEnergy{ 0.0 };

    // NEW: material name is set once per run by the master thread (BeginOfRunAction)
    // and only ever read afterwards, so a simple mutex-protected string is enough
    // (G4String has no lock-free atomic equivalent).
    std::mutex g_materialMutex;
    G4String g_sampleMaterial = "S1";

    void SetGlobalSampleMaterial(const G4String& name) {
        std::lock_guard<std::mutex> lock(g_materialMutex);
        g_sampleMaterial = name;
    }
    G4String GetGlobalSampleMaterial() {
        std::lock_guard<std::mutex> lock(g_materialMutex);
        return g_sampleMaterial;
    }

    // NEW: same idea as material, but for sample thickness, so that
    // running the same material at several thicknesses in one batch
    // does not overwrite/mix ROOT files or CSV rows.
    std::atomic<G4double> g_sampleThickness{ 0.0 };
    void SetGlobalSampleThickness(G4double t) {
        g_sampleThickness.store(t, std::memory_order_relaxed);
    }
    G4double GetGlobalSampleThickness() {
        return g_sampleThickness.load(std::memory_order_relaxed);
    }
}

MyRunAction::MyRunAction()
    : G4UserRunAction(),
    fEdepNaIDet(0.0), fPulseNaIDet(0.0),
    fNCompton(0.0), fNPhotoelectric(0.0), fNPairProduction(0.0),
    fNRayleigh(0.0), fNOther(0.0), fNTotal(0.0),
    // NEW: expected-value (track-length style) counters -- xem AddExpectedCount()
    fNComptonExpected(0.0), fNPhotoelectricExpected(0.0), fNPairProductionExpected(0.0),
    fNRayleighExpected(0.0), fNTotalExpected(0.0)
{
    auto* accMan = G4AccumulableManager::Instance();
    accMan->RegisterAccumulable(fEdepNaIDet);
    accMan->RegisterAccumulable(fPulseNaIDet);
    accMan->RegisterAccumulable(fNCompton);
    accMan->RegisterAccumulable(fNPhotoelectric);
    accMan->RegisterAccumulable(fNPairProduction);
    accMan->RegisterAccumulable(fNRayleigh);
    accMan->RegisterAccumulable(fNOther);
    accMan->RegisterAccumulable(fNTotal);
    // NEW: dang ky 5 accumulable expected-value song song voi ban analog cu
    accMan->RegisterAccumulable(fNComptonExpected);
    accMan->RegisterAccumulable(fNPhotoelectricExpected);
    accMan->RegisterAccumulable(fNPairProductionExpected);
    accMan->RegisterAccumulable(fNRayleighExpected);
    accMan->RegisterAccumulable(fNTotalExpected);

    auto* analysisManager = G4AnalysisManager::Instance();
    analysisManager->SetVerboseLevel(1);
    analysisManager->SetNtupleMerging(true);
    analysisManager->SetDefaultFileType("root");

    analysisManager->CreateH1("E_dep", "Deposited Energy Spectrum in NaI(Tl)", 1000, 0.0, 3000.0, "keV");
    analysisManager->CreateH1("Pulse_Counts", "Counts per Event in NaI(Tl) Detector", 10, 0.0, 10.0, "none");

    analysisManager->CreateNtuple("Hits", "Event Data");
    analysisManager->CreateNtupleDColumn("Edep_keV");
    analysisManager->FinishNtuple();
}

MyRunAction::~MyRunAction() = default;

void MyRunAction::BeginOfRunAction(const G4Run*) {
    G4AccumulableManager::Instance()->Reset();
    auto* analysisManager = G4AnalysisManager::Instance();
    analysisManager->Reset();

    // NEW: read the sample material for THIS run from DetectorConstruction
    // (shared instance across master/worker threads) and store it so every
    // thread/EndOfRunAction call knows which material this run belongs to.
    auto detector = static_cast<const MyDetectorConstruction*>(
        G4RunManager::GetRunManager()->GetUserDetectorConstruction());
    G4String matName = detector ? detector->GetSampleMaterialName() : "Unknown";
    SetGlobalSampleMaterial(matName);

    // NEW: also capture the sample thickness for this run (mm), so that
    // running the same material at different thicknesses in one batch
    // (e.g. S1 at 3mm, 5mm, 8mm) produces distinct output files instead
    // of the later run silently overwriting the earlier one.
    G4double thicknessMM = detector ? detector->GetSampleThickness() / mm : 0.0;
    SetGlobalSampleThickness(thicknessMM);

    auto pga = static_cast<const MyPrimaryGeneratorAction*>(
        G4RunManager::GetRunManager()->GetUserPrimaryGeneratorAction());
    if (pga) {
        g_primaryEnergy.store(pga->GetParticleEnergy(), std::memory_order_relaxed);
    }
    G4double energyKeV = g_primaryEnergy.load(std::memory_order_relaxed) / keV;

    // NEW: your run_*.mac generator already writes a unique name per combo
    // via "/analysis/setFileName ..." (executed before /run/beamOn, i.e.
    // before this BeginOfRunAction runs). Prefer that name as-is; only build
    // a fallback name from material+thickness+energy if the macro didn't
    // set one (e.g. running interactively without /analysis/setFileName).
    G4String presetName = analysisManager->GetFileName();
    if (!presetName.empty()) {
        analysisManager->OpenFile(); // uses the name set via the macro
    }
    else {
        char tag[128];
        std::snprintf(tag, sizeof(tag), "_t%.2fmm_E%.1fkeV_output", thicknessMM, energyKeV);
        analysisManager->OpenFile(matName + tag);
    }

    if (IsMaster()) {
        fTimer.Start();
        std::time_t t = std::time(nullptr);
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
        G4cout << "\n========== Run started: " << buf << " ==========" << G4endl;
    }
}

void MyRunAction::EndOfRunAction(const G4Run* run) {
    G4AccumulableManager::Instance()->Merge();
    auto* analysisManager = G4AnalysisManager::Instance();

    if (run->GetNumberOfEvent() == 0) return;

    G4int    nEvents = run->GetNumberOfEvent();
    G4double edep = fEdepNaIDet.GetValue();
    G4double pulse = fPulseNaIDet.GetValue();
    G4double nCompt = fNCompton.GetValue();
    G4double nPhot = fNPhotoelectric.GetValue();
    G4double nPair = fNPairProduction.GetValue();
    G4double nRayl = fNRayleigh.GetValue();
    G4double nOther = fNOther.GetValue();
    G4double nTot = fNTotal.GetValue();

    // NEW: gia tri expected-value (track-length style), zero-variance hon ban analog
    G4double nComptE = fNComptonExpected.GetValue();
    G4double nPhotE = fNPhotoelectricExpected.GetValue();
    G4double nPairE = fNPairProductionExpected.GetValue();
    G4double nRaylE = fNRayleighExpected.GetValue();
    G4double nTotE = fNTotalExpected.GetValue();

    if (IsMaster()) {
        // --- Human-readable output ---
        G4cout << "\n================ ANALYSIS RESULTS ================" << G4endl;
        G4cout << "Sample material  : " << GetGlobalSampleMaterial() << G4endl;
        G4cout << "Number of events: " << nEvents << G4endl;
        G4cout << "[Edep] NaI Detector = " << edep / MeV << " MeV" << G4endl;
        G4cout << "[Pulse] NaI Detector = " << pulse << G4endl;

        G4cout << "\n--- First Interaction in Sample ---" << G4endl;
        G4cout << "  Compton        = " << nCompt << G4endl; // 100 keV to 10 MeV: Compton dominates at intermediate energies
        G4cout << "  Photoelectric  = " << nPhot << G4endl; // Below ~200 keV, photoelectric dominates; above ~1 MeV, it becomes negligible
        G4cout << "  Pair Production = " << nPair << G4endl; // Only above 1.022 MeV, becomes significant above ~5 MeV; negligible at low energies
        G4cout << "  Rayleigh       = " << nRayl << G4endl; // Elastic scattering, significant at low energies and for high-Z materials; generally less than Compton and Photoelectric
        G4cout << "  Other          = " << nOther << G4endl; // Any other processes (e.g., ionization, bremsstrahlung) that might occur but are not categorized above
        G4cout << "  Total          = " << nTot << G4endl; // Should equal the sum of the above processes

        if (nTot > 0) {
            G4cout << "\n--- R = N_process / (N_total - N_Rayleigh) ---" << G4endl;
            G4cout << "  R_Compton        = " << nCompt / (nTot - nRayl) << G4endl; // At intermediate energies (e.g., 200 keV to 5 MeV), this ratio should be the highest, often around 0.5 to 0.8 depending on the energy and material
            G4cout << "  R_Photoelectric  = " << nPhot / (nTot - nRayl) << G4endl; // At low energies (e.g., below 200 keV), this ratio should be close to 1; at high energies (e.g., above 1 MeV), it should approach 0
            G4cout << "  R_PairProduction = " << nPair / (nTot - nRayl) << G4endl; // Below 1.022 MeV, this ratio should be 0; above 5 MeV, it can become significant (e.g., 0.1 to 0.3); at intermediate energies, it should be negligible
            G4cout << "  R_Rayleigh       = " << nRayl / (nTot - nRayl) << G4endl; // This ratio can be significant at low energies, especially for high-Z materials, but is generally less than the Compton and Photoelectric ratios; it may range from 0.01 to 0.1 depending on the energy and material
            G4cout << "  Ratio Checking   = " << (nCompt + nPhot + nPair + nRayl + nOther) / nTot << G4endl; // This should be 1 (or very close to it) if all processes are correctly categorized and counted
        }

        // NEW: ket qua expected-value (track-length style) -- xem AddExpectedCount()
        // Day la ban "muot", zero-variance hon, dung de doi chieu voi OpenMC / XCOM,
        // dac biet o vung xac suat nho (analog o tren de bi nhay/bang 0 o vung nay).
        G4cout << "\n--- Expected-Value Estimator (track-length style, xem G4EmCalculator) ---" << G4endl;
        G4cout << "  Compton         = " << nComptE << G4endl;
        G4cout << "  Photoelectric   = " << nPhotE << G4endl;
        G4cout << "  Pair Production = " << nPairE << G4endl;
        G4cout << "  Rayleigh        = " << nRaylE << G4endl;
        G4cout << "  Total           = " << nTotE << G4endl;
        if (nTotE > 0) {
            G4cout << "  R_Compton_exp        = " << nComptE / nTotE << G4endl;
            G4cout << "  R_Photoelectric_exp  = " << nPhotE / nTotE << G4endl;
            G4cout << "  R_PairProduction_exp = " << nPairE / nTotE << G4endl;
            G4cout << "  R_Rayleigh_exp       = " << nRaylE / nTotE << G4endl;
        }
        G4cout << "==================================================\n" << G4endl;

        fTimer.Stop();
        std::time_t t = std::time(nullptr);
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
        G4double real = fTimer.GetRealElapsed();
        G4int h = (G4int)(real / 3600);
        G4int m = (G4int)((real - h * 3600) / 60);
        G4int s = (G4int)(real - h * 3600 - m * 60);
        G4cout << "========== Run ended  : " << buf << " ==========" << G4endl;
        G4cout << "========== Elapsed    : "
            << h << "h " << m << "m " << s << "s"
            << "  (real: " << std::fixed << std::setprecision(1) << real << " s)"
            << " ==========" << G4endl;

        // --- Get energy from PrimaryGenerator (Worker threads capture it) ---
        G4double energy_keV = g_primaryEnergy.load(std::memory_order_relaxed) / keV;

        // NEW: which material this run used (S1, S2, S3, ...)
        G4String matName = GetGlobalSampleMaterial();

        // --- CSV output: append to file for easy post-processing ---
        // File: process_results.csv (SHARED by all materials, one row per run)
        // One line per run: Material, Energy_keV, N_events, N_compton, N_photo, N_pair,
        //                   N_rayleigh, N_other, N_total, R_compton, R_photo, R_pair,
        //                   R_rayleigh, Pulse_NaI, Edep_MeV
        // NEW: running several materials in a row (e.g. via /control/foreach) now just
        // appends more rows to the SAME csv, tagged by the Material column, instead of
        // each material overwriting/mixing with the others.
        std::string csvFile = "process_results.csv";

        // Write header if file doesn't exist
        std::ifstream checkFile(csvFile);
        bool writeHeader = !checkFile.good();
        checkFile.close();

        std::ofstream csv(csvFile, std::ios::app);
        if (writeHeader) {
            csv << "Material,Thickness_mm,Energy_keV,N_events,N_compton,N_photo,N_pair,N_rayleigh,N_other,N_total,"
                << "R_compton,R_photo,R_pair,R_rayleigh,Pulse_NaI,Edep_MeV,"
                // NEW: cot expected-value (track-length style), doi chieu voi ban analog o tren
                << "N_compton_exp,N_photo_exp,N_pair_exp,N_rayleigh_exp,N_total_exp,"
                << "R_compton_exp,R_photo_exp,R_pair_exp,R_rayleigh_exp" << std::endl; // Header for CSV columns
        }

        G4double rC = (nTot > 0) ? nCompt / (nTot - nRayl) : 0; // If nTot is zero, set ratio to 0 to avoid division by zero. Else, calculate the ratio as usual.
        G4double rP = (nTot > 0) ? nPhot / (nTot - nRayl) : 0;
        G4double rPP = (nTot > 0) ? nPair / (nTot - nRayl) : 0;
        G4double rR = (nTot > 0) ? nRayl / (nTot - nRayl) : 0;

        G4double thicknessMM = GetGlobalSampleThickness();

        // NEW: ratio expected-value, phai check nTotE > 0 truoc khi chia (giong rC/rP/rPP/rR o tren)
        G4double rCe = (nTotE > 0) ? nComptE / nTotE : 0;
        G4double rPe = (nTotE > 0) ? nPhotE / nTotE : 0;
        G4double rPPe = (nTotE > 0) ? nPairE / nTotE : 0;
        G4double rRe = (nTotE > 0) ? nRaylE / nTotE : 0;

        csv << matName << ","
            << std::fixed
            << std::setprecision(3) << thicknessMM << ","
            << std::setprecision(2)
            << energy_keV << ","
            << nEvents << ","
            << std::setprecision(0)
            << nCompt << "," << nPhot << "," << nPair << "," << nRayl << "," << nOther << "," << nTot << ","
            << std::setprecision(8)
            << rC << "," << rP << "," << rPP << "," << rR << ","
            << std::setprecision(0) << pulse << ","
            << std::setprecision(6) << edep / MeV << ","
            // NEW: expected-value counts + ratios
            << std::setprecision(6)
            << nComptE << "," << nPhotE << "," << nPairE << "," << nRaylE << "," << nTotE << ","
            << std::setprecision(8)
            << rCe << "," << rPe << "," << rPPe << "," << rRe
            << std::endl;
        csv.close();
    }

    analysisManager->Write();
    analysisManager->CloseFile();
}

void MyRunAction::AddEdepNaIDet(G4double edep) {
    fEdepNaIDet += edep;
    auto* analysisManager = G4AnalysisManager::Instance();
    analysisManager->FillH1(0, edep);
    analysisManager->FillNtupleDColumn(0, edep / keV);
    analysisManager->AddNtupleRow();
}

void MyRunAction::AddCountedNaIDet() {
    fPulseNaIDet += 1.0;
    auto* analysisManager = G4AnalysisManager::Instance();
    analysisManager->FillH1(1, 1.0);
}

void MyRunAction::AddProcessCount(const G4String& processName) {
    fNTotal += 1.0;
    if (processName == "compt") fNCompton += 1.0;
    else if (processName == "phot")  fNPhotoelectric += 1.0;
    else if (processName == "conv")  fNPairProduction += 1.0;
    else if (processName == "Rayl")  fNRayleigh += 1.0;
    else                             fNOther += 1.0;
}

// NEW: cong don XAC SUAT KY VONG (weight) thay vi dem su kien that (analog).
// weight = P_interact_trong_step_nay * (sigma_i/sigma_total), tinh boi
// G4EmCalculator trong MySteppingAction -- xem giai thich "expected-value /
// track-length estimator", tuong duong ban chat voi tally track-length cua OpenMC.
// KHONG can dieu kien loai tru "Transportation" o day vi ham nay chi duoc goi
// khi co sigma_total > 0, tu MySteppingAction.
void MyRunAction::AddExpectedCount(const G4String& processName, G4double weight) {
    fNTotalExpected += weight;
    if (processName == "compt") fNComptonExpected += weight;
    else if (processName == "phot")  fNPhotoelectricExpected += weight;
    else if (processName == "conv")  fNPairProductionExpected += weight;
    else if (processName == "Rayl")  fNRayleighExpected += weight;
}