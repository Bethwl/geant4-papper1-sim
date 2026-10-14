// =========================================================================
// Header Definition
// =========================================================================

// 1. Include Local headers
#include "MyDetectorConstruction.hh" // Geometry definition class
#include "MySensitiveDetector.hh"    // Custom Sensitive Detector

// 2. Include Geant4 headers
#include "G4NistManager.hh"          // Material database
#include "G4Box.hh"                  // Box geometry
#include "G4Tubs.hh"                 // Cylinder geometry
#include "G4Sphere.hh"
#include "G4LogicalVolume.hh"        // Logical volume
#include "G4VPhysicalVolume.hh"
#include "G4PVPlacement.hh"          // Physical volume placement
#include "G4SubtractionSolid.hh"     // Boolean subtraction solid
#include "G4VisAttributes.hh"        // Visualization attributes
#include "G4Colour.hh"               // Colors
#include "G4Region.hh"               // Regions
#include "G4SDManager.hh"            // Sensitive detector manager
#include "G4SystemOfUnits.hh"        // Units
#include "G4RegionStore.hh"
#include "G4LogicalVolumeStore.hh"
#include "G4IStore.hh"
#include "G4RunManager.hh"           // Run manager
#include "G4OpticalSurface.hh"
#include "G4MaterialPropertiesTable.hh"
#include "G4LogicalSkinSurface.hh"
#include "G4ProductionCuts.hh"
#include <map>

// =========================================================================
// Geometry Dimensions
// =========================================================================
namespace GEO {
	// ===============================================
	// 1. World Volume
	// ===============================================
	const G4double WorldSize = 3 * m;

	// ===============================================
	// 2. Source
	// ===============================================
	const G4double Souce_Radius = 1 * cm;

	// ===============================================
	// 3. First Collimator
	// ===============================================
	const G4double Collimator1_InnerRadius = 5 * mm;
	const G4double Collimator1_Thickness = 30 * mm;
	const G4double Collimator1_OutterRadius = Collimator1_InnerRadius + Collimator1_Thickness;
	const G4double Collimator1_Height = 6 * cm;


	// ===============================================
	// 4. Sample
	// ===============================================
	const G4double Sample_Radius = 1 * cm;
	const G4double Sample_Thickness = 5 * mm;

	// ===============================================
	// 5. 2nd Collimator
	// ===============================================
	const G4double Collimator2_InnerRadius = 5 * mm;
	const G4double Collimator2_Thickness = 20 * mm;
	const G4double Collimator2_OutterRadius = Collimator2_InnerRadius + Collimator2_Thickness;
	// NOTE: Height is no longer fixed here — it's now a runtime-configurable
	// member variable (fCollimator2_Height), settable via
	// /DetectorConstruction/Collimator2_Height in .mac files. See constructor.

	// ===============================================
	// 5b. Gap between Collimator2 and NaI Detector
	// ===============================================
	const G4double Collimator2_To_NaI_Gap = 0.5 * cm;

	// ===============================================
	// 6. NaI Detector
	// ===============================================
	const G4double Solid_NaI_Radius = 2.5 * cm;
	const G4double Solid_NaI_Height = 10 * cm;

	// ===============================================

}

// =========================================================================
// Sample box dimensions per material (length x width x thickness).
// Length/width are the in-plane dimensions of the box; thickness is
// along the beam axis (Z), same role fSample_Thickness played before.
// S0 ("no sample") is not listed here: it falls back to fSample_Thickness
// (still settable via /DetectorConstruction/Sample_Thickness) for its
// dimensions, since it is just a vacuum placeholder.
// =========================================================================
namespace {
	struct SampleDims { G4double length, width, thickness; };
	const std::map<G4String, SampleDims> kSampleDims = {
		{"S1", {1.3 * cm, 0.2 * cm, 0.2 * cm}},
		{"S2", {1.3 * cm, 0.2 * cm, 0.2 * cm}},
		{"S3", {3.6 * cm, 0.4 * cm, 0.3 * cm}},
		{"S4", {3.6 * cm, 0.4 * cm, 0.3 * cm}},
		{"S5", {3.6 * cm, 0.4 * cm, 0.3 * cm}},
		{"S6", {3.6 * cm, 0.4 * cm, 0.3 * cm}},
		{"S7", {2.5 * cm, 0.2 * cm, 0.15 * cm}},
		{"S8", {2.5 * cm, 0.2 * cm, 0.15 * cm}},
	};
}

// =========================================================================
// Constructor / Destructor
// =========================================================================
MyDetectorConstruction::MyDetectorConstruction()
	: G4VUserDetectorConstruction(),
	fMessenger(nullptr),
	fSample_Thickness(5.0 * mm), // Default HDPE+hBN Layer Thickness
	fSampleMaterial("S1"),        // NEW: default sample material, can be changed via messenger
	// NEW: Collimator2 height, now configurable via macro (default matches old GEO constant)
	fCollimator2_Height(3.0 * cm)
{
	// Messenger for setting the volume fraction of hBN in the composite material
	fMessenger = new G4GenericMessenger(this, "/DetectorConstruction/", "Detector Construction Control");
	auto& fSample_Thicknesscmd = fMessenger->DeclareProperty("Sample_Thickness", fSample_Thickness, "Set Sample_Thickness");
	// NEW: allow choosing which sample material (S0, S1, S2, ..., S7, S8) to use for the run
	auto& fSampleMaterialCmd = fMessenger->DeclareProperty("Sample_Material", fSampleMaterial, "Set Sample material name (e.g. S1, S2, S3...)");
	// NEW: Collimator2 height, settable from .mac files, e.g.:
	//   /DetectorConstruction/Collimator2_Height 3 cm
	//   /DetectorConstruction/update
	auto& fCollimator2_HeightCmd = fMessenger->DeclareProperty("Collimator2_Height", fCollimator2_Height, "Set Collimator2 height");
	fMessenger->DeclareMethod("update", &MyDetectorConstruction::UpdateGeometry, "Update geometry after parameter changes");

}
MyDetectorConstruction::~MyDetectorConstruction() {}

// =========================================================================
// Define Materials
// =========================================================================
void MyDetectorConstruction::DefineMaterials() {
	G4NistManager* NIST = G4NistManager::Instance();
	// ===============================================
	// 1. NIST Materials
	// ===============================================
	NIST->FindOrBuildMaterial("G4_AIR");
	NIST->FindOrBuildMaterial("G4_Galactic");
	NIST->FindOrBuildMaterial("G4_SODIUM_IODIDE");
	NIST->FindOrBuildMaterial("G4_Pb");


	// ===============================================
	// 2. Sample Materials
	// ===============================================

	// [0]. S0: "No sample" placeholder - vacuum, so the Sample volume
	//      still exists geometrically (Collimator2 position unaffected)
	//      but is effectively transparent to radiation.
	//      Select via macro:
	//        /DetectorConstruction/Sample_Material S0
	//        /DetectorConstruction/update
	if (!G4Material::GetMaterial("S0", false)) {
		new G4Material("S0", 1, 1.008 * g / mole, 1e-25 * g / cm3,
			kStateGas, 2.73 * kelvin, 3e-18 * pascal);
	}

	// MAX-phase composite system, mass fractions taken from:
	//   COMPOSITION[Zxxxxx.00c] / DENSITY (g/cm3)
	// ZAID mapping used: 6000=C, 7000=N, 13000=Al, 22000=Ti, 23000=V,
	//                     24000=Cr, 41000=Nb, 73000=Ta
	// NOTE: S0 above is unchanged; all others (S1-S8) use the new composition.

// [1]. S1: (80-x) B2O3 -10 CaO-10SiO2 - x La2O3 with x = 10 mol%
if (!G4Material::GetMaterial("S1", false)) {
    G4Material* S1 = new G4Material("S1", 3.0099 * g / cm3, 4);
    S1->AddMaterial(B2O3, 0.5244);
    S1->AddMaterial(CaO, 0.0603);
    S1->AddMaterial(SiO2, 0.0647);
    S1->AddMaterial(La2O3, 0.3506);
}

// [2]. S2: (80-x) B2O3 -10 CaO-10SiO2 - x La2O3 with x = 15 mol%
if (!G4Material::GetMaterial("S2", false)) {
    G4Material* S2 = new G4Material("S2", 3.1999 * g / cm3, 4);
    S2->AddMaterial(B2O3, 0.4280);
    S2->AddMaterial(CaO, 0.0530);
    S2->AddMaterial(SiO2, 0.0568);
    S2->AddMaterial(La2O3, 0.4622);
}

// [3]. S3: (80-x) B2O3 -10 CaO-10SiO2 - x La2O3 with x = 20 mol%
if (!G4Material::GetMaterial("S3", false)) {
    G4Material* S3 = new G4Material("S3", 3.3899 * g / cm3, 4);
    S3->AddMaterial(B2O3, 0.3524);
    S3->AddMaterial(CaO, 0.0473);
    S3->AddMaterial(SiO2, 0.0507);
    S3->AddMaterial(La2O3, 0.5497);
}

// [4]. S4: (80-x) B2O3 -10 CaO-10SiO2 - x La2O3 with x = 25 mol%
if (!G4Material::GetMaterial("S4", false)) {
    G4Material* S4 = new G4Material("S4", 3.5799 * g / cm3, 4);
    S4->AddMaterial(B2O3, 0.2915);
    S4->AddMaterial(CaO, 0.0427);
    S4->AddMaterial(SiO2, 0.0457);
    S4->AddMaterial(La2O3, 0.6201);
}

// [5]. S5: (80-x) B2O3 -10 CaO-10SiO2 - x La2O3 with x = 30 mol%
if (!G4Material::GetMaterial("S5", false)) {
    G4Material* S5 = new G4Material("S5", 3.7699 * g / cm3, 4);
    S5->AddMaterial(B2O3, 0.2414);
    S5->AddMaterial(CaO, 0.0389);
    S5->AddMaterial(SiO2, 0.0417);
    S5->AddMaterial(La2O3, 0.6780);
}

// [6]. S6: (80-x) B2O3 -10 CaO-10SiO2 - x La2O3 with x = 35 mol%
if (!G4Material::GetMaterial("S6", false)) {
    G4Material* S6 = new G4Material("S6", 3.9599 * g / cm3, 4);
    S6->AddMaterial(B2O3, 0.1996);
    S6->AddMaterial(CaO, 0.0357);
    S6->AddMaterial(SiO2, 0.0383);
    S6->AddMaterial(La2O3, 0.7264);
}
		
}
// =========================================================================
// Construct Geometry
// =========================================================================
G4VPhysicalVolume* MyDetectorConstruction::Construct() {
	DefineMaterials();
	G4NistManager* NIST = G4NistManager::Instance();
	G4double FITNESS = 0.1 * mm; // For boolean solids, set to 1.0 for best performance

	// ===============================================
	// 1. World Volume
	// ===============================================
	G4Box* solidWorld = new G4Box("World", GEO::WorldSize / 2, GEO::WorldSize / 2, GEO::WorldSize / 2);
	G4LogicalVolume* World_LV = new G4LogicalVolume(solidWorld, NIST->FindOrBuildMaterial("G4_Galactic"), "WorldLV");
	World_Phys = new G4PVPlacement(nullptr, {}, World_LV, "World", nullptr, false, 0, true);

	// ===============================================
	// 2. Source
	// ===============================================
	G4Sphere* solidSource = new G4Sphere("Source", 0, GEO::Souce_Radius, 0, 360 * deg, 0, 180 * deg);
	G4LogicalVolume* Source_LV = new G4LogicalVolume(solidSource, NIST->FindOrBuildMaterial("G4_AIR"), "Source_LV");
	G4double Source_PosZ = 0;
	new G4PVPlacement(nullptr, G4ThreeVector(0, 0, Source_PosZ), Source_LV, "Source", World_LV, false, 0, true);

	// ===============================================
	// 3. Collimator 1
	// ===============================================
	G4Tubs* solidCollimator1 = new G4Tubs("Collimator1", GEO::Collimator1_InnerRadius, GEO::Collimator1_OutterRadius, GEO::Collimator1_Height / 2, 0, 360 * deg);
	G4LogicalVolume* Collimator1_LV = new G4LogicalVolume(solidCollimator1, NIST->FindOrBuildMaterial("G4_Pb"), "Collimator1_LV");
	G4double Collimator1_PosZ = Source_PosZ + GEO::Souce_Radius + GEO::Collimator1_Height / 2 + 1 * cm;
	new G4PVPlacement(nullptr, G4ThreeVector(0, 0, Collimator1_PosZ), Collimator1_LV, "Collimator1", World_LV, false, 0, true);

	// ===============================================
	// 3. Sample
	// ===============================================
	// NEW: Sample is now a rectangular box (length x width x thickness),
	// with dimensions looked up per sample material via kSampleDims.
	// S0 (no dedicated entry) keeps a default footprint and uses
	// fSample_Thickness for its thickness (still settable via macro).
	G4double sampleLength = 2 * GEO::Sample_Radius;
	G4double sampleWidth = 2 * GEO::Sample_Radius;
	G4double sampleThickness = fSample_Thickness;
	auto dimsIt = kSampleDims.find(fSampleMaterial);
	if (dimsIt != kSampleDims.end()) {
		sampleLength = dimsIt->second.length;
		sampleWidth = dimsIt->second.width;
		sampleThickness = dimsIt->second.thickness;
	}
	fSample_Thickness = sampleThickness; // keep getter/CSV output consistent with actual thickness used

	G4Box* solidSample = new G4Box("Sample", sampleLength / 2, sampleWidth / 2, sampleThickness / 2);
	// NEW: material is no longer hardcoded to "S1", it now comes from fSampleMaterial
	//      (settable at runtime via /DetectorConstruction/Sample_Material <name>)
	G4Material* sampleMat = G4Material::GetMaterial(fSampleMaterial, false);
	if (!sampleMat) {
		G4cerr << "\n*** ERROR: Sample material \"" << fSampleMaterial
			<< "\" not found (did you define it in DefineMaterials()?). "
			<< "Falling back to S1.\n" << G4endl;
		sampleMat = G4Material::GetMaterial("S1");
	}
	G4LogicalVolume* Sample_LV = new G4LogicalVolume(solidSample, sampleMat, "Sample_LV");
	G4double Sample_PosZ = Collimator1_PosZ + GEO::Collimator1_Height / 2 + sampleThickness / 2;
	new G4PVPlacement(nullptr, G4ThreeVector(0, 0, Sample_PosZ), Sample_LV, "Sample", World_LV, false, 0, true);

	// ===============================================
	// 5. Collimator 2
	// ===============================================
	// NEW: if fCollimator2_Height <= 0, skip creating/placing Collimator2
	// entirely instead of building a degenerate (zero-thickness) G4Tubs.
	// Downstream positions (NaI_PosZ) already reduce correctly to "no gap
	// from Collimator2" since fCollimator2_Height/2 becomes 0 in that case.
	G4LogicalVolume* Collimator2_LV = nullptr;
	G4double Collimator2_PosZ = Sample_PosZ + sampleThickness / 2 + fCollimator2_Height / 2;
	if (fCollimator2_Height > 0.) {
		G4Tubs* solidCollimator2 = new G4Tubs("Collimator2", GEO::Collimator2_InnerRadius, GEO::Collimator2_OutterRadius, fCollimator2_Height / 2, 0, 360 * deg);
		Collimator2_LV = new G4LogicalVolume(solidCollimator2, NIST->FindOrBuildMaterial("G4_Pb"), "Collimator2_LV");
		new G4PVPlacement(nullptr, G4ThreeVector(0, 0, Collimator2_PosZ), Collimator2_LV, "Collimator2", World_LV, false, 0, true);
	}


	// ===============================================
	// 6. NaI Crystal
	// ===============================================
	G4Tubs* solidNaI = new G4Tubs("NaI", 0, GEO::Solid_NaI_Radius, GEO::Solid_NaI_Height / 2, 0, 360 * deg);
	G4LogicalVolume* NaI_LV = new G4LogicalVolume(solidNaI, NIST->FindOrBuildMaterial("G4_SODIUM_IODIDE"), "NaI_LV");
	G4double NaI_PosZ = Collimator2_PosZ + fCollimator2_Height / 2 + GEO::Collimator2_To_NaI_Gap + GEO::Solid_NaI_Height / 2;
	new G4PVPlacement(nullptr, G4ThreeVector(0, 0, NaI_PosZ), NaI_LV, "NaI", World_LV, false, 0, true);


	// ===============================================
	// VISUALIZATION
	// ===============================================

	// 1. World
	G4VisAttributes* World_VisAtt = new G4VisAttributes(G4Colour(1, 1, 1, 0.0));
	World_VisAtt->SetVisibility(false);
	World_LV->SetVisAttributes(World_VisAtt);

	// 2. Source: Red
	G4VisAttributes* Source_VisAtt = new G4VisAttributes(G4Colour(1, 0, 0));
	Source_VisAtt->SetForceSolid(true);
	Source_LV->SetVisAttributes(Source_VisAtt);

	// 3. First Collimator: Yellow
	G4VisAttributes* Collimator1_VisAtt = new G4VisAttributes(G4Colour(1, 1, 0));
	Collimator1_VisAtt->SetForceSolid(true);
	Collimator1_LV->SetVisAttributes(Collimator1_VisAtt);

	// 4. Sample: Cyan
	G4VisAttributes* Sample_VisAtt = new G4VisAttributes(G4Colour(0, 1, 1));
	Sample_VisAtt->SetForceSolid(true);
	Sample_LV->SetVisAttributes(Sample_VisAtt);

	// 5. Second Collimator: Blue
	G4VisAttributes* Collimator2_VisAtt = new G4VisAttributes(G4Colour(0, 0, 1));
	Collimator2_VisAtt->SetForceSolid(true);
	if (Collimator2_LV) {
		Collimator2_LV->SetVisAttributes(Collimator2_VisAtt);
	}

	// 6. NaI Detector: Green
	G4VisAttributes* NaI_VisAtt = new G4VisAttributes(G4Colour(0, 1, 0));
	NaI_VisAtt->SetForceSolid(true);
	NaI_LV->SetVisAttributes(NaI_VisAtt);

	// ===============================================
	// ADDITIONAL SETTINGS
	// ===============================================

	// 1. Define a region for the Sample to set production cuts
	G4Region* Sample_Region = G4RegionStore::GetInstance()->FindOrCreateRegion("SampleRegion");
	Sample_Region->AddRootLogicalVolume(Sample_LV);
	G4ProductionCuts* sampleCuts = new G4ProductionCuts();
	sampleCuts->SetProductionCut(1 * nm, G4ProductionCuts::GetIndex("gamma"));
	sampleCuts->SetProductionCut(1 * nm, G4ProductionCuts::GetIndex("e-"));
	sampleCuts->SetProductionCut(1 * nm, G4ProductionCuts::GetIndex("e+"));
	Sample_Region->SetProductionCuts(sampleCuts);

	return World_Phys;
}

void MyDetectorConstruction::UpdateGeometry() {
	auto runManager = G4RunManager::GetRunManager();
	runManager->ReinitializeGeometry();         // rebuild geometry
	ConstructSDandField();                      // <--- reattach SDs manually
	// Print updated parameters
	G4cout << "\n================ Updated Geometry ================\n";
	G4cout << " Sample Thickness: " << fSample_Thickness / mm << " mm\n";
	G4cout << " Sample Material : " << fSampleMaterial << "\n";
	G4cout << " Collimator2 Height     : " << fCollimator2_Height / mm << " mm\n";
}

// NEW: getter so other classes (e.g. MyRunAction) can tag their output
// with the material that was actually used for that run.
G4String MyDetectorConstruction::GetSampleMaterialName() const {
	return fSampleMaterial;
}

// NEW: getter so other classes (e.g. MyRunAction) can tag their output
// with the sample thickness actually used for that run.
G4double MyDetectorConstruction::GetSampleThickness() const {
	return fSample_Thickness;
}

// =========================================================================
// Construct Sensitive Detectors
// =========================================================================
void MyDetectorConstruction::ConstructSDandField() {

	G4cout << "\n================ ConstructSDandField() =================\n";

	// --- 1. Lấy SDManager và LogicalVolumeStore ---
	auto sdManager = G4SDManager::GetSDMpointer();
	auto LVS = G4LogicalVolumeStore::GetInstance();

	// --- 2. NaI Detector ---
	G4String SDname = "NaI_SD";
	auto NaI_Detector = dynamic_cast<MySensitiveDetector*>(sdManager->FindSensitiveDetector(SDname, false));
	if (!NaI_Detector) {
		NaI_Detector = new MySensitiveDetector(SDname, "NaIHitsCollection");
		sdManager->AddNewDetector(NaI_Detector);
		//G4cout << "[SD] Created new NaI_Detector" << G4endl;
	}
	else {
		//G4cout << "[SD] Reusing existing NaI_Detector" << G4endl;
	}
	// Gán SD
	for (auto lv : *LVS) {
		if (lv->GetName() == "NaI_LV") {
			lv->SetSensitiveDetector(NaI_Detector);
			//G4cout << "Attached NaI_Detector to " << lv->GetName() << G4endl;
		}
	}

	G4cout << "========================================================\n\n";
}